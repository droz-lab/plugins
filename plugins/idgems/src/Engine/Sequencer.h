#pragma once
#include "../Core/Dsp.h"
#include "../Core/ParamDefs.h"

namespace idg
{
constexpr int MaxSteps = 32;

/** Données d'un séquenceur d'effet (écrites par l'interface / le random, lues par l'audio). */
struct SeqData
{
    std::atomic<bool> enabled { false }, locked { false };
    std::atomic<int> length { 16 }, rate { 3 };
    std::atomic<float> swing { 0.0f };
    std::atomic<bool> on[MaxSteps];
    std::atomic<float> prob[MaxSteps];
    std::atomic<int> ratchet[MaxSteps];
    std::atomic<int> current { -1 };

    SeqData() { clear(); }
    void clear()
    {
        for (int i = 0; i < MaxSteps; ++i) { on[i] = (i % 4 == 0); prob[i] = 1.0f; ratchet[i] = 1; }
        length = 16; rate = 3; swing = 0.0f;
    }
    void copyFrom (const SeqData& o)
    {
        enabled = o.enabled.load(); length = o.length.load(); rate = o.rate.load(); swing = o.swing.load();
        for (int i = 0; i < MaxSteps; ++i) { on[i] = o.on[i].load(); prob[i] = o.prob[i].load(); ratchet[i] = o.ratchet[i].load(); }
    }
    juce::ValueTree toTree() const;
    void fromTree (const juce::ValueTree&);
};

/** Partie temps réel d'un séquenceur. */
class SeqRuntime
{
public:
    void reset() { lastStep = INT64_MIN; lastSub = -1; fires = false; }
    /** Calcule la porte et le déclenchement pour le bloc qui commence à ppq. */
    void tick (SeqData& d, double ppq, float chaos, dsp::Rng& rng, bool& gate, bool& trig);

private:
    int64_t lastStep = INT64_MIN;
    int lastSub = -1;
    bool fires = false;
    int ratchets = 1;
};
} // namespace idg
