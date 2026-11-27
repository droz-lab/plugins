#pragma once
#include "Stft.h"
#include "EffectBase.h"

namespace idg
{
/** Pixel sort audio : tri des magnitudes spectrales par segments (comme on trie des pixels). */
class PixelSort : public EffectModule
{
public:
    enum { Thresh, Dir, Segment, Smear };
    void prepare (double, int) override { reset(); }
    void reset() override { for (auto& s : stft) s.reset(); for (auto& p : prev) std::fill (std::begin (p), std::end (p), 0.0f); rng.seed (0x91Eu); }
    int latency (bool zeroLat) const override { return zeroLat ? 0 : StftFrame::latency; }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        if (ctx.zeroLatency) return; // indisponible sans latence (grisé dans l'interface)
        float* ch[2] { L, R };
        for (int k = 0; k < 2; ++k)
            for (int i = 0; i < n; ++i)
                ch[k][i] = stft[k].process (ch[k][i], [&] (float* d) { sortFrame (k, d); });
    }

private:
    void sortFrame (int k, float* d)
    {
        constexpr int bins = StftFrame::numBins;
        static const int segs[] { 8, 16, 32, 64, 128 };
        const int seg = segs[juce::jlimit (0, 4, I (Segment))];
        const int dir = I (Dir);
        const float smear = P (Smear) * 0.97f;
        float maxMag = 1.0e-9f;
        for (int b = 0; b < bins; ++b)
        {
            mag[b] = std::sqrt (d[2 * b] * d[2 * b] + d[2 * b + 1] * d[2 * b + 1]);
            phs[b] = std::atan2 (d[2 * b + 1], d[2 * b]);
            maxMag = juce::jmax (maxMag, mag[b]);
        }
        const float th = P (Thresh) * P (Thresh) * maxMag;
        for (int s = 1; s < bins; s += seg)
        {
            int cnt = 0;
            const int end = juce::jmin (bins, s + seg);
            for (int b = s; b < end; ++b)
                if (mag[b] >= th) { idx[cnt] = b; tmp[cnt++] = mag[b]; }
            if (cnt < 2) continue;
            if (dir == 0) std::sort (tmp, tmp + cnt);
            else if (dir == 1) std::sort (tmp, tmp + cnt, [] (float a, float b) { return a > b; });
            else for (int j = cnt - 1; j > 0; --j) std::swap (tmp[j], tmp[rng.range (j + 1)]);
            for (int j = 0; j < cnt; ++j) mag[idx[j]] = tmp[j];
        }
        for (int b = 0; b < bins; ++b)
        {
            float m = mag[b] * (1.0f - smear) + prev[k][b] * smear;
            prev[k][b] = m;
            d[2 * b] = m * std::cos (phs[b]);
            d[2 * b + 1] = m * std::sin (phs[b]);
        }
    }

    StftFrame stft[2];
    float mag[StftFrame::numBins] {}, phs[StftFrame::numBins] {}, tmp[StftFrame::numBins] {};
    int idx[StftFrame::numBins] {};
    float prev[2][StftFrame::numBins] {};
    dsp::Rng rng;
};
} // namespace idg
