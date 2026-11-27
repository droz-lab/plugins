#pragma once
#include "EffectBase.h"

namespace idg
{
/** Vocoder à porteuse interne (dent de scie, carré, bruit ou accord dans la gamme). */
class Vocoder : public EffectModule
{
public:
    enum { Carrier, Note, Bands, Release, ShiftP };
    static constexpr int maxBands = 24;

    void prepare (double, int) override { reset(); }
    void reset() override
    {
        for (auto& f : mod) f.reset();
        for (auto& f : car) f.reset();
        for (auto& e : env) e = 0;
        for (auto& p : ph) p = 0;
        rng.seed (0x70C0u);
    }

    static float polyBlep (double t, double dt)
    {
        if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        static const int bandCounts[] { 8, 12, 16, 24 };
        const int nb = bandCounts[juce::jlimit (0, 3, I (Bands))];
        const float shift = std::pow (2.0f, P (ShiftP) / 12.0f);
        const float q = 2.0f + (float) nb * 0.45f;
        if (nb != lastBands || std::abs (shift - lastShift) > 1.0e-4f || ctx.sr != lastSr)
        {
            for (int b = 0; b < nb; ++b)
            {
                float f = 80.0f * std::pow (10000.0f / 80.0f, (float) b / (float) (nb - 1));
                mod[b].set (ctx.sr, f, q);
                car[b].set (ctx.sr, juce::jmin (f * shift, (float) ctx.sr * 0.45f), q);
            }
            lastBands = nb; lastShift = shift; lastSr = ctx.sr;
        }
        const double relA = std::exp (-1.0 / (P (Release) * 0.001 * ctx.sr));
        const double attA = std::exp (-1.0 / (0.002 * ctx.sr));
        const int type = I (Carrier);
        const float note = P (Note);
        const int root = (int) std::round (note);
        int chordNotes[3] { root, ctx.scale->degreeAbove (root, 2), ctx.scale->degreeAbove (root, 4) };
        double inc[3];
        for (int v = 0; v < 3; ++v)
            inc[v] = 440.0 * std::pow (2.0, ((v == 0 ? note : (float) chordNotes[v]) - 69.0) / 12.0) / ctx.sr;

        const float gainOut = 3.0f * std::sqrt ((float) nb);
        for (int i = 0; i < n; ++i)
        {
            float m = 0.5f * (L[i] + R[i]);
            // porteuse
            float c = 0;
            if (type == 2) c = rng.bi();
            else
            {
                int nv = type == 3 ? 3 : 1;
                for (int v = 0; v < nv; ++v)
                {
                    float saw = (float) (2.0 * ph[v] - 1.0) - polyBlep (ph[v], inc[v]);
                    if (type == 1)
                    {
                        double p2 = ph[v] + 0.5; p2 -= std::floor (p2);
                        float saw2 = (float) (2.0 * p2 - 1.0) - polyBlep (p2, inc[v]);
                        saw = 0.5f * (saw - saw2);
                    }
                    c += saw * (nv == 3 ? 0.5f : 1.0f);
                    ph[v] += inc[v];
                    if (ph[v] >= 1.0) ph[v] -= 1.0;
                }
            }
            float out = 0;
            for (int b = 0; b < nb; ++b)
            {
                float mb = mod[b].bandpass (0, m) / q;
                float level = std::abs (mb);
                double a = level > env[b] ? attA : relA;
                env[b] = (float) (level + (env[b] - level) * a);
                out += car[b].bandpass (0, c) / q * env[b];
            }
            out *= gainOut;
            L[i] = out;
            R[i] = out;
        }
        for (int b = 0; b < nb; ++b) { mod[b].sanitize(); car[b].sanitize(); }
    }

private:
    dsp::SVF mod[maxBands], car[maxBands];
    float env[maxBands] {};
    double ph[3] {};
    int lastBands = -1;
    float lastShift = 0;
    double lastSr = 0;
    dsp::Rng rng;
};
} // namespace idg
