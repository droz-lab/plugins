// Tests of the pure Markov engine (no JUCE). Run by ctest as markov_moteur.
#include "MarkovEngine.h"

#include <cstdio>
#include <vector>

using namespace markov;

namespace
{
int failures = 0;

void check (bool cond, const char* name, const char* detail = "")
{
    std::printf ("  %s  %s %s\n", cond ? "OK   " : "ECHEC", name, detail);
    if (! cond)
        ++failures;
}

void testTransitionStatistics()
{
    // 10 000 draws per row: each transition frequency matches the matrix within 2 %.
    for (int kind : { (int) major, (int) pentatonic, (int) chromatic })
    {
        const Scale scale = makeScale (kind, 0, 0);
        TrackData data {};
        resetMatrix (data);
        ChainTables tables;
        buildTables (data, scale.size, 1.0f, tables);

        double worst = 0.0;
        for (int i = 0; i < scale.size; ++i)
        {
            float row[maxDegrees], p[maxDegrees];
            for (int j = 0; j < scale.size; ++j)
                row[j] = weight (data, scale.size, i, j);
            normaliseRow (row, scale.size, 1.0f, p);

            int counts[maxDegrees] {};
            const int n = 10000;
            for (int k = 0; k < n; ++k)
                ++counts[pick (tables.cum1[i], scale.size, random01 (42, i, k, Draw::note))];
            for (int j = 0; j < scale.size; ++j)
                worst = std::max (worst, std::abs (counts[j] / (double) n - p[j]));
        }
        char detail[96];
        std::snprintf (detail, sizeof detail, "(gamme %d degres, plus grand ecart %.2f %%)", scale.size, 100.0 * worst);
        check (worst <= 0.02, "statistique des transitions", detail);
    }

    // Every default transition is possible (no impossible jump).
    bool allPositive = true;
    for (int size : { 5, 7, 12 })
        for (int i = 0; i < size; ++i)
            for (int j = 0; j < size; ++j)
                allPositive = allPositive && defaultWeight (size, i, j) > 0.0f;
    check (allPositive, "matrice par defaut sans saut impossible");
    check (defaultWeight (7, 0, 1) > defaultWeight (7, 0, 3) && defaultWeight (7, 0, 2) > defaultWeight (7, 0, 4),
           "matrice par defaut favorise seconde et tierce");
}

void testTemperature()
{
    const float w[3] = { 0.6f, 0.3f, 0.1f };
    float cold[3], warm[3], neutral[3];
    normaliseRow (w, 3, 0.2f, cold);
    normaliseRow (w, 3, 1.0f, neutral);
    normaliseRow (w, 3, 5.0f, warm);
    check (std::abs (neutral[0] - 0.6f) < 1e-5f, "temperature 1 = matrice telle quelle");
    check (cold[0] > 0.95f, "temperature basse : previsible");
    check (warm[0] < 0.42f && warm[2] > 0.25f, "temperature haute : plus aleatoire");
    const float zero[3] = { 0.0f, 0.0f, 0.0f };
    float uni[3];
    normaliseRow (zero, 3, 1.0f, uni);
    check (std::abs (uni[1] - 1.0f / 3.0f) < 1e-5f, "ligne vide : uniforme (jamais bloque)");
}

void testSteps()
{
    const int n = 100000;
    int always = 0, never = 0, half = 0;
    for (int k = 0; k < n; ++k)
    {
        always += stepPlays (1.0f, random01 (7, 0, k, Draw::play));
        never += stepPlays (0.0f, random01 (7, 0, k, Draw::play));
        half += stepPlays (0.5f, random01 (7, 0, k, Draw::play));
    }
    check (always == n, "pas a 100 % joue toujours");
    check (never == 0, "pas a 0 % ne joue jamais");
    char detail[64];
    std::snprintf (detail, sizeof detail, "(%.2f %% sur %d tirages)", 100.0 * half / n, n);
    check (std::abs (half / (double) n - 0.5) < 0.01, "pas a 50 % joue environ une fois sur deux", detail);
}

void testVelocity()
{
    bool inside = true, constant = true, reachesEnds = false, reachesLow = false;
    for (int k = 0; k < 20000; ++k)
    {
        const double u = random01 (3, 1, k, Draw::velocity);
        const int v = drawVelocity (90, 15, u);
        inside = inside && v >= 75 && v <= 105;
        reachesEnds = reachesEnds || v == 105;
        reachesLow = reachesLow || v == 75;
        constant = constant && drawVelocity (64, 0, u) == 64;
        const int edge = drawVelocity (120, 30, u);
        inside = inside && edge >= 90 && edge <= 127;
    }
    check (inside, "velocite dans l'ecart (et dans 1..127)");
    check (reachesEnds && reachesLow, "velocite atteint les deux bornes de l'ecart");
    check (constant, "ecart 0 : toujours la meme velocite");
    check (drawVelocity (90, 15, 0.999999) == 105 && drawVelocity (90, 15, 0.0) == 75, "bornes exactes");
}

std::vector<StepEvent> runTrack (uint32_t seed, int track, int steps, const TrackSettings& s, const Scale& scale, float temperature)
{
    TrackData data {};
    resetMatrix (data);
    resetSteps (data, track);
    ChainTables tables;
    buildTables (data, scale.size, temperature, tables);
    TrackSim sim;
    sim.reset();
    std::vector<StepEvent> out;
    for (int k = 0; k < steps; ++k)
        out.push_back (sim.step (k, track, seed, data, tables, scale, s));
    return out;
}

bool same (const std::vector<StepEvent>& a, const std::vector<StepEvent>& b)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].play != b[i].play || (a[i].play && (a[i].pitch != b[i].pitch || a[i].velocity != b[i].velocity)))
            return false;
    return true;
}

