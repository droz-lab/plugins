#pragma once
#include "Stft.h"
#include "EffectBase.h"

namespace idg
{
/** Formant : déplace l'enveloppe spectrale sans toucher à la hauteur.
    Mode normal : FFT (enveloppe lissée puis étirée). Mode sans latence : banc de filtres formantiques simplifié. */
class Formant : public EffectModule
{
public:
    enum { Shift };
    void prepare (double, int) override { reset(); }
    void reset() override
    {
        for (auto& s : stft) s.reset();
        for (auto& f : bands) f.reset();
        for (auto& f : bandsIn) f.reset();
    }
    int latency (bool zeroLat) const override { return zeroLat ? 0 : StftFrame::latency; }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        const float ratio = std::pow (2.0f, P (Shift) / 12.0f);
        float* ch[2] { L, R };
        if (ctx.zeroLatency)
        {
            static const float fr[3] { 600.0f, 1500.0f, 2800.0f };
            for (int b = 0; b < 3; ++b)
            {
                bands[b].set (ctx.sr, fr[b] * ratio, 4.0f);
                bandsIn[b].set (ctx.sr, fr[b], 4.0f);
            }
            for (int k = 0; k < 2; ++k)
                for (int i = 0; i < n; ++i)
                {
                    float x = ch[k][i], y = x;
                    for (int b = 0; b < 3; ++b)
                        y += 1.6f * bands[b].bandpass (k, x) - 1.0f * bandsIn[b].bandpass (k, x);
                    ch[k][i] = y * 0.8f;
                }
            return;
        }
        for (int k = 0; k < 2; ++k)
            for (int i = 0; i < n; ++i)
                ch[k][i] = stft[k].process (ch[k][i], [&] (float* d) { warp (d, ratio); });
    }

private:
    void warp (float* d, float ratio)
    {
        constexpr int bins = StftFrame::numBins;
        for (int b = 0; b < bins; ++b)
            mag[b] = std::sqrt (d[2 * b] * d[2 * b] + d[2 * b + 1] * d[2 * b + 1]) + 1.0e-9f;
        // enveloppe : moyenne glissante du log-magnitude
        constexpr int w = 6;
        for (int b = 0; b < bins; ++b) logm[b] = std::log (mag[b]);
        double acc = 0; int cnt = 0;
        for (int b = 0; b < juce::jmin (w, bins); ++b) { acc += logm[b]; ++cnt; }
        for (int b = 0; b < bins; ++b)
        {
            int add = b + w, rem = b - w - 1;
            if (add < bins) { acc += logm[add]; ++cnt; }
            if (rem >= 0) { acc -= logm[rem]; --cnt; }
            env[b] = (float) (acc / juce::jmax (1, cnt));
        }
        for (int b = 1; b < bins; ++b)
        {
            float src = (float) b / ratio;
            int i0 = juce::jlimit (0, bins - 2, (int) src);
            float f = juce::jlimit (0.0f, 1.0f, src - (float) i0);
            float e = env[i0] + (env[i0 + 1] - env[i0]) * f;
            float g = std::exp (juce::jlimit (-12.0f, 6.0f, e - env[b]));
            d[2 * b] *= g;
            d[2 * b + 1] *= g;
        }
    }

    StftFrame stft[2];
    float mag[StftFrame::numBins] {}, logm[StftFrame::numBins] {}, env[StftFrame::numBins] {};
    dsp::SVF bands[3], bandsIn[3];
};
} // namespace idg
