// Tests de droz_dsp : aucun framework, un executable qui renvoie 0 si tout passe.
#include <droz/dsp.h>

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

    if (echecs == 0)
        std::printf ("droz_dsp : tous les tests passent\n");
    return echecs == 0 ? 0 : 1;
}
