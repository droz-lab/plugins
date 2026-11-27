#pragma once
#include "EffectBase.h"

namespace idg
{
/** Frequency shifter (transformée de Hilbert par deux chaînes de passe-tout). */
class FreqShift : public EffectModule
{
public:
    enum { Shift, Fb, Spread };
    void prepare (double, int) override { reset(); }
    void reset() override
    {
        for (auto& c : st) for (auto& s : c) s = {};
        for (int k = 0; k < 2; ++k) { ph[k] = 0; last[k] = 0; delayed[k] = 0; }
    }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        const double sh = P (Shift);
        const double inc[2] { sh / ctx.sr, sh * (1.0 - 2.0 * P (Spread)) / ctx.sr };
        const float fb = P (Fb);
        float* ch[2] { L, R };
        static const float ca[4] { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
        static const float cb[4] { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
        for (int k = 0; k < 2; ++k)
            for (int i = 0; i < n; ++i)
            {
                float x = ch[k][i] + fb * last[k];
                float a = x, b = x;
                for (int s = 0; s < 4; ++s) a = ap (st[k][s], ca[s], a);
                for (int s = 0; s < 4; ++s) b = ap (st[k][4 + s], cb[s], b);
                float re = delayed[k];
                delayed[k] = a;
                float im = b;
                float c = std::cos (dsp::twoPi * (float) ph[k]), sn = std::sin (dsp::twoPi * (float) ph[k]);
                float y = re * c - im * sn;
                ph[k] += inc[k];
                ph[k] -= std::floor (ph[k]);
                last[k] = dsp::fastTanh (y);
                ch[k][i] = y;
            }
    }

private:
    struct AP { float x1 = 0, x2 = 0, y1 = 0, y2 = 0; };
    static inline float ap (AP& s, float c, float x)
    {
        float c2 = c * c;
        float y = c2 * (x + s.y2) - s.x2;
        s.x2 = s.x1; s.x1 = x; s.y2 = s.y1; s.y1 = y;
        return y;
    }
    AP st[2][8];
    double ph[2] {};
    float last[2] {}, delayed[2] {};
};
} // namespace idg
