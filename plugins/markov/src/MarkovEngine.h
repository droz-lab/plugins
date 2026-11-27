// Markov engine: pure C++, no JUCE, no allocation. Everything here is tested in tests/engine_tests.cpp.
//
// Design (see NOTES.md):
// - States are scale degrees. One transition matrix per track (weights, normalised at use).
// - All randomness is counter-based: random01 (seed, track, step, purpose). The same position
//   always gives the same draw, whatever the block size, loop or export. A track's melodic state
//   at step k is obtained by walking the chain from step 0 (TrackSim), so playback is a pure
//   function of (parameters, seed, position).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace markov
{
constexpr int maxDegrees = 12;
constexpr int maxSteps = 32;
constexpr int numTracks = 8;

//==============================================================================
// Random numbers
inline uint64_t mix64 (uint64_t x) noexcept
{
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

enum class Draw : uint64_t { play = 1, note = 2, velocity = 3 };

/** Uniform in [0, 1), fully determined by its arguments. */
inline double random01 (uint32_t seed, int track, int64_t step, Draw purpose) noexcept
{
    uint64_t h = mix64 (seed);
    h = mix64 (h ^ (static_cast<uint64_t> (track) + 0x100));
    h = mix64 (h ^ static_cast<uint64_t> (step));
    h = mix64 (h ^ static_cast<uint64_t> (purpose));
    return static_cast<double> (h >> 11) * (1.0 / 9007199254740992.0);
}

//==============================================================================
// Scales
enum ScaleKind
{
    major, minor, dorian, phrygian, lydian, mixolydian, locrian, pentatonic, chromatic, custom, numScaleKinds
};

struct Scale
{
    int size = 7;
    int root = 0; // pitch class of degree 0
    int offsets[maxDegrees] {};

    int pitchClass (int degree) const noexcept { return (root + offsets[degree % size]) % 12; }

    /** Nearest degree to a MIDI pitch (circular distance in semitones, ties go to the lower degree). */
    int degreeOf (int pitch) const noexcept
    {
        const int pc = ((pitch % 12) + 12) % 12;
        int best = 0, bestDist = 99;
        for (int d = 0; d < size; ++d)
        {
            const int diff = std::abs (pc - pitchClass (d));
            const int dist = std::min (diff, 12 - diff);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = d;
            }
        }
        return best;
    }
};

/** customMask: bit i set = pitch class (root + i) is in the scale. */
inline Scale makeScale (int kind, int root, uint16_t customMask) noexcept
{
    static const int tables[9][13] = {
        { 7, 0, 2, 4, 5, 7, 9, 11 },  // major
        { 7, 0, 2, 3, 5, 7, 8, 10 },  // natural minor
        { 7, 0, 2, 3, 5, 7, 9, 10 },  // dorian
        { 7, 0, 1, 3, 5, 7, 8, 10 },  // phrygian
        { 7, 0, 2, 4, 6, 7, 9, 11 },  // lydian
        { 7, 0, 2, 4, 5, 7, 9, 10 },  // mixolydian
        { 7, 0, 1, 3, 5, 6, 8, 10 },  // locrian
        { 5, 0, 2, 4, 7, 9 },         // major pentatonic
        { 12, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },
    };
    Scale s;
    s.root = ((root % 12) + 12) % 12;
    if (kind >= 0 && kind < custom)
    {
        s.size = tables[kind][0];
        for (int i = 0; i < s.size; ++i)
            s.offsets[i] = tables[kind][i + 1];
        return s;
    }
    s.size = 0;
    for (int i = 0; i < 12; ++i)
        if (customMask & (1u << i))
            s.offsets[s.size++] = i;
    if (s.size == 0) // nothing ticked: the root alone
        s.offsets[s.size++] = 0;
    return s;
}

//==============================================================================
// Per-track data edited in the interface (matrices, step grid). Fixed size: copied as a block.
struct TrackData
{
    bool edited = false;                      // false = use defaultWeight() for the current scale size
    float w[maxDegrees][maxDegrees] {};       // order 1: weight of i -> j (any scale, >= 0)
    bool has2[maxDegrees][maxDegrees] {};     // order 2 rows that exist (learned)
    float w2[maxDegrees][maxDegrees][maxDegrees] {}; // order 2: (previous, current) -> next
    float prob[maxSteps] {};                  // 0..1
    uint8_t velocity[maxSteps] {};            // 1..127
    uint8_t spread[maxSteps] {};              // 0..126
};

struct Pattern
{
    TrackData tracks[numTracks];
};

/** Default transition weight: short intervals (second, third) favoured, every jump possible. */
inline float defaultWeight (int size, int from, int to) noexcept
{
    static const float byDistance[] = { 0.35f, 1.0f, 0.75f, 0.35f, 0.25f, 0.18f, 0.12f };
    int d = std::abs (from - to);
    d = std::min (d, size - d);
    return byDistance[std::min (d, 6)];
}

inline float defaultProb (int track, int step) noexcept
{
    static const float accents[4][4] = { { 1.0f, 0.1f, 0.4f, 0.1f },
                                         { 1.0f, 0.3f, 0.7f, 0.3f },
                                         { 0.8f, 0.5f, 0.8f, 0.5f },
                                         { 0.5f, 0.25f, 0.5f, 0.25f } };
    return accents[track % 4][step % 4];
}

inline uint8_t defaultVelocity (int step) noexcept { return step % 4 == 0 ? 104 : 84; }
constexpr uint8_t defaultSpread = 12;

inline void resetMatrix (TrackData& t) noexcept
{
    t.edited = false;
    std::memset (t.w, 0, sizeof (t.w));
    std::memset (t.has2, 0, sizeof (t.has2));
    std::memset (t.w2, 0, sizeof (t.w2));
}

inline void resetSteps (TrackData& t, int track) noexcept
{
    for (int s = 0; s < maxSteps; ++s)
    {
        t.prob[s] = defaultProb (track, s);
        t.velocity[s] = defaultVelocity (s);
        t.spread[s] = defaultSpread;
    }
}

inline void resetPattern (Pattern& p) noexcept
{
    for (int t = 0; t < numTracks; ++t)
    {
        resetMatrix (p.tracks[t]);
        resetSteps (p.tracks[t], t);
    }
}

/** Weight actually used for i -> j with a scale of `size` degrees. */
inline float weight (const TrackData& t, int size, int from, int to) noexcept
{
    return t.edited ? t.w[from][to] : defaultWeight (size, from, to);
}

/** Before a first edit of a default matrix: freeze the default into the editable weights. */
inline void materialise (TrackData& t, int size) noexcept
{
    if (t.edited)
        return;
    for (int i = 0; i < maxDegrees; ++i)
        for (int j = 0; j < maxDegrees; ++j)
            t.w[i][j] = (i < size && j < size) ? defaultWeight (size, i, j) : 0.0f;
    t.edited = true;
}

//==============================================================================
// Sampling tables: rows normalised after the temperature, as cumulative sums.
struct ChainTables
{
    int size = 7;
    float cum1[maxDegrees][maxDegrees] {};
    bool row2[maxDegrees][maxDegrees] {};
    float cum2[maxDegrees][maxDegrees][maxDegrees] {};
};

/** Probabilities of one row: p_j proportional to w_j ^ (1 / temperature). Empty row = uniform. */
inline void normaliseRow (const float* w, int size, float temperature, float* probOut) noexcept
{
    const float exponent = 1.0f / std::clamp (temperature, 0.05f, 20.0f);
    float maxW = 0.0f;
    for (int j = 0; j < size; ++j)
        maxW = std::max (maxW, std::max (0.0f, w[j]));
    float sum = 0.0f;
    for (int j = 0; j < size; ++j)
    {
        // Scaled by the row maximum first: a large exponent cannot underflow the whole row.
        const float x = maxW > 0.0f ? std::max (0.0f, w[j]) / maxW : 0.0f;
        probOut[j] = x > 0.0f ? std::pow (x, exponent) : 0.0f;
        sum += probOut[j];
    }
    for (int j = 0; j < size; ++j)
        probOut[j] = sum > 0.0f ? probOut[j] / sum : 1.0f / static_cast<float> (size);
}

inline void toCumulative (const float* p, int size, float* cum) noexcept
{
    float acc = 0.0f;
    for (int j = 0; j < size; ++j)
        cum[j] = (acc += p[j]);
    cum[size - 1] = 1.0f + 1.0e-6f; // never fall off the end through rounding
}

inline void buildTables (const TrackData& t, int size, float temperature, ChainTables& out) noexcept
{
    size = std::clamp (size, 1, maxDegrees);
    out.size = size;
    float row[maxDegrees], p[maxDegrees];
    for (int i = 0; i < size; ++i)
    {
        for (int j = 0; j < size; ++j)
            row[j] = weight (t, size, i, j);
        normaliseRow (row, size, temperature, p);
        toCumulative (p, size, out.cum1[i]);
    }
    for (int a = 0; a < size; ++a)
        for (int b = 0; b < size; ++b)
        {
            out.row2[a][b] = t.has2[a][b];
            if (! t.has2[a][b])
                continue;
            normaliseRow (t.w2[a][b], size, temperature, p);
            toCumulative (p, size, out.cum2[a][b]);
        }
}

inline int pick (const float* cum, int size, double u) noexcept
{
    for (int j = 0; j < size; ++j)
        if (u < static_cast<double> (cum[j]))
            return j;
    return size - 1;
}

//==============================================================================
// Steps
inline bool stepPlays (float prob, double u) noexcept
{
    if (prob >= 1.0f)
        return true;
    if (prob <= 0.0f)
        return false;
    return u < static_cast<double> (prob);
}

/** Uniform integer in [centre - spread, centre + spread], kept in 1..127. */
inline int drawVelocity (int centre, int spread, double u) noexcept
{
    spread = std::max (0, spread);
    const int v = centre - spread + std::min (2 * spread, static_cast<int> (u * (2 * spread + 1)));
    return std::clamp (v, 1, 127);
}

/** Pitch of `degree` inside [lo, hi] nearest to `previous` (ties: lower). -1 if none in range. */
inline int nearestPitch (const Scale& scale, int degree, int previous, int lo, int hi) noexcept
{
    const int pc = scale.pitchClass (degree);
    int best = -1, bestDist = 1000;
    for (int p = lo; p <= hi; ++p)
        if (p % 12 == pc && std::abs (p - previous) < bestDist)
        {
            bestDist = std::abs (p - previous);
            best = p;
        }
    return best;
}

struct TrackSettings
{
    int lo = 48, hi = 72;   // tessitura (MIDI notes)
    int numSteps = 16;      // 1..32
    bool chainRhythm = false;
    float afterHit = 0.6f;  // chain rhythm: factor when the previous step played
    float afterRest = 0.9f; // chain rhythm: factor when the previous step was silent
    int order = 1;          // 1 or 2
};

struct StepEvent
{
    bool play = false;
    int pitch = 0, velocity = 0, degree = 0;
};

/** Walks one track's chains (rhythm and melody), one step at a time. */
class TrackSim
{
public:
    void reset() noexcept
    {
        started = false;
        degree = 0;
        previousDegree = -1;
        previousPitch = -1;
        lastPlayed = true;
    }

    StepEvent step (int64_t k, int track, uint32_t seed, const TrackData& data, const ChainTables& tables,
                    const Scale& scale, const TrackSettings& s) noexcept
    {
        StepEvent ev;
        const int numSteps = std::clamp (s.numSteps, 1, maxSteps);
        const int index = static_cast<int> (((k % numSteps) + numSteps) % numSteps);
        float p = data.prob[index];
        if (s.chainRhythm)
            p *= lastPlayed ? s.afterHit : s.afterRest;
        ev.play = stepPlays (p, random01 (seed, track, k, Draw::play));
        lastPlayed = ev.play;
        if (! ev.play)
            return ev;

        const int lo = std::min (s.lo, s.hi), hi = std::max (s.lo, s.hi);
        const int size = tables.size;
        int next = 0;
        if (started)
        {
            const int cur = degree % size;
            const double u = random01 (seed, track, k, Draw::note);
            if (s.order == 2 && previousDegree >= 0 && previousDegree < size && tables.row2[previousDegree][cur])
                next = pick (tables.cum2[previousDegree][cur], size, u);
            else
                next = pick (tables.cum1[cur], size, u);
        }
        const int from = previousPitch >= 0 ? previousPitch : (lo + hi) / 2;
        const int pitch = nearestPitch (scale, next, from, lo, hi);
        if (pitch < 0) // this degree does not exist in the tessitura: rest, state unchanged
        {
            ev.play = false;
            return ev;
        }
        previousDegree = started ? degree % size : -1;
        degree = next;
        previousPitch = pitch;
        started = true;

        ev.pitch = pitch;
        ev.degree = next;
        ev.velocity = drawVelocity (data.velocity[index], data.spread[index], random01 (seed, track, k, Draw::velocity));
        return ev;
    }

    int currentDegree() const noexcept { return degree; }

private:
    bool started = false, lastPlayed = true;
    int degree = 0, previousDegree = -1, previousPitch = -1;
};

//==============================================================================
// Learning: counts degree transitions of incoming notes.
class Learner
{
public:
    void reset() noexcept
    {
        std::memset (c1, 0, sizeof (c1));
        std::memset (c2, 0, sizeof (c2));
        prev = prev2 = -1;
        total = 0;
    }

    void addDegree (int d) noexcept
    {
        if (d < 0 || d >= maxDegrees)
            return;
        if (prev >= 0)
        {
            ++c1[prev][d];
            if (prev2 >= 0)
                ++c2[prev2][prev][d];
        }
        prev2 = prev;
        prev = d;
        ++total;
    }

    /** Writes normalised probabilities into `t`. Rows never seen keep their current values. */
    void apply (TrackData& t, int size) const noexcept
    {
        size = std::clamp (size, 1, maxDegrees);
        materialise (t, size);
        for (int i = 0; i < size; ++i)
        {
            uint32_t sum = 0;
            for (int j = 0; j < size; ++j)
                sum += c1[i][j];
            if (sum > 0)
                for (int j = 0; j < size; ++j)
                    t.w[i][j] = static_cast<float> (c1[i][j]) / static_cast<float> (sum);
        }
        for (int a = 0; a < size; ++a)
            for (int b = 0; b < size; ++b)
            {
                uint32_t sum = 0;
                for (int j = 0; j < size; ++j)
                    sum += c2[a][b][j];
                t.has2[a][b] = sum > 0;
                for (int j = 0; j < size; ++j)
                    t.w2[a][b][j] = sum > 0 ? static_cast<float> (c2[a][b][j]) / static_cast<float> (sum) : 0.0f;
            }
    }

    uint32_t c1[maxDegrees][maxDegrees] {};
    uint32_t c2[maxDegrees][maxDegrees][maxDegrees] {};
    int prev = -1, prev2 = -1;
    uint32_t total = 0;
};
} // namespace markov
