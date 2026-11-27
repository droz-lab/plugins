#pragma once
#include "EffectBase.h"

namespace idg
{
/** Comb / résonateur accordé, jusqu'à 4 voix (accords dans la gamme globale). */
class Comb : public EffectModule
{
public:
    enum { Freq, Fb, Damp, Chord };
    void prepare (double sr, int) override { for (auto& b : lines) b.allocate ((int) (sr / 15.0) + 8); reset(); }
    void reset() override { for (auto& b : lines) b.clear(); for (auto& z : lp) z[0] = z[1] = 0; }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        const int chord = I (Chord);
        const int voices = chord == 0 ? 1 : chord == 1 ? 2 : chord == 2 ? 2 : chord == 3 ? 3 : 4;
        const float fb = P (Fb);
        const float damp = P (Damp) * 0.95f;
        double delays[4][2];
        for (int k = 0; k < 2; ++k)
        {
            const float f0 = P (Freq, k);
            const float midi = 69.0f + 12.0f * std::log2 (juce::jmax (10.0f, f0) / 440.0f);
            const int m = (int) std::round (midi);
            float notes[4] { midi, midi, midi, midi };
            const Scale& sc = *ctx.scale;
            if (chord == 1) notes[1] = midi + 12.0f;
            if (chord == 2) notes[1] = midi + 7.0f;
            if (chord >= 3) { notes[1] = (float) sc.degreeAbove (m, 2) + (midi - (float) m); notes[2] = (float) sc.degreeAbove (m, 4) + (midi - (float) m); }
            if (chord == 4) notes[3] = (float) sc.degreeAbove (m, 6) + (midi - (float) m);
            for (int v = 0; v < 4; ++v)
            {
                double hz = 440.0 * std::pow (2.0, (notes[v] - 69.0) / 12.0);
                delays[v][k] = juce::jlimit (2.0, (double) lines[0].size - 4.0, ctx.sr / hz);
            }
        }
        const float norm = (0.5f + 0.5f * (1.0f - std::abs (fb))) / std::sqrt ((float) voices);
        for (int i = 0; i < n; ++i)
        {
            float in[2] { L[i], R[i] }, out[2] { 0, 0 };
            for (int v = 0; v < voices; ++v)
            {
                float y[2];
                for (int k = 0; k < 2; ++k)
                {
                    float d = lines[v].delay (k, delays[v][k] - 1.0);
                    lp[v][k] = d + (lp[v][k] - d) * damp;
                    y[k] = in[k] + fb * dsp::fastTanh (lp[v][k]);
                    out[k] += y[k];
                }
                lines[v].push (y[0], y[1]);
            }
            L[i] = out[0] * norm;
            R[i] = out[1] * norm;
        }
    }

private:
    dsp::RingBuffer lines[4];
    float lp[4][2] {};
};
} // namespace idg
