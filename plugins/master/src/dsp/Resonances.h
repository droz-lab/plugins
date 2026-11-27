// Suppresseur de resonances.
//
// Principe (notre conception, a partir de notions publiques de traitement spectral) :
//  1. Analyse court-terme (fenetres de 2048 a 48 kHz, recouvrement 75 %).
//  2. Pour chaque bande fine, on compare son niveau a une "enveloppe" : la moyenne de ses voisines
//     sur environ une octave. Ce qui depasse nettement l'enveloppe est une resonance.
//  3. La reduction depend de l'exces, de la profondeur et de la ponderation en frequence
//     (zone de travail, inclinaison vers les aigus), puis elle est lissee en frequence (nettete)
//     et dans le temps (attaque, relache) pour ne pas "gargouiller".
//  4. Mode delta : on n'entend que ce qui est retire, pour regler a l'oreille.
// Canaux lies : meme reduction sur G et D (l'image ne bouge pas).
#pragma once

#include "Module.h"

#include <complex>

namespace master
{
/** FFT complexe radix 2, iterative, tables calculees dans prepare(). */
class Fft
{
public:
    void prepare (int ordre)
    {
        n = 1 << ordre;
        tw.resize (static_cast<size_t> (n / 2));
        for (int i = 0; i < n / 2; ++i)
            tw[static_cast<size_t> (i)] = std::polar (1.0f, -2.0f * pi * static_cast<float> (i) / static_cast<float> (n));
        inv.resize (static_cast<size_t> (n));
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < ordre; ++b)
                if (i & (1 << b))
                    r |= 1 << (ordre - 1 - b);
            inv[static_cast<size_t> (i)] = r;
        }
    }

    void traite (std::complex<float>* x, bool inverse) const noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const int j = inv[static_cast<size_t> (i)];
            if (j > i)
                std::swap (x[i], x[j]);
        }
        for (int taille = 2; taille <= n; taille <<= 1)
        {
            const int moitie = taille / 2, pas = n / taille;
            for (int debut = 0; debut < n; debut += taille)
                for (int k = 0; k < moitie; ++k)
                {
                    auto w = tw[static_cast<size_t> (k * pas)];
                    if (inverse)
                        w = std::conj (w);
                    const auto a = x[debut + k], b = x[debut + k + moitie] * w;
                    x[debut + k] = a + b;
                    x[debut + k + moitie] = a - b;
                }
        }
    }

    int taille() const noexcept { return n; }

private:
    int n = 0;
    std::vector<std::complex<float>> tw;
    std::vector<int> inv;
};

//==============================================================================
class Resonances final : public Module
{
public:
    static constexpr int pointsAffichage = 256;

    struct Reglages
    {
        float profondeur = 50.0f;   // %
        float nettete = 50.0f;      // %
        float selectivite = 50.0f;  // %
        float attaque = 5.0f;       // ms
        float relache = 80.0f;      // ms
        float freqBas = 200.0f, freqHaut = 16000.0f;
        float inclinaison = 0.0f;   // % : positif = travaille plus dans les aigus
        bool delta = false;
    } r;

    void prepare (double sampleRate) override
    {
        sr = sampleRate;
        ordre = sr <= 50000.0 ? 11 : (sr <= 100000.0 ? 12 : 13);
        N = 1 << ordre;
        hop = N / 4;
        nb = N / 2 + 1;
        fft.prepare (ordre);
        fenetre.resize (static_cast<size_t> (N));
        for (int i = 0; i < N; ++i)
            fenetre[static_cast<size_t> (i)] = std::sqrt (0.5f - 0.5f * std::cos (2.0f * pi * static_cast<float> (i) / static_cast<float> (N)));
        for (int c = 0; c < maxCanaux; ++c)
        {
            entree[c].assign (static_cast<size_t> (N), 0.0f);
            sortie[c].assign (static_cast<size_t> (2 * N), 0.0f);
            spectre[c].assign (static_cast<size_t> (N), {});
        }
        for (int g = 0; g < maxCanaux; ++g)
            reduc[g].assign (static_cast<size_t> (nb), 0.0f);
        niveau.assign (static_cast<size_t> (nb), 0.0f);
        cumul.assign (static_cast<size_t> (nb + 1), 0.0);
        brut.assign (static_cast<size_t> (nb), 0.0f);
        lisse.assign (static_cast<size_t> (nb), 0.0f);
        poids.assign (static_cast<size_t> (nb), 0.0f);
        vide();
    }

    void vide() override
    {
        for (int c = 0; c < maxCanaux; ++c)
        {
            std::fill (entree[c].begin(), entree[c].end(), 0.0f);
            std::fill (sortie[c].begin(), sortie[c].end(), 0.0f);
        }
        for (int g = 0; g < maxCanaux; ++g)
            std::fill (reduc[g].begin(), reduc[g].end(), 0.0f);
        pos = 0;
        compte = 0;
    }

