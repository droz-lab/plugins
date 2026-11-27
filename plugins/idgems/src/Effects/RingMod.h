#pragma once
#include "EffectBase.h"

namespace idg
{
class RingMod : public EffectModule
{
public:
    enum { Freq, Wave, Spread };
    void prepare (double, int) override { reset(); }
    void reset() override { ph[0] = ph[1] = 0; }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        const int w = I (Wave);
        const double f0 = P (Freq, 0), f1 = P (Freq, 1) * (1.0 + P (Spread) * 0.12);
        const double inc[2] { f0 / ctx.sr, f1 / ctx.sr };
        float* ch[2] { L, R };
        for (int k = 0; k < 2; ++k)
            for (int i = 0; i < n; ++i)
            {
                float m;
                if (w == 2) m = dsp::fastTanh (std::sin (dsp::twoPi * (float) ph[k]) * 6.0f); // carré adouci
                else m = dsp::shapeValue (w, (float) ph[k]);
                ch[k][i] *= m;
                ph[k] += inc[k];
                if (ph[k] >= 1.0) ph[k] -= 1.0;
            }
    }

private:
    double ph[2] {};
};
} // namespace idg
