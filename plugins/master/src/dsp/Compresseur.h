// Coeur de compression (un par groupe de canaux) et module compresseur a caracteres.
// Principes publics : calcul de gain a genou doux, lissage dans le domaine du gain (attaque / relache
// separees), detection crete ou RMS, topologie directe ou en retour. Aucun code tiers.
#pragma once

#include "Module.h"

namespace master
{
/** Calcul de gain + lissage. Travaille en dB. Un objet par groupe de canaux lies. */
class CoeurCompression
{
public:
    enum Style { propre, punch, opto, vintage, bus };

    struct Reglages
    {
        int style = propre;
        float seuil = -18.0f, ratio = 2.0f, attaque = 10.0f, relache = 120.0f, genou = 6.0f;
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        vide();
    }
    void vide() { gr = 0.0f; rapide = 0.0f; rms = 0.0f; charge = 0.0f; lent = 0.0f; }

    /** Une fois par morceau. */
    void regle (const Reglages& rg) noexcept
    {
        r = rg;
        float att = rg.attaque, rel = rg.relache;
        switch (rg.style)
        {
            case punch: att = std::max (att, 1.5f); break;                  // laisse passer l'attaque des transitoires
            case opto: att = std::max (att * 2.0f, 5.0f); break;            // cellule optique : lente a reagir
            case bus: att = std::max (att, 0.3f); break;
            default: break;
        }
        aAtt = coefTemps (sr, att);
        aRel = coefTemps (sr, rel);
        aRelLent = coefTemps (sr, rel * 5.0f);
        aRms = coefTemps (sr, rg.style == bus ? 8.0f : 25.0f);
        aCharge = coefTemps (sr, 1500.0f);
        genou = rg.style == opto ? std::max (rg.genou, 12.0f) : rg.genou;
    }

    /** `niveau` = valeur absolue du signal de detection (crete du groupe). Renvoie la reduction en dB (>= 0). */
    float traite (float niveau) noexcept
    {
        float det = niveau;
        if (r.style == opto || r.style == bus)
        {
            // Detection en energie : plus lisse, reagit au volume ressenti.
            rms = niveau * niveau + aRms * (rms - niveau * niveau);
            det = std::sqrt (std::max (rms, 0.0f)) * 1.4142f;
        }
        const float cible = calcule (gainVersDb (det));

        if (cible > rapide)
            rapide = cible + aAtt * (rapide - cible);
        else
        {
            float a = aRel;
            if (r.style == punch)
                a = coefPlusRapide (rapide); // relache plus vive quand la reduction est forte : garde l'energie
            else if (r.style == opto)
                a = aRel + (aRelLent - aRel) * charge; // longue compression -> relache plus lente
            rapide = cible + a * (rapide - cible);
        }
        if (rapide < 1.0e-6f)
            rapide = 0.0f;
        gr = rapide;

        if (r.style == bus)
        {
            // Relache automatique a deux vitesses : l'etage rapide suit les crêtes, le lent fait la colle.
            lent = cible + (cible > lent ? aAtt : aRelLent) * (lent - cible);
            if (lent < 1.0e-6f)
                lent = 0.0f;
            gr = 0.65f * rapide + 0.35f * lent;
        }
        if (r.style == opto)
        {
            const float c = std::min (1.0f, gr / 6.0f);
            charge = c + aCharge * (charge - c);
        }
        return gr;
    }

    float reduction() const noexcept { return gr; }

    /** Courbe statique : reduction (dB) pour un niveau d'entree (dB). */
    float calcule (float x) const noexcept
    {
        const float pente = 1.0f - 1.0f / std::max (1.0f, r.ratio);
        const float d = x - r.seuil;
        if (genou > 0.0f && std::abs (d) <= genou * 0.5f)
        {
            const float t = d + genou * 0.5f;
            return pente * t * t / (2.0f * genou);
        }
        return d > 0.0f ? pente * d : 0.0f;
    }

private:
    float coefPlusRapide (float reduc) const noexcept
    {
        const float facteur = 1.0f + reduc / 6.0f;
        return std::pow (aRel, facteur);
    }

    double sr = 48000.0;
    Reglages r;
    float genou = 6.0f;
    float aAtt = 0, aRel = 0, aRelLent = 0, aRms = 0, aCharge = 0;
    float gr = 0, rapide = 0, rms = 0, charge = 0, lent = 0;
};

//==============================================================================
class Compresseur final : public Module
{
public:
    struct Reglages
    {
        CoeurCompression::Reglages coeur;
        float gain = 0.0f;     // gain de compensation (dB)
        float filtre = 20.0f;  // passe-haut de la detection (Hz)
    } r;

    void prepare (double sampleRate) override
    {
        sr = sampleRate;
        for (auto& c : coeurs)
            c.prepare (sr);
        gain.prepare (sr);
        filtre.prepare (sr);
        seuil.prepare (sr);
        vide();
    }

    void vide() override
    {
        for (auto& c : coeurs)
            c.vide();
        hp.vide();
        retour.fill (0.0f);
    }

    void traite (Morceau& m) override
    {
        gain.cible (r.gain);
        filtre.cible (r.filtre);
        seuil.cible (r.coeur.seuil);
        const bool enRetour = r.coeur.style == CoeurCompression::vintage;
        float maxi = 0.0f;

        for (int i = 0; i < m.n; ++i)
        {
            const float g = dbVersGain (gain.suivant());
            const float fh = filtre.suivant();
            auto rg = r.coeur;
            rg.seuil = seuil.suivant();
            if (differe (fh, dernierFiltre))
            {
                hp.regle (Svf::passeHaut, sr, fh, 0.7071f);
                dernierFiltre = fh;
            }
            for (int grp = 0; grp < m.groupes(); ++grp)
            {
                auto& coeur = coeurs[static_cast<size_t> (grp)];
                if (i == 0 || differe (rg.seuil, seuilPrec))
                    coeur.regle (rg);
                const int c0 = m.premier (grp), nc = m.tailleGroupe (grp);
                float niveau = 0.0f;
                for (int c = c0; c < c0 + nc; ++c)
                {
                    const float src = enRetour ? retour[static_cast<size_t> (c)] : m.ch[c][i];
                    niveau = std::max (niveau, std::abs (hp.traite (src, c)));
                }
                const float red = coeur.traite (niveau);
                maxi = std::max (maxi, red);
                const float gr = dbVersGain (-red);
                for (int c = c0; c < c0 + nc; ++c)
                {
                    float y = m.ch[c][i] * gr;
                    if (enRetour)
                    {
                        retour[static_cast<size_t> (c)] = y;
                        // Couleur "vintage" : un peu de saturation qui monte avec la reduction.
                        const float coul = std::min (1.0f, red / 12.0f) * 0.35f;
                        y = y + coul * (std::tanh (y) - y);
                    }
                    m.ch[c][i] = y * g;
                }
            }
            seuilPrec = rg.seuil;
        }
        activite.store (maxi);
    }

    /** Pour la courbe de l'interface. */
    float courbe (float entreeDb) const noexcept { return entreeDb - coeurs[0].calcule (entreeDb) + r.gain; }

private:
    std::array<CoeurCompression, maxCanaux> coeurs;
    Svf hp;
    Lisse gain, filtre, seuil;
    float dernierFiltre = -1.0f, seuilPrec = 1.0e9f;
    std::array<float, maxCanaux> retour {};
};
} // namespace master
