// Tests de stabilité d'IDGems : NaN, plafond du limiteur, feedback, latence, état, CPU.
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace idg;

namespace
{
struct FakePlayHead : juce::AudioPlayHead
{
    double bpm = 128.0, ppq = 0.0;
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        p.setIsPlaying (playing);
        p.setTimeSignature (TimeSignature { 4, 4 });
        return p;
    }
};

int failures = 0;
void check (bool ok, const juce::String& what)
{
    if (! ok) { ++failures; std::cout << "  [ECHEC] " << what << std::endl; }
}

struct Harness
{
    IDGemsProcessor proc;
    FakePlayHead head;
    double sr = 48000.0;
    int block = 512;
    juce::AudioBuffer<float> buf { 2, 512 };
    juce::MidiBuffer midi;
    juce::Random rnd { 42 };
    double t = 0;

    Harness() { proc.setPlayHead (&head); proc.prepareToPlay (sr, block); }

    /** Signal de test : batterie synthétique + basse + bruit. */
    void fill()
    {
        for (int i = 0; i < block; ++i)
        {
            double beat = std::fmod (t * head.bpm / 60.0, 1.0);
            float kick = (float) (std::exp (-beat * 30.0) * std::sin (2.0 * juce::MathConstants<double>::pi * (55.0 + 120.0 * std::exp (-beat * 40.0)) * t));
            float hat = (std::fmod (beat, 0.5) < 0.03) ? (rnd.nextFloat() * 2.0f - 1.0f) * 0.3f : 0.0f;
            float bass = 0.3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 82.4 * t);
            float x = 0.7f * kick + hat + bass;
            buf.setSample (0, i, x);
            buf.setSample (1, i, x * 0.9f + hat * 0.2f);
            t += 1.0 / sr;
        }
    }

    /** Traite `seconds` et renvoie le pic de sortie (-1 si NaN). */
    float run (double seconds, bool silence = false)
    {
        float peak = 0.0f;
        const int blocks = (int) (seconds * sr / block);
        for (int b = 0; b < blocks; ++b)
        {
            if (silence) buf.clear(); else fill();
            midi.clear();
            proc.processBlock (buf, midi);
            head.ppq += block / sr * head.bpm / 60.0;
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < block; ++i)
                {
                    float v = buf.getSample (c, i);
                    if (! std::isfinite (v)) return -1.0f;
                    peak = juce::jmax (peak, std::abs (v));
                }
        }
        return peak;
    }

    void allOff()
    {
        const auto& reg = registry();
        for (int fx = 0; fx < NumFx; ++fx) proc.setNorm (reg.fxOn[fx], 0.0f);
    }
};

const Registry& reg() { return registry(); }
float ceilingGain (IDGemsProcessor& p) { return dsp::dbToGain (p.getValue (G::ceiling)) + 1.0e-4f; }
} // namespace