void testReproducibility()
{
    const Scale scale = makeScale (dorian, 2, 0);
    TrackSettings s;
    s.lo = 50;
    s.hi = 74;
    const auto a = runTrack (1234, 2, 512, s, scale, 1.0f);
    const auto b = runTrack (1234, 2, 512, s, scale, 1.0f);
    const auto c = runTrack (1235, 2, 512, s, scale, 1.0f);
    check (same (a, b), "meme graine, memes reglages : meme sequence");
    check (! same (a, c), "autre graine : autre sequence");

    bool inRangeAndScale = true;
    int played = 0;
    for (const auto& e : a)
        if (e.play)
        {
            ++played;
            inRangeAndScale = inRangeAndScale && e.pitch >= 50 && e.pitch <= 74 && scale.pitchClass (scale.degreeOf (e.pitch)) == e.pitch % 12;
        }
    check (played > 50 && inRangeAndScale, "notes dans la tessiture et dans la gamme");

    // Random access: walking from 0 to k then continuing = the uninterrupted run (loops, jumps).
    TrackData data {};
    resetMatrix (data);
    resetSteps (data, 2);
    ChainTables tables;
    buildTables (data, scale.size, 1.0f, tables);
    TrackSim sim;
    sim.reset();
    for (int k = 0; k < 300; ++k)
        sim.step (k, 2, 1234, data, tables, scale, s);
    bool resumed = true;
    for (int k = 300; k < 512; ++k)
    {
        const auto e = sim.step (k, 2, 1234, data, tables, scale, s);
        resumed = resumed && e.play == a[(size_t) k].play && (! e.play || e.pitch == a[(size_t) k].pitch);
    }
    check (resumed, "reprise a une position = meme suite (boucles, sauts)");
}

