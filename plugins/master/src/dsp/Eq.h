// EQ parametrique, 5 bandes. Filtres TPT : on peut tourner les potards sans clic.
#pragma once

#include "Module.h"

namespace master
{
class Eq final : public Module
{
public:
    static constexpr int nbBandes = 5;
    enum Forme { coupeBas, plateauBas, cloche, plateauHaut, coupeHaut };

    struct Bande
    {
        bool actif = false;
        int forme = cloche;
        float freq = 1000.0f, gain = 0.0f, q = 0.7f;
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
            b.g.prepare (sr);
            b.q.prepare (sr);
            b.on.prepare (sr);
        }
        vide();
    }

    void vide() override
    {
        for (auto& b : etat)
        {
            b.filtre.vide();
            b.filtre2.vide();
        }
    }

    void traite (Morceau& m) override
    {
        for (size_t i = 0; i < nbBandes; ++i)
        {
            auto& b = etat[i];
            const auto& rb = r.bandes[i];
            b.f.cible (rb.freq);
            b.g.cible (rb.gain);
            b.q.cible (rb.q);
            b.on.cible (rb.actif ? 1.0f : 0.0f);
            if (rb.forme != b.forme)
            {
                b.forme = rb.forme;
                b.change = true;
            }
        }

        for (int i = 0; i < m.n; ++i)
            for (auto& b : etat)
            {
                const float f = b.f.suivant(), g = b.g.suivant(), q = b.q.suivant(), on = b.on.suivant();
                if (on <= 0.0f)
                    continue;
                if (b.change || differe (f, b.dernier[0]) || differe (g, b.dernier[1]) || differe (q, b.dernier[2]))
                    regle (b, f, g, q);
                for (int c = 0; c < m.canaux; ++c)
                {
                    const float x = m.ch[c][i];
                    float y = b.filtre.traite (x, c);
                    if (b.forme == coupeBas || b.forme == coupeHaut)
                        y = b.filtre2.traite (y, c);
                    m.ch[c][i] = x + on * (y - x);
                }
            }
    }

    /** Courbe totale (amplitude) a la frequence f, d'apres les reglages demandes. Pour l'interface. */
    static float reponse (const Reglages& rg, double sampleRate, float f)
    {
        float total = 1.0f;
        for (const auto& b : rg.bandes)
        {
            if (! b.actif)
                continue;
            Etat e;
            e.forme = b.forme;
            regle (e, b.freq, b.gain, b.q, sampleRate);
            total *= e.filtre.reponse (sampleRate, f);
            if (b.forme == coupeBas || b.forme == coupeHaut)
                total *= e.filtre2.reponse (sampleRate, f);
        }
        return total;
    }

private:
    struct Etat
    {
        Lisse f, g, q, on;
        Svf filtre, filtre2;
        int forme = -1;
        bool change = true;
        float dernier[3] { -1, -1, -1 };
    };

    void regle (Etat& b, float f, float g, float q) { regle (b, f, g, q, sr); }

    static void regle (Etat& b, float f, float g, float q, double sampleRate)
    {
        switch (b.forme)
        {
            // Coupes : deux etages, 24 dB/oct ; Q agit sur le second (resonance au coude).
            case coupeBas:
                b.filtre.regle (Svf::passeHaut, sampleRate, f, 0.5412f);
                b.filtre2.regle (Svf::passeHaut, sampleRate, f, 1.3066f * q / 0.7071f);
                break;
            case coupeHaut:
                b.filtre.regle (Svf::passeBas, sampleRate, f, 0.5412f);
                b.filtre2.regle (Svf::passeBas, sampleRate, f, 1.3066f * q / 0.7071f);
                break;
            case plateauBas: b.filtre.regle (Svf::plateauBas, sampleRate, f, q, g); break;
            case plateauHaut: b.filtre.regle (Svf::plateauHaut, sampleRate, f, q, g); break;
            default: b.filtre.regle (Svf::cloche, sampleRate, f, q, g); break;
        }
        b.change = false;
        b.dernier[0] = f;
        b.dernier[1] = g;
        b.dernier[2] = q;
    }

    std::array<Etat, nbBandes> etat;
};
} // namespace master
