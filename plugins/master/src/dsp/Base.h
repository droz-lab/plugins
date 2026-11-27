// Briques de base de Droz Master. C++ pur, sans JUCE : testables seules.
// Regle : rien n'alloue ailleurs que dans prepare().
#pragma once

#include <droz/dsp.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace master
{
constexpr int maxCanaux = 4;     // 4 = transitoire G/D + tonal G/D
constexpr int tailleMorceau = 256; // la chaine travaille par morceaux de 256 echantillons au plus
constexpr float pi = 3.14159265358979f;

/** Un morceau de signal decoupe selon le mode du module.
    lie = vrai : les canaux vont par paires (0,1) et (2,3) qui partagent la detection (image stereo gardee).
    lie = faux : chaque canal est traite seul (mid et side, gauche et droite). */
struct Morceau
{
    float* ch[maxCanaux] {};
    int canaux = 2;
    int n = 0;
    bool lie = true;

    int groupes() const noexcept { return lie ? (canaux + 1) / 2 : canaux; }
    int premier (int g) const noexcept { return lie ? 2 * g : g; }
    int tailleGroupe (int g) const noexcept { return lie ? std::min (2, canaux - 2 * g) : 1; }
};

/** "La valeur a bouge" : comparaison exacte voulue, sans l'avertissement -Wfloat-equal. */
inline bool differe (float a, float b) noexcept { return a < b || a > b; }

inline float dbVersGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainVersDb (float g) noexcept { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float coefTemps (double sr, float ms) noexcept
{
    return ms <= 0.0f ? 0.0f : static_cast<float> (std::exp (-1.0 / (0.001 * ms * sr)));
}

/** Lissage leger pour les parametres : 20 ms, etat en double (voir droz::Smoother). */
struct Lisse
{
    droz::Smoother s;
    bool pret = false;
    void prepare (double sr, float ms = 20.0f) { s.reset (sr, ms); pret = false; }
    void cible (float v) noexcept
    {
        if (! pret) { s.snap (v); pret = true; }
        else s.setTarget (v);
    }
    float suivant() noexcept { return s.next(); }
    float valeur() const noexcept { return s.current(); }
};

//==============================================================================
/** Filtre a variables d'etat (topologie TPT d'Andrew Simper) : stable quand on le module. */
struct Svf
{
    enum Type { passeBas, passeHaut, passeBande, cloche, plateauBas, plateauHaut, passeTout };

    void regle (Type type, double sr, float f, float q, float gainDb = 0.0f) noexcept
    {
        f = std::clamp (f, 10.0f, static_cast<float> (0.49 * sr));
        q = std::max (q, 0.05f);
        float g = std::tan (pi * f / static_cast<float> (sr));
        float k = 1.0f / q;
        const float A = std::pow (10.0f, gainDb / 40.0f);
        switch (type)
        {
            case passeBas:   m0 = 0; m1 = 0; m2 = 1; break;
            case passeHaut:  m0 = 1; m1 = -k; m2 = -1; break;
            case passeBande: m0 = 0; m1 = k; m2 = 0; break; // gain unite au centre
            case cloche:     k = 1.0f / (q * A); m0 = 1; m1 = k * (A * A - 1.0f); m2 = 0; break;
            case plateauBas: g /= std::sqrt (A); m0 = 1; m1 = k * (A - 1.0f); m2 = A * A - 1.0f; break;
            case plateauHaut: g *= std::sqrt (A); m0 = A * A; m1 = k * (1.0f - A) * A; m2 = 1.0f - A * A; break;
            case passeTout:  m0 = 1; m1 = -2.0f * k; m2 = 0; break;
        }
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    float traite (float v0, int c) noexcept
    {
        auto& s1 = ic1[static_cast<size_t> (c)];
        auto& s2 = ic2[static_cast<size_t> (c)];
        const float v3 = v0 - s2;
        const float v1 = a1 * s1 + a2 * v3;
        const float v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2.0f * v1 - s1;
        s2 = 2.0f * v2 - s2;
        return m0 * v0 + m1 * v1 + m2 * v2;
    }

    void vide() noexcept { ic1.fill (0.0f); ic2.fill (0.0f); }

    /** Gain en amplitude a la frequence f (pour dessiner les courbes). */
    float reponse (double sr, float f) const noexcept
    {
        // H(z) d'un SVF TPT : on evalue le schema avec z = e^jw par les formules du filtre analogique prototype.
        const double w = std::tan (pi * f / sr);
        const double g = a2 / a1; // retrouve g
        const double k = (1.0 / a1 - 1.0 - g * g) / g;
        // s normalise = j * w / g
        const double sr_ = w / g;
        const double re = 1.0 - sr_ * sr_, im = k * sr_;
        const double den = re * re + im * im;
        // V1/V0 = s/(s^2+ks+1), V2/V0 = 1/(s^2+ks+1)
        const double h2re = re / den, h2im = -im / den;
        const double h1re = sr_ * (-h2im), h1im = sr_ * h2re;
        const double hr = m0 + m1 * h1re + m2 * h2re, hi = m1 * h1im + m2 * h2im;
        return static_cast<float> (std::sqrt (hr * hr + hi * hi));
    }

    float a1 = 1, a2 = 0, a3 = 0, m0 = 1, m1 = 0, m2 = 0;
    std::array<float, maxCanaux> ic1 {}, ic2 {};
};

//==============================================================================
/** Repartiteur Linkwitz-Riley 24 dB/oct, 3 ou 4 bandes, phases compensees :
    la somme des bandes est un passe-tout (amplitude plate). */
class Repartiteur
{
public:
    void prepare (double sampleRate, int bandes)
    {
        sr = sampleRate;
        nb = std::clamp (bandes, 2, 4);
        for (auto& f : freq)
            f.prepare (sr, 30.0f);
        vide();
    }

    void vide()
    {
        for (auto& f : bas)
            f.vide();
        for (auto& f : haut)
            f.vide();
        for (auto& f : comp)
            f.vide();
    }

    /** Frequences de coupure (nb - 1 valeurs), lissees. */
    void cibles (const float* f) noexcept
    {
        for (int i = 0; i < nb - 1; ++i)
            freq[static_cast<size_t> (i)].cible (f[i]);
    }

    /** A appeler une fois par echantillon, avant les traite(). */
    void avance() noexcept
    {
        bool bouge = ! regle;
        float f[3] {};
        for (int i = 0; i < nb - 1; ++i)
        {
            const float avant = freq[static_cast<size_t> (i)].valeur();
            f[i] = freq[static_cast<size_t> (i)].suivant();
            bouge = bouge || differe (f[i], avant);
        }
        if (! bouge)
            return;
        // Ordre garanti : chaque coupure au moins 1/3 d'octave au-dessus de la precedente.
        for (int i = 1; i < nb - 1; ++i)
            f[i] = std::max (f[i], f[i - 1] * 1.26f);
        for (int i = 0; i < nb - 1; ++i)
        {
            for (int j = 0; j < 2; ++j)
            {
                bas[static_cast<size_t> (i * 2 + j)].regle (Svf::passeBas, sr, f[i], 0.70710678f);
                haut[static_cast<size_t> (i * 2 + j)].regle (Svf::passeHaut, sr, f[i], 0.70710678f);
            }
            comp[static_cast<size_t> (i)].regle (Svf::passeTout, sr, f[i], 0.70710678f);
        }
        regle = true;
    }

    /** Coupe x en nb bandes dans sortie[0..nb-1], pour le canal c. */
    void traite (float x, int c, float* sortie) noexcept
    {
        if (nb == 2)
        {
            sortie[0] = lr (0, true, x, c);
            sortie[1] = lr (0, false, x, c);
        }
        else if (nb == 3)
        {
            // coupe a f1, puis le haut a f2 ; le bas recoit le passe-tout de f2.
            const float b = lr (0, true, x, c), h = lr (0, false, x, c);
            sortie[0] = comp[1].traite (b, c);
            sortie[1] = lr (1, true, h, c);
            sortie[2] = lr (1, false, h, c);
        }
        else
        {
            // coupe a f2 ; bas -> passe-tout f3 puis coupe a f1 ; haut -> passe-tout f1 puis coupe a f3.
            const float b = comp[2].traite (lr (1, true, x, c), c);
            const float h = comp[0].traite (lr (1, false, x, c), c);
            sortie[0] = lr (0, true, b, c);
            sortie[1] = lr (0, false, b, c);
            sortie[2] = lr (2, true, h, c);
            sortie[3] = lr (2, false, h, c);
        }
    }

    int bandes() const noexcept { return nb; }

private:
    // Un etage LR4 = deux Butterworth 12 dB en serie. Chaque filtre a son etat par canal.
    float lr (int i, bool passeBas, float x, int c) noexcept
    {
        auto& tab = passeBas ? bas : haut;
        return tab[static_cast<size_t> (i * 2 + 1)].traite (tab[static_cast<size_t> (i * 2)].traite (x, c), c);
    }

    double sr = 48000.0;
    int nb = 4;
    bool regle = false;
    std::array<Lisse, 3> freq;
    std::array<Svf, 6> bas, haut;
    std::array<Svf, 3> comp;
};

//==============================================================================
/** Ligne de retard entiere, taille fixee dans prepare(). */
class Retard
{
public:
    void prepare (int echantillons)
    {
        d = std::max (0, echantillons);
        buf.assign (static_cast<size_t> (d + 1), 0.0f);
        pos = 0;
    }
    void vide() { std::fill (buf.begin(), buf.end(), 0.0f); pos = 0; }
    float traite (float x) noexcept
    {
        if (d == 0)
            return x;
        buf[static_cast<size_t> (pos)] = x;
        pos = (pos + 1) % (d + 1);
        return buf[static_cast<size_t> (pos)];
    }
    int duree() const noexcept { return d; }

private:
    std::vector<float> buf;
    int d = 0, pos = 0;
};

//==============================================================================
/** Demi-bande FIR (sinus cardinal fenetre) pour suréchantillonner par 2.
    Retard de groupe entier : K echantillons a la frequence haute. */
class DemiBande
{
public:
    void prepare (int K_)
    {
        K = K_;
        const int taille = 2 * K + 1;
        h.assign (static_cast<size_t> (taille), 0.0f);
        double somme = 0.0;
        for (int i = 0; i < taille; ++i)
        {
            const double t = i - K;
            const double sinc = i == K ? 1.0 : std::sin (pi * t * 0.5) / (pi * t * 0.5);
            // Fenetre de Blackman-Harris : rejet > 90 dB.
            const double a = 2.0 * pi * (i + 0.5) / taille;
            const double w = 0.35875 - 0.48829 * std::cos (a) + 0.14128 * std::cos (2 * a) - 0.01168 * std::cos (3 * a);
            h[static_cast<size_t> (i)] = static_cast<float> (0.5 * sinc * w);
            somme += h[static_cast<size_t> (i)];
        }
        for (auto& v : h)
            v = static_cast<float> (v / somme);
        // Phases : y[2m+p] = 2 * sum_q h[2q+p] x[m-q]
        for (int p = 0; p < 2; ++p)
        {
            phase[p].clear();
            for (int i = p; i < taille; i += 2)
                phase[p].push_back (2.0f * h[static_cast<size_t> (i)]);
        }
        histHaut.assign (static_cast<size_t> (maxCanaux * 2 * taille), 0.0f);
        histBas.assign (static_cast<size_t> (maxCanaux * 2 * taille), 0.0f);
        posHaut.fill (0);
        posBas.fill (0);
    }

    void vide()
    {
        std::fill (histHaut.begin(), histHaut.end(), 0.0f);
        std::fill (histBas.begin(), histBas.end(), 0.0f);
        posHaut.fill (0);
        posBas.fill (0);
    }

    /** Un echantillon en entree, deux en sortie (frequence double). */
    void monte (float x, int c, float* deux) noexcept
    {
        const int taille = 2 * K + 1;
        const int lq = static_cast<int> (phase[0].size());
        // historique double (ecrit deux fois) : lecture contigue sans modulo
        float* hist = histHaut.data() + c * 2 * taille;
        int& p = posHaut[static_cast<size_t> (c)];
        p = (p == 0 ? lq : p) - 1;
        hist[p] = x;
        hist[p + lq] = x;
        for (int ph = 0; ph < 2; ++ph)
        {
            const auto& t = phase[ph];
            float acc = 0.0f;
            for (size_t q = 0; q < t.size(); ++q)
                acc += t[q] * hist[p + static_cast<int> (q)];
            deux[ph] = acc;
        }
    }

    /** Deux echantillons en entree (frequence double), un en sortie. */
    float descend (const float* deux, int c) noexcept
    {
        const int taille = 2 * K + 1;
        float* hist = histBas.data() + c * 2 * taille;
        int& p = posBas[static_cast<size_t> (c)];
        float sortie = 0.0f;
        for (int k = 0; k < 2; ++k)
        {
            p = (p == 0 ? taille : p) - 1;
            hist[p] = deux[k];
            hist[p + taille] = deux[k];
        }
        // Sortie alignee sur l'echantillon pair (hist[p + 1]) : retard entier, pas de demi-echantillon.
        for (int i = 0; i < taille; ++i)
            sortie += h[static_cast<size_t> (i)] * hist[p + 1 + i];
        return sortie;
    }

    int retardHaut() const noexcept { return K; }

private:
    int K = 16;
    std::vector<float> h, histHaut, histBas;
    std::vector<float> phase[2];
    std::array<int, maxCanaux> posHaut {}, posBas {};
};

/** Surechantillonnage x2 ou x4. Retard total (aller-retour) entier, a la frequence de base. */
class Surech
{
public:
    void prepare (int facteur_)
    {
        facteur = facteur_ >= 4 ? 4 : 2;
        e1.prepare (64);
        e2.prepare (16);
    }
    void vide() { e1.vide(); e2.vide(); }
    int fois() const noexcept { return facteur; }
    /** Retard de l'aller-retour, en echantillons a la frequence de base. */
    int latence() const noexcept { return facteur == 2 ? 64 : 64 + 8; }
    /** Retard de la seule montee (pour une detection) : x2 -> 32, x4 -> 36. */
    int latenceMontee() const noexcept { return facteur == 2 ? 32 : 32 + 4; }

    void monte (float x, int c, float* sortie) noexcept
    {
        float deux[2];
        e1.monte (x, c, deux);
        if (facteur == 2)
        {
            sortie[0] = deux[0];
            sortie[1] = deux[1];
            return;
        }
        e2.monte (deux[0], c, sortie);
        e2.monte (deux[1], c, sortie + 2);
    }

    float descend (const float* entree, int c) noexcept
    {
        if (facteur == 2)
            return e1.descend (entree, c);
        float deux[2] = { e2.descend (entree, c), e2.descend (entree + 2, c) };
        return e1.descend (deux, c);
    }

private:
    int facteur = 2;
    DemiBande e1, e2;
};
} // namespace master
