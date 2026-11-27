#pragma once
#include "EffectBase.h"

namespace idg
{
class Distortion : public EffectModule
{
public:
    enum { Drive, Curve, Bias, Tone, Out };
    void prepare (double, int) override { reset(); }
    void reset() override { tone.reset(); dc.reset(); }

    static float shape (int c, float x)
    {
        switch (c)
        {
            case 0: return dsp::fastTanh (x);
            case 1: return juce::jlimit (-1.0f, 1.0f, x);
            case 2: { float t = 0.25f * x + 0.25f; t -= std::floor (t); return 4.0f * std::abs (t - 0.5f) - 1.0f; }
            case 3: return x > 0 ? dsp::fastTanh (x) : dsp::fastTanh (0.4f * x) * 0.8f + 0.1f * x / (1.0f + std::abs (x));
            case 4: return std::sin (x);
            default: return 2.0f * std::abs (dsp::fastTanh (x)) - 0.5f;
        }
    }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        const float g = dsp::dbToGain (P (Drive));
        const int c = I (Curve);
        const float bias = P (Bias) * 0.5f;
        const float offset = shape (c, bias);
        const float out = dsp::dbToGain (P (Out));
        if (dcSr != ctx.sr) { dc.prepare (ctx.sr); dcSr = ctx.sr; }
        tone.set (ctx.sr, P (Tone), 0.707f);
        float* ch[2] { L, R };
        for (int k = 0; k < 2; ++k)
            for (int i = 0; i < n; ++i)
            {
                float y = shape (c, ch[k][i] * g + bias) - offset;
                y = tone.lowpass (k, y);
                ch[k][i] = dc.process (k, y) * out;
            }
    }

private:
    dsp::SVF tone;
    dsp::DCBlocker dc;
    double dcSr = 0;
};
} // namespace idg
