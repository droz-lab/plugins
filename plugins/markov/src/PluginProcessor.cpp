#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
// Raise at each change of the state format, and convert the old one in setStateInformation.
constexpr int stateVersion = 1;

constexpr int defaultLo[markov::numTracks] = { 36, 48, 60, 67, 55, 72, 43, 64 };
constexpr int defaultHi[markov::numTracks] = { 48, 64, 76, 84, 72, 91, 60, 84 };

juce::String floatsToString (const float* values, int count)
{
    juce::String s;
    s.preallocateBytes (static_cast<size_t> (count) * 7);
    for (int i = 0; i < count; ++i)
    {
        if (i > 0)
            s << ' ';
        s << juce::String (values[i], 4);
    }
    return s;
}

/** Tolerant reading: wrong count or garbage = false, nothing written. */
bool stringToFloats (const juce::String& text, float* out, int count, float lo, float hi)
{
    const auto tokens = juce::StringArray::fromTokens (text, " ", "");
    if (tokens.size() != count)
        return false;
    for (int i = 0; i < count; ++i)
    {
        const float v = tokens[i].getFloatValue();
        out[i] = std::isfinite (v) ? juce::jlimit (lo, hi, v) : lo;
    }
    return true;
}
} // namespace

//==============================================================================
namespace markovui
{
juce::String noteName (int midiNote, bool withOctave)
{
    static const char* names[] = { "Do", "Do#", "Ré", "Ré#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" };
    const int n = juce::jlimit (0, 127, midiNote);
    auto s = juce::String::fromUTF8 (names[n % 12]);
    if (withOctave)
        s << (n / 12 - 1);
    return s;
}

const juce::StringArray& scaleNames()
{
    static const juce::StringArray names {
        "Majeur", "Mineur", "Dorien", "Phrygien", "Lydien", "Mixolydien", "Locrien", "Pentatonique", "Chromatique",
        juce::String::fromUTF8 ("Personnalisée")
    };
    return names;
}

const juce::StringArray& divisionNames()
{
    static const juce::StringArray names { "1/4", "1/8", "1/8 triolet", "1/16", "1/16 triolet", "1/32" };
    return names;
}

double divisionPpq (int index)
{
    static const double values[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
    return values[juce::jlimit (0, 5, index)];
}
} // namespace markovui

//==============================================================================
PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ETAT", createLayout())
{
    pScale = apvts.getRawParameterValue ("gamme");
    pRoot = apvts.getRawParameterValue ("tonique");
    for (int i = 0; i < 12; ++i)
        pCustom[(size_t) i] = apvts.getRawParameterValue ("perso_" + juce::String (i));
    pSeed = apvts.getRawParameterValue ("graine");
    pTemperature = apvts.getRawParameterValue ("temperature");
    pOrder = apvts.getRawParameterValue ("ordre");
    pRhythm = apvts.getRawParameterValue ("rythme");
    pLearn = apvts.getRawParameterValue ("apprendre");
    pPreview = apvts.getRawParameterValue ("ecoute");
    pPreviewLevel = apvts.getRawParameterValue ("volume_ecoute");
    for (int t = 0; t < markov::numTracks; ++t)
    {
        auto& p = trackParams[(size_t) t];
        p.enabled = apvts.getRawParameterValue (trackParam (t, "actif"));
        p.lo = apvts.getRawParameterValue (trackParam (t, "min"));
        p.hi = apvts.getRawParameterValue (trackParam (t, "max"));
        p.steps = apvts.getRawParameterValue (trackParam (t, "pas"));
        p.division = apvts.getRawParameterValue (trackParam (t, "division"));
        p.gate = apvts.getRawParameterValue (trackParam (t, "duree"));
        p.afterHit = apvts.getRawParameterValue (trackParam (t, "apres_note"));
        p.afterRest = apvts.getRawParameterValue (trackParam (t, "apres_silence"));
    }

    markov::resetPattern (master);
    working = master;
    learner.reset();
    learnedCopy.reset();
    startTimerHz (10);
}

PluginProcessor::~PluginProcessor()
{
    stopTimer();
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createLayout()
{
    // Identifiers (and their version number 1) NEVER change after a release:
    // DAW projects and automations depend on them. Labels may change.
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    using juce::ParameterID;
    const auto fr = [] (const char* s) { return juce::String::fromUTF8 (s); };
    // On/off switches as two-item lists ("Non", "Oui") rather than a Bool parameter: a Bool keeps a raw
    // host value like 0.3 that does not survive save / reload (seen with the common "etat" test).
    const auto onOff = [] (const juce::String& id, const juce::String& name, bool on) {
        return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, juce::StringArray { "Non", "Oui" }, on ? 1 : 0);
    };

    layout.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { "gamme", 1 }, "Gamme", markovui::scaleNames(), 0));
    juce::StringArray roots;
    for (int i = 0; i < 12; ++i)
        roots.add (markovui::noteName (i, false));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { "tonique", 1 }, "Tonique", roots, 0));
    for (int i = 0; i < 12; ++i)
    {
        const bool inMajor = i == 0 || i == 2 || i == 4 || i == 5 || i == 7 || i == 9 || i == 11;
        layout.add (onOff ("perso_" + juce::String (i), fr ("Gamme perso +") + juce::String (i), inMajor));
    }
    layout.add (std::make_unique<juce::AudioParameterInt> (ParameterID { "graine", 1 }, "Graine", 1, 9999, 1));

    juce::NormalisableRange<float> tempRange (0.1f, 4.0f, 0.01f);
    tempRange.setSkewForCentre (1.0f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterID { "temperature", 1 }, fr ("Température"), tempRange, 1.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { "ordre", 1 }, "Ordre",
                                                              juce::StringArray { "Ordre 1", "Ordre 2" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { "rythme", 1 }, "Rythme",
                                                              juce::StringArray { fr ("Pas à pas"), fr ("Chaîne") }, 0));
    layout.add (onOff ("apprendre", "Apprendre", false));
    layout.add (onOff ("ecoute", fr ("Écoute"), false));
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterID { "volume_ecoute", 1 }, fr ("Volume écoute"),
                                                             juce::NormalisableRange<float> (-40.0f, 0.0f, 0.1f), -12.0f,
                                                             juce::AudioParameterFloatAttributes().withLabel ("dB")));

    const auto noteAttributes = juce::AudioParameterIntAttributes().withStringFromValueFunction (
        [] (int v, int) { return markovui::noteName (v); });
    const auto percent = juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (
        [] (float v, int) { return juce::String (juce::roundToInt (v)); });
    for (int t = 0; t < markov::numTracks; ++t)
    {
        const auto name = "Piste " + juce::String (t + 1) + " ";
        layout.add (onOff (trackParam (t, "actif"), name + "active", t < 4));
        layout.add (std::make_unique<juce::AudioParameterInt> (ParameterID { trackParam (t, "min"), 1 }, name + "note min", 0, 127,
                                                               defaultLo[t], noteAttributes));
        layout.add (std::make_unique<juce::AudioParameterInt> (ParameterID { trackParam (t, "max"), 1 }, name + "note max", 0, 127,
                                                               defaultHi[t], noteAttributes));
        layout.add (std::make_unique<juce::AudioParameterInt> (ParameterID { trackParam (t, "pas"), 1 }, name + "nombre de pas", 1,
                                                               markov::maxSteps, 16));
        layout.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { trackParam (t, "division"), 1 }, name + "division",
                                                                  markovui::divisionNames(), 3));
        layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterID { trackParam (t, "duree"), 1 }, name + fr ("durée"),
                                                                 juce::NormalisableRange<float> (5.0f, 100.0f, 1.0f), 50.0f, percent));
        layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterID { trackParam (t, "apres_note"), 1 }, name + fr ("après une note"),
                                                                 juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 60.0f, percent));
        layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterID { trackParam (t, "apres_silence"), 1 }, name + fr ("après un silence"),
                                                                 juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 90.0f, percent));
    }
    return layout;
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr > 0.0 ? sr : 48000.0;
    previewGain.reset (sampleRate, 20.0f);
    previewGain.snap (0.0f);
    const auto coeff = [this] (float ms) { return 1.0f - static_cast<float> (std::exp (-1.0 / (0.001 * ms * sampleRate))); };
    attackCoeff = coeff (2.0f);
    decayCoeff = coeff (350.0f);
    releaseCoeff = coeff (40.0f);
    for (auto& v : voices)
        v = {};
    for (auto& r : runtime)
        r = {};
    wasPlaying = false;
    tablesDirty = true;
}

