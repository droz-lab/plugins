#pragma once
#include "EffectBase.h"

namespace idg
{
class Chorus : public EffectModule
{
public:
    enum { Rate, Depth, Voices, Spread, Fb };
    void prepare (double sr, int) override { buf.allocate ((int) (sr * 0.1)); reset(); }
    void reset() override { buf.clear(); ph = 0; last[0] = last[1] = 0; }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        const int voices = 2 + I (Voices);
        const double baseD = 0.012 * ctx.sr, depth = P (Depth) * 0.007 * ctx.sr;
        const double inc = P (Rate) / ctx.sr;
        const float spread = P (Spread), fb = P (Fb);
        for (int i = 0; i < n; ++i)
        {
            buf.push (L[i] + fb * last[0], R[i] + fb * last[1]);
            float o[2] { 0, 0 };
            for (int v = 0; v < voices; ++v)
            {
                float vp = (float) ph + (float) v / (float) voices;
                for (int k = 0; k < 2; ++k)
                {
                    float m = std::sin (dsp::twoPi * (vp + (k == 1 ? spread * 0.25f : 0.0f)));
                    o[k] += buf.delay (k, baseD + depth * (0.5 + 0.5 * m) + v * 0.0013 * ctx.sr);
                }
            }
            ph += inc; if (ph >= 1.0) ph -= 1.0;
            for (int k = 0; k < 2; ++k) { o[k] /= (float) voices; last[k] = dsp::fastTanh (o[k]); }
            L[i] = o[0]; R[i] = o[1];
        }
    }

private:
    dsp::RingBuffer buf;
    double ph = 0;
    float last[2] {};
};
} // namespace idg
