#pragma once
#include "EffectBase.h"

namespace idg
{
/** Scratch : la tête de lecture se déplace dans le passé récent comme un disque manipulé. */
class Scratch : public EffectModule
{
public:
    enum { Pos, Auto, Rate, Depth, Smooth };

    void prepare (double sr, int) override { buf.allocate ((int) (sr * 6.0)); reset(); }
    void reset() override { buf.clear(); curDelay = 0; }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        const double maxDelay = juce::jmin ((double) buf.size * 0.4, ctx.spb * 2.0);
        double target;
        if (B (Auto))
        {
            double rate = divisionBeats (I (Rate));
            double ph = std::fmod (ctx.ppq / rate, 1.0);
            // aller-retour façon main sur le disque : rapide en arrière, plus lent en avant
            double tri = ph < 0.35 ? ph / 0.35 : 1.0 - (ph - 0.35) / 0.65;
            target = (tri * P (Depth) + P (Pos) * (1.0 - P (Depth))) * maxDelay;
        }
        else target = P (Pos) * P (Depth) * maxDelay + P (Pos) * (1.0 - P (Depth)) * maxDelay * 0.25;

        const double tau = (1.0 + P (Smooth) * 150.0) * 0.001 * ctx.sr;
        const double a = std::exp (-1.0 / tau);
        for (int i = 0; i < n; ++i)
        {
            buf.push (L[i], R[i]);
            curDelay = target + (curDelay - target) * a;
            L[i] = buf.delay (0, curDelay);
            R[i] = buf.delay (1, curDelay);
        }
    }

private:
    dsp::RingBuffer buf;
    double curDelay = 0.0;
};
} // namespace idg
