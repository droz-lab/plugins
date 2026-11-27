#pragma once
#include "EffectBase.h"

namespace idg
{
class Filter : public EffectModule
{
public:
    enum { Type, Cutoff, Reso, Drive };
    void prepare (double, int) override { reset(); }
    void reset() override { a.reset(); b.reset(); a2.reset(); b2.reset(); }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        const int t = I (Type);
        const float q = 0.5f + P (Reso) * P (Reso) * 18.0f;
        const float drive = dsp::dbToGain (P (Drive));
        const bool cascade = (t == 1 || t == 3);
        float* ch[2] { L, R };
        for (int k = 0; k < 2; ++k)
        {
            auto& f1 = k == 0 ? a : a2;
            auto& f2 = k == 0 ? b : b2;
            f1.set (ctx.sr, P (Cutoff, k), cascade ? juce::jmax (0.5f, q * 0.7f) : q);
            f2.set (ctx.sr, P (Cutoff, k), 0.707f);
            for (int i = 0; i < n; ++i)
            {
                float x = ch[k][i];
                if (drive > 1.01f) x = dsp::fastTanh (x * drive) / juce::jmin (drive, 3.0f);
                float y;
                switch (t)
                {
                    case 0: y = f1.lowpass (0, x); break;
                    case 1: y = f2.lowpass (0, f1.lowpass (0, x)); break;
                    case 2: y = f1.highpass (0, x); break;
                    case 3: y = f2.highpass (0, f1.highpass (0, x)); break;
                    case 4: y = f1.bandpass (0, x) * (1.5f / q); break;
                    default: y = f1.notch (0, x); break;
                }
                ch[k][i] = dsp::fastTanh (y * 0.7f) / 0.7f;
            }
            f1.sanitize(); f2.sanitize();
        }
    }

private:
    dsp::SVF a, b, a2, b2; // un jeu par canal (cutoff stéréo modulable)
};
} // namespace idg
