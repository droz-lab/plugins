// Limiteur a anticipation (1,5 ms) avec detection des crêtes inter-echantillons (x4).
// Gain requis -> minimum glissant sur la fenetre d'anticipation -> relache -> moyenne glissante :
// le gain atteint exactement la valeur voulue quand la crête arrive, sans couper net.
#pragma once

#include "Module.h"

namespace master
{
class Limiteur final : public Module
{
public:
    enum Style { transparent, fort };

    struct Reglages
    {
        float gain = 0.0f;       // dB pousses dans le limiteur
        float plafond = -1.0f;   // dBFS
        float relache = 120.0f;  // ms
        int style = transparent;
        bool cretesVraies = true;
    } r;

    void prepare (double sampleRate) override
    {
        sr = sampleRate;
        L = std::clamp (static_cast<int> (std::round (0.0015 * sr)), 2, 500);
        os.prepare (4);
        retardDetect = os.latenceMontee();
        for (auto& d : retardEchantillon)
            d.prepare (retardDetect);
        for (auto& d : retardAudio)
            d.prepare (L - 1 + retardDetect);
        gain.prepare (sr);
        plafond.prepare (sr);
        vide();
    }

    void vide() override
    {
        os.vide();
        for (auto& d : retardEchantillon)
            d.vide();
        for (auto& d : retardAudio)
            d.vide();
        temps = 0;
        env.fill (1.0f);
        lent.fill (1.0f);
        for (auto& g : groupesMin)
            g = {};
    }

    int latence() const override { return L - 1 + retardDetect; }

    void traite (Morceau& m) override
    {
        gain.cible (r.gain);
        plafond.cible (r.plafond);
        const float rel = r.style == fort ? r.relache * 0.35f : r.relache;
        aRel = coefTemps (sr, std::max (rel, 1.0f));
        aRelLent = coefTemps (sr, std::max (rel, 1.0f) * 6.0f);
        float pire = 0.0f;

        for (int i = 0; i < m.n; ++i)
        {
            const float g = dbVersGain (gain.suivant());
            const float c = dbVersGain (plafond.suivant());
            for (int grp = 0; grp < m.groupes(); ++grp)
            {
                const int c0 = m.premier (grp), nc = m.tailleGroupe (grp);
                float crete = 0.0f;
                for (int ch = c0; ch < c0 + nc; ++ch)
                {
                    const float x = m.ch[ch][i] * g;
                    float quatre[4];
                    os.monte (x, ch, quatre);
                    const float pe = std::abs (retardEchantillon[static_cast<size_t> (ch)].traite (x));
                    float pv = pe;
                    for (float v : quatre)
                        pv = std::max (pv, std::abs (v));
                    // Marge de 0,3 dB : le filtre x4 voit mal l'extreme haut du spectre (mesure : bruit blanc).
                    pv *= 1.035f;
                    crete = std::max (crete, r.cretesVraies ? pv : pe);
                    m.ch[ch][i] = retardAudio[static_cast<size_t> (ch)].traite (x);
                }
                const float requis = crete > c ? c / crete : 1.0f;
                const float gr = calculeGain (grp, requis);
                pire = std::max (pire, -gainVersDb (gr));
                for (int ch = c0; ch < c0 + nc; ++ch)
                {
                    float y = m.ch[ch][i] * gr;
                    // Filet : jamais au-dessus du plafond, meme avec un arrondi de calcul.
                    y = std::clamp (y, -c, c);
                    m.ch[ch][i] = y;
                }
            }
            ++temps;
        }
        activite.store (pire);
    }

private:
    // Chaque groupe a son minimum glissant, sa relache et sa moyenne. Pour rester simple et sans
    // allocation, l'etat par groupe vit dans des tableaux de taille fixe.
    struct GroupeMin
    {
        std::array<float, 512> val {};
        std::array<long long, 512> idx {};
        int tete = 0, queue = 0;
        std::array<float, 512> moy {};
        double somme = 0.0;
        int pos = 0;
        bool init = false;
    };

    float calculeGain (int grp, float requis) noexcept
    {
        auto& G = groupesMin[static_cast<size_t> (grp)];
        const int cap = static_cast<int> (G.val.size());
        if (! G.init)
        {
            G.moy.fill (1.0f);
            G.somme = L;
            G.init = true;
        }
        // 1. Minimum glissant sur L echantillons (file monotone).
        while (G.queue != G.tete && G.val[static_cast<size_t> ((G.queue - 1 + cap) % cap)] >= requis)
            G.queue = (G.queue - 1 + cap) % cap;
        G.val[static_cast<size_t> (G.queue)] = requis;
        G.idx[static_cast<size_t> (G.queue)] = temps;
        G.queue = (G.queue + 1) % cap;
        while (G.idx[static_cast<size_t> (G.tete)] <= temps - L)
            G.tete = (G.tete + 1) % cap;
        const float mini = G.val[static_cast<size_t> (G.tete)];

        // 2. Relache : descend tout de suite, remonte doucement. Style transparent : relache a deux vitesses,
        //    la lente prend le relais quand la limitation dure (moins de pompage).
        auto& e = env[static_cast<size_t> (grp)];
        auto& l = lent[static_cast<size_t> (grp)];
        if (mini < e)
            e = mini;
        else
            e = mini + aRel * (e - mini);
        if (mini < l)
            l = mini + 0.999f * (l - mini);
        else
            l = mini + aRelLent * (l - mini);
        const float lisse = r.style == transparent ? std::min (e, 0.5f * (l + e)) : e;

        // 3. Moyenne glissante sur L : la descente commence L echantillons avant la crête.
        G.somme += lisse - G.moy[static_cast<size_t> (G.pos)];
        G.moy[static_cast<size_t> (G.pos)] = lisse;
        G.pos = (G.pos + 1) % L;
        return static_cast<float> (G.somme / L);
    }

    Surech os;
    int L = 72, retardDetect = 20;
    std::array<Retard, maxCanaux> retardEchantillon, retardAudio;
    long long temps = 0;
    std::array<float, maxCanaux> env {}, lent {};
    std::array<GroupeMin, maxCanaux> groupesMin {};
    float aRel = 0.99f, aRelLent = 0.999f;
    Lisse gain, plafond;
};
} // namespace master
