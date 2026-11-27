#pragma once
#include "EffectBase.h"

namespace idg
{
class Flanger : public EffectModule
{
public:
    enum { Rate, Depth, Fb, Manual };
    void prepare (double sr, int) override { buf.allocate ((int) (sr * 0.05)); reset(); }
    void reset() override { buf.clear(); ph = 0; last[0] = last[1] = 0; }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        const double man = P (Manual) * 0.001 * ctx.sr;
        const float depth = P (Depth), fb = P (Fb);
        const double inc = P (Rate) / ctx.sr;
        float* ch[2] { L, R };
        for (int i = 0; i < n; ++i)
        {
            float x[2] { L[i], R[i] };
            buf.push (x[0] + fb * last[0], x[1] + fb * last[1]);
            for (int k = 0; k < 2; ++k)
            {
                float m = std::sin (dsp::twoPi * ((float) ph + (k == 1 ? 0.25f : 0.0f)));
                double d = juce::jmax (1.0, man * (1.0 + depth * 0.9 * m));
                float y = buf.delay (k, d);
                last[k] = dsp::fastTanh (y);
                ch[k][i] = (x[k] + y) * 0.6f;
            }
            ph += inc; if (ph >= 1.0) ph -= 1.0;
        }
    }

private:
    dsp::RingBuffer buf;
    double ph = 0;
    float last[2] {};
};
} // namespace idg
