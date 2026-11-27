#pragma once
#include "../Core/ParamBank.h"
#include "../Core/Dsp.h"

namespace idg
{
/** Contexte transmis à chaque bloc (32 échantillons max). */
struct ProcCtx
{
    double sr = 44100.0;       // fréquence d'échantillonnage effective (suréchantillonnée si besoin)
    double baseSr = 44100.0;
    int os = 1;
    double bpm = 120.0;
    double ppq = 0.0;          // position en noires au début du bloc
    double spb = 22050.0;      // échantillons par noire (à sr effectif)
    bool playing = false;
    int beatsPerBar = 4;
    double barStartPpq = 0.0;
    const Scale* scale = nullptr;
    bool zeroLatency = false;
    bool gate = true;          // porte du séquenceur / pads
    bool gateRising = false;

    double beatsPerSample() const { return 1.0 / spb; }
};

class EffectModule
{
public:
    virtual ~EffectModule() = default;
    void bind (MParam** ps) { p = ps; }

    virtual void prepare (double maxSampleRate, int maxBlock) = 0;
    virtual void reset() = 0;
    /** Traite en place. Les buffers contiennent n échantillons. */
    virtual void process (float* L, float* R, int n, const ProcCtx& ctx) = 0;
    /** Déclenchement (pas du séquenceur, MIDI, transitoire, pad). */
    virtual void trigger (const ProcCtx&) {}
    /** Latence introduite (en samples, au sr de base). */
    virtual int latency (bool /*zeroLatency*/) const { return 0; }
    /** true si l'effet ne doit pas être coupé par la porte du séquenceur (throws). */
    virtual bool throwMode() const { return false; }

protected:
    MParam** p = nullptr;
    inline float P (int i, int ch = 0) const { return p[i]->val[ch]; }
    inline int I (int i) const { return p[i]->geti(); }
    inline bool B (int i) const { return p[i]->on(); }
};
} // namespace idg
