#include "Parametres.h"

namespace master
{
namespace
{
DescParam continu (juce::String id, const char* nom, const char* court, float mn, float mx, float def, const char* unite, const char* aide, float centre = 0.0f)
{
    DescParam p;
    p.id = std::move (id);
    p.nom = T (nom);
    p.court = T (court);
    p.min = mn;
    p.max = mx;
    p.defaut = def;
    p.centre = centre;
    p.unite = T (unite);
    p.aide = T (aide);
    return p;
}

DescParam liste (juce::String id, const char* nom, const char* court, std::initializer_list<const char*> choix, int def, const char* aide)
{
    DescParam p;
    p.id = std::move (id);
    p.nom = T (nom);
    p.court = T (court);
    p.genre = Genre::liste;
    for (auto* c : choix)
        p.choix.add (T (c));
    p.min = 0;
    p.max = static_cast<float> (p.choix.size() - 1);
    p.defaut = static_cast<float> (def);
    p.aide = T (aide);
    return p;
}

DescParam interrupteur (juce::String id, const char* nom, const char* court, bool def, const char* aide)
{
    DescParam p;
    p.id = std::move (id);
    p.nom = T (nom);
    p.court = T (court);
    p.genre = Genre::interrupteur;
    p.defaut = def ? 1.0f : 0.0f;
    p.aide = T (aide);
    return p;
}

juce::String num (const char* prefixe, int i, const char* suffixe)
{
    return juce::String (prefixe) + juce::String (i) + "_" + suffixe;
}

std::vector<DescModule> construit()
{
    std::vector<DescModule> mods;
    const auto styles = { "Propre", "Punch", "Opto", "Vintage", "Bus" };

    // --- EQ
    {
        DescModule m { "eq", "EQ", "EQ", T ("Égaliseur paramétrique, 5 bandes."), {} };
        const char* formes[] = { "Coupe-bas", "Plateau bas", "Cloche", "Plateau haut", "Coupe-haut" };
        const int defForme[] = { 0, 1, 2, 2, 3 };
        const float defF[] = { 30, 100, 600, 3000, 10000 };
        for (int i = 1; i <= 5; ++i)
        {
            DescGroupe g { "Bande " + juce::String (i), {} };
            g.params.push_back (interrupteur (num ("eq", i, "actif"), "Active", "Active", i != 1, "Allume ou éteint cette bande."));
            auto f = liste (num ("eq", i, "forme"), "Forme", "Forme", { formes[0], formes[1], formes[2], formes[3], formes[4] }, defForme[i - 1], "Forme du filtre.");
            g.params.push_back (f);
            g.params.push_back (continu (num ("eq", i, "freq"), "Fréquence", "Fréq", 20, 20000, defF[i - 1], "Hz", "Fréquence de la bande.", 632));
            g.params.push_back (continu (num ("eq", i, "gain"), "Gain", "Gain", -18, 18, 0, "dB", "Gain de la bande (sans effet sur les coupes)."));
            g.params.push_back (continu (num ("eq", i, "q"), "Largeur (Q)", "Q", 0.1f, 10, 0.7f, "", "Plus le Q est grand, plus la bande est étroite.", 1));
            m.groupes.push_back (g);
        }
        mods.push_back (m);
    }

    // --- EQ dynamique
    {
        DescModule m { "deq", "EQ dynamique", "DYN", T ("Bandes qui bougent seules quand leur zone dépasse le seuil."), {} };
        const char* formes[] = { "Cloche", "Plateau bas", "Plateau haut" };
        const int defForme[] = { 1, 0, 2 };
        const float defF[] = { 120, 3000, 9000 };
        for (int i = 1; i <= 3; ++i)
        {
            DescGroupe g { "Bande " + juce::String (i), {} };
            g.params.push_back (interrupteur (num ("deq", i, "actif"), "Active", "Active", true, "Allume ou éteint cette bande."));
            g.params.push_back (liste (num ("deq", i, "forme"), "Forme", "Forme", { formes[0], formes[1], formes[2] }, defForme[i - 1], "Forme du filtre."));
            g.params.push_back (continu (num ("deq", i, "freq"), "Fréquence", "Fréq", 20, 20000, defF[i - 1], "Hz", "Zone écoutée et corrigée.", 632));
            g.params.push_back (continu (num ("deq", i, "q"), "Largeur (Q)", "Q", 0.1f, 10, 1.0f, "", "Largeur de la cloche.", 1));
            g.params.push_back (continu (num ("deq", i, "seuil"), "Seuil", "Seuil", -60, 0, -24, "dB", "Niveau à partir duquel la bande agit."));
            g.params.push_back (continu (num ("deq", i, "plage"), "Plage", "Plage", -18, 18, -6, "dB", "Correction maximale : négatif = creuse quand ça déborde, positif = remonte."));
            g.params.push_back (continu (num ("deq", i, "attaque"), "Attaque", "Att", 0.1f, 200, 5, "ms", "Vitesse de réaction.", 10));
            g.params.push_back (continu (num ("deq", i, "relache"), "Relâche", "Rel", 5, 2000, 120, "ms", "Vitesse de retour.", 150));
            m.groupes.push_back (g);
        }
        mods.push_back (m);
    }

    // --- Compresseur
    {
        DescModule m { "comp", "Compresseur", "COMP", T ("Compresseur à caractères : Propre, Punch, Opto, Vintage, Bus."), {} };
        DescGroupe g1 { T ("Caractère"), {} };
        g1.params.push_back (liste ("comp_style", "Caractère", "Style", styles, 0,
                                    "Propre : transparent. Punch : laisse passer l'attaque. Opto : lent et doux. Vintage : en retour, avec un peu de grain. Bus : colle le mix."));
        g1.params.push_back (continu ("comp_filtre", "Filtre de détection", "Filtre", 20, 500, 20, "Hz", "Le compresseur n'écoute pas les basses sous cette fréquence (moins de pompage).", 100));
        DescGroupe g2 { T ("Niveau"), {} };
        g2.params.push_back (continu ("comp_seuil", "Seuil", "Seuil", -60, 0, -18, "dB", "Niveau à partir duquel ça compresse."));
        g2.params.push_back (continu ("comp_ratio", "Ratio", "Ratio", 1, 20, 2, ":1", "Force de la compression.", 4));
        g2.params.push_back (continu ("comp_genou", "Genou", "Genou", 0, 24, 6, "dB", "Entrée en compression : 0 = nette, grand = progressive."));
        DescGroupe g3 { T ("Temps"), {} };
        g3.params.push_back (continu ("comp_attaque", "Attaque", "Att", 0.05f, 200, 10, "ms", "Vitesse de réaction.", 10));
        g3.params.push_back (continu ("comp_relache", "Relâche", "Rel", 5, 2000, 150, "ms", "Vitesse de retour.", 150));
        g3.params.push_back (continu ("comp_gain", "Gain de sortie", "Gain", -12, 24, 0, "dB", "Rattrape le volume perdu."));
        m.groupes = { g1, g2, g3 };
        mods.push_back (m);
    }

    // --- Multibande
    {
        DescModule m { "mbc", "Compresseur multibande", "MULTI", T ("4 bandes compressées séparément, somme à phase compensée."), {} };
        DescGroupe g0 { T ("Coupures"), {} };
        g0.params.push_back (liste ("mbc_style", "Caractère", "Style", styles, 0, "Caractère des 4 compresseurs."));
        g0.params.push_back (continu ("mbc_x1", "Coupure basse", "Bas", 40, 1000, 120, "Hz", "Entre les bandes 1 et 2.", 200));
        g0.params.push_back (continu ("mbc_x2", "Coupure médium", "Méd", 200, 5000, 1000, "Hz", "Entre les bandes 2 et 3.", 1000));
        g0.params.push_back (continu ("mbc_x3", "Coupure haute", "Haut", 1000, 16000, 6000, "Hz", "Entre les bandes 3 et 4.", 4000));
        m.groupes.push_back (g0);
        for (int i = 1; i <= 4; ++i)
        {
            DescGroupe g { "Bande " + juce::String (i), {} };
            g.params.push_back (continu (num ("mbc", i, "seuil"), "Seuil", "Seuil", -60, 0, -18, "dB", "Niveau à partir duquel la bande compresse."));
            g.params.push_back (continu (num ("mbc", i, "ratio"), "Ratio", "Ratio", 1, 20, 2, ":1", "Force de la compression.", 4));
            g.params.push_back (continu (num ("mbc", i, "attaque"), "Attaque", "Att", 0.05f, 200, i == 1 ? 20.0f : 8.0f, "ms", "Vitesse de réaction.", 10));
            g.params.push_back (continu (num ("mbc", i, "relache"), "Relâche", "Rel", 5, 2000, i == 1 ? 250.0f : 120.0f, "ms", "Vitesse de retour.", 150));
            g.params.push_back (continu (num ("mbc", i, "gain"), "Gain", "Gain", -12, 12, 0, "dB", "Volume de la bande."));
            m.groupes.push_back (g);
        }
        mods.push_back (m);
    }

    // --- Resonances
    {
        DescModule m { "res", T ("Suppresseur de résonances"), T ("RÉSO"), T ("Repère les fréquences qui sonnent trop et les calme, seulement quand elles ressortent."), {} };
        DescGroupe g1 { T ("Action"), {} };
        g1.params.push_back (continu ("res_profondeur", "Profondeur", "Prof", 0, 100, 50, "%", "Quantité de réduction."));
        g1.params.push_back (continu ("res_nettete", "Netteté", "Net", 0, 100, 50, "%", "0 = corrections larges et douces, 100 = chirurgicales."));
        g1.params.push_back (continu ("res_selectivite", "Sélectivité", "Sél", 0, 100, 50, "%", "0 = réagit au moindre relief, 100 = seulement aux pics marqués."));
        DescGroupe g2 { T ("Temps"), {} };
        g2.params.push_back (continu ("res_attaque", "Attaque", "Att", 0.5f, 100, 5, "ms", "Vitesse de réaction.", 8));
        g2.params.push_back (continu ("res_relache", "Relâche", "Rel", 5, 500, 80, "ms", "Vitesse de retour.", 80));
        g2.params.push_back (interrupteur ("res_delta", "Écoute delta", "Delta", false, "N'écoute que ce qui est retiré, pour régler à l'oreille."));
        DescGroupe g3 { T ("Zone"), {} };
        g3.params.push_back (continu ("res_fbas", "Fréquence basse", "Bas", 20, 20000, 200, "Hz", "Rien n'est touché en dessous.", 632));
        g3.params.push_back (continu ("res_fhaut", "Fréquence haute", "Haut", 20, 20000, 16000, "Hz", "Rien n'est touché au-dessus.", 632));
        g3.params.push_back (continu ("res_inclinaison", "Inclinaison", "Incl", -100, 100, 0, "%", "Positif : travaille plus dans les aigus. Négatif : plus dans les bas."));
        m.groupes = { g1, g2, g3 };
        mods.push_back (m);
    }

    // --- Exciter
    {
        DescModule m { "exc", "Exciter multibande", "EXCIT", T ("Ajoute des harmoniques par bande : lampe, bande, transistor ou douce."), {} };
        DescGroupe g0 { T ("Coupures"), {} };
        g0.params.push_back (continu ("exc_x1", "Coupure basse", "Bas", 50, 2000, 250, "Hz", "Entre les bandes 1 et 2.", 300));
        g0.params.push_back (continu ("exc_x2", "Coupure haute", "Haut", 1000, 16000, 4000, "Hz", "Entre les bandes 2 et 3.", 4000));
        m.groupes.push_back (g0);
        const float defDose[] = { 0, 15, 30 };
        for (int i = 1; i <= 3; ++i)
        {
            DescGroupe g { "Bande " + juce::String (i), {} };
            g.params.push_back (liste (num ("exc", i, "couleur"), "Couleur", "Couleur", { "Lampe", "Bande", "Transistor", "Douce" }, i == 3 ? 2 : 0,
                                       "Lampe : chaleur (harmoniques paires). Bande : rond. Transistor : brillant. Douce : discret."));
            g.params.push_back (continu (num ("exc", i, "drive"), "Drive", "Drive", 0, 24, 6, "dB", "Plus fort = plus d'harmoniques."));
            g.params.push_back (continu (num ("exc", i, "dose"), "Dose", "Dose", 0, 100, defDose[i - 1], "%", "Quantité d'harmoniques ajoutées."));
            m.groupes.push_back (g);
        }
        mods.push_back (m);
    }

    // --- Clipper
    {
        DescModule m { "clip", "Clipper", "CLIP", T ("Coupe les crêtes, suréchantillonné x4."), {} };
        DescGroupe g { T ("Clipper"), {} };
        g.params.push_back (continu ("clip_entree", "Entrée", "Entrée", 0, 18, 0, "dB", "Pousse le signal dans le clipper."));
        g.params.push_back (continu ("clip_plafond", "Plafond", "Plafond", -12, 0, -0.3f, "dBFS", "Niveau où les crêtes sont coupées."));
        g.params.push_back (continu ("clip_douceur", "Douceur", "Douceur", 0, 100, 30, "%", "0 = coupe nette, 100 = arrondie."));
        g.params.push_back (continu ("clip_sortie", "Sortie", "Sortie", -12, 6, 0, "dB", "Volume après le clipper."));
        m.groupes = { g };
        mods.push_back (m);
    }

    // --- Limiteur
    {
        DescModule m { "lim", "Limiteur", "LIMIT", T ("Limiteur à anticipation, crêtes inter-échantillons."), {} };
        DescGroupe g1 { T ("Niveau"), {} };
        g1.params.push_back (continu ("lim_gain", "Gain", "Gain", 0, 24, 0, "dB", "Pousse le signal dans le limiteur (le volume monte)."));
        g1.params.push_back (continu ("lim_plafond", "Plafond", "Plafond", -12, 0, -1, "dBFS", "Niveau maximal en sortie. -1 dBFS conseillé pour le streaming."));
        DescGroupe g2 { T ("Caractère"), {} };
        g2.params.push_back (continu ("lim_relache", "Relâche", "Rel", 1, 1000, 120, "ms", "Vitesse de retour.", 100));
        g2.params.push_back (liste ("lim_style", "Caractère", "Style", { "Transparent", "Fort" }, 0, "Transparent : retour en deux vitesses, moins de pompage. Fort : plus dense."));
        g2.params.push_back (interrupteur ("lim_cretes", "Crêtes vraies", "TP", true, "Surveille aussi les crêtes entre les échantillons (conseillé)."));
        m.groupes = { g1, g2 };
        mods.push_back (m);
    }
    return mods;
}
} // namespace

const std::vector<DescModule>& modules()
{
    static const auto m = construit();
    return m;
}

const std::vector<DescParam>& globaux()
{
    static const std::vector<DescParam> g {
        continu ("entree", "Entrée", "Entrée", -24, 24, 0, "dB", "Volume avant la chaîne."),
        continu ("sortie", "Sortie", "Sortie", -24, 24, 0, "dB", "Volume après la chaîne (un limiteur de sécurité garde la sortie sous 0 dBFS)."),
    };
    return g;
}

juce::String nomPartie (int mode, bool b)
{
    switch (mode)
    {
        case 1: return b ? "Side" : "Mid";
        case 2: return b ? "Droite" : "Gauche";
        case 3: return b ? "Tonal" : "Transitoires";
        default: return b ? "" : "Dose";
    }
}

namespace
{
// Interrupteur = liste a deux choix, pas AudioParameterBool : ce dernier garde la valeur brute envoyee
// par l'hote (0,37 par ex.) mais n'en sauvegarde que 0 ou 1, et l'etat recharge differe (test "etat").
std::unique_ptr<juce::RangedAudioParameter> interrupteurJuce (const juce::ParameterID& pid, const juce::String& nom, bool def)
{
    return std::make_unique<juce::AudioParameterChoice> (pid, nom, juce::StringArray { T ("Éteint"), T ("Allumé") }, def ? 1 : 0);
}

std::unique_ptr<juce::RangedAudioParameter> fabrique (const DescParam& d)
{
    const juce::ParameterID pid { d.id, 1 };
    if (d.genre == Genre::interrupteur)
        return interrupteurJuce (pid, d.nom, d.defaut > 0.5f);
    if (d.genre == Genre::liste)
        return std::make_unique<juce::AudioParameterChoice> (pid, d.nom, d.choix, static_cast<int> (d.defaut));

    juce::NormalisableRange<float> plage (d.min, d.max, 0.0f);
    if (d.centre > 0.0f)
        plage.setSkewForCentre (d.centre);
    const auto unite = d.unite;
    const bool hz = unite == "Hz";
    auto texte = [unite, hz] (float v, int) {
        if (hz)
            return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
        const int dec = std::abs (v) >= 100.0f ? 0 : (std::abs (v) >= 10.0f ? 1 : 2);
        return juce::String (v, dec) + (unite.isEmpty() ? "" : " " + unite);
    };
    return std::make_unique<juce::AudioParameterFloat> (pid, d.nom, plage, d.defaut,
                                                        juce::AudioParameterFloatAttributes().withLabel (d.unite).withStringFromValueFunction (texte));
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout creeLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& g : globaux())
        layout.add (fabrique (g));

    const juce::StringArray modes { T ("Stéréo"), "Mid-Side", T ("Gauche-Droite"), T ("Transitoire-Tonal") };
    for (int m = 0; m < nbModules; ++m)
    {
        const auto& mod = modules()[static_cast<size_t> (m)];
        const juce::ParameterID idA { idActif (m), 1 }, idM { idMode (m), 1 }, idDa { idDoseA (m), 1 }, idDb { idDoseB (m), 1 };
        layout.add (interrupteurJuce (idA, mod.nom + " : actif", m == mLimiteur));
        layout.add (std::make_unique<juce::AudioParameterChoice> (idM, mod.nom + " : mode", modes, 0));
        auto pct = juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; });
        layout.add (std::make_unique<juce::AudioParameterFloat> (idDa, mod.nom + T (" : dose A"), juce::NormalisableRange<float> (0, 100), 100.0f, pct));
        layout.add (std::make_unique<juce::AudioParameterFloat> (idDb, mod.nom + T (" : dose B"), juce::NormalisableRange<float> (0, 100), 100.0f, pct));
        for (const auto& g : mod.groupes)
            for (const auto& p : g.params)
                layout.add (fabrique (p));
    }
    return layout;
}
} // namespace master
