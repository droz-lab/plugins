#pragma once
#include "EffectBase.h"

namespace idg
{
/** Tape Stop : la bande ralentit jusqu'à l'arrêt, puis redémarre. */
class TapeStop : public EffectModule
{
public:
    enum { Stop, Start, Curve, Auto };

    void prepare (double sr, int) override { buf.allocate ((int) (sr * 10.0)); reset(); }
    void reset() override
    {
        buf.clear(); state = Running; t = 0; speed = 1.0; readPos = 0; lag = 0; xfade = 0; forced = true;
    }
    void trigger (const ProcCtx&) override { forced = true; }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        const int mode = I (Auto);
        bool wantStop;
        if (mode == 0) wantStop = true; // mode Gate : arrêt dès que l'effet est actif (la porte du séquenceur coupe)
        else
        {
            static const double periods[] { 0, 2.0, 4.0, 8.0 };
            double period = periods[mode] / 4.0 * ctx.beatsPerBar;
            double ph = std::fmod (ctx.ppq, period);
            double stopBeats = (P (Stop) * 0.001 * ctx.baseSr) / (ctx.spb / ctx.os);
            wantStop = ph > period - juce::jmin (period * 0.9, stopBeats);
        }
        if (forced && mode == 0) { state = Running; forced = false; }
        forced = false;

        const double stopLen = P (Stop) * 0.001 * ctx.sr;
        const double startLen = P (Start) * 0.001 * ctx.sr;
        const double expo = 0.4 + P (Curve) * 2.6;

        for (int i = 0; i < n; ++i)
        {
            buf.push (L[i], R[i]);
            if (state == Running && wantStop) { state = Stopping; t = 0; readPos = (double) (buf.w - 1); }
            if (state == Stopped && ! wantStop) { state = Starting; t = 0; }

            if (state == Running)
            {
                if (xfade > 0)
                {
                    float w = (float) xfade / 256.0f;
                    readPos += 1.0;
                    L[i] = L[i] * (1.0f - w) + buf.read (0, readPos) * w;
                    R[i] = R[i] * (1.0f - w) + buf.read (1, readPos) * w;
                    --xfade;
                }
                continue;
            }

            if (state == Stopping)
            {
                double prog = juce::jmin (1.0, (double) t / juce::jmax (1.0, stopLen));
                speed = std::pow (1.0 - prog, expo);
                if (prog >= 1.0) { state = Stopped; speed = 0.0; }
            }
            else if (state == Starting)
            {
                double prog = juce::jmin (1.0, (double) t / juce::jmax (1.0, startLen));
                speed = std::pow (prog, expo);
                if (prog >= 1.0) { state = Running; xfade = 256; }
            }
            else speed = 0.0;

            ++t;
            readPos += speed;
            float g = (float) juce::jmin (1.0, speed * 4.0);
            L[i] = buf.read (0, readPos) * g;
            R[i] = buf.read (1, readPos) * g;
        }
    }

private:
    enum State { Running, Stopping, Stopped, Starting };
    dsp::RingBuffer buf;
    State state = Running;
    int64_t t = 0;
    double speed = 1.0, readPos = 0.0, lag = 0.0;
    int xfade = 0;
    bool forced = true;
};
} // namespace idg
