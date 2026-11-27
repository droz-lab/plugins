// Briques DSP communes a tous les plugins Droz. C++ pur, sans JUCE, sans allocation.
// Tout ce qui est ici doit avoir son test dans libs/droz_dsp/tests/tests.cpp.
#pragma once

#include <algorithm>
#include <cmath>

namespace droz
{
/** dB vers gain lineaire. Sous `plancher`, renvoie 0 (silence vrai). */
inline float dbToGain (float db, float plancher = -100.0f) noexcept
{
    return db <= plancher ? 0.0f : std::pow (10.0f, db * 0.05f);
}

/** Gain lineaire vers dB, borne en bas par `plancher`. */
inline float gainToDb (float gain, float plancher = -100.0f) noexcept
{
    return gain <= 0.0f ? plancher : std::max (plancher, 20.0f * std::log10 (gain));
}

/** Lissage un pole d'un parametre, pour eviter les clics quand il saute. */
class Smoother
{
public:
    /** `ms` = temps pour faire environ 63 % du chemin. */
    void reset (double sampleRate, float ms) noexcept
    {
        coeff = ms <= 0.0f ? 0.0 : std::exp (-1.0 / (0.001 * ms * sampleRate));
    }

    void setTarget (float t) noexcept { target = t; }
    void snap (float v) noexcept { target = value = v; }
    float current() const noexcept { return static_cast<float> (value); }

    float next() noexcept
    {
        value = target + coeff * (value - target);
        // Colle a la cible quand l'ecart devient inaudible : pas de denormaux, repos exact.
        if (std::abs (value - target) < 1.0e-6)
            value = target;
        return static_cast<float> (value);
    }

private:
    // Etat en double : en float, le lissage se fige a ~3e-5 de la cible pres de 1.0
    // (le pas devient plus petit que la precision). Trouve par le test, ne pas "optimiser".
    double coeff = 0.0, target = 0.0, value = 0.0;
};

/** Suiveur d'enveloppe crete, attaque et relachement separes. */
class EnvelopeFollower
{
public:
    void reset (double sampleRate, float attackMs, float releaseMs) noexcept
    {
        att = coef (sampleRate, attackMs);
        rel = coef (sampleRate, releaseMs);
        env = 0.0f;
    }

    float process (float x) noexcept
    {
        const float in = std::abs (x);
        const float c = in > env ? att : rel;
        env = in + c * (env - in);
        if (env < 1.0e-9f)
            env = 0.0f;
        return env;
    }

private:
    static float coef (double sr, float ms) noexcept
    {
        return ms <= 0.0f ? 0.0f : static_cast<float> (std::exp (-1.0 / (0.001 * ms * sr)));
    }

    float att = 0.0f, rel = 0.0f, env = 0.0f;
};
} // namespace droz
