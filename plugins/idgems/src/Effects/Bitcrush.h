#pragma once
#include "EffectBase.h"

namespace idg
{
class Bitcrush : public EffectModule
{
public:
    enum { Bits, Rate, Jitter };
    void prepare (double, int) override { reset(); }
    void reset() override { hold[0] = hold[1] = 0; counter = 0; rng.seed (0xB17u); }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        const float bits = P (Bits);
        const float levels = std::pow (2.0f, bits - 1.0f);
        const float factor = P (Rate) * (float) ctx.os;
        const float jitter = P (Jitter);
        for (int i = 0; i < n; ++i)
        {
            counter -= 1.0f;
            if (counter <= 0.0f)
            {
                counter += juce::jmax (1.0f, factor * (1.0f + jitter * rng.bi() * 0.9f));
                hold[0] = std::round (L[i] * levels) / levels;
                hold[1] = std::round (R[i] * levels) / levels;
            }
            L[i] = hold[0];
            R[i] = hold[1];
        }
    }

private:
    float hold[2] {}, counter = 0;
    dsp::Rng rng;
};
} // namespace idg
