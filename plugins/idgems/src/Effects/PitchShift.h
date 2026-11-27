#pragma once
#include "Stft.h"
#include "EffectBase.h"

namespace idg
{
/** Pitch shift. Mode normal : vocodeur de phase (FFT, latence). Mode sans latence : granulaire à 2 têtes. */
class PitchShift : public EffectModule
{
public:
    enum { Semi, Fine, Fb };

    void prepare (double sr, int) override { gbuf.allocate ((int) (sr * 1.0)); reset(); }
    void reset() override
    {
        for (int k = 0; k < 2; ++k)
        {
            stft[k].reset();
            std::fill (std::begin (lastPhase[k]), std::end (lastPhase[k]), 0.0f);
            std::fill (std::begin (sumPhase[k]), std::end (sumPhase[k]), 0.0f);
            last[k] = 0;
        }
        gbuf.clear(); gph = 0;
    }
    int latency (bool zeroLat) const override { return zeroLat ? 0 : StftFrame::latency; }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        const float ratio = std::pow (2.0f, (P (Semi) + P (Fine) * 0.01f) / 12.0f);
        const float fb = P (Fb);
        float* ch[2] { L, R };
        if (ctx.zeroLatency)
        {
            const double win = 0.045 * ctx.sr;
            const double inc = (1.0 - ratio) / win;
            for (int i = 0; i < n; ++i)
            {
                gbuf.push (L[i] + fb * last[0], R[i] + fb * last[1]);
                for (int k = 0; k < 2; ++k)
                {
                    double ph2 = gph + 0.5; ph2 -= std::floor (ph2);
                    float a = gbuf.delay (k, 2.0 + gph * win), b = gbuf.delay (k, 2.0 + ph2 * win);
                    float wa = std::sin (dsp::pi * (float) gph), wb = std::sin (dsp::pi * (float) ph2);
                    float y = a * wa * wa + b * wb * wb;
                    last[k] = dsp::fastTanh (y);
                    ch[k][i] = y;
                }
                gph += inc; gph -= std::floor (gph);
            }
            return;
        }

        for (int k = 0; k < 2; ++k)
            for (int i = 0; i < n; ++i)
            {
                float y = stft[k].process (ch[k][i] + fb * last[k], [&] (float* d) { shiftFrame (k, d, ratio); });
                last[k] = dsp::fastTanh (y);
                ch[k][i] = y;
            }
    }

private:
    void shiftFrame (int k, float* d, float ratio)
    {
        constexpr int N = StftFrame::fftSize, bins = StftFrame::numBins, osamp = StftFrame::overlap;
        const float expct = dsp::twoPi * (float) StftFrame::hop / (float) N;
        for (int b = 0; b < bins; ++b)
        {
            float re = d[2 * b], im = d[2 * b + 1];
            float mag = 2.0f * std::sqrt (re * re + im * im);
            float phase = std::atan2 (im, re);
            float tmp = phase - lastPhase[k][b];
            lastPhase[k][b] = phase;
            tmp -= (float) b * expct;
            int qpd = (int) (tmp / dsp::pi);
            qpd += qpd >= 0 ? (qpd & 1) : -(qpd & 1);
            tmp -= dsp::pi * (float) qpd;
            tmp = (float) osamp * tmp / dsp::twoPi;
            anaFreq[b] = (float) b + tmp;
            anaMag[b] = mag;
        }
        std::fill (std::begin (synMag), std::end (synMag), 0.0f);
        std::fill (std::begin (synFreq), std::end (synFreq), 0.0f);
        for (int b = 0; b < bins; ++b)
        {
            int idx = (int) ((float) b * ratio);
            if (idx < bins && idx >= 0)
            {
                synMag[idx] += anaMag[b];
                synFreq[idx] = anaFreq[b] * ratio;
            }
        }
        for (int b = 0; b < bins; ++b)
        {
            float tmp = synFreq[b] - (float) b;
            tmp = dsp::twoPi * tmp / (float) osamp;
            tmp += (float) b * expct;
            sumPhase[k][b] += tmp;
            if (std::abs (sumPhase[k][b]) > 1.0e5f) sumPhase[k][b] = std::fmod (sumPhase[k][b], dsp::twoPi);
            float mag = synMag[b] * 0.5f;
            d[2 * b] = mag * std::cos (sumPhase[k][b]);
            d[2 * b + 1] = mag * std::sin (sumPhase[k][b]);
        }
    }

    StftFrame stft[2];
    float lastPhase[2][StftFrame::numBins] {}, sumPhase[2][StftFrame::numBins] {};
    float anaFreq[StftFrame::numBins] {}, anaMag[StftFrame::numBins] {};
    float synFreq[StftFrame::numBins] {}, synMag[StftFrame::numBins] {};
    float last[2] {};
    dsp::RingBuffer gbuf;
    double gph = 0;
};
} // namespace idg
