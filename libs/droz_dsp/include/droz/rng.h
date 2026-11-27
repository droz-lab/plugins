// Aleatoire et formes d'onde, sans allocation. Migre depuis IDGems (Core/Dsp.h).
// Tout ce qui est ici doit avoir son test dans libs/droz_dsp/tests/tests.cpp.
#pragma once

#include <cmath>
#include <cstdint>

namespace droz
{
/** Generateur xorshift rapide. Meme graine = meme suite : le rendu reste deterministe. */
struct Rng
{
    std::uint32_t s = 0x9E3779B9u;

    void seed (std::uint32_t v) noexcept { s = v ? v : 0x1234567u; }
    std::uint32_t next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    /** Dans [0, 1). */
    float uni() noexcept { return static_cast<float> (next() >> 8) * (1.0f / 16777216.0f); }
    /** Dans [-1, 1). */
    float bi() noexcept { return uni() * 2.0f - 1.0f; }
    /** Entier dans [0, n). */
    int range (int n) noexcept { return n <= 1 ? 0 : static_cast<int> (next() % static_cast<std::uint32_t> (n)); }
};

/** Hash entier vers [0, 1). Sans etat : meme entree, meme sortie. */
inline float hash01 (std::uint32_t x) noexcept
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return static_cast<float> (x >> 8) * (1.0f / 16777216.0f);
}

/** Bruit de valeur lisse, dans [0, 1]. Pour les derives et le hasard lent. */
inline float smoothNoise (double t, std::uint32_t graine) noexcept
{
    const auto i = static_cast<std::int64_t> (std::floor (t));
    const float f = static_cast<float> (t - static_cast<double> (i));
    const float a = hash01 (static_cast<std::uint32_t> (i) * 747796405u + graine * 2891336453u);
    const float b = hash01 (static_cast<std::uint32_t> (i + 1) * 747796405u + graine * 2891336453u);
    const float u = f * f * (3.0f - 2.0f * f);
    return a + (b - a) * u;
}

/** Forme d'onde : phase 0..1 (ramenee dans 0..1), sortie -1..1.
    0 = sinus, 1 = triangle, 2 = carre, 3 = dent de scie. */
inline float shapeValue (int forme, float phase) noexcept
{
    phase -= std::floor (phase);
    switch (forme)
    {
        case 0:  return std::sin (2.0f * 3.14159265358979f * phase);
        case 1:  return 1.0f - 4.0f * std::abs (phase - 0.5f);
        case 2:  return phase < 0.5f ? 1.0f : -1.0f;
        default: return 2.0f * phase - 1.0f;
    }
}
} // namespace droz