void PluginProcessor::releaseResources()
{
    // The host will not call processBlock any more: forget the notes (it is the host's job to silence
    // a source that stops; we never keep a note "on" across a restart).
    for (auto& r : runtime)
        r.activeNote = -1;
    wasPlaying = false;
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (! layouts.getMainInputChannelSet().isDisabled() && layouts.inputBuses.size() > 0)
        return false;
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

markov::Scale PluginProcessor::currentScale() const
{
    uint16_t mask = 0;
    for (int i = 0; i < 12; ++i)
        if (pCustom[(size_t) i]->load() > 0.5f)
            mask = static_cast<uint16_t> (mask | (1u << i));
    return markov::makeScale (static_cast<int> (pScale->load()), static_cast<int> (pRoot->load()), mask);
}

//==============================================================================
// Audio thread
void PluginProcessor::syncPattern()
{
    const auto v = patternVersion.load (std::memory_order_acquire);
    if (v == workingVersion)
        return;
    const juce::SpinLock::ScopedTryLockType lock (patternLock);
    if (! lock.isLocked())
        return; // keep the previous copy, retry next block
    std::memcpy (&working, &master, sizeof (markov::Pattern));
    workingVersion = v;
    tablesDirty = true;
}

void PluginProcessor::readSettings()
{
    const auto newScale = currentScale();
    const bool scaleChanged = newScale.size != scale.size || newScale.root != scale.root
                              || std::memcmp (newScale.offsets, scale.offsets, sizeof (scale.offsets)) != 0;
    scale = newScale;
    const float newTemperature = pTemperature->load();
    if (scaleChanged || ! juce::exactlyEqual (newTemperature, temperature))
        tablesDirty = true;
    temperature = newTemperature;

    const bool chain = pRhythm->load() > 0.5f;
    const int order = pOrder->load() > 0.5f ? 2 : 1;
    for (int t = 0; t < markov::numTracks; ++t)
    {
        const auto& p = trackParams[(size_t) t];
        auto& s = settings[(size_t) t];
        s.lo = static_cast<int> (p.lo->load());
        s.hi = static_cast<int> (p.hi->load());
        s.numSteps = static_cast<int> (p.steps->load());
        s.chainRhythm = chain;
        s.afterHit = p.afterHit->load() * 0.01f;
        s.afterRest = p.afterRest->load() * 0.01f;
        s.order = order;
    }

    if (tablesDirty)
    {
        for (int t = 0; t < markov::numTracks; ++t)
            markov::buildTables (working.tracks[t], scale.size, temperature, tables[(size_t) t]);
        tablesDirty = false;
    }
}

void PluginProcessor::resync (int t, double ppq)
{
    auto& r = runtime[(size_t) t];
    const double len = markovui::divisionPpq (r.division);
    const auto first = static_cast<int64_t> (std::ceil (ppq / len - 1.0e-9));
    r.nextStep = std::max<int64_t> (0, first);
    r.sim.reset();
    // Walk the chain up to the current position: playback is a function of the position,
    // so a loop or a jump replays exactly what was heard (and what an export will give).
    // Very far positions: start the walk 65536 steps earlier (bounded cost).
    const int64_t from = std::max<int64_t> (0, r.nextStep - 65536);
    for (int64_t k = from; k < r.nextStep; ++k)
        r.sim.step (k, t, seed, working.tracks[t], tables[(size_t) t], scale, settings[(size_t) t]);
}

void PluginProcessor::allNotesOff (juce::MidiBuffer& midi, int sample)
{
    for (int t = 0; t < markov::numTracks; ++t)
    {
        auto& r = runtime[(size_t) t];
        if (r.activeNote >= 0)
            midi.addEvent (juce::MidiMessage::noteOff (t + 1, r.activeNote), sample);
        r.activeNote = -1;
    }
}

void PluginProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();
    midi.clear();
    allNotesOff (midi, 0);
    wasPlaying = false;
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int numSamples = buffer.getNumSamples();

    syncPattern();
    readSettings();

    // Learning: incoming notes become degree transitions, then the input MIDI is consumed.
    if (learnResetRequest.exchange (false))
    {
        learner.reset();
        learnedNotes = 0;
    }
    if (pLearn->load() > 0.5f)
    {
        bool learned = false;
        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            if (m.isNoteOn())
            {
                learner.addDegree (scale.degreeOf (m.getNoteNumber()));
                learned = true;
            }
        }
        if (learned)
        {
            learnedNotes = learner.total;
            const juce::SpinLock::ScopedTryLockType lock (learnLock);
            if (lock.isLocked())
            {
                std::memcpy (&learnedCopy, &learner, sizeof (markov::Learner));
                learnVersion.fetch_add (1, std::memory_order_release);
            }
        }
    }
    midi.clear();

    // Transport: always from the host (tempo, position, play state), never from a system timer.
    bool playing = false;
    double ppq0 = 0.0, bpm = 0.0;
    if (auto* head = getPlayHead())
        if (const auto pos = head->getPosition())
            if (pos->getIsPlaying() && pos->getPpqPosition().hasValue() && pos->getBpm().hasValue())
            {
                ppq0 = *pos->getPpqPosition();
                bpm = *pos->getBpm();
                playing = bpm > 1.0 && std::isfinite (bpm) && std::isfinite (ppq0);
            }
    if (bpm > 1.0)
        hostBpm = bpm;
    hostPlaying = playing;

    if (! playing || numSamples == 0)
    {
        allNotesOff (midi, 0); // stop: every note gets its note-off (nothing if none is on)
        wasPlaying = false;
        renderPreview (buffer, midi);
        return;
    }

    const double samplesPerPpq = sampleRate * 60.0 / bpm;
    const double ppq1 = ppq0 + numSamples / samplesPerPpq;
    // Positions are compared in samples (not in beats): float noise on the host position
    // must not pull an event that belongs to the next block into this one.
    const auto offsetOf = [&] (double ppq) {
        return static_cast<int64_t> (std::ceil ((ppq - ppq0) * samplesPerPpq - 1.0e-6));
    };
    const auto sampleAt = [&] (double ppq) { return static_cast<int> (std::max<int64_t> (0, offsetOf (ppq))); };

    // Jump, loop or (re)start: silence everything and re-derive the state at the new position.
    const bool jumped = ! wasPlaying || std::abs (ppq0 - expectedPpq) > 1.0e-3;
    if (jumped)
        allNotesOff (midi, 0);

    const uint32_t newSeed = static_cast<uint32_t> (pSeed->load());
    const bool seedChanged = newSeed != seed;
    seed = newSeed;

    for (int t = 0; t < markov::numTracks; ++t)
    {
        auto& r = runtime[(size_t) t];
        const auto& p = trackParams[(size_t) t];
        const bool enabled = p.enabled->load() > 0.5f;
        const int division = static_cast<int> (p.division->load());

        if (! enabled)
        {
            if (r.activeNote >= 0) // track switched off: its note stops now
                midi.addEvent (juce::MidiMessage::noteOff (t + 1, r.activeNote), 0);
            r.activeNote = -1;
            r.enabled = false;
            continue;
        }
        if (jumped || seedChanged || ! r.enabled || division != r.division)
        {
            if (r.activeNote >= 0)
                midi.addEvent (juce::MidiMessage::noteOff (t + 1, r.activeNote), 0);
            r.activeNote = -1;
            r.division = division;
            r.enabled = true;
            resync (t, ppq0);
        }

        const double len = markovui::divisionPpq (division);
        const double gate = juce::jlimit (0.05, 1.0, static_cast<double> (p.gate->load()) * 0.01);
        const auto& s = settings[(size_t) t];

        for (;;)
        {
            const double stepPpq = static_cast<double> (r.nextStep) * len;
            if (offsetOf (stepPpq) >= numSamples)
                break;
            // Note-off of the previous note first (gate <= 100 %: never after this step).
            if (r.activeNote >= 0 && r.offPpq <= stepPpq)
            {
                midi.addEvent (juce::MidiMessage::noteOff (t + 1, r.activeNote), sampleAt (r.offPpq));
                r.activeNote = -1;
            }
            const auto ev = r.sim.step (r.nextStep, t, seed, working.tracks[t], tables[(size_t) t], scale, s);
            const int numSteps = juce::jlimit (1, markov::maxSteps, s.numSteps);
            currentStep[(size_t) t].store (static_cast<int> (r.nextStep % numSteps), std::memory_order_relaxed);
            if (ev.play)
            {
                const int at = sampleAt (stepPpq);
                if (r.activeNote >= 0)
                    midi.addEvent (juce::MidiMessage::noteOff (t + 1, r.activeNote), at);
                midi.addEvent (juce::MidiMessage::noteOn (t + 1, ev.pitch, static_cast<juce::uint8> (ev.velocity)), at);
                r.activeNote = ev.pitch;
                r.offPpq = stepPpq + gate * len;
                currentDegree[(size_t) t].store (ev.degree, std::memory_order_relaxed);
                notesPlayed[(size_t) t].fetch_add (1, std::memory_order_relaxed);
            }
            ++r.nextStep;
        }
        if (r.activeNote >= 0 && offsetOf (r.offPpq) < numSamples)
        {
            midi.addEvent (juce::MidiMessage::noteOff (t + 1, r.activeNote), sampleAt (r.offPpq));
            r.activeNote = -1;
        }
    }

    expectedPpq = ppq1;
    wasPlaying = true;
    renderPreview (buffer, midi);
}

