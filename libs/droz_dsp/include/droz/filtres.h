// Filtres et utilitaires stereo, sans allocation. Migre depuis IDGems (Core/Dsp.h).
// Tout ce qui est ici doit avoir son test dans libs/droz_dsp/tests/tests.cpp.
#pragma once

#include <algorithm>
#include <cmath>

namespace droz
{
/** Filtre a variable d'etat (TPT, Cytomic), stereo. Passe-bas, passe-bande, passe-haut, coupe-bande. */
class FiltreSVF
{
public:
    void regle (double sampleRate, float frequence, float q) noexcept
    {
        frequence = std::clamp (frequence, 5.0f, static_cast<float> (sampleRate * 0.49));
        g = static_cast<float> (std::tan (3.14159265358979 * frequence / sampleRate));
        k = 1.0f / std::max (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    void reset() noexcept { ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0.0f; }

    float passeBas (int canal, float x) noexcept { traite (canal, x); return lp; }
    float passeHaut (int canal, float x) noexcept { traite (canal, x); return hp; }
    float passeBande (int canal, float x) noexcept { traite (canal, x); return bp; }
    float coupeBande (int canal, float x) noexcept { traite (canal, x); return lp + hp; }

    /** Remet a zero un etat devenu non fini (protection contre le NaN). */
    void assainir() noexcept
    {
        for (int c = 0; c < 2; ++c)
            if (! std::isfinite (ic1[c]) || ! std::isfinite (ic2[c]))
                ic1[c] = ic2[c] = 0.0f;
    }

private:
    void traite (int canal, float v0) noexcept
    {
        const float v3 = v0 - ic2[canal];
        const float v1 = a1 * ic1[canal] + a2 * v3;
        const float v2 = ic2[canal] + a2 * ic1[canal] + a3 * v3;
        ic1[canal] = 2.0f * v1 - ic1[canal];
        ic2[canal] = 2.0f * v2 - ic2[canal];
        lp = v2;
        bp = v1;
        hp = v0 - k * v1 - v2;
    }

    float g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1[2] {}, ic2[2] {};
    float lp = 0.0f, bp = 0.0f, hp = 0.0f;
};

/** Filtre un pole, stereo. Etat en double : en float il se fige loin de sa cible. */
class UnPole
{
public:
    /** Coupure en Hz : a = exp(-2 pi f / sr). */
    void regleCoupure (double sampleRate, float hz) noexcept
    {
        a = std::exp (-2.0 * 3.14159265358979 * std::max (1.0f, hz) / sampleRate);
    }

    /** Temps en ms pour faire environ 63 % du chemin. */
    void regleTemps (double sampleRate, float ms) noexcept
    {
        a = std::exp (-1.0 / (std::max (0.01, static_cast<double> (ms)) * 0.001 * sampleRate));
    }

    float passeBas (int canal, float x) noexcept
    {
        z[canal] = x + (z[canal] - x) * a;
        return static_cast<float> (z[canal]);
    }

    void reset() noexcept { z[0] = z[1] = 0.0; }

private:
    double z[2] {}, a = 0.0;
};

/** Retire la composante continue. */
class BloqueurDC
{
public:
    void prepare (double sampleRate) noexcept
    {
        r = std::exp (-2.0 * 3.14159265358979 * 8.0 / sampleRate);
    }

    float traite (int canal, float x) noexcept
    {
        const float y = x - x1[canal] + r * y1[canal];
        x1[canal] = x;
        y1[canal] = y;
        return y;
    }

    void reset() noexcept { x1[0] = x1[1] = y1[0] = y1[1] = 0.0f; }

private:
    float x1[2] {}, y1[2] {}, r = 0.995f;
};
} // namespace droz