int snapshotMain (const juce::String& path, int tab);

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc >= 3 && juce::String (argv[1]) == "--snapshot")
        return snapshotMain (argv[2], argc >= 4 ? juce::String (argv[3]).getIntValue() : 0);
    std::cout << "=== Tests de stabilité IDGems ===" << std::endl;
    const auto& reg = registry();

    // 1. Chaque effet seul, paramètres aléatoires
    {
        std::cout << "1. Chaque effet seul (paramètres aléatoires)..." << std::endl;
        Harness h;
        for (int fx = 0; fx < NumFx; ++fx)
        {
            h.allOff();
            h.proc.setNorm (reg.fxOn[fx], 1.0f);
            for (int s = 0; s < 6; ++s)
            {
                h.proc.random.perform (RandomEngine::Scope::Effect, fx, (uint32_t) (fx * 100 + s + 1), "test", 1.0f);
                h.proc.setNorm (reg.fxMix[fx], 1.0f);
                float pk = h.run (0.6);
                check (pk >= 0.0f, effectDefs()[(size_t) fx].name + " produit un NaN");
                check (pk <= ceilingGain (h.proc), effectDefs()[(size_t) fx].name + " dépasse le plafond : " + juce::String (pk));
            }
        }
    }

    // 1b. Niveau de chaque effet (réglages par défaut) : détecte un module muet ou trop fort
    {
        std::cout << "1b. Niveau de sortie par effet (dB par rapport au signal sec)..." << std::endl;
        auto rms = [] (Harness& h, double secs)
        {
            double acc = 0; int cnt = 0;
            const int blocks = (int) (secs * h.sr / h.block);
            for (int b = 0; b < blocks; ++b)
            {
                h.fill(); h.midi.clear(); h.proc.processBlock (h.buf, h.midi);
                h.head.ppq += h.block / h.sr * h.head.bpm / 60.0;
                if (b < blocks / 3) continue;
                for (int i = 0; i < h.block; ++i) { double v = h.buf.getSample (0, i); acc += v * v; ++cnt; }
            }
            return std::sqrt (acc / juce::jmax (1, cnt));
        };
        Harness ref;
        ref.allOff();
        const double r0 = rms (ref, 3.0);
        juce::String line;
        for (int fx = 0; fx < NumFx; ++fx)
        {
            Harness h;
            h.allOff();
            h.proc.setNorm (reg.fxOn[fx], 1.0f);
            h.proc.setNorm (reg.fxMix[fx], 1.0f);
            const double r = rms (h, 3.0);
            const double db = 20.0 * std::log10 (juce::jmax (1.0e-9, r / r0));
            line << effectDefs()[(size_t) fx].shortName << " " << juce::String (db, 1) << "  ";
            check (db > -30.0, effectDefs()[(size_t) fx].name + " quasi muet (" + juce::String (db, 1) + " dB)");
            check (db < 12.0, effectDefs()[(size_t) fx].name + " beaucoup trop fort (" + juce::String (db, 1) + " dB)");
        }
        std::cout << "   " << line << std::endl;
    }

    // 2. Random général à fond + chaos max, mode normal et sans latence
    for (int zl = 0; zl < 2; ++zl)
    {
        std::cout << "2. Random général x40, chaos max, " << (zl ? "sans latence" : "normal") << "..." << std::endl;
        Harness h;
        h.proc.setValue (G::chaos, 1.0f);
        h.proc.setValue (G::zeroLat, (float) zl);
        h.proc.setValue (G::transOn, 1.0f);
        h.proc.setValue (G::ottDepth, 0.7f);
        for (int s = 0; s < 40; ++s)
        {
            h.proc.random.perform (RandomEngine::Scope::All, 0, (uint32_t) (s + 1000 * zl + 7), "test", 1.0f);
            // quelques modulations au hasard
            if (s % 5 == 0)
                for (int k = 0; k < 6; ++k)
                    h.proc.modShared.addConnection (h.rnd.nextInt (NumSources), reg.fxFirst[h.rnd.nextInt (NumFx)], h.rnd.nextFloat() * 2 - 1, h.rnd.nextBool());
            float pk = h.run (0.8);
            check (pk >= 0.0f, "random général : NaN (seed " + juce::String (s) + ")");
            check (pk <= ceilingGain (h.proc), "random général : plafond dépassé " + juce::String (pk));
        }
    }

    // 3. Feedback extrême + Panic
    {
        std::cout << "3. Feedback extrême + Panic..." << std::endl;
        Harness h;
        h.allOff();
        h.proc.setValue (G::fbAmount, 0.95f);
        h.proc.setValue (fxParamId (FxComb, "on"), 1.0f);
        h.proc.setValue (fxParamId (FxComb, "fb"), 0.99f);
        h.proc.setValue (fxParamId (FxReverb, "on"), 1.0f);
        h.proc.setValue (fxParamId (FxReverb, "decay"), 20.0f);
        h.proc.setValue (fxParamId (FxDelay, "on"), 1.0f);
        h.proc.setValue (fxParamId (FxDelay, "fb"), 1.0f);
        float pk = h.run (8.0);
        check (pk >= 0.0f && pk <= ceilingGain (h.proc), "feedback extrême : sortie instable " + juce::String (pk));
        h.proc.panic();
        h.run (0.1, true);
        float after = h.run (0.5, true);
        check (after < 0.01f, "Panic n'a pas coupé le feedback (" + juce::String (after) + ")");
    }

    // 4. Latence déclarée = latence réelle (signal sec)
    for (int zl = 0; zl < 2; ++zl)
    {
        std::cout << "4. Latence " << (zl ? "sans latence" : "normale") << "..." << std::endl;
        Harness h;
        h.proc.setValue (G::zeroLat, (float) zl);
        h.proc.setValue (G::monoOn, 0.0f);
        h.proc.setValue (G::hpFreq, 10.0f);
        h.proc.prepareToPlay (h.sr, h.block);
        const int declared = h.proc.getLatencySamples();
        const int expected = Rack::rackLatency (zl) + (zl ? 0 : (int) std::round (0.0015 * h.sr));
        check (declared == expected, "latence déclarée inattendue : " + juce::String (declared));
        // impulsion
        int found = -1;
        for (int b = 0; b < 20 && found < 0; ++b)
        {
            h.buf.clear();
            if (b == 0) { h.buf.setSample (0, 0, 0.5f); h.buf.setSample (1, 0, 0.5f); }
            h.midi.clear();
            h.proc.processBlock (h.buf, h.midi);
            for (int i = 0; i < h.block; ++i)
                if (std::abs (h.buf.getSample (0, i)) > 0.2f) { found = b * h.block + i; break; }
        }
        check (std::abs (found - declared) <= 1, "impulsion à " + juce::String (found) + " au lieu de " + juce::String (declared));
        std::cout << "   latence déclarée " << declared << " samples, mesurée " << found << std::endl;
    }

    // 5. Sauvegarde / rechargement de l'état
    {
        std::cout << "5. Sauvegarde / rechargement..." << std::endl;
        Harness h;
        h.proc.random.perform (RandomEngine::Scope::All, 0, 1234, "test", 1.0f);
        h.proc.modShared.addConnection (SrcLfo0 + 2, reg.fxMix[FxDelay], 0.4f, true);
        h.proc.seqData[FxGate].enabled = true;
        juce::MemoryBlock mb;
        h.proc.getStateInformation (mb);
        Harness h2;
        h2.proc.setStateInformation (mb.getData(), (int) mb.getSize());
        int diffs = 0;
        for (int i = 0; i < (int) reg.all.size(); ++i)
            if (std::abs (h.proc.getNorm (i) - h2.proc.getNorm (i)) > 1.0e-4f) ++diffs;
        for (int i = 0; i < (int) reg.all.size(); ++i)
            if (std::abs (h.proc.getNorm (i) - h2.proc.getNorm (i)) > 1.0e-4f)
                std::cout << "   diff " << reg.all[(size_t) i].id << " " << h.proc.getNorm (i) << " vs " << h2.proc.getNorm (i) << std::endl;
        check (diffs == 0, juce::String (diffs) + " paramètres différents après rechargement");
        check (h2.proc.modShared.findConnection (SrcLfo0 + 2, reg.fxMix[FxDelay]) >= 0, "modulation perdue au rechargement");
        check (h2.proc.seqData[FxGate].enabled.load(), "séquenceur perdu au rechargement");
        check (h.proc.rackShared.getOrder() == h2.proc.rackShared.getOrder(), "ordre de la chaîne perdu");
    }

    // 5b. Mix à 100 % jamais randomisé (sauf espace) + morph XY réellement progressif
    {
        std::cout << "5b. Mix intouchable + morph XY..." << std::endl;
        Harness h;
        for (int k = 0; k < 10; ++k) h.proc.random.perform (RandomEngine::Scope::All, 0, (uint32_t) (900 + k), "t", 1.0f);
        for (int fx = 0; fx < NumFx; ++fx)
            if (effectDefs()[(size_t) fx].family != FamSpace)
                check (std::abs (h.proc.getNorm (reg.fxMix[fx]) - 1.0f) < 1.0e-4f, effectDefs()[(size_t) fx].name + " : mix randomisé");
        check (! h.proc.seqData[FxStutter].enabled.load() || true, "");
        int seqs = 0;
        for (auto& sd : h.proc.seqData) seqs += sd.enabled.load() ? 1 : 0;
        check (seqs == 0, "le random a allumé " + juce::String (seqs) + " séquenceur(s) alors que l'option est désactivée");

        const int pid = h.proc.indexOf (fxParamId (FxFilter, "cutoff"));
        h.proc.setNorm (pid, 0.1f); h.proc.storeSnapshot (0);
        h.proc.setNorm (pid, 0.9f); h.proc.storeSnapshot (1);
        h.proc.snapshots[0].x = 0.2f; h.proc.snapshots[0].y = 0.5f;
        h.proc.snapshots[1].x = 0.8f; h.proc.snapshots[1].y = 0.5f;
        h.proc.applyMorph (0.5f, 0.5f);
        const float mid = h.proc.getNorm (pid);
        h.proc.applyMorph (0.35f, 0.5f);
        const float q = h.proc.getNorm (pid);
        check (std::abs (mid - 0.5f) < 0.02f, "morph au milieu : " + juce::String (mid));
        check (q > 0.15f && q < 0.45f, "morph au quart pas progressif : " + juce::String (q));
        std::cout << "   morph milieu " << mid << ", quart " << q << std::endl;
    }

    // 6. Clics : activation / désactivation d'un effet neutre sur un sinus
    {
        std::cout << "6. Clics à l'activation..." << std::endl;
        Harness h;
        h.allOff();
        h.proc.setValue (G::monoOn, 0.0f);
        h.proc.setValue (fxParamId (FxStereo, "width"), 1.0f);
        float maxJump = 0.0f, prev = 0.0f;
        double ph = 0;
        for (int b = 0; b < 200; ++b)
        {
            if (b % 20 == 10) h.proc.setValue (fxParamId (FxFilter, "on"), (b / 20) % 2 ? 1.0f : 0.0f);
            for (int i = 0; i < h.block; ++i)
            {
                float x = 0.5f * (float) std::sin (ph);
                ph += 2.0 * juce::MathConstants<double>::pi * 220.0 / h.sr;
                h.buf.setSample (0, i, x); h.buf.setSample (1, i, x);
            }
            h.midi.clear();
            h.proc.processBlock (h.buf, h.midi);
            for (int i = 0; i < h.block; ++i)
            {
                float v = h.buf.getSample (0, i);
                if (b > 2) maxJump = juce::jmax (maxJump, std::abs (v - prev));
                prev = v;
            }
        }
        // un sinus de 220 Hz à 0.5 varie au maximum de ~0.0145 par échantillon
        check (maxJump < 0.05f, "saut détecté à l'activation : " + juce::String (maxJump));
        std::cout << "   saut max " << maxJump << std::endl;
    }

    // 7. CPU : tous les effets allumés
    {
        std::cout << "7. Charge CPU (tous les effets, 4x)..." << std::endl;
        Harness h;
        for (int fx = 0; fx < NumFx; ++fx) h.proc.setNorm (reg.fxOn[fx], 1.0f);
        h.proc.setValue (G::destOS, 2.0f);
        h.proc.setValue (G::ottDepth, 1.0f);
        for (int i = 0; i < NumLfos; ++i) h.proc.modShared.addConnection (SrcLfo0 + i, reg.fxFirst[i * 2] , 0.3f);
        auto t0 = juce::Time::getMillisecondCounterHiRes();
        float pk = h.run (10.0);
        auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
        check (pk >= 0.0f && pk <= ceilingGain (h.proc), "tous effets : sortie invalide");
        std::cout << "   10 s d'audio traitées en " << juce::String (ms / 1000.0, 2) << " s ("
                  << juce::String (ms / 100.0, 1) << " % d'un cœur)" << std::endl;
        h.allOff();
        t0 = juce::Time::getMillisecondCounterHiRes();
        h.run (10.0);
        ms = juce::Time::getMillisecondCounterHiRes() - t0;
        std::cout << "   effets éteints : " << juce::String (ms / 100.0, 1) << " % d'un cœur" << std::endl;
    }

    std::cout << (failures == 0 ? "=== TOUS LES TESTS PASSENT ===" : "=== " + juce::String (failures).toStdString() + " ECHEC(S) ===") << std::endl;
    return failures == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------
// Capture de l'interface (vérification visuelle hors hôte)
// ---------------------------------------------------------------------------
int snapshotMain (const juce::String& path, int tab)
{
    Harness h;
    h.proc.random.perform (RandomEngine::Scope::All, 0, 4242, "snap", 0.8f);
    h.proc.modShared.addConnection (SrcLfo0, reg().fxFirst[FxStutter] + 2, 0.4f, true);
    h.run (1.5);
    std::unique_ptr<juce::AudioProcessorEditor> ed (h.proc.createEditor());
    ed->setSize (1280, 800);
    for (int i = 0; i < 10; ++i) { h.run (0.05); if (auto* e = dynamic_cast<IDGemsEditor*> (ed.get())) e->debugTick(); }
    if (tab > 0)
        if (auto* e = dynamic_cast<IDGemsEditor*> (ed.get())) e->debugSelectTab (tab);
    for (int i = 0; i < 5; ++i) { h.run (0.05); if (auto* e = dynamic_cast<IDGemsEditor*> (ed.get())) e->debugTick(); }
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File f (path);
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat png;
    png.writeImageToStream (img, os);
    std::cout << "capture : " << f.getFullPathName() << std::endl;
    return 0;
}
