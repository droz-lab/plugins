#pragma once
#include "EffectBase.h"

namespace idg
{
/** Beat Repeat : à chaque intervalle, chance de répéter une tranche de grille. */
class BeatRepeat : public EffectModule
{
public:
    enum { Interval, Grid, Chance, Length, PDecay, VDecay };

    void prepare (double sr, int) override { buf.allocate ((int) (sr * 6.0)); reset(); }
    void reset() override { buf.clear(); repeating = false; lastIdx = -1; since = 0; forced = false; rng.seed (0xBEA7u); }
    void trigger (const ProcCtx&) override { forced = true; }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        static const double intervals[] { 1.0, 2.0, 4.0, 8.0 };
        double interval = intervals[juce::jlimit (0, 3, I (Interval))];
        if (I (Interval) >= 2) interval = interval / 4.0 * ctx.beatsPerBar;
        const double grid = juce::jlimit (32.0, (double) buf.size * 0.4, divisionBeats (I (Grid)) * ctx.spb);

        auto idx = (int64_t) std::floor (ctx.ppq / interval);
        if (idx != lastIdx || forced)
        {
            lastIdx = idx;
            if (forced || rng.uni() < P (Chance))
            {
                repeating = true; since = 0; startPos = buf.w;
                duration = juce::jmax (grid, P (Length) * interval * ctx.spb);
            }
            else repeating = false;
            forced = false;
        }

        const float pd = P (PDecay), vd = P (VDecay);
        const double fade = juce::jmin (64.0 * ctx.os, grid * 0.2);
        for (int i = 0; i < n; ++i)
        {
            buf.push (L[i], R[i]);
            if (! repeating) continue;
            if ((double) since >= duration) { repeating = false; continue; }
            if ((double) since >= grid)
            {
                const auto rep = (int) ((double) since / grid);
                const double local = std::fmod ((double) since, grid);
                const double speed = std::pow (1.0 - pd * 0.25, (double) rep);
                float amp = std::pow (1.0f - vd * 0.8f, (float) rep);
                amp *= (float) juce::jlimit (0.0, 1.0, juce::jmin (local / fade, (grid - local) / fade));
                double tail = duration - (double) since;
                amp *= (float) juce::jlimit (0.0, 1.0, tail / fade);
                L[i] = amp * buf.read (0, (double) startPos + local * speed);
                R[i] = amp * buf.read (1, (double) startPos + local * speed);
            }
            ++since;
        }
    }

private:
    dsp::RingBuffer buf;
    dsp::Rng rng;
    bool repeating = false, forced = false;
    int64_t lastIdx = -1;
    int since = 0, startPos = 0;
    double duration = 0.0;
};
} // namespace idg
