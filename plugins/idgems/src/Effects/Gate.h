#pragma once
#include "EffectBase.h"

namespace idg
{
/** Gate rythmique (trance gate) avec motifs euclidiens. */
class Gate : public EffectModule
{
public:
    enum { Div, Duty, Smooth, Pattern, Depth };
    void prepare (double, int) override { reset(); }
    void reset() override { g = 1.0f; }

    static bool stepOn (int pattern, int64_t step)
    {
        auto euclid = [] (int64_t i, int k, int nn) { return ((i % nn) * k) % nn < k; };
        switch (pattern)
        {
            case 0: return true;
            case 1: return euclid (step, 3, 8);
            case 2: return euclid (step, 5, 8);
            case 3: return euclid (step, 7, 16);
            default: return dsp::hash01 ((uint32_t) (step % 16) * 2654435761u + 77u) > 0.4f;
        }
    }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        const double div = divisionBeats (I (Div));
        const float duty = P (Duty), depth = P (Depth);
        const float tau = (0.5f + P (Smooth) * 40.0f) * 0.001f * (float) ctx.sr;
        const float a = std::exp (-1.0f / tau);
        const int pat = I (Pattern);
        double ppq = ctx.ppq;
        const double bps = ctx.beatsPerSample();
        for (int i = 0; i < n; ++i)
        {
            double pos = ppq / div;
            auto step = (int64_t) std::floor (pos);
            double frac = pos - (double) step;
            float target = (stepOn (pat, step) && frac < duty) ? 1.0f : 0.0f;
            g = target + (g - target) * a;
            float gain = 1.0f - depth * (1.0f - g);
            L[i] *= gain; R[i] *= gain;
            ppq += bps;
        }
    }

private:
    float g = 1.0f;
};
} // namespace idg
