// Un emplacement de la chaine : decoupe le signal selon le mode, fait traiter le module,
// dose chaque partie, recompose, et fond l'activation (pas de clic a l'allumage).
#pragma once

#include "Module.h"

namespace master
{
enum Mode { stereo, midSide, gaucheDroite, transitoireTonal };

/** Separe transitoires et partie tonale : x = t + n exactement (w * x et (1 - w) * x).
    w vient de la difference entre une enveloppe rapide et une lente. */
class SeparateurTransitoires
{
public:
    void prepare (double sr)
    {
        attR = coefTemps (sr, 0.2f);
        relR = coefTemps (sr, 20.0f);
        attL = coefTemps (sr, 15.0f);
        relL = coefTemps (sr, 80.0f);
        lisseW = coefTemps (sr, 1.0f);
        vide();
    }
    void vide() { rapide = lent = w = 0.0f; }

    float poids (float niveau) noexcept
    {
        rapide = niveau + (niveau > rapide ? attR : relR) * (rapide - niveau);
        lent = niveau + (niveau > lent ? attL : relL) * (lent - niveau);
        const float t = rapide > 1.0e-7f ? std::clamp (2.5f * (rapide - lent) / rapide, 0.0f, 1.0f) : 0.0f;
        w = t + lisseW * (w - t);
        if (w < 1.0e-6f)
            w = 0.0f;
        return w;
    }

private:
    float attR = 0, relR = 0, attL = 0, relL = 0, lisseW = 0;
    float rapide = 0, lent = 0, w = 0;
};

class Emplacement
{
public:
    explicit Emplacement (Module& m) : module (m) {}

    struct Reglages
    {
        bool actif = false;
        int mode = stereo;
        float doseA = 100.0f, doseB = 100.0f; // %
    } r;

    void prepare (double sampleRate)
    {
        module.prepare (sampleRate);
        for (auto& d : retards)
            d.prepare (module.latence());
        on.prepare (sampleRate, 15.0f);
        doseA.prepare (sampleRate);
        doseB.prepare (sampleRate);
        separateur.prepare (sampleRate);
        vide();
    }

    void vide()
    {
        module.vide();
        for (auto& d : retards)
            d.vide();
        separateur.vide();
    }

    int latence() const { return module.latence(); }
    bool allume() const { return on.valeur() > 0.0f; }
    Module& leModule() { return module; }

    void traite (float* L, float* R, int n)
    {
        on.cible (r.actif ? 1.0f : 0.0f);
        doseA.cible (r.doseA / 100.0f);
        doseB.cible (r.doseB / 100.0f);

        Morceau m;
        m.n = n;
        m.canaux = r.mode == transitoireTonal ? 4 : 2;
        m.lie = r.mode == stereo || r.mode == transitoireTonal;
        float* P[maxCanaux];
        float* S[maxCanaux];
        for (size_t c = 0; c < maxCanaux; ++c)
        {
            P[c] = parts[c].data();
            S[c] = sec[c].data();
            m.ch[c] = P[c];
        }

        // 1. Decoupe + copie seche retardee de la meme latence que le module.
        for (int i = 0; i < n; ++i)
        {
            const float l = L[i], rr = R[i];
            switch (r.mode)
            {
                case midSide:
                    P[0][i] = 0.5f * (l + rr);
                    P[1][i] = 0.5f * (l - rr);
                    break;
                case transitoireTonal:
                {
                    const float w = separateur.poids (std::max (std::abs (l), std::abs (rr)));
                    P[0][i] = w * l;
                    P[1][i] = w * rr;
                    P[2][i] = l - P[0][i];
                    P[3][i] = rr - P[1][i];
                    break;
                }
                default:
                    P[0][i] = l;
                    P[1][i] = rr;
                    break;
            }
            for (int c = 0; c < m.canaux; ++c)
                S[c][i] = retards[static_cast<size_t> (c)].traite (P[c][i]);
        }

        // 2. Le module.
        module.traite (m);

        // 3. Dosage par partie, recomposition, fondu d'activation.
        for (int i = 0; i < n; ++i)
        {
            const float a = doseA.suivant(), b = doseB.suivant(), o = on.suivant();
            float sortie[maxCanaux];
            for (int c = 0; c < m.canaux; ++c)
            {
                const bool partieB = (r.mode == midSide || r.mode == gaucheDroite) ? c == 1 : c >= 2;
                const float d = (r.mode == stereo) ? a : (partieB ? b : a);
                sortie[c] = S[c][i] + d * (P[c][i] - S[c][i]);
            }
            float l, rr, sl, sr_;
            recompose (sortie, l, rr);
            float s[maxCanaux];
            for (int c = 0; c < m.canaux; ++c)
                s[c] = S[c][i];
            recompose (s, sl, sr_);
            L[i] = sl + o * (l - sl);
            R[i] = sr_ + o * (rr - sr_);
        }
    }

private:
    void recompose (const float* p, float& l, float& rr) const noexcept
    {
        switch (r.mode)
        {
            case midSide: l = p[0] + p[1]; rr = p[0] - p[1]; break;
            case transitoireTonal: l = p[0] + p[2]; rr = p[1] + p[3]; break;
            default: l = p[0]; rr = p[1]; break;
        }
    }

    Module& module;
    std::array<Retard, maxCanaux> retards;
    std::array<std::array<float, tailleMorceau>, maxCanaux> parts {}, sec {};
    Lisse on, doseA, doseB;
    SeparateurTransitoires separateur;
};
} // namespace master
