#pragma once
#include "JuceIncludes.h"

namespace idg::dsp
{
constexpr float twoPi = juce::MathConstants<float>::twoPi;
constexpr float pi = juce::MathConstants<float>::pi;

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) { return 20.0f * std::log10 (juce::jmax (1.0e-9f, g)); }

inline float fastTanh (float x)
{
    x = juce::jlimit (-4.5f, 4.5f, x);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

inline float softSat (float x) { return fastTanh (x); }

inline bool finite (float x) { return std::isfinite (x); }

/** Générateur pseudo-aléatoire rapide, sans allocation (thread audio). */
struct Rng
{
    uint32_t s = 0x9E3779B9u;
    void seed (uint32_t v) { s = v ? v : 0x1234567u; }
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
    float bi() { return uni() * 2.0f - 1.0f; }
    int range (int n) { return n <= 1 ? 0 : (int) (next() % (uint32_t) n); }
};

inline float hash01 (uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return (float) (x >> 8) * (1.0f / 16777216.0f);
}

/** Bruit de valeur lissé (pour drift, random Perlin). */
inline float smoothNoise (double t, uint32_t seed)
{
    auto i = (int64_t) std::floor (t);
    float f = (float) (t - (double) i);
    float a = hash01 ((uint32_t) i * 747796405u + seed * 2891336453u);
    float b = hash01 ((uint32_t) (i + 1) * 747796405u + seed * 2891336453u);
    float u = f * f * (3.0f - 2.0f * f);
    return a + (b - a) * u; // 0..1
}

/** Buffer circulaire stéréo préalloué (puissance de 2). */
struct RingBuffer
{
    std::vector<float> d[2];
    int size = 0, mask = 0, w = 0;

    void allocate (int minSize)
    {
        size = juce::nextPowerOfTwo (juce::jmax (16, minSize));
        mask = size - 1;
        d[0].assign ((size_t) size, 0.0f);
        d[1].assign ((size_t) size, 0.0f);
        w = 0;
    }
    void clear() { for (auto& c : d) std::fill (c.begin(), c.end(), 0.0f); w = 0; }
    void push (float l, float r) { d[0][(size_t) w] = l; d[1][(size_t) w] = r; w = (w + 1) & mask; }
    float at (int ch, int absIndex) const { return d[ch][(size_t) (absIndex & mask)]; }

    /** Lecture fractionnaire (Hermite) à une position absolue (peut être négative, elle est repliée). */
    float read (int ch, double pos) const
    {
        auto i = (int) std::floor (pos);
        float f = (float) (pos - (double) i);
        const auto& c = d[ch];
        float xm1 = c[(size_t) ((i - 1) & mask)], x0 = c[(size_t) (i & mask)];
        float x1 = c[(size_t) ((i + 1) & mask)], x2 = c[(size_t) ((i + 2) & mask)];
        float c1 = 0.5f * (x1 - xm1);
        float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
    /** Lecture à un retard (en samples) par rapport au dernier échantillon écrit. */
    float delay (int ch, double samples) const { return read (ch, (double) (w - 1) - samples); }
};

/** Filtre à variable d'état (TPT / Cytomic), stéréo. */
struct SVF
{
    float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0;
    float ic1[2] {}, ic2[2] {};
    float lp = 0, bp = 0, hp = 0;

    void set (double sr, float fc, float q)
    {
        fc = juce::jlimit (5.0f, (float) (sr * 0.49), fc);
        g = (float) std::tan (juce::MathConstants<double>::pi * fc / sr);
        k = 1.0f / juce::jmax (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() { ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0; }
    inline void tick (int ch, float v0)
    {
        float v3 = v0 - ic2[ch];
        float v1 = a1 * ic1[ch] + a2 * v3;
        float v2 = ic2[ch] + a2 * ic1[ch] + a3 * v3;
        ic1[ch] = 2.0f * v1 - ic1[ch];
        ic2[ch] = 2.0f * v2 - ic2[ch];
        lp = v2; bp = v1; hp = v0 - k * v1 - v2;
    }
    inline float lowpass (int ch, float x) { tick (ch, x); return lp; }
    inline float highpass (int ch, float x) { tick (ch, x); return hp; }
    inline float bandpass (int ch, float x) { tick (ch, x); return bp; }
    inline float notch (int ch, float x) { tick (ch, x); return lp + hp; }
    void sanitize() { for (int c = 0; c < 2; ++c) if (! finite (ic1[c]) || ! finite (ic2[c])) { ic1[c] = ic2[c] = 0; } }
};

struct OnePole
{
    float z[2] {}, a = 0.0f;
    void setTime (double sr, float ms) { a = (float) std::exp (-1.0 / (juce::jmax (0.01, (double) ms) * 0.001 * sr)); }
    void setCutoff (double sr, float hz) { a = (float) std::exp (-juce::MathConstants<double>::twoPi * juce::jmax (1.0f, hz) / sr); }
    inline float lp (int ch, float x) { z[ch] = x + (z[ch] - x) * a; return z[ch]; }
    void reset() { z[0] = z[1] = 0; }
};

struct DCBlocker
{
    float x1[2] {}, y1[2] {}, r = 0.995f;
    void prepare (double sr) { r = (float) std::exp (-juce::MathConstants<double>::twoPi * 8.0 / sr); }
    inline float process (int ch, float x) { float y = x - x1[ch] + r * y1[ch]; x1[ch] = x; y1[ch] = y; return y; }
    void reset() { x1[0] = x1[1] = y1[0] = y1[1] = 0; }
};

/** Formes d'onde 0..1 -> -1..1 */
inline float shapeValue (int shape, float ph)
{
    ph -= std::floor (ph);
    switch (shape)
    {
        case 0: return std::sin (twoPi * ph);
        case 1: return 1.0f - 4.0f * std::abs (ph - 0.5f);
        case 2: return ph < 0.5f ? 1.0f : -1.0f;
        default: return 2.0f * ph - 1.0f;
    }
}

/** Retard simple (en samples) pour compensation de latence. */
struct LatencyDelay
{
    RingBuffer buf;
    int delay = 0;
    void prepare (int maxDelay) { buf.allocate (maxDelay + 4); }
    void reset() { buf.clear(); }
    inline void process (float& l, float& r)
    {
        buf.push (l, r);
        if (delay > 0) { l = buf.at (0, buf.w - 1 - delay); r = buf.at (1, buf.w - 1 - delay); }
    }
};
} // namespace idg::dsp
