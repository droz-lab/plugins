// Exciter multibande : 3 bandes, chacune avec sa saturation (lampe, bande, transistor, douce).
// Tout le module tourne en suréchantillonnage x2 : les harmoniques ajoutees ne replient pas.
// On ajoute seulement la difference (harmoniques) a la bande d'origine, dosee par bande.
#pragma once

#include "Module.h"

namespace master
{
class Exciter final : public Module
{
public:
    static constexpr int nbBandes = 3;
    enum Couleur { lampe, bande, transistor, douce };

    struct Bande
    {
        int couleur = lampe;
        float drive = 6.0f;  // dB
        float dose = 0.0f;   // %
    };
    struct Reglages
    {
        std::array<float, nbBandes - 1> coupures { 250.0f, 4000.0f };
        std::array<Bande, nbBandes> bandes;
    } r;

    void prepare (double sampleRate) override
    {
        sr = sampleRate;
        os.prepare (2);
        rep.prepare (sr * 2.0, nbBandes);
        for (auto& b : etat)
        {
            b.drive.prepare (sr * 2.0);
            b.dose.prepare (sr * 2.0);
        }
        // Coupe-continu a 8 Hz sur les harmoniques (la lampe est asymetrique).
        rDc = static_cast<float> (std::exp (-2.0 * pi * 8.0 / (sr * 2.0)));
        vide();
    }

    void vide() override
    {
        os.vide();
        rep.vide();
        for (auto& b : etat)
        {
            b.dcX.fill (0.0f);
            b.dcY.fill (0.0f);
        }
    }

    int latence() const override { return os.latence(); }

    void traite (Morceau& m) override
    {
        rep.cibles (r.coupures.data());
        for (size_t k = 0; k < nbBandes; ++k)
        {
            etat[k].drive.cible (r.bandes[k].drive);
            etat[k].dose.cible (r.bandes[k].dose / 100.0f);
        }
        float maxi = 0.0f;
        for (int i = 0; i < m.n; ++i)
        {
            float haut[maxCanaux][2];
            for (int c = 0; c < m.canaux; ++c)
                os.monte (m.ch[c][i], c, haut[c]);

            for (int j = 0; j < 2; ++j)
            {
                rep.avance();
                float d[nbBandes], dose[nbBandes];
                for (size_t k = 0; k < nbBandes; ++k)
                {
                    d[k] = dbVersGain (etat[k].drive.suivant());
                    dose[k] = etat[k].dose.suivant();
                }
                for (int c = 0; c < m.canaux; ++c)
                {
                    float parts[4];
                    rep.traite (haut[c][j], c, parts);
                    float somme = 0.0f;
                    for (size_t k = 0; k < nbBandes; ++k)
                    {
                        const float x = parts[k];
                        auto& b = etat[k];
                        float y = x;
                        if (dose[k] > 0.0f)
                        {
                            const float h = sature (r.bandes[k].couleur, x * d[k]) / d[k] - x;
                            // Coupe-continu, puis dosage des harmoniques.
                            const float hc = h - b.dcX[static_cast<size_t> (c)] + rDc * b.dcY[static_cast<size_t> (c)];
                            b.dcX[static_cast<size_t> (c)] = h;
                            b.dcY[static_cast<size_t> (c)] = hc;
                            y = x + dose[k] * hc * 2.0f;
                            maxi = std::max (maxi, std::abs (hc));
                        }
                        somme += y;
                    }
                    haut[c][j] = somme;
                }
            }
            for (int c = 0; c < m.canaux; ++c)
                m.ch[c][i] = os.descend (haut[c], c);
        }
        activite.store (gainVersDb (1.0f + maxi * 4.0f));
    }

    /** Courbes de saturation, pente 1 a l'origine, f(0) = 0. */
    static float sature (int couleur, float x) noexcept
    {
        switch (couleur)
        {
            case lampe:
            {
                // Asymetrique : harmoniques paires (chaleur).
                constexpr float b = 0.35f, t = 0.33637554f, pente = 0.88685f; // tanh(b), 1 - tanh(b)^2
                return (std::tanh (x + b) - t) / pente;
            }
            case bande: return std::tanh (x);
            case transistor:
            {
                // Genou plus dur, harmoniques impaires plus brillantes.
                const float x2 = x * x;
                return x / std::sqrt (std::sqrt (1.0f + x2 * x2));
            }
            default:
            {
                // Douce : cubique bornee.
                const float c = std::clamp (x, -1.5f, 1.5f);
                return c - c * c * c * (4.0f / 27.0f);
            }
        }
    }

private:
    struct Etat
    {
        Lisse drive, dose;
        std::array<float, maxCanaux> dcX {}, dcY {};
    };
    Surech os;
    Repartiteur rep;
    std::array<Etat, nbBandes> etat;
    float rDc = 0.999f;
};
} // namespace master