void PluginProcessor::renderPreview (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi)
{
    // Simple pluck per track, to hear the patterns without a synth (and for the WAV renders).
    const bool on = pPreview->load() > 0.5f;
    previewGain.setTarget (on ? droz::dbToGain (pPreviewLevel->load(), -40.0f) : 0.0f);
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numChannels == 0)
        return;

    auto it = midi.cbegin();
    for (int i = 0; i < numSamples; ++i)
    {
        for (; it != midi.cend() && (*it).samplePosition <= i; ++it)
        {
            const auto m = (*it).getMessage();
            const int t = m.getChannel() - 1;
            if (t < 0 || t >= markov::numTracks)
                continue;
            auto& v = voices[(size_t) t];
            if (m.isNoteOn())
            {
                v.increment = juce::MathConstants<double>::twoPi * juce::MidiMessage::getMidiNoteInHertz (m.getNoteNumber()) / sampleRate;
                v.peak = 0.25f * m.getFloatVelocity();
                v.attack = v.held = true;
            }
            else if (m.isNoteOff())
                v.held = v.attack = false;
        }

        const float g = previewGain.next();
        float left = 0.0f, right = 0.0f;
        for (int t = 0; t < markov::numTracks; ++t)
        {
            auto& v = voices[(size_t) t];
            if (v.attack)
            {
                v.env += (v.peak - v.env) * attackCoeff;
                if (v.env >= 0.98f * v.peak)
                    v.attack = false;
            }
            else
                v.env -= v.env * (v.held ? decayCoeff : releaseCoeff);
            if (v.env < 1.0e-6f)
            {
                v.env = 0.0f;
                continue;
            }
            const float x = v.env * static_cast<float> (std::sin (v.phase) + 0.3 * std::sin (2.0 * v.phase));
            v.phase += v.increment;
            if (v.phase > juce::MathConstants<double>::twoPi)
                v.phase -= juce::MathConstants<double>::twoPi;
            const float pan = 0.2f + 0.6f * static_cast<float> (t) / static_cast<float> (markov::numTracks - 1);
            left += x * (1.0f - pan);
            right += x * pan;
        }
        // Safety limiter: soft clip, and nothing non-finite leaves the plugin.
        left = std::tanh (left * g);
        right = std::tanh (right * g);
        if (! std::isfinite (left) || ! std::isfinite (right))
            left = right = 0.0f;
        buffer.setSample (0, i, numChannels > 1 ? left : 0.5f * (left + right));
        if (numChannels > 1)
            buffer.setSample (1, i, right);
    }
}

