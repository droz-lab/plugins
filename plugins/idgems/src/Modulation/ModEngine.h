#pragma once
#include "LfoShape.h"
#include "../Core/ParamBank.h"
#include "../Core/Dsp.h"

namespace idg
{
enum
{
    SrcLfo0 = 0,
    SrcRnd0 = SrcLfo0 + NumLfos,
    SrcEnv = SrcRnd0 + NumRandMods,
    SrcMacro0 = SrcEnv + 1,
    NumSources = SrcMacro0 + NumMacros
};

juce::String sourceName (int src);
juce::Colour sourceColour (int src);

struct ModSlot
{
    std::atomic<int> src { -1 }, dst { -1 };
    std::atomic<float> depth { 0.0f };
    std::atomic<bool> bipolar { false };
};
constexpr int MaxModSlots = 256;

/** Partie partagée (interface <-> audio) des modulations. */
struct ModShared
{
    ModSlot slots[MaxModSlots];
    float tables[NumLfos][2][LfoShape::tableSize] {};
    std::atomic<int> tableIndex[NumLfos];
    std::atomic<float> value[NumSources];   // valeur courante (affichage)
    std::atomic<float> lfoPhase[NumLfos];
    LfoShape shapes[NumLfos];                // côté interface uniquement

    ModShared()
    {
        for (int i = 0; i < NumLfos; ++i) { tableIndex[i] = 0; lfoPhase[i] = 0.0f; setShape (i, LfoShape::preset (0)); }
        for (auto& v : value) v = 0.0f;
    }

    /** Thread interface : publie une nouvelle forme. */
    void setShape (int lfo, const LfoShape& s)
    {
        shapes[lfo] = s;
        int next = 1 - tableIndex[lfo].load();
        s.render (tables[lfo][next]);
        tableIndex[lfo] = next;
    }

    int addConnection (int src, int dst, float depth, bool bipolar = false);
    void removeConnection (int slot) { slots[slot].src = -1; slots[slot].dst = -1; }
    void removeConnectionsFor (int dst) { for (auto& s : slots) if (s.dst.load() == dst) { s.src = -1; s.dst = -1; } }
    int findConnection (int src, int dst) const;
    void clearAll() { for (auto& s : slots) { s.src = -1; s.dst = -1; } }
    juce::ValueTree toTree() const;
    void fromTree (const juce::ValueTree&);
};

class ModEngine
{
public:
    void init (ParamBank& bank, ModShared& shared);
    void prepare (double sr);
    void reset();
    /** Calcule les sources pour ce bloc et applique les modulations aux paramètres. */
    void process (int n, double ppq, double spb, bool playing, float envInputPeak, bool trigger, float chaos);

private:
    ParamBank* bank = nullptr;
    ModShared* shared = nullptr;
    double sr = 44100.0;

    struct LfoRt { double phase = 0; double sinceTrig = 1e9; bool envDone = false; float sm[2] {}; };
    struct RndRt { double t = 0; float cur[2] {}, from[2] {}, to[2] {}; int64_t lastIdx = -1; double lx = 0.1, ly = 0, lz = 0; };
    LfoRt lfo[NumLfos];
    RndRt rnd[NumRandMods];
    float env = 0.0f;
    float values[NumSources][2] {};
    dsp::Rng rng;

    struct LfoParams { MParam *rate, *sync, *div, *mode, *smooth, *phase, *delay, *stereo; };
    struct RndParams { MParam *rate, *sync, *div, *style, *stereo; };
    LfoParams lp[NumLfos];
    RndParams rp[NumRandMods];
    MParam* macros[NumMacros] {};
    MParam *envAtt = nullptr, *envRel = nullptr, *envGain = nullptr;
};
} // namespace idg
