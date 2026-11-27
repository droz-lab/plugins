// Rendu hors ligne de l'effet Stretch : écrit un WAV sec + un WAV par réglage, pour écoute.
// Usage : IDGemsStretchRender <dossier_sortie> [boucle_seche.wav] [bpm]
// Sans boucle fournie, une boucle de test (batterie + basse + arpège) est synthétisée.
#include "Core/JuceIncludes.h"
#include "Effects/StretchEngine.h"
#include "StretchProcessor.h"

namespace
{
constexpr double kSr = 44100.0;

juce::AudioBuffer<float> makeTestLoop (double bpm, int bars)
{
    const double spb = kSr * 60.0 / bpm;
    const int n = (int) (spb * 4 * bars);
    juce::AudioBuffer<float> b (2, n);
    b.clear();
    juce::Random rnd (42);
    auto* L = b.getWritePointer (0);
    auto* R = b.getWritePointer (1);
    const int arp[] { 0, 7, 12, 15, 19, 15, 12, 7, 3, 10, 15, 19, 22, 19, 15, 10 };
    const int bassNotes[] { 0, 0, -2, -4 };
    double bassPh = 0, arpPh = 0, lp = 0, lpR = 0;

    for (int i = 0; i < n; ++i)
    {
        const double beat = i / spb;
        const double inBeat = beat - std::floor (beat);
        const auto beatIdx = (int) beat;
        const double six = beat * 4.0;
        const double inSix = six - std::floor (six);
        const auto sixIdx = (int) six;
        float l = 0, r = 0;

        // kick
        {
            double tt = inBeat * spb / kSr;
            double f = 45.0 + 110.0 * std::exp (-tt * 30.0);
            static double kp = 0; kp += f / kSr;
            float k = (float) (std::sin (juce::MathConstants<double>::twoPi * kp) * std::exp (-tt * 7.0));
            if (inBeat * spb < 2) kp = 0;
            l += 0.8f * k; r += 0.8f * k;
        }
        // snare sur 2 et 4
        if (beatIdx % 2 == 1)
        {
            double tt = inBeat * spb / kSr;
            float s = (float) ((rnd.nextFloat() * 2 - 1) * std::exp (-tt * 18.0) * 0.45
                               + std::sin (juce::MathConstants<double>::twoPi * 190.0 * tt) * std::exp (-tt * 25.0) * 0.3);
            l += s; r += s;
        }
        // charleston en doubles croches
        {
            double tt = inSix * spb * 0.25 / kSr;
            float h = (float) ((rnd.nextFloat() * 2 - 1) * std::exp (-tt * 60.0) * (sixIdx % 2 ? 0.12 : 0.2));
            static float hp = 0; float hh = h - hp; hp = h;
            l += hh * 0.9f; r += hh * 1.1f;
        }
        // basse (scie filtrée)
        {
            int bar = (int) (beat / 4.0) % 4;
            double f = 55.0 * std::pow (2.0, (bassNotes[bar] + (sixIdx % 8 == 6 ? 12 : 0)) / 12.0);
            bassPh += f / kSr; bassPh -= std::floor (bassPh);
            float saw = (float) (2.0 * bassPh - 1.0);
            lp += (saw - lp) * 0.06;
            float env = (float) std::exp (-inSix * 1.2);
            l += (float) lp * 0.55f * env; r += (float) lp * 0.55f * env;
        }
        // arpège (pulse filtré, un peu stéréo)
        {
            double f = 440.0 * std::pow (2.0, (arp[sixIdx % 16] - 9) / 12.0);
            arpPh += f / kSr; arpPh -= std::floor (arpPh);
            float pulse = arpPh < 0.3 ? 1.0f : -1.0f;
            lpR += (pulse - lpR) * 0.12;
            float env = (float) std::exp (-inSix * 2.5);
            l += (float) lpR * 0.16f * env; r += (float) lpR * 0.2f * env;
        }
        L[i] = l * 0.5f; R[i] = r * 0.5f;
    }
    return b;
}

bool writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
    if (os == nullptr) return false;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 2, 16, {}, 0));
    if (w == nullptr) return false;
    os.release();
    return w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