//==============================================================================
// Message thread
markov::TrackData PluginProcessor::getTrack (int track) const
{
    const juce::SpinLock::ScopedLockType lock (patternLock);
    return master.tracks[juce::jlimit (0, markov::numTracks - 1, track)];
}

void PluginProcessor::editTrack (int track, const std::function<void (markov::TrackData&)>& edit)
{
    {
        const juce::SpinLock::ScopedLockType lock (patternLock);
        edit (master.tracks[juce::jlimit (0, markov::numTracks - 1, track)]);
    }
    patternVersion.fetch_add (1, std::memory_order_release);
}

void PluginProcessor::resetLearning()
{
    learnResetRequest = true;
    learnAppliedVersion = learnVersion.load();
    learnedNotes = 0;
    editTrack (selectedTrack.load(), [] (markov::TrackData& t) { markov::resetMatrix (t); });
}

void PluginProcessor::applyLearning()
{
    const auto v = learnVersion.load (std::memory_order_acquire);
    if (v == learnAppliedVersion || pLearn->load() < 0.5f)
        return;
    learnAppliedVersion = v;
    {
        const juce::SpinLock::ScopedLockType lock (learnLock);
        std::memcpy (&learnedApplied, &learnedCopy, sizeof (markov::Learner));
    }
    const int size = currentScale().size;
    editTrack (selectedTrack.load(), [&] (markov::TrackData& t) { learnedApplied.apply (t, size); });
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
// State
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("version_etat", stateVersion, nullptr);
    state.setProperty ("largeur", editorWidth.load(), nullptr);
    state.setProperty ("hauteur", editorHeight.load(), nullptr);
    state.setProperty ("piste_choisie", selectedTrack.load(), nullptr);

    markov::Pattern copy;
    {
        const juce::SpinLock::ScopedLockType lock (patternLock);
        std::memcpy (&copy, &master, sizeof (markov::Pattern));
    }
    juce::ValueTree motif ("MOTIF");
    for (int t = 0; t < markov::numTracks; ++t)
    {
        const auto& d = copy.tracks[t];
        juce::ValueTree piste ("PISTE");
        piste.setProperty ("numero", t, nullptr);
        piste.setProperty ("editee", d.edited, nullptr);
        if (d.edited)
            piste.setProperty ("matrice", floatsToString (&d.w[0][0], markov::maxDegrees * markov::maxDegrees), nullptr);
        bool any2 = false;
        float rows2[markov::maxDegrees * markov::maxDegrees];
        for (int a = 0; a < markov::maxDegrees; ++a)
            for (int b = 0; b < markov::maxDegrees; ++b)
            {
                rows2[a * markov::maxDegrees + b] = d.has2[a][b] ? 1.0f : 0.0f;
                any2 = any2 || d.has2[a][b];
            }
        if (any2)
        {
            piste.setProperty ("ordre2_lignes", floatsToString (rows2, markov::maxDegrees * markov::maxDegrees), nullptr);
            piste.setProperty ("ordre2", floatsToString (&d.w2[0][0][0], markov::maxDegrees * markov::maxDegrees * markov::maxDegrees), nullptr);
        }
        float vel[markov::maxSteps], spread[markov::maxSteps];
        for (int s = 0; s < markov::maxSteps; ++s)
        {
            vel[s] = d.velocity[s];
            spread[s] = d.spread[s];
        }
        piste.setProperty ("proba", floatsToString (d.prob, markov::maxSteps), nullptr);
        piste.setProperty ("velocite", floatsToString (vel, markov::maxSteps), nullptr);
        piste.setProperty ("ecart", floatsToString (spread, markov::maxSteps), nullptr);
        motif.appendChild (piste, nullptr);
    }
    state.appendChild (motif, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return; // unreadable state: keep the current settings rather than crash

    auto state = juce::ValueTree::fromXml (*xml);
    // Version 1 is the first format. Convert older formats here when version_etat < stateVersion.
    editorWidth = juce::jlimit (baseWidth / 2, baseWidth * 2, static_cast<int> (state.getProperty ("largeur", 980)));
    editorHeight = juce::jlimit (baseHeight / 2, baseHeight * 2, static_cast<int> (state.getProperty ("hauteur", 665)));
    selectedTrack = juce::jlimit (0, markov::numTracks - 1, static_cast<int> (state.getProperty ("piste_choisie", 0)));

    markov::Pattern loaded;
    markov::resetPattern (loaded);
    const auto motif = state.getChildWithName ("MOTIF");
    for (const auto& piste : motif)
    {
        const int t = static_cast<int> (piste.getProperty ("numero", -1));
        if (t < 0 || t >= markov::numTracks)
            continue;
        auto& d = loaded.tracks[t];
        d.edited = static_cast<bool> (piste.getProperty ("editee", false))
                   && stringToFloats (piste.getProperty ("matrice").toString(), &d.w[0][0], markov::maxDegrees * markov::maxDegrees, 0.0f, 1.0f);
        float rows2[markov::maxDegrees * markov::maxDegrees];
        if (stringToFloats (piste.getProperty ("ordre2_lignes").toString(), rows2, markov::maxDegrees * markov::maxDegrees, 0.0f, 1.0f)
            && stringToFloats (piste.getProperty ("ordre2").toString(), &d.w2[0][0][0],
                               markov::maxDegrees * markov::maxDegrees * markov::maxDegrees, 0.0f, 1.0f))
            for (int a = 0; a < markov::maxDegrees; ++a)
                for (int b = 0; b < markov::maxDegrees; ++b)
                    d.has2[a][b] = rows2[a * markov::maxDegrees + b] > 0.5f;
        stringToFloats (piste.getProperty ("proba").toString(), d.prob, markov::maxSteps, 0.0f, 1.0f);
        float vel[markov::maxSteps], spread[markov::maxSteps];
        if (stringToFloats (piste.getProperty ("velocite").toString(), vel, markov::maxSteps, 1.0f, 127.0f)
            && stringToFloats (piste.getProperty ("ecart").toString(), spread, markov::maxSteps, 0.0f, 126.0f))
            for (int s = 0; s < markov::maxSteps; ++s)
            {
                d.velocity[s] = static_cast<uint8_t> (juce::roundToInt (vel[s]));
                d.spread[s] = static_cast<uint8_t> (juce::roundToInt (spread[s]));
            }
    }
    {
        const juce::SpinLock::ScopedLockType lock (patternLock);
        std::memcpy (&master, &loaded, sizeof (markov::Pattern));
    }
    patternVersion.fetch_add (1, std::memory_order_release);

    state.removeChild (state.getChildWithName ("MOTIF"), nullptr);
    apvts.replaceState (state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
