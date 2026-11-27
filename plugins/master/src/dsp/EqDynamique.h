// EQ dynamique, 3 bandes. Chaque bande ecoute sa propre zone (filtre passe-bande ou plateau),
// et son gain bouge selon le niveau qui depasse le seuil : plage negative = elle creuse quand ca deborde,
// plage positive = elle remonte quand ca depasse.
#pragma once

#include "Module.h"

namespace master
{
class EqDynamique final : public Module
{
public:
    static constexpr int nbBandes = 3;
    enum Forme { cloche, plateauBas, plateauHaut };

    struct Bande
    {
        bool actif = false;
        int forme = cloche;
        float freq = 1000.0f, q = 1.0f, seuil = -24.0f, plage = -6.0f, attaque = 5.0f, relache = 120.0f;
    };
    struct Reglages
    {
        std::array<Bande, nbBandes> bandes;
    } r;

    void prepare (double sampleRate) override
    {
        sr = sampleRate;
        for (auto& b : etat)
        {
            b.f.prepare (sr);
            b.q.prepare (sr);
            b.seuil.prepare (sr);
            b.plage.prepare (sr);
            b.on.prepare (sr);
        }
        vide();
    }

    void vide() override
    {
        for (auto& b : etat)
        {
            b.detect.vide();
            for (auto& f : b.filtres)
                f.vide();
            b.env.fill (-120.0f);
            b.gain.fill (0.0f);
            b.dernierGain.fill (1.0e9f);
        }
    }

    void traite (Morceau& m) override
    {
        float maxi = 0.0f;
        for (size_t k = 0; k < nbBandes; ++k)
        {
            auto& b = etat[k];
            const auto& rb = r.bandes[k];
            b.f.cible (rb.freq);
            b.q.cible (rb.q);
            b.seuil.cible (rb.seuil);
            b.plage.cible (rb.plage);
            b.on.cible (rb.actif ? 1.0f : 0.0f);
            b.aAtt = coefTemps (sr, rb.attaque);
            b.aRel = coefTemps (sr, rb.relache);
            if (rb.forme != b.forme)
            {
                b.forme = rb.forme;
                b.dernierF = -1.0f;
            }
        }

        for (int i = 0; i < m.n; ++i)
            for (auto& b : etat)
            {
                const float f = b.f.suivant(), q = b.q.suivant(), seuil = b.seuil.suivant();
                const float plage = b.plage.suivant(), on = b.on.suivant();
                if (on <= 0.0f)
                    continue;
                if (differe (f, b.dernierF) || differe (q, b.dernierQ))
                {
                    // Detection : on n'ecoute que la zone de la bande.
                    const auto type = b.forme == plateauBas ? Svf::passeBas : (b.forme == plateauHaut ? Svf::passeHaut : Svf::passeBande);
                    b.detect.regle (type, sr, f, b.forme == cloche ? q : 0.7071f);
                    b.dernierF = f;
                    b.dernierQ = q;
                    b.dernierGain.fill (1.0e9f);
                }
                for (int g = 0; g < m.groupes(); ++g)
                {
                    const int c0 = m.premier (g), nc = m.tailleGroupe (g);
                    float niveau = 0.0f;
                    for (int c = c0; c < c0 + nc; ++c)
                        niveau = std::max (niveau, std::abs (b.detect.traite (m.ch[c][i], c)));
                    // Suiveur de niveau en dB, attaque / relache.
                    const float db = gainVersDb (niveau);
                    auto& e = b.env[static_cast<size_t> (g)];
                    e = db + (db > e ? b.aAtt : b.aRel) * (e - db);
                    const float depasse = std::max (0.0f, e - seuil);
                    // Pente 2:1 environ, bornee par la plage.
                    const float cible = std::copysign (std::min (std::abs (plage), depasse * 0.6f), plage) * on;
                    auto& gn = b.gain[static_cast<size_t> (g)];
                    gn = cible;
                    maxi = std::max (maxi, std::abs (gn));
                    if (std::abs (gn - b.dernierGain[static_cast<size_t> (g)]) > 0.01f)
                    {
                        const auto type = b.forme == plateauBas ? Svf::plateauBas : (b.forme == plateauHaut ? Svf::plateauHaut : Svf::cloche);
                        b.filtres[static_cast<size_t> (g)].regle (type, sr, f, q, gn);
                        b.dernierGain[static_cast<size_t> (g)] = gn;
                    }
                    for (int c = c0; c < c0 + nc; ++c)
                        m.ch[c][i] = b.filtres[static_cast<size_t> (g)].traite (m.ch[c][i], c);
                }
            }
        activite.store (maxi);
        for (size_t k = 0; k < nbBandes; ++k)
            gainActuel[k].store (etat[k].gain[0]);
    }

    /** Gain dynamique courant de chaque bande (dB), pour l'interface. */
    std::array<std::atomic<float>, nbBandes> gainActuel {};

private:
    struct Etat
    {
        Lisse f, q, seuil, plage, on;
        Svf detect, filtre;
        std::array<Svf, maxCanaux> filtres; // un filtre par groupe (gain propre a chaque groupe)
        std::array<float, maxCanaux> env {}, gain {}, dernierGain {};
        float aAtt = 0, aRel = 0, dernierF = -1, dernierQ = -1;
        int forme = -1;
    };
    std::array<Etat, nbBandes> etat;
};
} // namespace master
