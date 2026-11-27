// Tampon circulaire stereo, lecture fractionnaire (Hermite). Migre depuis IDGems (Core/Dsp.h).
// L'allocation se fait hors du fil audio (prepare). Tout ce qui est ici doit avoir son test.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace droz
{
/** Tampon stereo de taille puissance de 2. */
class TamponCirculaire
{
public:
    /** A appeler hors du fil audio. */
    void alloue (int tailleMin)
    {
        int t = 16;
        while (t < tailleMin)
            t <<= 1;
        taille = t;
        masque = t - 1;
        for (auto& c : d)
            c.assign (static_cast<std::size_t> (t), 0.0f);
        w = 0;
    }

    void vide() noexcept
    {
        for (auto& c : d)
            std::fill (c.begin(), c.end(), 0.0f);
        w = 0;
    }

    void ecrit (float gauche, float droite) noexcept
    {
        d[0][static_cast<std::size_t> (w)] = gauche;
        d[1][static_cast<std::size_t> (w)] = droite;
        w = (w + 1) & masque;
    }

    /** Lecture fractionnaire a une position absolue (repliee si negative). Interpolation Hermite. */
    float lit (int canal, double position) const noexcept
    {
        const auto i = static_cast<std::int64_t> (std::floor (position));
        const float f = static_cast<float> (position - static_cast<double> (i));
        const auto& c = d[canal];
        const float xm1 = c[static_cast<std::size_t> ((i - 1) & masque)];
        const float x0 = c[static_cast<std::size_t> (i & masque)];
        const float x1 = c[static_cast<std::size_t> ((i + 1) & masque)];
        const float x2 = c[static_cast<std::size_t> ((i + 2) & masque)];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    /** Lecture a un retard en echantillons, par rapport au dernier ecrit. */
    float retard (int canal, double echantillons) const noexcept
    {
        return lit (canal, static_cast<double> (w - 1) - echantillons);
    }

    int tailleTampon() const noexcept { return taille; }

private:
    std::vector<float> d[2];
    int taille = 0, masque = 0, w = 0;
};
} // namespace droz
