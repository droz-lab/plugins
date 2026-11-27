#pragma once
#include "EffectBase.h"

namespace idg
{
class Phaser : public EffectModule
{
public:
    enum { Rate, Depth, Stages, Fb, Center };
    void prepare (double, int) override { reset(); }
    void reset() override { for (auto& c : z) for (auto& v : c) v = 0; ph = 0; last[0] = last[1] = 0; }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        static const int st[] { 4, 6, 8, 12 };
        const int stages = st[juce::jlimit (0, 3, I (Stages))];
        const float depth = P (Depth), fb = P (Fb), centre = P (Center);
        const double inc = P (Rate) / ctx.sr;
        float* ch[2] { L, R };
        for (int i = 0; i < n; ++i)
        {
            for (int k = 0; k < 2; ++k)
            {
                float m = std::sin (dsp::twoPi * ((float) ph + (k == 1 ? 0.2f : 0.0f)));
                float f = juce::jlimit (20.0f, (float) ctx.sr * 0.45f, centre * std::pow (2.0f, m * depth * 2.0f));
                float t = std::tan (dsp::pi * f / (float) ctx.sr);
                float a = (t - 1.0f) / (t + 1.0f);
                float x = ch[k][i];
                float y = x + fb * last[k];
                for (int s = 0; s < stages; ++s)
                {
                    float o = a * y + z[k][s];
                    z[k][s] = y - a * o;
                    y = o;
                }
                last[k] = dsp::fastTanh (y);
                ch[k][i] = (x + y) * 0.6f;
            }
            ph += inc; if (ph >= 1.0) ph -= 1.0;
        }
    }

private:
    float z[2][12] {};
    double ph = 0;
    float last[2] {};
};
} // namespace idg
