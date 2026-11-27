// Porte rythmique (trance gate) avec motifs euclidiens. Migre depuis IDGems (Effects/Gate.h).
// Etat en double : le gain atteint bien 1 (en float il se fige, voir droz/dsp.h Smoother).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace droz
{
/** Motif actif a ce pas ? 0 = toujours, 1..3 = euclidien 3/8, 5/8, 7/16, 4 = pseudo-aleatoire fixe. */
inline bool pasActif (int motif, std::int64_t pas, float hashPas) noexcept
{
    auto euclide = [] (std::int64_t i, int k, int n) { return ((i % n) * k) % n < k; };
    switch (motif)
    {
        case 0:  return true;
        case 1:  return euclide (pas, 3, 8);
        case 2:  return euclide (pas, 5, 8);
        case 3:  return euclide (pas, 7, 16);
        default: return hashPas > 0.4f;
    }
}

/** Lissage de la porte : un pole en double, vers la cible 0 ou 1. */
class EtatPorte
{
public:
    /** `ms` = temps de lissage (environ 63 %). */
    void regle (double sampleRate, float ms) noexcept
    {
        a = std::exp (-1.0 / (std::max (0.5, static_cast<double> (ms)) * 0.001 * sampleRate));
    }

    void reset() noexcept { g = 1.0; }

    /** Avance d'un echantillon vers `cible` (0 ou 1) et renvoie le gain resultant. */
    float suivant (float cible, float profondeur) noexcept
    {
        g = cible + (g - cible) * a;
        return 1.0f - profondeur * static_cast<float> (1.0 - g);
    }

private:
    double g = 1.0, a = 0.0;
};
} // namespace droz
