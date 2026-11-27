// Compresseur multibande, 4 bandes Linkwitz-Riley a phases compensees (somme plate quand rien ne compresse).
#pragma once

#include "Compresseur.h"

namespace master
{
class Multibande final : public Module
{
public:
    static constexpr int nbBandes = 4;

    struct Bande
    {
        float seuil = -18.0f, ratio = 2.0f, attaque = 10.0f, relache = 150.0f, gain = 0.0f;
    };
    struct Reglages
    {
        std::array<float, nbBandes - 1> coupures { 120.0f, 1000.0f, 6000.0f };
        std::array<Bande, nbBandes> bandes;
        int style = CoeurCompression::propre;
    } r;

    void prepare (double sampleRate) override
    {
        sr = sampleRate;
        rep.prepare (sr, nbBandes);
        for (auto& b : bandes)
        {
            for (auto& c : b.coeurs)
                c.prepare (sr);
            b.gain.prepare (sr);
            b.seuil.prepare (sr);
        }
        vide();
    }

    void vide() override
    {
        rep.vide();
        for (auto& b : bandes)
            for (auto& c : b.coeurs)
                c.vide();
    }

    void traite (Morceau& m) override
    {
        rep.cibles (r.coupures.data());
        for (size_t k = 0; k < nbBandes; ++k)
        {
            bandes[k].gain.cible (r.bandes[k].gain);
            bandes[k].seuil.cible (r.bandes[k].seuil);
        }

        std::array<float, nbBandes> maxi {};
        float parts[maxCanaux][nbBandes];
        for (int i = 0; i < m.n; ++i)
        {
            rep.avance();
            for (int c = 0; c < m.canaux; ++c)
                rep.traite (m.ch[c][i], c, parts[c]);

            for (size_t k = 0; k < nbBandes; ++k)
            {
                auto& b = bandes[k];
                const float gb = dbVersGain (b.gain.suivant());
                const float seuil = b.seuil.suivant();
                if (i == 0 || differe (seuil, b.dernierSeuil))
                {
                    CoeurCompression::Reglages rg;
                    rg.style = r.style;
                    rg.seuil = seuil;
                    rg.ratio = r.bandes[k].ratio;
                    rg.attaque = r.bandes[k].attaque;
                    rg.relache = r.bandes[k].relache;
                    rg.genou = 6.0f;
                    for (auto& c : b.coeurs)
                        c.regle (rg);
                    b.dernierSeuil = seuil;
                }
                for (int g = 0; g < m.groupes(); ++g)
                {
                    const int c0 = m.premier (g), nc = m.tailleGroupe (g);
                    float niveau = 0.0f;
                    for (int c = c0; c < c0 + nc; ++c)
                        niveau = std::max (niveau, std::abs (parts[c][k]));
                    const float red = b.coeurs[static_cast<size_t> (g)].traite (niveau);
                    maxi[k] = std::max (maxi[k], red);
                    const float gr = dbVersGain (-red) * gb;
                    for (int c = c0; c < c0 + nc; ++c)
                        parts[c][k] *= gr;
                }
            }
            for (int c = 0; c < m.canaux; ++c)
            {
                float s = 0.0f;
                for (size_t k = 0; k < nbBandes; ++k)
                    s += parts[c][k];
                m.ch[c][i] = s;
            }
        }
        float tout = 0.0f;
        for (size_t k = 0; k < nbBandes; ++k)
        {
            reductionBande[k].store (maxi[k]);
            tout = std::max (tout, maxi[k]);
        }
        activite.store (tout);
    }

    std::array<std::atomic<float>, nbBandes> reductionBande {};

private:
    struct Etat
    {
        std::array<CoeurCompression, maxCanaux> coeurs;
        Lisse gain, seuil;
        float dernierSeuil = 1.0e9f;
    };
    Repartiteur rep;
    std::array<Etat, nbBandes> bandes;
};
} // namespace master