    int latence() const override { return N; }

    void traite (Morceau& m) override
    {
        for (int i = 0; i < m.n; ++i)
        {
            for (int c = 0; c < m.canaux; ++c)
            {
                entree[c][static_cast<size_t> (pos)] = m.ch[c][i];
                // Sortie retardee de N : l'echantillon qui sort a fini d'etre reconstruit.
                auto& s = sortie[c];
                m.ch[c][i] = s[static_cast<size_t> (pos)];
                s[static_cast<size_t> (pos)] = 0.0f;
            }
            pos = (pos + 1) % N;
            if (++compte == hop)
            {
                compte = 0;
                trame (m);
            }
        }
    }

    /** Reduction moyenne (dB, positive) sur une echelle log 20 Hz - 20 kHz, pour l'interface.
        Ecrite par le fil audio sous verrou essaye ; l'interface copie sous le meme verrou. */
    struct Affichage
    {
        std::array<float, pointsAffichage> db {};
    };
    template <typename Verrou>
    void publie (Verrou& verrou, Affichage& dest) const
    {
        if (verrou.tryEnter())
        {
            dest = affichage;
            verrou.exit();
        }
    }

private:
    void trame (const Morceau& m)
    {
        regleTrame();
        // 1. Analyse de chaque canal (les N derniers echantillons, dans l'ordre).
        for (int c = 0; c < m.canaux; ++c)
        {
            auto* X = spectre[c].data();
            for (int k = 0; k < N; ++k)
            {
                const int idx = (pos + k) % N;
                X[k] = { entree[c][static_cast<size_t> (idx)] * fenetre[static_cast<size_t> (k)], 0.0f };
            }
            fft.traite (X, false);
        }

        // 2. Reduction par groupe de canaux.
        float pire = 0.0f;
        for (int g = 0; g < m.groupes(); ++g)
        {
            const int c0 = m.premier (g), nc = m.tailleGroupe (g);
            for (int k = 0; k < nb; ++k)
            {
                float p = 0.0f;
                for (int c = c0; c < c0 + nc; ++c)
                    p += std::norm (spectre[c][static_cast<size_t> (k)]);
                // Normalise : une sinusoide pleine echelle donne environ 0 dB.
                const float norme = 9.0f / (static_cast<float> (N) * static_cast<float> (N));
                niveau[static_cast<size_t> (k)] = 10.0f * std::log10 (p * norme / static_cast<float> (nc) + 1.0e-14f);
            }
            calculeReduction (reduc[g]);

            for (int k = 0; k < nb; ++k)
            {
                const float red = reduc[g][static_cast<size_t> (k)];
                pire = std::max (pire, red);
                float gain = dbVersGain (-red);
                if (r.delta)
                    gain = 1.0f - gain;
                for (int c = c0; c < c0 + nc; ++c)
                {
                    spectre[c][static_cast<size_t> (k)] *= gain;
                    if (k > 0 && k < N / 2)
                        spectre[c][static_cast<size_t> (N - k)] = std::conj (spectre[c][static_cast<size_t> (k)]);
                }
            }
        }
        activite.store (pire);
        majAffichage();

        // 3. Synthese et addition-recouvrement. Hann^2 a 75 % se somme a 2 -> echelle 1/(2N) avec l'IFFT non normalisee.
        const float echelle = 0.5f / static_cast<float> (N);
        for (int c = 0; c < m.canaux; ++c)
        {
            auto* X = spectre[c].data();
            fft.traite (X, true);
            auto& s = sortie[c];
            for (int k = 0; k < N; ++k)
            {
                const int idx = (pos + k) % N;
                s[static_cast<size_t> (idx)] += X[k].real() * fenetre[static_cast<size_t> (k)] * echelle;
            }
        }
    }

