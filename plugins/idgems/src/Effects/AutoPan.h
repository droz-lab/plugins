#pragma once
#include "EffectBase.h"

namespace idg
{
class AutoPan : public EffectModule
{
public:
    enum { Rate, Sync, Div, Depth, Shape };
    void prepare (double, int) override { reset(); }
    void reset() override { ph = 0; sm = 0; }

    void process (float* L, float* R, int n, const ProcCtx& ctx) override
    {
        const bool sync = B (Sync);
        const double div = divisionBeats (I (Div));
        const double inc = P (Rate) / ctx.sr;
        const float depth = P (Depth);
        const int shape = I (Shape);
        double ppq = ctx.ppq;
        const double bps = ctx.beatsPerSample();
        for (int i = 0; i < n; ++i)
        {
            double phase = sync ? ppq / div : ph;
            float pan = depth * dsp::shapeValue (shape, (float) phase);
            sm = pan + (sm - pan) * 0.92f;
            float gl = sm > 0 ? 1.0f - sm : 1.0f;
            float gr = sm < 0 ? 1.0f + sm : 1.0f;
            L[i] *= gl; R[i] *= gr;
            ph += inc; ph -= std::floor (ph);
            ppq += bps;
        }
    }

private:
    double ph = 0;
    float sm = 0;
};
} // namespace idg
