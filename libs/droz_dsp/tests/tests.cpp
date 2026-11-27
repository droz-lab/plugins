// Tests de droz_dsp : aucun framework, un executable qui renvoie 0 si tout passe.
#include <droz/dsp.h>
#include <droz/filtres.h>
#include <droz/porte.h>
#include <droz/rng.h>
#include <droz/tampon.h>

#include <cstdio>

static int echecs = 0;

#define VERIFIE(cond)                                                        \
    do {                                                                     \
        if (! (cond)) {                                                      \
            std::printf ("ECHEC ligne %d : %s\n", __LINE__, #cond);          \
            ++echecs;                                                        \
        }                                                                    \
    } while (false)

static bool proche (float a, float b, float tol = 1.0e-4f) { return std::abs (a - b) <= tol; }

int main()
{
    // dB
    VERIFIE (proche (droz::dbToGain (0.0f), 1.0f));
    VERIFIE (proche (droz::dbToGain (-6.0206f), 0.5f));
    VERIFIE (droz::dbToGain (-100.0f) == 0.0f);
    VERIFIE (proche (droz::gainToDb (0.5f), -6.0206f, 1.0e-3f));
    VERIFIE (droz::gainToDb (0.0f) == -100.0f);

    // Smoother : arrive a la cible, sans depassement, repos exact
    {
        droz::Smoother s;
        s.reset (48000.0, 20.0f);
        s.snap (0.0f);
        s.setTarget (1.0f);
        float prec = 0.0f;
        bool monotone = true;
        for (int i = 0; i < 48000; ++i)
        {
            const float v = s.next();
            monotone = monotone && v >= prec && v <= 1.0f;
            prec = v;
        }
        VERIFIE (monotone);
        VERIFIE (s.current() == 1.0f);

        // 63 % du chemin apres le temps demande (20 ms = 960 echantillons)
        s.snap (0.0f);
        s.setTarget (1.0f);
        for (int i = 0; i < 960; ++i)
            s.next();
        VERIFIE (proche (s.current(), 0.632f, 0.01f));

        // temps nul = saut immediat
        s.reset (48000.0, 0.0f);
        s.setTarget (0.25f);
        VERIFIE (s.next() == 0.25f);
    }

    // Suiveur d'enveloppe : monte vers la crete, retombe a zero exact
    {
        droz::EnvelopeFollower e;
        e.reset (48000.0, 1.0f, 50.0f);
        float v = 0.0f;
        for (int i = 0; i < 4800; ++i)
            v = e.process (i % 2 == 0 ? 0.8f : -0.8f);
        VERIFIE (proche (v, 0.8f, 0.01f));
        for (int i = 0; i < 480000; ++i)
            v = e.process (0.0f);
        VERIFIE (v == 0.0f);
    }

    // Motifs euclidiens : nombre de pas actifs sur un cycle
    {
        int c3 = 0, c5 = 0, c7 = 0, cToujours = 0;
        for (int i = 0; i < 8; ++i) { c3 += droz::pasActif (1, i, 0.0f); c5 += droz::pasActif (2, i, 0.0f); }
        for (int i = 0; i < 16; ++i) c7 += droz::pasActif (3, i, 0.0f);
        for (int i = 0; i < 8; ++i) cToujours += droz::pasActif (0, i, 0.0f);
        VERIFIE (c3 == 3);
        VERIFIE (c5 == 5);
        VERIFIE (c7 == 7);
        VERIFIE (cToujours == 8);
    }

    // Porte : la cible 0 coupe a (1 - profondeur), et le gain revient bien a 1 (pas de figeage)
    {
        droz::EtatPorte p;
        p.regle (48000.0, 5.0f);
        p.reset();
        float gain = 1.0f;
        for (int i = 0; i < 48000; ++i) gain = p.suivant (0.0f, 1.0f);
        VERIFIE (gain < 1.0e-4f);
        for (int i = 0; i < 48000; ++i) gain = p.suivant (1.0f, 1.0f);
        VERIFIE (gain > 0.999999f);
    }

    // Generateur : meme graine, meme suite ; valeurs dans les bornes
    {
        droz::Rng a, b;
        a.seed (42);
        b.seed (42);
        bool same = true, bornes = true;
        for (int i = 0; i < 1000; ++i)
            same = same && a.next() == b.next();
        for (int i = 0; i < 1000; ++i)
        {
            const float u = a.uni();
            bornes = bornes && u >= 0.0f && u < 1.0f;
        }
        VERIFIE (same);
        VERIFIE (bornes);
        VERIFIE (droz::hash01 (7u) == droz::hash01 (7u));
    }

    // Formes d'onde
    VERIFIE (proche (droz::shapeValue (0, 0.25f), 1.0f));
    VERIFIE (proche (droz::shapeValue (1, 0.0f), -1.0f));
    VERIFIE (proche (droz::shapeValue (2, 0.25f), 1.0f));
    VERIFIE (proche (droz::shapeValue (2, 0.75f), -1.0f));
    VERIFIE (proche (droz::shapeValue (3, 0.0f), -1.0f));

    // Filtre SVF : passe-bas laisse passer le continu une fois etabli
    {
        droz::FiltreSVF f;
        f.regle (48000.0, 1000.0f, 0.707f);
        f.reset();
        float y = 0.0f;
        for (int i = 0; i < 4800; ++i) y = f.passeBas (0, 1.0f);
        VERIFIE (proche (y, 1.0f, 0.01f));
        f.assainir();
    }

    // Un pole : arrive a la cible
    {
        droz::UnPole u;
        u.regleTemps (48000.0, 5.0f);
        u.reset();
        float y = 0.0f;
        for (int i = 0; i < 48000; ++i) y = u.passeBas (0, 1.0f);
        VERIFIE (proche (y, 1.0f, 1.0e-4f));
    }

    // Bloqueur DC : la continue disparait
    {
        droz::BloqueurDC dc;
        dc.prepare (48000.0);
        dc.reset();
        float y = 1.0f;
        for (int i = 0; i < 48000; ++i) y = dc.traite (0, 1.0f);
        VERIFIE (std::abs (y) < 1.0e-3f);
    }

    // Tampon circulaire : retard entier et fractionnaire (rampe = exacte en Hermite)
    {
        droz::TamponCirculaire t;
        t.alloue (128);
        for (int i = 0; i < 100; ++i) t.ecrit (static_cast<float> (i), 0.0f);
        VERIFIE (proche (t.retard (0, 10.0), 89.0f));
        VERIFIE (proche (t.retard (0, 10.5), 88.5f, 1.0e-3f));
        VERIFIE (t.tailleTampon() == 128);
    }

    if (echecs == 0)
        std::printf ("droz_dsp : tous les tests passent\n");
    return echecs == 0 ? 0 : 1;
}
