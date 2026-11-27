// Clipper suréchantillonne x4 : coupe les crêtes au plafond, du plus dur (0 %) au plus arrondi (100 %).
#pragma once

#include "Module.h"

namespace master
{
class Clipper final : public Module
{
public:
    struct Reglages
    {
        float entree = 0.0f;    // dB pousses dans le clipper
        float plafond = -0.3f;  // dBFS
        float douceur = 30.0f;  // %
        float sortie = 0.0f;    // dB apres
    } r;

    void prepare (double sampleRate) override
    {
        sr = sampleRate;
        os.prepare (4);
        entree.prepare (sr);
        plafond.prepare (sr);
        douceur.prepare (sr);
        sortie.prepare (sr);
        vide();
    }

    void vide() override { os.vide(); }
    int latence() const override { return os.latence(); }

    void traite (Morceau& m) override
    {
        entree.cible (r.entree);
        plafond.cible (r.plafond);
        douceur.cible (r.douceur / 100.0f);
        sortie.cible (r.sortie);
        float maxi = 0.0f;
        for (int i = 0; i < m.n; ++i)
        {
            const float gIn = dbVersGain (entree.suivant());
            const float c = dbVersGain (plafond.suivant());
            const float k = douceur.suivant();
            const float gOut = dbVersGain (sortie.suivant());
            const float t = c * (1.0f - 0.9f * k); // debut de l'arrondi
            for (int ch = 0; ch < m.canaux; ++ch)
            {
                float quatre[4];
                os.monte (m.ch[ch][i] * gIn, ch, quatre);
                for (float& v : quatre)
                {
                    const float a = std::abs (v);
                    if (a > t)
                    {
                        const float y = t + (c - t) * std::tanh ((a - t) / std::max (c - t, 1.0e-6f));
                        maxi = std::max (maxi, a / y);
                        v = std::copysign (y, v);
                    }
                }
                m.ch[ch][i] = os.descend (quatre, ch) * gOut;
            }
        }
        activite.store (gainVersDb (std::max (1.0f, maxi)));
    }

private:
    Surech os;
    Lisse entree, plafond, douceur, sortie;
};
} // namespace master