void testChainRhythm()
{
    // afterHit 0: never two notes in a row. afterRest 1 with prob 1: always a note after a rest.
    const Scale scale = makeScale (major, 0, 0);
    TrackData data {};
    resetMatrix (data);
    for (auto& p : data.prob)
        p = 1.0f;
    for (auto& v : data.velocity)
        v = 100;
    ChainTables tables;
    buildTables (data, scale.size, 1.0f, tables);
    TrackSettings s;
    s.chainRhythm = true;
    s.afterHit = 0.0f;
    s.afterRest = 1.0f;
    TrackSim sim;
    sim.reset();
    bool alternates = true;
    bool last = false;
    for (int k = 0; k < 64; ++k)
    {
        const auto e = sim.step (k, 0, 9, data, tables, scale, s);
        if (k > 0)
            alternates = alternates && e.play != last;
        last = e.play;
    }
    check (alternates, "chaine rythmique : depend du pas precedent");
}

void testLearning()
{
    // Sequence 0 1 2 0 1 2 0 2 : 0->1 twice, 0->2 once, 1->2 twice, 2->0 twice.
    Learner learner;
    learner.reset();
    for (int d : { 0, 1, 2, 0, 1, 2, 0, 2 })
        learner.addDegree (d);
    TrackData t {};
    resetMatrix (t);
    learner.apply (t, 7);
    const bool row0 = std::abs (t.w[0][1] - 2.0f / 3.0f) < 1e-6f && std::abs (t.w[0][2] - 1.0f / 3.0f) < 1e-6f && t.w[0][0] == 0.0f;
    const bool row1 = std::abs (t.w[1][2] - 1.0f) < 1e-6f;
    const bool row2 = std::abs (t.w[2][0] - 1.0f) < 1e-6f;
    check (row0 && row1 && row2, "apprentissage : probabilites attendues");
    check (std::abs (t.w[4][5] - defaultWeight (7, 4, 5)) < 1e-6f, "apprentissage : lignes jamais vues gardent leur valeur");
    // Order 2: (0,1) -> 2 twice ; (1,2) -> 0 twice ; (2,0) -> 1 once, 2 once.
    check (t.has2[0][1] && std::abs (t.w2[0][1][2] - 1.0f) < 1e-6f && t.has2[2][0] && std::abs (t.w2[2][0][1] - 0.5f) < 1e-6f
               && ! t.has2[3][4],
           "apprentissage ordre 2");

    // Mapping of incoming pitches to degrees (C major): C#4 -> nearest degree.
    const Scale cMajor = makeScale (major, 0, 0);
    check (cMajor.degreeOf (60) == 0 && cMajor.degreeOf (67) == 4 && cMajor.degreeOf (71) == 6 && cMajor.degreeOf (61) == 0,
           "note entrante vers degre de la gamme");

    // Order 2 sampling follows the learned row.
    ChainTables tables;
    buildTables (t, 7, 1.0f, tables);
    int hits = 0;
    for (int k = 0; k < 1000; ++k)
        hits += pick (tables.cum2[0][1], 7, random01 (5, 0, k, Draw::note)) == 2;
    check (hits == 1000, "ordre 2 : la note suivante depend des deux precedentes");
}

void testScales()
{
    check (makeScale (pentatonic, 0, 0).size == 5 && makeScale (chromatic, 0, 0).size == 12 && makeScale (locrian, 0, 0).size == 7,
           "tailles des gammes");
    const Scale c = makeScale (custom, 9, (1u << 0) | (1u << 3) | (1u << 7));
    check (c.size == 3 && c.pitchClass (1) == 0 && c.pitchClass (2) == 4, "gamme personnalisee");
    check (makeScale (custom, 0, 0).size == 1, "gamme personnalisee vide : tonique seule");
    check (nearestPitch (makeScale (major, 0, 0), 4, 60, 48, 72) == 55 && nearestPitch (makeScale (major, 0, 0), 4, 60, 61, 62) == -1,
           "hauteur la plus proche dans la tessiture");
}
} // namespace

int main()
{
    std::printf ("Markov : tests du moteur\n");
    testTransitionStatistics();
    testTemperature();
    testSteps();
    testVelocity();
    testReproducibility();
    testChainRhythm();
    testLearning();
    testScales();
    std::printf ("%d echec(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