    void regleTrame()
    {
        const double trameSr = sr / hop;
        aAtt = coefTemps (trameSr, std::max (r.attaque, 0.1f));
        aRel = coefTemps (trameSr, std::max (r.relache, 1.0f));
        // Seuil au-dessus de l'enveloppe : 2 dB (tout petit relief) a 14 dB (seulement les pics marques).
        seuil = 2.0f + 0.12f * r.selectivite;
        // Profondeur : part de l'exces retiree et plafond de reduction.
        part = r.profondeur / 100.0f;
        plafond = 3.0f + 21.0f * part;
        // Nettete : largeur du lissage de la reduction, de 1/3 d'octave (doux) a 1/48 (chirurgical).
        largeurReduc = std::pow (2.0f, -(1.585f + 4.0f * r.nettete / 100.0f)); // en octaves : 1/3 .. 1/48

        const float fb = std::min (r.freqBas, r.freqHaut * 0.5f), fh = std::max (r.freqHaut, fb * 2.0f);
        for (int k = 0; k < nb; ++k)
        {
            const float f = std::max (1.0f, static_cast<float> (k) * static_cast<float> (sr) / static_cast<float> (N));
            // Zone de travail aux bords adoucis (une demi-octave).
            float w = 1.0f;
            const float sousBas = std::log2 (fb / f), surHaut = std::log2 (f / fh);
            if (sousBas > 0.0f)
                w *= std::max (0.0f, 1.0f - sousBas * 2.0f);
            if (surHaut > 0.0f)
                w *= std::max (0.0f, 1.0f - surHaut * 2.0f);
            // Inclinaison : +/- 100 % = profondeur x2 ou x0 a 5 octaves de 1 kHz.
            w *= std::clamp (1.0f + (r.inclinaison / 100.0f) * std::log2 (f / 1000.0f) / 5.0f, 0.0f, 2.0f);
            poids[static_cast<size_t> (k)] = k < 2 ? 0.0f : w;
        }
    }

    /** Moyenne glissante de `src` sur une largeur relative (octaves) autour de chaque bande. */
    void moyenneOctave (const std::vector<float>& src, std::vector<float>& dst, float octaves)
    {
        cumul[0] = 0.0;
        for (int k = 0; k < nb; ++k)
            cumul[static_cast<size_t> (k + 1)] = cumul[static_cast<size_t> (k)] + src[static_cast<size_t> (k)];
        const float r_ = std::pow (2.0f, octaves * 0.5f);
        for (int k = 0; k < nb; ++k)
        {
            const float fk = static_cast<float> (std::max (k, 1));
            int lo = static_cast<int> (fk / r_), hi = static_cast<int> (std::ceil (fk * r_));
            lo = std::clamp (std::min (lo, k - 1), 0, nb - 1);
            hi = std::clamp (std::max (hi, k + 1), 0, nb - 1);
            dst[static_cast<size_t> (k)] = static_cast<float> ((cumul[static_cast<size_t> (hi + 1)] - cumul[static_cast<size_t> (lo)]) / (hi - lo + 1));
        }
    }

    void calculeReduction (std::vector<float>& red)
    {
        // Enveloppe spectrale : moyenne sur une octave.
        moyenneOctave (niveau, lisse, 1.0f);
        for (int k = 0; k < nb; ++k)
        {
            const float exces = niveau[static_cast<size_t> (k)] - lisse[static_cast<size_t> (k)] - seuil;
            // Pas de reduction sous -90 dB : on ne "corrige" pas le bruit de fond.
            const float vivant = niveau[static_cast<size_t> (k)] > -90.0f ? 1.0f : 0.0f;
            brut[static_cast<size_t> (k)] = std::min (plafond, std::max (0.0f, exces) * part) * poids[static_cast<size_t> (k)] * vivant;
        }
        // Lissage en frequence (nettete).
        moyenneOctave (brut, lisse, largeurReduc);
        // Lissage dans le temps, bande par bande.
        for (int k = 0; k < nb; ++k)
        {
            const float cible = lisse[static_cast<size_t> (k)];
            auto& v = red[static_cast<size_t> (k)];
            v = cible + (cible > v ? aAtt : aRel) * (v - cible);
            if (v < 1.0e-4f)
                v = 0.0f;
        }
    }

    void majAffichage()
    {
        const auto& red = reduc[0];
        for (int p = 0; p < pointsAffichage; ++p)
        {
            const float f0 = 20.0f * std::pow (1000.0f, static_cast<float> (p) / pointsAffichage);
            const float f1 = 20.0f * std::pow (1000.0f, static_cast<float> (p + 1) / pointsAffichage);
            int k0 = static_cast<int> (f0 * N / sr), k1 = static_cast<int> (f1 * N / sr);
            k0 = std::clamp (k0, 0, nb - 1);
            k1 = std::clamp (std::max (k1, k0), 0, nb - 1);
            float v = 0.0f;
            for (int k = k0; k <= k1; ++k)
                v = std::max (v, red[static_cast<size_t> (k)]);
            affichage.db[static_cast<size_t> (p)] = v;
        }
    }

    Fft fft;
    int ordre = 11, N = 2048, hop = 512, nb = 1025, pos = 0, compte = 0;
    std::vector<float> fenetre;
    std::vector<float> entree[maxCanaux], sortie[maxCanaux];
    std::vector<std::complex<float>> spectre[maxCanaux];
    std::vector<float> reduc[maxCanaux];
    std::vector<float> niveau, brut, lisse, poids;
    std::vector<double> cumul;
    float aAtt = 0, aRel = 0, seuil = 8, part = 0.5f, plafond = 12, largeurReduc = 0.1f;
    Affichage affichage;
};
} // namespace master
