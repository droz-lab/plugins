#pragma once
#include "EffectBase.h"

namespace idg
{
class Tremolo : public EffectModule
{
public:
    enum { Rate, Sync, Div, Depth, Shape, Stereo };
    void prepare (double, int) override { reset(); }
    void reset() override { ph = 0; sm[0] = sm[1] = 1; }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        const bool sync = B (Sync);
        const double div = divisionBeats (I (Div));
        const double inc = P (Rate) / ctx.sr;
        const float depth = P (Depth), stereo = P (Stereo);
        const int shape = I (Shape);
        double ppq = ctx.ppq;
        const double bps = ctx.beatsPerSample();
        float* ch[2] { L, R };
        for (int i = 0; i < n; ++i)
        {
            double phase = sync ? ppq / div : ph;
            for (int k = 0; k < 2; ++k)
            {
                float u = 0.5f + 0.5f * dsp::shapeValue (shape, (float) (phase + (k == 1 ? stereo * 0.5 : 0.0)));
                float gain = 1.0f - depth * (1.0f - u);
                sm[k] = gain + (sm[k] - gain) * 0.9f; // anti-clic
                ch[k][i] *= sm[k];
            }
            ph += inc; ph -= std::floor (ph);
            ppq += bps;
        }
    }

private:
    double ph = 0;
    float sm[2] { 1, 1 };
};
} // namespace idg
