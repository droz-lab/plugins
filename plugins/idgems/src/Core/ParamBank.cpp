#include "ParamBank.h"
#include "Dsp.h"

namespace idg
{
void ParamBank::init (juce::AudioProcessorValueTreeState& apvts)
{
    const auto& reg = registry();
    params = std::vector<MParam> (reg.all.size());
    for (size_t i = 0; i < reg.all.size(); ++i)
    {
        auto& p = params[i];
        p.def = &reg.all[i];
        p.raw = apvts.getRawParameterValue (p.def->id);
        jassert (p.raw != nullptr);
        p.range = p.def->range();
        p.val[0] = p.val[1] = p.raw->load();
        p.rmin = p.def->rmin;
        p.rmax = p.def->rmax;
    }
    for (int f = 0; f < NumFx; ++f)
    {
        fxPtrs[(size_t) f].clear();
        for (int k = 0; k < reg.fxCount[f]; ++k)
            fxPtrs[(size_t) f].push_back (&params[(size_t) (reg.fxFirst[f] + k)]);
    }
}

MParam* ParamBank::byId (const juce::String& id)
{
    auto i = registry().find (id);
    return i >= 0 ? &params[(size_t) i] : nullptr;
}

void ParamBank::update (const Scale& scale, float driftAmount, double driftTime, const bool* fxActive)
{
    const bool drift = driftAmount > 0.0001f;
    for (size_t i = 0; i < params.size(); ++i)
    {
        auto& p = params[i];
        const auto& d = *p.def;
        const float r = p.raw->load (std::memory_order_relaxed);

        if (d.effect >= 0 && fxActive != nullptr && ! fxActive[d.effect] && ! d.common)
        {
            p.val[0] = p.val[1] = r; // effet éteint : pas de calcul
            continue;
        }

        bool doDrift = drift && d.randomizable && d.type == PType::Float && ! d.common && ! p.locked.load (std::memory_order_relaxed);

        if (! p.modActive && ! doDrift && d.pitch == PitchKind::None)
        {
            p.val[0] = p.val[1] = r;
            p.display01.store (p.range.convertTo0to1 (r), std::memory_order_relaxed);
            continue;
        }

        float n = p.range.convertTo0to1 (r);
        if (doDrift)
        {
            float lo = p.rmin.load (std::memory_order_relaxed), hi = p.rmax.load (std::memory_order_relaxed);
            float nz = dsp::smoothNoise (driftTime + (double) i * 0.37, (uint32_t) i) * 2.0f - 1.0f;
            n = juce::jlimit (juce::jmin (lo, n), juce::jmax (hi, n), n + nz * driftAmount * 0.25f * (hi - lo));
        }

        for (int ch = 0; ch < 2; ++ch)
        {
            float v01 = juce::jlimit (0.0f, 1.0f, n + (p.modActive ? p.modAcc[ch] : 0.0f));
            float v = p.range.convertFrom0to1 (v01);
            if (d.type != PType::Float) v = std::round (v);
            else if (d.pitch == PitchKind::Semis) v = scale.snapInterval (v);
            else if (d.pitch == PitchKind::Midi) v = scale.snapMidi (v);
            else if (d.pitch == PitchKind::Hz) v = scale.snapHz (v);
            p.val[ch] = v;
            if (ch == 0) p.display01.store (v01, std::memory_order_relaxed);
        }
    }
}
} // namespace idg
