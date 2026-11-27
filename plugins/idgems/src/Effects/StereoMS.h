#pragma once
#include "EffectBase.h"

namespace idg
{
class StereoMS : public EffectModule
{
public:
    enum { Width, Mid, Side, Haas, Rotate };
    void prepare (double sr, int) override { buf.allocate ((int) (sr * 0.05)); reset(); }
    void reset() override { buf.clear(); }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        const float width = P (Width);
        const float mg = dsp::dbToGain (P (Mid)), sg = dsp::dbToGain (P (Side));
        const double haas = P (Haas) * 0.001 * ctx.sr;
        const float rot = P (Rotate) * dsp::pi * 0.25f;
        const float c = std::cos (rot), s = std::sin (rot);
        for (int i = 0; i < n; ++i)
        {
            float m = 0.5f * (L[i] + R[i]) * mg;
            float sd = 0.5f * (L[i] - R[i]) * sg * width;
            float l = m + sd, r = m - sd;
            float lr = c * l - s * r, rr = s * l + c * r;
            buf.push (lr, rr);
            L[i] = lr;
            R[i] = haas > 0.5 ? buf.delay (1, haas) : rr;
        }
    }

private:
    dsp::RingBuffer buf;
};
} // namespace idg
