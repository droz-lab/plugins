#pragma once
#include "EffectBase.h"

namespace idg
{
/** Delay synchronisable : ping-pong, filtres dans la boucle, wow, mode throw. */
class Delay : public EffectModule
{
public:
    enum { Time, Sync, Div, Fb, PingPong, Tone, LowCut, Wow, Throw };
    void prepare (double sr, int) override { buf.allocate ((int) (sr * 4.5)); reset(); }
    void reset() override { buf.clear(); lp.reset(); hp.reset(); cur[0] = cur[1] = -1; wowPh = 0; sendGain = 0; }
    bool throwMode() const override { return B (Throw); }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        double target[2];
        for (int k = 0; k < 2; ++k)
        {
            double t = B (Sync) ? divisionBeats (I (Div)) * ctx.spb : P (Time, k) * 0.001 * ctx.sr;
            target[k] = juce::jlimit (4.0, (double) buf.size - 8.0, t);
            if (cur[k] < 0) cur[k] = target[k];
        }
        const float fb = juce::jmin (1.05f, P (Fb));
        const bool pp = B (PingPong);
        lp.set (ctx.sr, P (Tone), 0.6f);
        hp.set (ctx.sr, P (LowCut), 0.6f);
        const float wow = P (Wow);
        const double smoothA = std::exp (-1.0 / (0.08 * ctx.sr));
        const float sendTarget = (! B (Throw) || ctx.gate) ? 1.0f : 0.0f;
        const float sendStep = 1.0f / (float) (0.004 * ctx.sr);

        for (int i = 0; i < n; ++i)
        {
            sendGain += juce::jlimit (-sendStep, sendStep, sendTarget - sendGain);
            wowPh += 0.7 / ctx.sr;
            if (wowPh >= 1.0) wowPh -= 1.0;
            float w = wow * (std::sin (dsp::twoPi * (float) wowPh) * 0.004f + std::sin (dsp::twoPi * (float) wowPh * 5.3f) * 0.0012f) * (float) ctx.sr;
            float d[2];
            for (int k = 0; k < 2; ++k)
            {
                cur[k] = target[k] + (cur[k] - target[k]) * smoothA;
                d[k] = buf.delay (k, juce::jmax (2.0, cur[k] + w));
            }
            float f[2];
            for (int k = 0; k < 2; ++k) f[k] = hp.highpass (k, lp.lowpass (k, d[k]));
            float inL = L[i] * sendGain, inR = R[i] * sendGain;
            float wl, wr;
            if (pp)
            {
                wl = 0.5f * (inL + inR) + fb * f[1];
                wr = fb * f[0];
            }
            else { wl = inL + fb * f[0]; wr = inR + fb * f[1]; }
            buf.push (dsp::fastTanh (wl * 0.8f) / 0.8f, dsp::fastTanh (wr * 0.8f) / 0.8f);
            L[i] = d[0];
            R[i] = d[1];
        }
        lp.sanitize(); hp.sanitize();
    }

private:
    dsp::RingBuffer buf;
    dsp::SVF lp, hp;
    double cur[2] { -1, -1 };
    double wowPh = 0;
    float sendGain = 0;
};
} // namespace idg