struct Preset { const char* file; const char* label; double rate; float hz, hp, lp, reso; };
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::printf ("usage: %s <dossier_sortie> [boucle.wav] [bpm]  |  --snapshot image.png\n", argv[0]); return 1; }
    if (juce::String (argv[1]) == "--snapshot" && argc > 2)
    {
        // capture de l'interface du plugin (vérification du rendu)
        idg::StretchProcessor proc;
        proc.prepareToPlay (44100.0, 512);
        if (argc > 3) // exemple de réglage du filtre pour vérifier la courbe
        {
            auto set = [&] (const char* id, float v) { auto* q = proc.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); };
            set ("hp", 150.0f); set ("lp", 4000.0f); set ("reso", 0.5f);
        }
        juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m;
        for (int k = 0; k < 40; ++k) { b.clear(); proc.processBlock (b, m); }
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        return 0;
    }
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    out.createDirectory();

    double bpm = argc > 3 ? std::atof (argv[3]) : 140.0;
    double sr = kSr;
    juce::AudioBuffer<float> dry;
    if (argc > 2)
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2])));
        if (rd == nullptr) { std::printf ("boucle illisible\n"); return 1; }
        sr = rd->sampleRate;
        dry.setSize (2, (int) rd->lengthInSamples);
        rd->read (&dry, 0, (int) rd->lengthInSamples, 0, true, true);
    }
    else
        dry = makeTestLoop (bpm, 4);

    writeWav (out.getChildFile ("00_sec.wav"), dry, sr);

    const Preset presets[] {
        { "01_1-16_40Hz.wav",            "1/16, grain 40 Hz (réglage Infiltrator)",  0.25, 40.0f,  20.0f,  20000.0f, 0.0f },
        { "02_1-16_20Hz.wav",            "1/16, grain 20 Hz",                        0.25, 20.0f,  20.0f,  20000.0f, 0.0f },
        { "03_1-16_80Hz.wav",            "1/16, grain 80 Hz",                        0.25, 80.0f,  20.0f,  20000.0f, 0.0f },
        { "04_1-8_40Hz.wav",             "1/8, grain 40 Hz",                         0.5,  40.0f,  20.0f,  20000.0f, 0.0f },
        { "05_1-32_40Hz.wav",            "1/32, grain 40 Hz",                        0.125, 40.0f, 20.0f,  20000.0f, 0.0f },
        { "07_1-4_grain_automatise_20-160Hz.wav", "1/4, grain automatisé 20 -> 160 Hz pendant le gel", 1.0, -1.0f, 20.0f, 20000.0f, 0.0f },
        { "06_1-16_40Hz_filtre.wav",     "1/16, 40 Hz, coupe-bas 200 Hz, coupe-haut 3 kHz, réso 40 %", 0.25, 40.0f, 200.0f, 3000.0f, 0.4f },
    };

    bool ok = true;
    for (auto& p : presets)
    {
        juce::AudioBuffer<float> b (dry);
        idg::StretchEngine e;
        e.prepare (sr);
        idg::StretchEngine::Settings s;
        s.rateBeats = p.rate; s.grainHz = p.hz; s.hpHz = p.hp; s.lpHz = p.lp; s.reso = p.reso;
        const double spb = sr * 60.0 / bpm;
        const int blk = 64;
        for (int pos = 0; pos < b.getNumSamples(); pos += blk)
        {
            const int n = juce::jmin (blk, b.getNumSamples() - pos);
            if (p.hz < 0.0f) // automation : balayage exponentiel du grain sur toute la durée
                s.grainHz = 20.0f * std::pow (8.0f, (float) pos / (float) b.getNumSamples());
            e.process (b.getWritePointer (0, pos), b.getWritePointer (1, pos), n, pos / spb, spb, s);
        }
        // contrôles : NaN, crête, plus gros saut entre deux échantillons (clics)
        float peak = 0, jump = 0; bool nan = false;
        for (int c = 0; c < 2; ++c)
        {
            auto* d = b.getReadPointer (c);
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                if (! std::isfinite (d[i])) nan = true;
                peak = juce::jmax (peak, std::abs (d[i]));
                if (i) jump = juce::jmax (jump, std::abs (d[i] - d[i - 1]));
            }
        }
        ok = ok && ! nan;
        std::printf ("%-34s %-45s crete %.2f  saut max %.3f %s\n", p.file, p.label, peak, jump, nan ? "NaN !" : "");
        writeWav (out.getChildFile (p.file), b, sr);
    }
    return ok ? 0 : 1;
}
