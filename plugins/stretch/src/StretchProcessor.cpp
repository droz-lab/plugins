#include "StretchProcessor.h"
#include "StretchEditor.h"

namespace idg
{
const juce::StringArray& StretchProcessor::rateNames()
{
    static const juce::StringArray r { "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1/1", "2/1" };
    return r;
}

double StretchProcessor::rateBeats (int i)
{
    static const double b[] { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.5, 2.0 / 3.0, 1.0, 2.0, 4.0, 8.0 };
    return b[juce::jlimit (0, 9, i)];
}

static juce::String hzText (float v)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz"
                        : juce::String (v, v < 10.0f ? 1 : 0) + " Hz";
}

static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto hz = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return hzText (v); });
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    auto logRange = [] (float lo, float hi)
    {
        return NormalisableRange<float> (lo, hi,
            [] (float a, float b, float v) { return a * std::pow (b / a, v); },
            [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); });
    };

    // v3 : identifiants neufs (les réglages des versions précédentes ne s'appliquent plus)
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "rate", 1 }, "Rate", StretchProcessor::rateNames(), 2));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "grain_hz", 1 }, "Grain", logRange (1.0f, 500.0f), 40.0f, hz));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "hp", 1 }, "Coupe-bas", logRange (20.0f, 20000.0f), 20.0f, hz));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "lp", 1 }, "Coupe-haut", logRange (20.0f, 20000.0f), 20000.0f, hz));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "reso", 1 }, "Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    return l;
}

StretchProcessor::StretchProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "IDGemsStretch", makeLayout())
{
    pRate = apvts.getRawParameterValue ("rate");
    pGrain = apvts.getRawParameterValue ("grain_hz");
    pHp = apvts.getRawParameterValue ("hp");
    pLp = apvts.getRawParameterValue ("lp");
    pReso = apvts.getRawParameterValue ("reso");
}

bool StretchProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return l.getMainInputChannelSet() == out;
}

void StretchProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    engine.prepare (sampleRate);
    freeBeat = 0.0;
}

void StretchProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numCh = juce::jmin (buffer.getNumChannels(), getTotalNumInputChannels());
    if (n == 0 || numCh == 0) return;
    for (int c = getTotalNumInputChannels(); c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);

    double bpm = 120.0, beat = freeBeat;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 999.0, *b);
            playing = pos->getIsPlaying();
            if (playing)
                if (auto q = pos->getPpqPosition()) beat = *q;
        }
    const double spb = sr * 60.0 / bpm;

    StretchEngine::Settings s;
    s.rateBeats = rateBeats ((int) pRate->load());
    s.grainHz = pGrain->load();
    s.hpHz = pHp->load();
    s.lpHz = pLp->load();
    s.reso = pReso->load();

    float* L = buffer.getWritePointer (0);
    float* R = numCh > 1 ? buffer.getWritePointer (1) : L;
    if (numCh > 1)
        engine.process (L, R, n, beat, spb, s);
    else
    {
        // mono : on traite une copie à droite qu'on jette (pas d'allocation : pile limitée par blocs)
        float tmp[256];
        for (int pos = 0; pos < n; pos += 256)
        {
            const int m = juce::jmin (256, n - pos);
            std::copy (L + pos, L + pos + m, tmp);
            engine.process (L + pos, tmp, m, beat + pos / spb, spb, s);
        }
    }

    freeBeat = playing ? beat + n / spb : freeBeat + n / spb;
    const double stepPos = (beat + n / spb) / s.rateBeats;
    uiPhase = (float) (stepPos - std::floor (stepPos));
    uiStep = (int) ((int64_t) std::floor (stepPos) & 0x7fffffff);
    uiLevel = buffer.getMagnitude (0, n);
    {
        int w = scopeWrite.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            scope[(size_t) w] = 0.5f * (L[i] + R[i]);
            w = (w + 1) & (scopeSize - 1);
        }
        scopeWrite.store (w, std::memory_order_release);
    }
    const double stepLen = s.rateBeats * spb;
    uiCapture = (float) (juce::jmin (sr / juce::jmax (1.0, (double) s.grainHz), stepLen * 0.5) / stepLen);
    uiRepeats = (float) (stepLen * (double) s.grainHz / sr);
}

juce::AudioProcessorEditor* StretchProcessor::createEditor() { return new StretchEditor (*this); }

void StretchProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("uiScale", editorScale.load(), nullptr);
    state.setProperty ("uiW", editorW.load(), nullptr);
    state.setProperty ("uiH", editorH.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void StretchProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto t = juce::ValueTree::fromXml (*xml);
        if (t.isValid() && t.hasType (apvts.state.getType()))
        {
            if (t.hasProperty ("uiScale")) editorScale = (float) t.getProperty ("uiScale");
            editorW = (int) t.getProperty ("uiW", 0);
            editorH = (int) t.getProperty ("uiH", 0);
            apvts.replaceState (t);
        }
    }
}
} // namespace idg

#if ! IDGEMS_TESTS
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new idg::StretchProcessor(); }
#endif
