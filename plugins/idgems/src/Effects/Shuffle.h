#pragma once
#include "EffectBase.h"

namespace idg
{
/** Buffer Shuffle : rejoue au hasard des tranches de la mémoire récente. */
class Shuffle : public EffectModule
{
public:
    enum { Div, Chance, Rev, Window };

    void prepare (double sr, int) override { buf.allocate ((int) (sr * 20.0)); reset(); }
    void reset() override { buf.clear(); lastIdx = -1; pos = 0; active = false; filled = 0; rng.seed (0x5A5Au); forced = false; }
    void trigger (const ProcCtx&) override { forced = true; }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        const double beats = divisionBeats (I (Div));
        sliceLen = (int) juce::jlimit (64.0, (double) buf.size * 0.05, beats * ctx.spb);
        static const double win[] { 1.0, 2.0, 4.0, 8.0 };
        double windowBeats = win[juce::jlimit (0, 3, I (Window))];
        if (I (Window) >= 2) windowBeats = windowBeats / 4.0 * ctx.beatsPerBar;
        const int slicesInWindow = juce::jmax (1, (int) std::round (windowBeats / beats));

        auto idx = (int64_t) std::floor (ctx.ppq / beats);
        if (idx != lastIdx)
        {
            lastIdx = idx;
            pos = 0;
            int available = juce::jmin (slicesInWindow, filled / juce::jmax (1, sliceLen));
            active = available > 0 && (forced || rng.uni() < P (Chance));
            forced = false;
            if (active)
            {
                int k = 1 + rng.range (available);
                srcStart = buf.w - k * sliceLen;
                reversed = rng.uni() < P (Rev);
            }
        }
        const float fade = juce::jmin (64.0f * (float) ctx.os, (float) sliceLen * 0.25f);
        for (int i = 0; i < n; ++i)
        {
            buf.push (L[i], R[i]);
            if (filled < buf.size) ++filled;
            if (active && pos < sliceLen)
            {
                float w = juce::jmin (1.0f, (float) pos / fade, (float) (sliceLen - pos) / fade);
                int rp = reversed ? srcStart + sliceLen - 1 - pos : srcStart + pos;
                L[i] = L[i] * (1.0f - w) + buf.at (0, rp) * w;
                R[i] = R[i] * (1.0f - w) + buf.at (1, rp) * w;
            }
            ++pos;
        }
    }

private:
    dsp::RingBuffer buf;
    dsp::Rng rng;
    int64_t lastIdx = -1;
    int pos = 0, sliceLen = 1024, srcStart = 0, filled = 0;
    bool active = false, reversed = false, forced = false;
};
} // namespace idg
