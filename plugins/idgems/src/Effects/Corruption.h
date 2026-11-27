#pragma once
#include "EffectBase.h"

namespace idg
{
/** Corruption : micro-coupures, paquets perdus, CD qui saute, décrochages. */
class Corruption : public EffectModule
{
public:
    enum { Rate, Size, Type, Jitter };
    void prepare (double sr, int) override { buf.allocate ((int) (sr * 3.0)); reset(); }
    void reset() override { buf.clear(); ev = None; left = 0; rng.seed (0xC0DEu); }
    void trigger (const ProcCtx&) override { forceEvent = true; }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        const double perSample = P (Rate) / ctx.sr;
        const double size = P (Size) * 0.001 * ctx.sr;
        const int type = I (Type);
        const float jitter = P (Jitter);
        const int fade = 24 * ctx.os;

        for (int i = 0; i < n; ++i)
        {
            buf.push (L[i], R[i]);
            if (ev == None && (forceEvent || rng.uni() < perSample))
            {
                forceEvent = false;
                startEvent (type == 0 ? 1 + rng.range (4) : type, size * (1.0 + jitter * rng.bi() * 0.8), ctx.sr);
            }
            if (ev == None) continue;

            int done = len - left;
            float w = juce::jmin (1.0f, (float) done / (float) fade, (float) left / (float) fade);
            float l = L[i], r = R[i];
            switch (ev)
            {
                case Dropout: l = 0; r = 0; break;
                case Packet:  { int rp = src + done; l = buf.at (0, rp); r = buf.at (1, rp); break; }
                case Skip:    { int rp = src + (done % juce::jmax (1, loopLen)); l = buf.at (0, rp); r = buf.at (1, rp);
                                float lw = juce::jmin (1.0f, (float) (done % juce::jmax (1, loopLen)) / 16.0f,
                                                       (float) (loopLen - done % juce::jmax (1, loopLen)) / 16.0f);
                                l *= lw; r *= lw; break; }
                case Stall:   { int rp = src + (done % juce::jmax (1, loopLen)); l = buf.at (0, rp) * 0.9f; r = buf.at (1, rp) * 0.9f; break; }
                default: break;
            }
            L[i] = L[i] * (1.0f - w) + l * w;
            R[i] = R[i] * (1.0f - w) + r * w;
            if (--left <= 0) ev = None;
        }
    }

private:
    enum Ev { None, Dropout, Packet, Skip, Stall };
    void startEvent (int t, double sz, double sr)
    {
        ev = (Ev) juce::jlimit (1, 4, t);
        len = left = juce::jmax (32, (int) sz);
        switch (ev)
        {
            case Packet: src = buf.w - len; break;
            case Skip:   loopLen = juce::jmax (64, (int) (sz / (2 + rng.range (3)))); src = buf.w - loopLen - rng.range (loopLen); break;
            case Stall:  loopLen = juce::jmax (16, (int) (sr * (0.002 + rng.uni() * 0.01))); src = buf.w - loopLen; break;
            default: break;
        }
        len = left = juce::jmin (len, buf.size / 2);
    }

    dsp::RingBuffer buf;
    dsp::Rng rng;
    Ev ev = None;
    int left = 0, len = 0, src = 0, loopLen = 64;
    bool forceEvent = false;
};
} // namespace idg
