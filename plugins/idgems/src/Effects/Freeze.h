#pragma once
#include "EffectBase.h"

namespace idg
{
/** Freeze granulaire : capture un fragment puis le fait vivre en nuage de grains. */
class Freeze : public EffectModule
{
public:
    enum { Size, Density, Spray, Pitch, Spread, Length };

    void prepare (double sr, int) override
    {
        cap[0].assign ((size_t) (sr * 2.2), 0.0f);
        cap[1].assign ((size_t) (sr * 2.2), 0.0f);
        reset();
    }
    void reset() override
    {
        capturing = true; capPos = 0; capLen = 0; spawnAcc = 0;
        for (auto& g : grains) g.active = false;
        rng.seed (0xF2EEu);
    }
    void trigger (const ProcCtx&) override { capturing = true; capPos = 0; }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        const int maxCap = (int) cap[0].size();
        const int wantLen = juce::jlimit (64, maxCap, (int) (P (Length) * 0.001 * ctx.sr));
        const double grainLen = juce::jmax (16.0, P (Size) * 0.001 * ctx.sr);
        const float density = P (Density);
        const double interval = grainLen / density;
        const float spray = P (Spray), spread = P (Spread);
        const double ratio = std::pow (2.0, P (Pitch) / 12.0);
        const float norm = 1.0f / juce::jmax (1.0f, std::sqrt (density * 0.5f));

        for (int i = 0; i < n; ++i)
        {
            if (capturing)
            {
                cap[0][(size_t) capPos] = L[i];
                cap[1][(size_t) capPos] = R[i];
                if (++capPos >= wantLen) { capturing = false; capLen = wantLen; }
                if (capLen == 0) continue; // rien de figé encore : on laisse passer
            }

            if (++spawnAcc >= interval)
            {
                spawnAcc = 0;
                spawn (grainLen, ratio, spray, spread);
            }

            float l = 0, r = 0;
            for (auto& g : grains)
            {
                if (! g.active) continue;
                float ph = (float) (g.age / g.len);
                float w = 0.5f - 0.5f * std::cos (dsp::twoPi * ph);
                double rp = std::fmod (g.pos + g.age * g.ratio, (double) capLen);
                if (rp < 0) rp += capLen;
                auto i0 = (int) rp; float fr = (float) (rp - i0);
                int i1 = (i0 + 1) % capLen;
                float sl = cap[0][(size_t) i0] + (cap[0][(size_t) i1] - cap[0][(size_t) i0]) * fr;
                float sr = cap[1][(size_t) i0] + (cap[1][(size_t) i1] - cap[1][(size_t) i0]) * fr;
                l += sl * w * g.gl;
                r += sr * w * g.gr;
                g.age += 1.0;
                if (g.age >= g.len) g.active = false;
            }
            L[i] = l * norm;
            R[i] = r * norm;
        }
    }

private:
    struct Grain { bool active = false; double pos = 0, age = 0, len = 1, ratio = 1; float gl = 1, gr = 1; };

    void spawn (double len, double ratio, float spray, float spread)
    {
        if (capLen <= 0) return;
        for (auto& g : grains)
        {
            if (g.active) continue;
            g.active = true;
            g.age = 0;
            g.len = len;
            double centre = capLen * 0.5;
            g.pos = centre + (rng.bi() * 0.5 * spray) * capLen - len * 0.5 * ratio;
            g.ratio = ratio * std::pow (2.0, (double) (rng.bi() * spray * 0.1f) / 12.0);
            float pan = rng.bi() * spread;
            g.gl = juce::jmin (1.0f, 1.0f - pan);
            g.gr = juce::jmin (1.0f, 1.0f + pan);
            return;
        }
    }

    std::vector<float> cap[2];
    std::array<Grain, 48> grains;
    dsp::Rng rng;
    bool capturing = true;
    int capPos = 0, capLen = 0;
    double spawnAcc = 0;
};
} // namespace idg
