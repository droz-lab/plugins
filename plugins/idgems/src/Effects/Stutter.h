#pragma once
#include "EffectBase.h"

namespace idg
{
/** Stutter / Retrigger : capture une tranche et la répète. */
class Stutter : public EffectModule
{
public:
    enum { Div, Retrig, Gate, Decay, Pitch };

    void prepare (double sr, int) override { buf.allocate ((int) (sr * 6.0)); reset(); }
    void reset() override { buf.clear(); since = 0; trigPos = 0; lastRetrigIndex = -1; needTrigger = true; }

    void trigger (const ProcCtx&) override { needTrigger = true; }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        const double slice = juce::jlimit (32.0, (double) buf.size * 0.4, divisionBeats (I (Div)) * ctx.spb);
        static const double retrigBeats[] { 0.0, 0.25, 0.5, 1.0, 2.0, 4.0 };
        const double rb = retrigBeats[juce::jlimit (0, 5, I (Retrig))];
        if (rb > 0.0)
        {
            auto idx = (int64_t) std::floor (ctx.ppq / rb);
            if (idx != lastRetrigIndex) { lastRetrigIndex = idx; needTrigger = true; }
        }

        const float gate = P (Gate), decay = P (Decay), pitch = P (Pitch);
        const double fade = juce::jmin (96.0 * ctx.os, slice * 0.25);

        for (int i = 0; i < n; ++i)
        {
            buf.push (L[i], R[i]);
            if (needTrigger) { needTrigger = false; since = 0; trigPos = buf.w - 1; }

            if ((double) since < slice) { ++since; continue; } // premier passage : direct

            const auto rep = (int) ((double) since / slice);
            const double local = std::fmod ((double) since, slice);
            const double ratio = std::pow (2.0, (double) pitch * rep / 12.0);
            const double readLocal = std::fmod (local * ratio, slice);
            float amp = std::pow (1.0f - decay * 0.9f, (float) rep);
            const double played = slice * gate;
            if (local > played) amp = 0.0f;
            else
            {
                double env = juce::jmin (1.0, local / fade, (played - local) / fade);
                amp *= (float) juce::jmax (0.0, env);
            }
            // le tout premier point de chaque répétition reste un peu atténué pour éviter les clics
            L[i] = amp * buf.read (0, (double) trigPos + readLocal);
            R[i] = amp * buf.read (1, (double) trigPos + readLocal);
            if (since < (1 << 30)) ++since;
        }
    }

private:
    dsp::RingBuffer buf;
    int since = 0;
    int trigPos = 0;
    int64_t lastRetrigIndex = -1;
    bool needTrigger = true;
};
} // namespace idg
