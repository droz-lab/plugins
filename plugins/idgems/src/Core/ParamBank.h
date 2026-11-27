#pragma once
#include "ParamDefs.h"
#include "Scale.h"

namespace idg
{
/** Un paramètre tel que vu par le thread audio : valeur de base + modulation + drift. */
struct MParam
{
    std::atomic<float>* raw = nullptr;
    const ParamDef* def = nullptr;
    juce::NormalisableRange<float> range;
    float val[2] {};          // valeur finale (dénormalisée) par canal
    float modAcc[2] {};       // somme des modulations (normalisée)
    bool modActive = false;
    std::atomic<float> display01 { 0.0f }; // valeur modulée pour l'affichage
    std::atomic<bool> locked { false };
    std::atomic<float> rmin { 0.0f }, rmax { 1.0f };

    inline float get (int ch = 0) const { return val[ch]; }
    inline int geti (int ch = 0) const { return (int) std::lround (val[ch]); }
    inline bool on() const { return val[0] > 0.5f; }
};

class ParamBank
{
public:
    void init (juce::AudioProcessorValueTreeState& apvts);

    /** Thread audio : calcule les valeurs finales. */
    void update (const Scale& scale, float driftAmount, double driftTime, const bool* fxActive);

    MParam& operator[] (int i) { return params[(size_t) i]; }
    const MParam& operator[] (int i) const { return params[(size_t) i]; }
    int size() const { return (int) params.size(); }
    MParam* byId (const juce::String& id);

    MParam** fxParams (int fx) { return fxPtrs[(size_t) fx].data(); }

    void clearMod()
    {
        for (auto& p : params) { p.modAcc[0] = p.modAcc[1] = 0.0f; p.modActive = false; }
    }

private:
    std::vector<MParam> params;
    std::array<std::vector<MParam*>, NumFx> fxPtrs;
};
} // namespace idg
