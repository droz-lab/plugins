#pragma once
#include "EffectBase.h"

namespace idg
{
/** Reverse : joue la tranche précédente à l'envers (tranches calées sur la grille). */
class Reverse : public EffectModule
{
public:
    enum { Size, Chance, Smooth };

    void prepare (double sr, int) override { buf.allocate ((int) (sr * 10.0)); reset(); }
    void reset() override { buf.clear(); lastIdx = -1; rev = false; pos = 0; chunkLen = 0; prevStart = 0; forced = false; rng.seed (0x5EEDu); }
    void trigger (const ProcCtx&) override { forced = true; }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        const double beats = divisionBeats (I (Size));
        const int len = (int) juce::jlimit (64.0, (double) buf.size * 0.45, beats * ctx.spb);
        auto idx = (int64_t) std::floor (ctx.ppq / beats);
        if (idx != lastIdx)
        {
            lastIdx = idx;
            prevStart = buf.w - (chunkLen > 0 ? chunkLen : len);
            chunkLen = len;
            pos = 0;
            rev = forced || rng.uni() < P (Chance);
            forced = false;
        }
        const float fade = juce::jmax (32.0f, P (Smooth) * (float) chunkLen * 0.5f);
        for (int i = 0; i < n; ++i)
        {
            buf.push (L[i], R[i]);
            if (rev && pos < chunkLen)
            {
                float w = juce::jmin (1.0f, (float) pos / fade, (float) (chunkLen - pos) / fade);
                int rp = prevStart + (chunkLen - 1 - pos);
                L[i] = L[i] * (1.0f - w) + buf.at (0, rp) * w;
                R[i] = R[i] * (1.0f - w) + buf.at (1, rp) * w;
            }
            ++pos;
        }
    }

private:
    dsp::RingBuffer buf;
    dsp::Rng rng;
    int64_t lastIdx = -1;
    bool rev = false, forced = false;
    int pos = 0, chunkLen = 0, prevStart = 0;
};
} // namespace idg
