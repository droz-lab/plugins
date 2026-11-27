#pragma once
#include "EffectBase.h"

namespace idg
{
/** État partagé du rack (écrit par l'interface, lu par le thread audio, sans verrou). */
struct RackShared
{
    std::atomic<int> order[2][NumFx];
    std::atomic<int> orderIndex { 0 };
    std::atomic<bool> solo[NumFx];
    std::atomic<bool> famSolo[NumFamilies];
    std::atomic<bool> momentary[NumFx];
    std::atomic<bool> transTrig[NumFx];
    std::atomic<bool> fxLocked[NumFx];
    std::atomic<float> activity[NumFx];   // niveau de sortie de chaque effet (pour l'interface)
    std::atomic<bool> processing[NumFx];  // l'effet consomme du CPU

    RackShared()
    {
        for (int i = 0; i < NumFx; ++i)
        {
            order[0][i] = i; order[1][i] = i;
            solo[i] = false; momentary[i] = false; fxLocked[i] = false; activity[i] = 0.0f; processing[i] = false;
            transTrig[i] = (i <= FxScratch); // effets glitch : relance sur transitoires par défaut
        }
        for (auto& f : famSolo) f = false;
    }

    std::array<int, NumFx> getOrder() const
    {
        std::array<int, NumFx> o {};
        int idx = orderIndex.load();
        for (int i = 0; i < NumFx; ++i) o[(size_t) i] = order[idx][i].load();
        return o;
    }

    void setOrder (const std::array<int, NumFx>& o)
    {
        // validation : permutation complète
        std::array<bool, NumFx> seen {};
        for (int v : o) { if (v < 0 || v >= NumFx || seen[(size_t) v]) return; seen[(size_t) v] = true; }
        int next = 1 - orderIndex.load();
        for (int i = 0; i < NumFx; ++i) order[next][i] = o[(size_t) i];
        orderIndex = next;
    }
};

/** Commandes calculées par le processeur pour chaque bloc. */
struct RackControl
{
    bool gate[NumFx];      // porte (séquenceur / pad)
    bool trig[NumFx];      // déclenchement dans ce bloc
    float chaos = 0.5f;
    int osFactor = 1;
};

class Rack
{
public:
    Rack();
    ~Rack();
    void init (ParamBank& bank, RackShared& shared);
    void prepare (double sr, int maxBlock);
    void reset();
    void process (float* L, float* R, int n, ProcCtx& ctx, const RackControl& rc);

    /** Latence totale du rack (constante dans chaque mode). */
    static int rackLatency (bool zeroLatency);
    bool isActive (int fx) const { return activeArr[fx]; }
    const bool* activeFlags() const { return activeArr; }

private:
    struct Runtime;
    void processSlot (int fx, float* L, float* R, int n, ProcCtx& ctx, const RackControl& rc);

    ParamBank* bank = nullptr;
    RackShared* shared = nullptr;
    std::vector<Runtime> rt;
    double sampleRate = 44100.0;
    bool activeArr[NumFx] {};
    int famMixIdx[NumFamilies] {};
    // buffers de travail (taille max de bloc)
    std::vector<float> dryL, dryR, ddL, ddR, xL, xR, gL, gR, aL, aR, tL, tR;
};
} // namespace idg
