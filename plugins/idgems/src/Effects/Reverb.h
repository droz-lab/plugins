#pragma once
#include "EffectBase.h"

namespace idg
{
/** Reverb FDN 8 lignes (matrice de Hadamard), diffusion d'entrée, modulation, mode throw. */
class Reverb : public EffectModule
{
public:
    enum { Size, Decay, Damp, Predelay, Width, Mod, Throw };
    void prepare (double sr, int) override
    {
        srMax = sr;
        for (auto& l : lines) l.allocate ((int) (sr * 0.35));
        for (auto& a : diff) a.allocate ((int) (sr * 0.03));
        pre.allocate ((int) (sr * 0.3));
        reset();
    }
    void reset() override
    {
        for (auto& l : lines) l.clear();
        for (auto& a : diff) a.clear();
        pre.clear();
        for (auto& z : lpz) z = 0;
        modPh = 0; sendGain = 0;
    }
    bool throwMode() const override { return B (Throw); }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        static const float base[8] { 0.0297f, 0.0371f, 0.0411f, 0.0437f, 0.0533f, 0.0617f, 0.0683f, 0.0751f };
        static const float diffT[4] { 0.0047f, 0.0071f, 0.0109f, 0.0137f };
        const float size = 0.35f + P (Size) * 1.6f;
        const float decay = P (Decay);
        float len[8], g[8];
        for (int j = 0; j < 8; ++j)
        {
            len[j] = juce::jmin ((float) lines[j].size - 64.0f, base[j] * size * (float) ctx.sr);
            g[j] = std::pow (10.0f, -3.0f * (len[j] / (float) ctx.sr) / decay);
        }
        const float damp = P (Damp) * 0.9f;
        const double preD = juce::jmax (1.0, P (Predelay) * 0.001 * ctx.sr);
        const float width = P (Width);
        const float modDepth = P (Mod) * 0.0012f * (float) ctx.sr;
        const float sendTarget = (! B (Throw) || ctx.gate) ? 1.0f : 0.0f;
        const float sendStep = 1.0f / (float) (0.004 * ctx.sr);

        for (int i = 0; i < n; ++i)
        {
            sendGain += juce::jlimit (-sendStep, sendStep, sendTarget - sendGain);
            pre.push (L[i] * sendGain, R[i] * sendGain);
            float x[2] { pre.delay (0, preD), pre.delay (1, preD) };
            // diffusion (passe-tout en série)
            for (int a = 0; a < 4; ++a)
            {
                const double dt = diffT[a] * ctx.sr;
                float y[2];
                for (int k = 0; k < 2; ++k)
                {
                    float dd = diff[a].delay (k, dt * (k == 0 ? 1.0 : 1.07));
                    float v = x[k] + 0.6f * dd;
                    y[k] = dd - 0.6f * v;
                    x[k] = y[k];
                    tmpv[k] = v;
                }
                diff[a].push (tmpv[0], tmpv[1]);
            }
            modPh += 0.3 / ctx.sr; if (modPh >= 1.0) modPh -= 1.0;
            float m1 = std::sin (dsp::twoPi * (float) modPh) * modDepth;
            float m2 = std::cos (dsp::twoPi * (float) modPh * 1.3f) * modDepth;
            float o[8];
            for (int j = 0; j < 8; ++j)
            {
                double d = len[j] + (j == 1 ? m1 : j == 6 ? m2 : 0.0f);
                float v = lines[j].delay (0, d);
                lpz[j] = v + (lpz[j] - v) * damp;
                o[j] = lpz[j] * g[j];
            }
            // Hadamard 8x8
            float h[8];
            hadamard (o, h);
            for (int j = 0; j < 8; ++j)
            {
                float in = (j & 1) ? x[1] : x[0];
                float v = h[j] * 0.35355339f + in * 0.5f;
                lines[j].push (dsp::fastTanh (v * 0.5f) * 2.0f, 0.0f);
            }
            float wl = o[0] + o[2] + o[4] + o[6];
            float wr = o[1] + o[3] + o[5] + o[7];
            float mid = 0.5f * (wl + wr), side = 0.5f * (wl - wr) * width;
            L[i] = (mid + side) * 0.9f;
            R[i] = (mid - side) * 0.9f;
        }
        for (auto& z : lpz) if (! dsp::finite (z)) z = 0;
    }

private:
    static void hadamard (const float* in, float* out)
    {
        float a[8];
        for (int j = 0; j < 8; ++j) a[j] = in[j];
        for (int s = 1; s < 8; s <<= 1)
            for (int j = 0; j < 8; j += s * 2)
                for (int k = j; k < j + s; ++k)
                {
                    float u = a[k], v = a[k + s];
                    a[k] = u + v; a[k + s] = u - v;
                }
        for (int j = 0; j < 8; ++j) out[j] = a[j];
    }

    dsp::RingBuffer lines[8], diff[4], pre;
    float lpz[8] {};
    float tmpv[2] {};
    double modPh = 0, srMax = 44100;
    float sendGain = 0;
};
} // namespace idg
