#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
// Monter ce numero a chaque changement de format de l'etat, et gerer l'ancien dans setStateInformation.
// v1 : une courbe, "vitesse". v2 : une courbe, "longueur". v3 : 8 onglets.
constexpr int stateVersion = 3;

// Le CC est reevalue tous les 32 echantillons (< 1 ms) et envoye seulement s'il change.
constexpr int pasEnvoi = 32;

uint32_t melanger (uint32_t a, uint64_t b) noexcept
{
    uint64_t x = (static_cast<uint64_t> (a) << 32) ^ (b * 0x9E3779B97F4A7C15ULL);
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    return static_cast<uint32_t> (x);
}

juce::String courbeVersTexte (const courbes::Courbe& c)
{
    juce::StringArray morceaux;
    for (int i = 0; i < c.taille(); ++i)
    {
        const auto& p = c.point (i);
        morceaux.add (juce::String (p.x, 5) + " " + juce::String (p.y, 5) + " " + juce::String (p.tension, 4) + " "
                      + juce::String (static_cast<int> (p.type)));
    }
    return morceaux.joinIntoString (";");
}

bool texteVersCourbe (const juce::String& texte, courbes::Courbe& c)
{
    c.vider();
    for (const auto& morceau : juce::StringArray::fromTokens (texte, ";", ""))
    {
        const auto v = juce::StringArray::fromTokens (morceau, " ", "");
        if (v.size() < 4)
            continue;
        c.ajouter (v[0].getFloatValue(), v[1].getFloatValue(), v[2].getFloatValue(),
                   static_cast<courbes::Segment> (juce::jlimit (0, 2, v[3].getIntValue())));
    }
    c.trier();
    return c.taille() >= 2;
}

juce::String reglageVersTexte (const courbes::Reglage& r)
{
    return juce::String (r.v[0], 4) + " " + juce::String (r.v[1], 4) + " " + juce::String (r.v[2], 4) + " " + juce::String (r.v[3], 4);
}

courbes::Reglage texteVersReglage (const juce::String& t, uint32_t graine)
{
    courbes::Reglage r;
    r.graine = graine;
    const auto v = juce::StringArray::fromTokens (t, " ", "");
    for (int i = 0; i < courbes::nombreReglages; ++i)
    {
        const float x = i < v.size() ? v[i].getFloatValue() : 0.5f;
        r.v[static_cast<size_t> (i)] = std::isfinite (x) ? juce::jlimit (0.0f, 1.0f, x) : 0.5f;
    }
    return r;
}

float defautEvo (int champ)
{
    switch (champ)
    {
        case evo::garder: return 50.0f;
        case evo::revenir: return 10.0f;
        case evo::varier: return 30.0f;
        case evo::changer: return 10.0f;
        case evo::intensite: return 40.0f;
        case evo::cycles: return 1.0f;
        default: return 50.0f;
    }
}
} // namespace

//==============================================================================
const char* PluginProcessor::nomParametreOnglet (int champ)
{
    static const char* noms[nombreChamps] = { "mute", "solo", "longueur", "ternaire", "grille", "grille_ternaire", "phase", "cc", "canal",
                                              "min", "max", "lissage", "swing", "depart", "evolution", "lie", "coherence", "fondu", "verrou" };
    return noms[juce::jlimit (0, nombreChamps - 1, champ)];
}

juce::String PluginProcessor::id (int onglet, const char* base)
{
    return "o" + juce::String (onglet + 1) + "_" + base;
}

juce::RangedAudioParameter* PluginProcessor::param (int onglet, const char* base) const
{
    return apvts.getParameter (id (onglet, base));
}

float PluginProcessor::valeur (int onglet, const char* base) const
{
    if (auto* v = apvts.getRawParameterValue (id (onglet, base)))
        return v->load();
    return 0.0f;
}

juce::String PluginProcessor::idEvoCommun (int champ)
{
    switch (champ)
    {
        case evo::garder: return "p_garder";
        case evo::revenir: return "p_revenir";
        case evo::varier: return "p_varier";
        case evo::changer: return "p_changer";
        case evo::intensite: return "intensite";
        case evo::cycles: return "evol_cycles";
        default: return "poids_" + juce::String (champ - evo::poids0);
    }
}

const std::vector<Duree>& PluginProcessor::longueurs()
{
    static const std::vector<Duree> liste = {
        { "1/32", 0.125, 0, false },          { "1/16 T", 1.0 / 6.0, 0, true },     { "1/16", 0.25, 0, false },
        { "1/8 T", 1.0 / 3.0, 0, true },      { "1/8", 0.5, 0, false },             { "1/4 T", 2.0 / 3.0, 0, true },
        { "1/4", 1.0, 0, false },             { "1/2 T", 4.0 / 3.0, 0, true },      { "1/2", 2.0, 0, false },
        { "1 mesure T", 0, 2.0 / 3.0, true }, { "1 mesure", 0, 1, false },          { "2 mesures T", 0, 4.0 / 3.0, true },
        { "2 mesures", 0, 2, false },         { "4 mesures", 0, 4, false },         { "8 mesures", 0, 8, false },
        { "16 mesures", 0, 16, false },       { "32 mesures", 0, 32, false },
    };
    return liste;
}

const std::vector<std::pair<const char*, double>>& PluginProcessor::mesuresEvolution()
{
    static const std::vector<std::pair<const char*, double>> liste = {
        { "1/4 mesure", 0.25 }, { "1/2 mesure", 0.5 }, { "1 mesure", 1.0 },   { "2 mesures", 2.0 },   { "3 mesures", 3.0 },
        { "4 mesures", 4.0 },   { "6 mesures", 6.0 },  { "8 mesures", 8.0 },  { "12 mesures", 12.0 }, { "16 mesures", 16.0 },
        { "32 mesures", 32.0 },
    };
    return liste;
}

const std::vector<Duree>& PluginProcessor::grilles()
{
    static const std::vector<Duree> liste = {
        { "1/4", 1.0, 0, false }, { "1/8", 0.5, 0, false }, { "1/16", 0.25, 0, false }, { "1/32", 0.125, 0, false },
    };
    return liste;
}

int PluginProcessor::longueurJouee (int index, bool ternaireAutorise)
{
    const auto& l = longueurs();
    index = juce::jlimit (0, static_cast<int> (l.size()) - 1, index);
    if (! ternaireAutorise && l[static_cast<size_t> (index)].ternaire)
        ++index; // la valeur binaire juste au-dessus
    return juce::jlimit (0, static_cast<int> (l.size()) - 1, index);
}

double PluginProcessor::enNoires (const Duree& d, double npm)
{
    return d.mesures > 0.0 ? d.mesures * npm : d.noires;
}

double PluginProcessor::cycleNoires (int t, double npm) const
{
    const int index = longueurJouee (juce::roundToInt (lire (t, longueur)), lire (t, ternaire) > 0.5f);
    return juce::jmax (1.0e-3, enNoires (longueurs()[static_cast<size_t> (index)], npm));
}

double PluginProcessor::caseNoires (int t) const
{
    const auto& gl = grilles();
    const int g = juce::jlimit (0, static_cast<int> (gl.size()) - 1, juce::roundToInt (lire (t, grille)));
    return gl[static_cast<size_t> (g)].noires * (lire (t, grilleTernaire) > 0.5f ? 2.0 / 3.0 : 1.0);
}

//==============================================================================
PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ETAT", createLayout())
{
    for (int t = 0; t < nombreOnglets; ++t)
        for (int c = 0; c < nombreChamps; ++c)
            pOnglet[static_cast<size_t> (t)][static_cast<size_t> (c)] = apvts.getRawParameterValue (id (t, nomParametreOnglet (c)));
    for (int c = 0; c < evo::total; ++c)
        evoCommun[static_cast<size_t> (c)] = apvts.getRawParameterValue (idEvoCommun (c));
    pEcoute = apvts.getRawParameterValue ("ecoute_on");
    pRythme = apvts.getRawParameterValue ("evol_rythme");
    pSeedDepart = apvts.getRawParameterValue ("seed_depart");
    pMesures = apvts.getRawParameterValue ("evol_mesures");

    for (int t = 0; t < nombreOnglets; ++t)
    {
        for (int c = 0; c < evo::total; ++c)
            evoPropre[static_cast<size_t> (t)][static_cast<size_t> (c)] = defautEvo (c);

        // Chaque onglet part d'une famille differente : on voit tout de suite qu'ils sont independants.
        const int f = (t * 3) % courbes::nombreFamilles;
        auto& v = voix[static_cast<size_t> (t)];
        reglageInterface[static_cast<size_t> (t)] = courbes::membre (f, 0);
        courbes::generer (f, reglageInterface[static_cast<size_t> (t)], courbeInterface[static_cast<size_t> (t)]);
        courbePartagee[static_cast<size_t> (t)] = v.base = v.jouee = courbeInterface[static_cast<size_t> (t)];
        reglagePartage[static_cast<size_t> (t)] = v.rBase = v.rJouee = reglageInterface[static_cast<size_t> (t)];
        famillePartagee[static_cast<size_t> (t)] = v.fBase = v.fJouee = f;
        famille[static_cast<size_t> (t)] = f;
        versionCourbe[static_cast<size_t> (t)] = 1;
        lectureUI[static_cast<size_t> (t)].jouee = v.base;
        lectureUI[static_cast<size_t> (t)].familleJouee = f;
        lectureUI[static_cast<size_t> (t)].reglageJouee = v.rBase;
        seedInterface[static_cast<size_t> (t)] = { v.base, f, v.rBase, {} };
        seedPartagee[static_cast<size_t> (t)] = v.base;
        reglageSeedPartage[static_cast<size_t> (t)] = v.rBase;
        familleSeedPartagee[static_cast<size_t> (t)] = f;
        versionSeed[static_cast<size_t> (t)] = 1;
        visible[static_cast<size_t> (t)] = t == 0; // comme "actif" : seul l'onglet 1 au depart
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    using Float = juce::AudioParameterFloat;
    using Attr = juce::AudioParameterFloatAttributes;

    juce::StringArray nomsLongueurs, nomsGrilles;
    for (const auto& d : longueurs())
        nomsLongueurs.add (d.nom);
    for (const auto& d : grilles())
        nomsGrilles.add (d.nom);

    // Identifiants et numeros de version : NE CHANGENT JAMAIS apres une sortie (projets, automations).
    // Onglets : "o1_longueur", "o2_longueur"... (la v0.2 ne reprend pas les reglages de la v0.1).
    // Interrupteurs : une liste Non / Oui plutot qu'AudioParameterBool (piege note dans PIEGES.md).
    for (int t = 0; t < nombreOnglets; ++t)
    {
        const auto nom = [t] (const char* n) { return juce::String (t + 1) + " " + n; };
        const auto bascule = [&] (const char* base, const char* n, bool defaut)
        {
            layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id (t, base), 1 }, nom (n),
                                                                      juce::StringArray { "Non", "Oui" }, defaut ? 1 : 0));
        };
        const auto pourcent = [&] (const char* base, const char* n, float maxi, float defaut)
        {
            layout.add (std::make_unique<Float> (juce::ParameterID { id (t, base), 1 }, nom (n),
                                                 juce::NormalisableRange<float> (0.0f, maxi, 0.1f), defaut, Attr().withLabel ("%")));
        };
        bascule ("mute", "Mute", t != 0); // au depart, seul l'onglet 1 envoie son CC
        bascule ("solo", "Solo", false);
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id (t, "longueur"), 1 }, nom ("Longueur"),
                                                                  nomsLongueurs, longueurParDefaut));
        bascule ("ternaire", "Longueurs ternaires", false);
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id (t, "grille"), 1 }, nom ("Grille"), nomsGrilles, 2));
        bascule ("grille_ternaire", "Grille ternaire", false);
        pourcent ("phase", "Decalage", 100.0f, 0.0f);
        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id (t, "cc"), 1 }, nom ("Numero de CC"), 0, 127, 20 + t));
        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id (t, "canal"), 1 }, nom ("Canal MIDI"), 1, 16, 1));
        pourcent ("min", "Minimum", 100.0f, 0.0f);
        pourcent ("max", "Maximum", 100.0f, 100.0f);
        layout.add (std::make_unique<Float> (juce::ParameterID { id (t, "lissage"), 1 }, nom ("Lissage"),
                                             juce::NormalisableRange<float> (0.0f, 200.0f, 0.1f, 0.5f), 2.0f, Attr().withLabel ("ms")));
        pourcent ("swing", "Swing", 75.0f, 0.0f);
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id (t, "depart"), 1 }, nom ("Depart"),
                                                                  juce::StringArray { "Tempo", "Note" }, 0));
        bascule ("evolution", "Evolution", false);
        bascule ("lie", "Evolution liee", true);
        pourcent ("coherence", "Coherence", 100.0f, 70.0f);
        pourcent ("fondu", "Fondu", 200.0f, 50.0f); // duree du passage d'une courbe a la suivante, en % d'un cycle
        bascule ("verrou", "Cadenas", false);
    }

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "evol_rythme", 1 }, "Evolution : rythme",
                                                              juce::StringArray { "Chaque iteration", "Toutes les N mesures" }, 0));
    {
        juce::StringArray noms;
        for (const auto& m : mesuresEvolution())
            noms.add (m.first);
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "evol_mesures", 1 }, "Evolution : tous les", noms, 2));
    }
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "seed_depart", 1 }, "Repartir de la seed a chaque lecture",
                                                              juce::StringArray { "Non", "Oui" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "ecoute_on", 1 }, "Ecoute",
                                                              juce::StringArray { "Non", "Oui" }, 0));

    // Reglages d'evolution communs.
    for (int c = 0; c < evo::total; ++c)
    {
        const auto idc = idEvoCommun (c);
        if (c == evo::cycles)
        {
            layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { idc, 1 }, "Evolution tous les", 1, 16, 1));
            continue;
        }
        juce::String n;
        switch (c)
        {
            case evo::garder: n = "Garder"; break;
            case evo::revenir: n = "Revenir"; break;
            case evo::varier: n = "Varier"; break;
            case evo::changer: n = "Changer de famille"; break;
            case evo::intensite: n = "Intensite"; break;
            default: n = "Poids " + juce::String::fromUTF8 (courbes::nomFamille (c - evo::poids0)); break;
        }
        layout.add (std::make_unique<Float> (juce::ParameterID { idc, 1 }, n,
                                             juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), defautEvo (c), Attr().withLabel ("%")));
    }
    return layout;
}

//==============================================================================
float PluginProcessor::getEvo (int t, bool communs, int champ) const
{
    champ = juce::jlimit (0, evo::total - 1, champ);
    if (communs)
        return evoCommun[static_cast<size_t> (champ)]->load();
    return evoPropre[static_cast<size_t> (juce::jlimit (0, nombreOnglets - 1, t))][static_cast<size_t> (champ)].load();
}

void PluginProcessor::setEvo (int t, bool communs, int champ, float v)
{
    champ = juce::jlimit (0, evo::total - 1, champ);
    if (communs)
    {
        if (auto* p = apvts.getParameter (idEvoCommun (champ)))
            p->setValueNotifyingHost (p->convertTo0to1 (v));
        return;
    }
    const float maxi = champ == evo::cycles ? 16.0f : 100.0f;
    const float mini = champ == evo::cycles ? 1.0f : 0.0f;
    evoPropre[static_cast<size_t> (juce::jlimit (0, nombreOnglets - 1, t))][static_cast<size_t> (champ)] = juce::jlimit (mini, maxi, v);
}

bool PluginProcessor::getSolo (int t, bool communs, int f) const
{
    f = juce::jlimit (0, courbes::nombreFamilles - 1, f);
    return communs ? soloCommun[static_cast<size_t> (f)].load()
                   : soloPropre[static_cast<size_t> (juce::jlimit (0, nombreOnglets - 1, t))][static_cast<size_t> (f)].load();
}

void PluginProcessor::setSolo (int t, bool communs, int f, bool enSolo)
{
    f = juce::jlimit (0, courbes::nombreFamilles - 1, f);
    if (communs)
        soloCommun[static_cast<size_t> (f)] = enSolo;
    else
        soloPropre[static_cast<size_t> (juce::jlimit (0, nombreOnglets - 1, t))][static_cast<size_t> (f)] = enSolo;
}

courbes::Evolution PluginProcessor::lireEvolution (int t, bool communs, int& cycles) const noexcept
{
    const auto lit = [this, t, communs] (int c)
    {
        return communs ? evoCommun[static_cast<size_t> (c)]->load()
                       : evoPropre[static_cast<size_t> (t)][static_cast<size_t> (c)].load();
    };
    courbes::Evolution e;
    e.garder = lit (evo::garder);
    e.revenir = lit (evo::revenir);
    e.varier = lit (evo::varier);
    e.changer = lit (evo::changer);
    e.intensite = lit (evo::intensite) * 0.01f;
    bool unSolo = false;
    for (int f = 0; f < courbes::nombreFamilles; ++f)
        unSolo = unSolo || (communs ? soloCommun[static_cast<size_t> (f)].load() : soloPropre[static_cast<size_t> (t)][static_cast<size_t> (f)].load());
    for (int f = 0; f < courbes::nombreFamilles; ++f)
    {
        const bool enSolo = communs ? soloCommun[static_cast<size_t> (f)].load() : soloPropre[static_cast<size_t> (t)][static_cast<size_t> (f)].load();
        e.poids[static_cast<size_t> (f)] = unSolo ? (enSolo ? 1.0f : 0.0f) : lit (evo::poids0 + f);
    }
    cycles = juce::jmax (1, juce::roundToInt (lit (evo::cycles)));
    return e;
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    phaseTon = 0.0;
    ecoute.reset (sr, 20.0f);
    ton.reset (sr, 1.5f);
    ecoute.snap (pEcoute->load() > 0.5f ? 1.0f : 0.0f);
    ton.snap (0.0f);
    for (int t = 0; t < nombreOnglets; ++t)
    {
        auto& v = voix[static_cast<size_t> (t)];
        v.phaseLibre = v.positionNote = 0.0;
        v.sortie.reset (sr, lire (t, lissage));
        for (auto* s : { &v.decalage, &v.bas, &v.haut })
            s->reset (sr, 20.0f);
        v.decalage.snap (lire (t, phase) * 0.01f);
        v.bas.snap (lire (t, min) * 0.01f);
        v.haut.snap (lire (t, max) * 0.01f);
        v.prochaineValide = false;
        v.sortie.snap (v.bas.current() + (v.haut.current() - v.bas.current()) * v.jouee.valeur (v.decalage.current()));
        v.valeur = v.sortie.current();
        v.dernierCycle = std::numeric_limits<int64_t>::min();
        v.dernierePeriode = std::numeric_limits<int64_t>::min();
        v.iterations = v.changements = 0;
        v.fondu = 1.0;
        v.dernierCc = v.numero = v.canal = -1; // renvoyer la valeur au redemarrage
        v.compteurEnvoi = 0;
        v.aPublier = true;
    }
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

bool PluginProcessor::envoie (int t) const noexcept
{
    bool unSolo = false;
    for (int k = 0; k < nombreOnglets; ++k)
        unSolo = unSolo || lire (k, solo) > 0.5f;
    return unSolo ? lire (t, solo) > 0.5f : lire (t, mute) < 0.5f;
}

/** Tire la prochaine courbe de l'onglet. Elle est affichee tout de suite et jouee au prochain changement. */
void PluginProcessor::preparerProchaine (int t, double cycle, double npm, double temps, double positionMesures) noexcept
{
    auto& v = voix[static_cast<size_t> (t)];
    const bool lieAuxAutres = lire (t, lie) > 0.5f;
    int n = 1;
    const auto e = lireEvolution (t, lieAuxAutres, n);

    // Dans combien de temps tombe le prochain changement (pour l'affichage et le tirage commun).
    double noiresAvant = 0.0;
    if (pRythme->load() > 0.5f)
    {
        const auto& liste = mesuresEvolution();
        const int index = juce::jlimit (0, static_cast<int> (liste.size()) - 1, juce::roundToInt (pMesures->load()));
        const double bloc = liste[static_cast<size_t> (index)].second;
        const double reste = bloc - std::fmod (juce::jmax (0.0, positionMesures), bloc);
        noiresAvant = reste * npm;
        v.cyclesAvant = juce::jmax (1, static_cast<int> (std::ceil (noiresAvant / cycle - 1.0e-6)));
    }
    else
    {
        v.cyclesAvant = static_cast<int> (n - (v.iterations % n));
        noiresAvant = static_cast<double> (v.cyclesAvant) * cycle;
    }

    // Tirage propre a l'onglet : seme par la graine et le numero du changement (meme suite a chaque lecture
    // depuis la seed, quelle que soit la facon dont le DAW boucle).
    const uint32_t graine = graineEvolution.load (std::memory_order_relaxed);
    courbes::Hasard propre (melanger (graine ^ (0x51ED2700u + static_cast<uint32_t> (t) * 0x9E3779B9u), static_cast<uint64_t> (v.changements + 1)));
    auto issue = courbes::tirerIssue (e, propre);
    int f = -1;
    if (lieAuxAutres)
    {
        // Tirage commun, seme par la mesure (du temps joue) ou tombe le changement : les onglets lies decident
        // ensemble, meme avec des longueurs differentes. La coherence dit a quel point chacun le suit.
        const auto mesure = static_cast<int64_t> (std::floor ((temps + noiresAvant) / npm + 1.0e-6));
        courbes::Hasard commun (melanger (graine, static_cast<uint64_t> (mesure)));
        const auto issueCommune = courbes::tirerIssue (e, commun);
        const int familleCommune = courbes::tirerFamille (e, -1, commun);
        if (propre.suivant() * 100.0f < lire (t, coherence))
        {
            issue = issueCommune;
            f = familleCommune;
        }
    }
    if (issue == courbes::Issue::changer)
    {
        if (f < 0 || f == v.fJouee)
            f = courbes::tirerFamille (e, v.fJouee, propre);
        if (f < 0)
            issue = courbes::Issue::varier; // aucune autre famille autorisee
    }

    v.prochaineEstLaBase = false;
    switch (issue)
    {
        case courbes::Issue::garder:
            v.prochaine = v.jouee;
            v.rProchaine = v.rJouee;
            v.fProchaine = v.fJouee;
            v.prochaineEstLaBase = v.joueeEstLaBase;
            break;
        case courbes::Issue::revenir:
            v.prochaine = v.base;
            v.rProchaine = v.rBase;
            v.fProchaine = v.fBase;
            v.prochaineEstLaBase = true;
            break;
        case courbes::Issue::varier:
        case courbes::Issue::changer:
            // Une variante (meme famille) ou une autre famille. Si le tirage redonne exactement la courbe en
            // cours (familles tres "carrees"), on retire, avec de plus en plus d'ecart : "varier" doit varier.
            for (int essai = 0; essai < 6; ++essai)
            {
                const float ecart = juce::jmin (1.0f, e.intensite + 0.2f * static_cast<float> (essai));
                if (issue == courbes::Issue::varier)
                {
                    v.fProchaine = v.fJouee;
                    v.rProchaine = courbes::perturber (v.rJouee, juce::jmax (ecart, essai > 0 ? 0.3f : 0.0f), propre);
                }
                else
                {
                    v.fProchaine = f;
                    v.rProchaine = courbes::perturber (courbes::membre (f, courbes::choix (propre.suivant(), courbes::membresParFamille)), ecart, propre);
                }
                courbes::generer (v.fProchaine, v.rProchaine, v.prochaine);
                if (! v.prochaine.memeForme (v.jouee))
                    break;
            }
            break;
    }
    v.prochaineValide = true;
    v.aPublier = true;
}

void PluginProcessor::repartirDeLaSeed (int t) noexcept
{
    auto& v = voix[static_cast<size_t> (t)];
    v.jouee = v.base;
    v.rJouee = v.rBase;
    v.fJouee = v.fBase;
    v.joueeEstLaBase = true;
    v.prochaineValide = false;
    v.iterations = v.changements = 0;
    v.dernierePeriode = std::numeric_limits<int64_t>::min();
    v.fondu = 1.0;
    v.reprise = true;
    v.compteurEnvoi = 0;
    v.dernierCc = -1;
    v.aPublier = true;
}

/** Debut d'un nouveau cycle de la courbe (en avancant, en bouclant ou apres un saut). */
void PluginProcessor::surCycle (int t, double p, bool saut, double cycleN, double npm, double temps) noexcept
{
    auto& v = voix[static_cast<size_t> (t)];
    if (lire (t, evolution) < 0.5f || lire (t, verrou) > 0.5f)
    {
        // Evolution eteinte ou courbe figee (cadenas) : on garde la courbe en cours.
        if (v.prochaineValide)
        {
            v.prochaineValide = false;
            v.aPublier = true;
        }
        return;
    }

    // Est-ce un moment de changement ? (Jamais au premier cycle d'une lecture : la courbe en cours joue d'abord.)
    const bool premier = v.reprise;
    v.reprise = false;
    const double positionMesures = p * cycleN / npm; // ou on en est, en mesures du DAW
    bool changement = false;
    if (pRythme->load() > 0.5f)
    {
        // Toutes les N mesures : on change a chaque fois qu'on entre dans une nouvelle tranche de N mesures
        // (en avancant ou quand le DAW boucle).
        const auto& liste = mesuresEvolution();
        const int index = juce::jlimit (0, static_cast<int> (liste.size()) - 1, juce::roundToInt (pMesures->load()));
        const double bloc = liste[static_cast<size_t> (index)].second;
        const auto periode = static_cast<int64_t> (std::floor (positionMesures / bloc + 1.0e-6));
        // Nouvelle tranche, ou le DAW reboucle au debut d'une tranche (boucle de N mesures pile).
        const bool debutDeTranche = std::fmod (positionMesures + 1.0e-6, bloc) < 1.0e-3;
        changement = ! premier && v.dernierePeriode != std::numeric_limits<int64_t>::min()
                     && (periode != v.dernierePeriode || (saut && debutDeTranche));
        v.dernierePeriode = periode;
    }
    else
    {
        // Chaque iteration : tous les N cycles joues, quelle que soit la longueur.
        int n = 1;
        lireEvolution (t, lire (t, lie) > 0.5f, n);
        if (! premier)
            ++v.iterations;
        changement = ! premier && v.prochaineValide && v.iterations % n == 0;
    }

    if (changement && v.prochaineValide)
    {
        // La courbe tiree devient la courbe jouee, avec un fondu depuis l'ancienne.
        v.ancienne = v.jouee;
        v.fondu = lire (t, fondu) > 0.0f ? 0.0 : 1.0;
        v.jouee = v.prochaine;
        v.rJouee = v.rProchaine;
        v.fJouee = v.fProchaine;
        v.joueeEstLaBase = v.prochaineEstLaBase;
        ++v.changements;

        // Historique : la nouvelle courbe en tete.
        for (int i = Lecture::tailleHistorique - 1; i > 0; --i)
        {
            v.historique[static_cast<size_t> (i)] = v.historique[static_cast<size_t> (i - 1)];
            v.famillesHistorique[static_cast<size_t> (i)] = v.famillesHistorique[static_cast<size_t> (i - 1)];
            v.reglagesHistorique[static_cast<size_t> (i)] = v.reglagesHistorique[static_cast<size_t> (i - 1)];
        }
        v.historique[0] = v.jouee;
        v.famillesHistorique[0] = v.fJouee;
        v.reglagesHistorique[0] = v.rJouee;
        v.nbHistorique = juce::jmin (Lecture::tailleHistorique, v.nbHistorique + 1);
        v.prochaineValide = false;
    }
    if (! v.prochaineValide || changement)
        preparerProchaine (t, cycleN, npm, temps, positionMesures);
    else
    {
        // Pas de changement : on met juste a jour le compte a rebours affiche.
        v.cyclesAvant = juce::jmax (1, v.cyclesAvant - 1);
    }
    v.aPublier = true;
}

void PluginProcessor::publier (int t) noexcept
{
    const juce::SpinLock::ScopedTryLockType essai (verrouLecture);
    if (! essai.isLocked())
        return; // l'interface lit en ce moment : on reessaie au bloc suivant
    auto& v = voix[static_cast<size_t> (t)];
    auto& l = lectureUI[static_cast<size_t> (t)];
    l.jouee = v.jouee;
    l.familleJouee = v.fJouee;
    l.reglageJouee = v.rJouee;
    l.versionCourbe = v.versionAudio;
    l.joueeEstLaBase = v.joueeEstLaBase;
    l.prochaineValide = v.prochaineValide;
    l.cyclesAvant = v.cyclesAvant;
    if (v.prochaineValide)
    {
        l.prochaine = v.prochaine;
        l.familleProchaine = v.fProchaine;
    }
    l.historique = v.historique;
    l.famillesHistorique = v.famillesHistorique;
    l.reglagesHistorique = v.reglagesHistorique;
    l.nbHistorique = v.nbHistorique;
    v.aPublier = false;
    versionLecture[static_cast<size_t> (t)].fetch_add (1, std::memory_order_release);
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    // Notes recues (pour le depart "Note"), relevees avant d'ajouter nos CC.
    std::array<int, 64> notes {};
    int nbNotes = 0;
    for (const auto meta : midi)
        if (meta.getMessage().isNoteOn() && nbNotes < static_cast<int> (notes.size()))
            notes[static_cast<size_t> (nbNotes++)] = meta.samplePosition;

    // Position dans le temps : celle du DAW. A l'arret, le plugin ne joue pas.
    double bpm = 120.0, ppq = 0.0, npm = 4.0;
    bool joue = false;
    if (auto* head = getPlayHead())
    {
        if (const auto pos = head->getPosition())
        {
            if (const auto b = pos->getBpm())
                bpm = juce::jlimit (10.0, 999.0, *b);
            if (const auto s = pos->getTimeSignature(); s && s->numerator > 0 && s->denominator > 0)
                npm = 4.0 * static_cast<double> (s->numerator) / static_cast<double> (s->denominator);
            if (const auto q = pos->getPpqPosition(); q && pos->getIsPlaying())
            {
                ppq = *q;
                joue = std::isfinite (ppq);
            }
        }
    }
    noiresParMesure.store (npm, std::memory_order_relaxed);
    const int ongletEcoute = juce::jlimit (0, nombreOnglets - 1, ongletAffiche.load (std::memory_order_relaxed));
    ecoute.setTarget (pEcoute->load() > 0.5f ? 1.0f : 0.0f);

    // Debut de lecture : option "repartir de la seed a chaque lecture".
    if (joue && ! etaitEnLecture && pSeedDepart->load() > 0.5f)
    {
        tempsNoires = 0.0; // l'horloge commune repart aussi : meme suite a chaque lecture
        for (int t = 0; t < nombreOnglets; ++t)
            repartirDeLaSeed (t);
    }
    const bool parMesures = pRythme->load() > 0.5f;
    double blocMesures = 1.0;
    {
        const auto& liste = mesuresEvolution();
        blocMesures = liste[static_cast<size_t> (juce::jlimit (0, static_cast<int> (liste.size()) - 1, juce::roundToInt (pMesures->load())))].second;
    }
    etaitEnLecture = joue;
    const double noiresParEchantillon = bpm / 60.0 / sr;

    for (int t = 0; t < nombreOnglets; ++t)
    {
        auto& v = voix[static_cast<size_t> (t)];
        auto& version = versionCourbe[static_cast<size_t> (t)];
        const double cycle = cycleNoires (t, npm);

        // Courbe en cours modifiee dans l'editeur : elle joue tout de suite, l'evolution repart d'elle.
        if (v.versionAudio != version.load (std::memory_order_acquire))
        {
            const juce::SpinLock::ScopedTryLockType essai (verrouPartage);
            if (essai.isLocked())
            {
                v.jouee = courbePartagee[static_cast<size_t> (t)];
                v.rJouee = reglagePartage[static_cast<size_t> (t)];
                v.fJouee = famillePartagee[static_cast<size_t> (t)];
                v.joueeEstLaBase = false;
                v.fondu = 1.0;
                v.versionAudio = version.load (std::memory_order_relaxed);
                v.aPublier = true;
                if (lire (t, evolution) > 0.5f && lire (t, verrou) < 0.5f)
                    preparerProchaine (t, cycle, npm, tempsNoires,
                                       v.dernierCycle == std::numeric_limits<int64_t>::min() ? 0.0 : static_cast<double> (v.dernierCycle) * cycle / npm);
                else
                    v.prochaineValide = false;
            }
        }
        if (v.versionSeedAudio != versionSeed[static_cast<size_t> (t)].load (std::memory_order_acquire))
        {
            const juce::SpinLock::ScopedTryLockType essai (verrouPartage);
            if (essai.isLocked())
            {
                v.base = seedPartagee[static_cast<size_t> (t)];
                v.rBase = reglageSeedPartage[static_cast<size_t> (t)];
                v.fBase = familleSeedPartagee[static_cast<size_t> (t)];
                v.versionSeedAudio = versionSeed[static_cast<size_t> (t)].load (std::memory_order_relaxed);
            }
        }

        const bool envoi = envoie (t);
        const bool modeNote = lire (t, depart) > 0.5f;
        const double pas = noiresParEchantillon / cycle;
        const double pasFondu = lire (t, fondu) > 0.0f ? pas / (lire (t, fondu) * 0.01) : 1.0;
        const double cases = cycle / juce::jmax (1.0e-3, caseNoires (t));
        const float swingOnglet = lire (t, PluginProcessor::swing) * 0.01f;
        double position = modeNote ? v.positionNote : (joue ? ppq / cycle : v.phaseLibre);

        v.sortie.reset (sr, lire (t, lissage));
        v.decalage.setTarget (lire (t, phase) * 0.01f);
        v.bas.setTarget (lire (t, min) * 0.01f);
        v.haut.setTarget (lire (t, max) * 0.01f);

        const int numero = juce::jlimit (0, 127, juce::roundToInt (lire (t, cc)));
        const int canalMidi = juce::jlimit (1, 16, juce::roundToInt (lire (t, canal)));
        if (numero != v.numero || canalMidi != v.canal)
        {
            v.numero = numero;
            v.canal = canalMidi;
            v.dernierCc = -1; // nouveau CC ou canal : renvoyer la valeur tout de suite
            v.compteurEnvoi = 0;
        }

        // DAW a l'arret : rien ne joue (pas de CC, pas d'evolution, ecoute muette). Reprise a la lecture.
        if (! joue)
        {
            v.dernierCc = -1;
            v.dernierCycle = std::numeric_limits<int64_t>::min(); // la reprise compte comme un nouveau depart
            v.reprise = true;
            // La prochaine courbe s'affiche deja (evolution allumee pendant l'arret).
            const bool evolue = lire (t, evolution) > 0.5f && lire (t, verrou) < 0.5f;
            if (evolue && ! v.prochaineValide)
                preparerProchaine (t, cycle, npm, tempsNoires, 0.0);
            else if (! evolue && v.prochaineValide)
            {
                v.prochaineValide = false;
                v.aPublier = true;
            }
            if (v.aPublier)
                publier (t);
            ccAffiche[static_cast<size_t> (t)].store (-1, std::memory_order_relaxed);
            continue;
        }

        int prochaineNote = 0;
        float phaseCourbe = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            if (modeNote && prochaineNote < nbNotes && notes[static_cast<size_t> (prochaineNote)] <= i)
            {
                while (prochaineNote < nbNotes && notes[static_cast<size_t> (prochaineNote)] <= i)
                    ++prochaineNote;
                position = std::floor (position) + 1.0; // une note : la courbe repart du debut (nouveau cycle)
            }

            const double p = position + static_cast<double> (v.decalage.next());
            const auto numeroCycle = static_cast<int64_t> (std::floor (p));
            // Nouveau cycle : on passe au cycle suivant, ou le DAW saute / reboucle (meme dans le cycle en cours,
            // quand la boucle du DAW est plus courte que la courbe).
            const bool premierCycle = v.dernierCycle == std::numeric_limits<int64_t>::min();
            const bool enArriere = ! premierCycle && p < v.derniereP - 0.01;
            // Toutes les N mesures : on verifie aussi a chaque nouvelle tranche, meme au milieu d'une courbe longue.
            const bool nouvelleTranche = parMesures && ! premierCycle
                                         && static_cast<int64_t> (std::floor (p * cycle / npm / blocMesures + 1.0e-6)) != v.dernierePeriode;
            if (numeroCycle != v.dernierCycle || enArriere || nouvelleTranche)
            {
                const bool saut = ! premierCycle && numeroCycle != v.dernierCycle + 1;
                v.dernierCycle = numeroCycle;
                surCycle (t, p, saut, cycle, npm, tempsNoires + noiresParEchantillon * i);
            }
            v.derniereP = p;

            phaseCourbe = courbes::swinguer (static_cast<float> (p - std::floor (p)), cases, swingOnglet);
            float brut = v.jouee.valeur (phaseCourbe);
            if (v.fondu < 1.0)
            {
                // Fondu : on glisse de l'ancienne courbe vers la nouvelle (en douceur, courbe en S).
                const float w = static_cast<float> (v.fondu * v.fondu * (3.0 - 2.0 * v.fondu));
                brut = v.ancienne.valeur (phaseCourbe) + (brut - v.ancienne.valeur (phaseCourbe)) * w;
                v.fondu = juce::jmin (1.0, v.fondu + pasFondu);
            }
            const float lo = v.bas.next(), hi = v.haut.next();
            v.sortie.setTarget (lo + (hi - lo) * brut);
            v.valeur = v.sortie.next();
            if (! std::isfinite (v.valeur))
            {
                v.valeur = 0.0f;
                v.sortie.snap (0.0f);
            }

            if (envoi && --v.compteurEnvoi <= 0)
            {
                v.compteurEnvoi = pasEnvoi;
                const int valeurCc = juce::jlimit (0, 127, juce::roundToInt (v.valeur * 127.0f));
                if (valeurCc != v.dernierCc)
                {
                    midi.addEvent (juce::MidiMessage::controllerEvent (canalMidi, numero, valeurCc), i);
                    v.dernierCc = valeurCc;
                }
            }

            // Ecoute : un son test dont le volume suit la courbe de l'onglet affiche.
            if (t == ongletEcoute)
            {
                ton.setTarget (envoi ? v.valeur : 0.0f);
                const float volume = ton.next();
                const float e = ecoute.next();
                if (e > 0.0f)
                {
                    const float s = static_cast<float> (std::sin (juce::MathConstants<double>::twoPi * phaseTon)) * 0.3f * e * volume;
                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                        buffer.getWritePointer (ch)[i] = s;
                }
                phaseTon += 220.0 / sr;
                if (phaseTon >= 1.0)
                    phaseTon -= 1.0;
            }
            position += pas;
        }
        if (! envoi)
            v.dernierCc = -1; // a la reprise (dé-mute), renvoyer la valeur tout de suite

        if (modeNote)
            v.positionNote = position;
        else
            v.phaseLibre = position - std::floor (position);
        if (v.aPublier)
            publier (t);
        phaseAffichee[static_cast<size_t> (t)].store (phaseCourbe, std::memory_order_relaxed);
        valeurAffichee[static_cast<size_t> (t)].store (v.valeur, std::memory_order_relaxed);
        ccAffiche[static_cast<size_t> (t)].store (envoi ? v.dernierCc : -1, std::memory_order_relaxed);
    }
    if (joue)
        tempsNoires += noiresParEchantillon * numSamples;
}

//==============================================================================
courbes::Courbe PluginProcessor::getCourbe (int t) const
{
    const juce::ScopedLock sl (verrouInterface);
    return courbeInterface[static_cast<size_t> (juce::jlimit (0, nombreOnglets - 1, t))];
}

courbes::Reglage PluginProcessor::getReglage (int t) const
{
    const juce::ScopedLock sl (verrouInterface);
    return reglageInterface[static_cast<size_t> (juce::jlimit (0, nombreOnglets - 1, t))];
}

void PluginProcessor::setCourbe (int t, const courbes::Courbe& c, int f, int m, const courbes::Reglage& r)
{
    t = juce::jlimit (0, nombreOnglets - 1, t);
    courbes::Courbe propre = c;
    for (int i = 0; i < propre.taille(); ++i)
        propre.point (i).choisi = false;
    {
        const juce::ScopedLock sl (verrouInterface);
        courbeInterface[static_cast<size_t> (t)] = propre;
        reglageInterface[static_cast<size_t> (t)] = r;
    }
    {
        const juce::SpinLock::ScopedLockType sl (verrouPartage);
        courbePartagee[static_cast<size_t> (t)] = propre;
        reglagePartage[static_cast<size_t> (t)] = r;
        famillePartagee[static_cast<size_t> (t)] = f;
    }
    famille[static_cast<size_t> (t)] = f;
    membre[static_cast<size_t> (t)] = m;
    versionCourbe[static_cast<size_t> (t)].fetch_add (1, std::memory_order_release);
}

void PluginProcessor::getLecture (int t, Lecture& l) const
{
    const juce::SpinLock::ScopedLockType sl (verrouLecture);
    l = lectureUI[static_cast<size_t> (juce::jlimit (0, nombreOnglets - 1, t))];
}

PluginProcessor::Seed PluginProcessor::getSeed (int t) const
{
    const juce::ScopedLock sl (verrouInterface);
    return seedInterface[static_cast<size_t> (juce::jlimit (0, nombreOnglets - 1, t))];
}

void PluginProcessor::setSeed (int t, const Seed& s)
{
    t = juce::jlimit (0, nombreOnglets - 1, t);
    Seed propre = s;
    for (int i = 0; i < propre.courbe.taille(); ++i)
        propre.courbe.point (i).choisi = false;
    {
        const juce::ScopedLock sl (verrouInterface);
        seedInterface[static_cast<size_t> (t)] = propre;
    }
    {
        const juce::SpinLock::ScopedLockType sl (verrouPartage);
        seedPartagee[static_cast<size_t> (t)] = propre.courbe;
        reglageSeedPartage[static_cast<size_t> (t)] = propre.reglage;
        familleSeedPartagee[static_cast<size_t> (t)] = propre.famille;
    }
    versionSeed[static_cast<size_t> (t)].fetch_add (1, std::memory_order_release);
}

std::vector<PluginProcessor::Seed> PluginProcessor::getSeedsStockees (int t) const
{
    const juce::ScopedLock sl (verrouInterface);
    return seedsStockees[static_cast<size_t> (juce::jlimit (0, nombreOnglets - 1, t))];
}

void PluginProcessor::stockerSeed (int t, const Seed& s)
{
    const juce::ScopedLock sl (verrouInterface);
    auto& liste = seedsStockees[static_cast<size_t> (juce::jlimit (0, nombreOnglets - 1, t))];
    liste.push_back (s);
    if (liste.size() > 32)
        liste.erase (liste.begin());
}

void PluginProcessor::supprimerSeed (int t, int index)
{
    const juce::ScopedLock sl (verrouInterface);
    auto& liste = seedsStockees[static_cast<size_t> (juce::jlimit (0, nombreOnglets - 1, t))];
    if (index >= 0 && index < static_cast<int> (liste.size()))
        liste.erase (liste.begin() + index);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("version_etat", stateVersion, nullptr);
    state.setProperty ("largeur", editorWidth.load(), nullptr);
    state.setProperty ("hauteur", editorHeight.load(), nullptr);
    state.setProperty ("aimant", aimant.load(), nullptr);
    state.setProperty ("onglet_affiche", ongletAffiche.load(), nullptr);
    state.setProperty ("graine_evolution", static_cast<juce::int64> (graineEvolution.load()), nullptr);
    state.setProperty ("insectes", insectes.load(), nullptr);
    state.setProperty ("panneau_evolution", panneauEvolution.load(), nullptr);
    {
        juce::String solos;
        for (int f = 0; f < courbes::nombreFamilles; ++f)
            solos << (soloCommun[static_cast<size_t> (f)].load() ? "1" : "0");
        state.setProperty ("solos", solos, nullptr);
    }

    for (int i = state.getNumChildren(); --i >= 0;)
        if (state.getChild (i).hasType ("COURBE") || state.getChild (i).hasType ("EVOLUTION") || state.getChild (i).hasType ("SEED"))
            state.removeChild (i, nullptr);

    const auto seedVersArbre = [] (const Seed& s, int t, bool stockee)
    {
        juce::ValueTree a ("SEED");
        a.setProperty ("onglet", t, nullptr);
        a.setProperty ("stockee", stockee, nullptr);
        a.setProperty ("nom", s.nom, nullptr);
        a.setProperty ("points", courbeVersTexte (s.courbe), nullptr);
        a.setProperty ("famille", s.famille, nullptr);
        a.setProperty ("graine", static_cast<juce::int64> (s.reglage.graine), nullptr);
        a.setProperty ("reglages", reglageVersTexte (s.reglage), nullptr);
        return a;
    };
    for (int t = 0; t < nombreOnglets; ++t)
    {
        state.appendChild (seedVersArbre (getSeed (t), t, false), nullptr);
        for (const auto& s : getSeedsStockees (t))
            state.appendChild (seedVersArbre (s, t, true), nullptr);
    }

    for (int t = 0; t < nombreOnglets; ++t)
    {
        const auto r = getReglage (t);
        juce::ValueTree courbe ("COURBE");
        courbe.setProperty ("onglet", t, nullptr);
        courbe.setProperty ("points", courbeVersTexte (getCourbe (t)), nullptr);
        courbe.setProperty ("famille", getFamille (t), nullptr);
        courbe.setProperty ("membre", getMembre (t), nullptr);
        courbe.setProperty ("graine", static_cast<juce::int64> (r.graine), nullptr);
        courbe.setProperty ("reglages", reglageVersTexte (r), nullptr);
        state.appendChild (courbe, nullptr);

        juce::ValueTree e ("EVOLUTION");
        e.setProperty ("onglet", t, nullptr);
        juce::StringArray valeurs;
        for (int c = 0; c < evo::total; ++c)
            valeurs.add (juce::String (evoPropre[static_cast<size_t> (t)][static_cast<size_t> (c)].load(), 2));
        e.setProperty ("valeurs", valeurs.joinIntoString (" "), nullptr);
        juce::String solos;
        for (int f = 0; f < courbes::nombreFamilles; ++f)
            solos << (soloPropre[static_cast<size_t> (t)][static_cast<size_t> (f)].load() ? "1" : "0");
        e.setProperty ("solos", solos, nullptr);
        e.setProperty ("visible", visible[static_cast<size_t> (t)].load(), nullptr);
        state.appendChild (e, nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return; // etat illisible : on garde les reglages courants plutot que de planter

    auto state = juce::ValueTree::fromXml (*xml);
    // v1/v2 -> v3 : une seule courbe (enfant COURBE sans "onglet") = onglet 1 ; les autres onglets gardent
    // leur courbe par defaut. Les parametres de l'onglet 1 ont garde leurs identifiants.
    editorWidth = juce::jlimit (1200, 2400, static_cast<int> (state.getProperty ("largeur", 1400)));
    editorHeight = juce::jlimit (880, 1600, static_cast<int> (state.getProperty ("hauteur", 960)));
    aimant = static_cast<bool> (state.getProperty ("aimant", true));
    ongletAffiche = juce::jlimit (0, nombreOnglets - 1, static_cast<int> (state.getProperty ("onglet_affiche", 0)));
    graineEvolution = static_cast<uint32_t> (static_cast<juce::int64> (state.getProperty ("graine_evolution", 12345)));
    insectes = static_cast<bool> (state.getProperty ("insectes", true));
    panneauEvolution = static_cast<bool> (state.getProperty ("panneau_evolution", true));
    {
        const auto solos = state.getProperty ("solos", "").toString();
        for (int f = 0; f < courbes::nombreFamilles; ++f)
            soloCommun[static_cast<size_t> (f)] = solos[f] == '1';
    }
    {
        const juce::ScopedLock sl (verrouInterface);
        for (auto& l : seedsStockees)
            l.clear();
    }

    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        const auto enfant = state.getChild (i);
        const int t = juce::jlimit (0, nombreOnglets - 1, static_cast<int> (enfant.getProperty ("onglet", 0)));
        if (enfant.hasType ("COURBE"))
        {
            courbes::Courbe c;
            if (texteVersCourbe (enfant["points"].toString(), c))
                setCourbe (t, c,
                           juce::jlimit (0, courbes::nombreFamilles - 1, static_cast<int> (enfant.getProperty ("famille", 0))),
                           juce::jlimit (-1, courbes::membresParFamille - 1, static_cast<int> (enfant.getProperty ("membre", 0))),
                           texteVersReglage (enfant["reglages"].toString(),
                                             static_cast<uint32_t> (static_cast<juce::int64> (enfant.getProperty ("graine", 1)))));
        }
        else if (enfant.hasType ("SEED"))
        {
            Seed s;
            s.nom = enfant["nom"].toString();
            s.famille = juce::jlimit (0, courbes::nombreFamilles - 1, static_cast<int> (enfant.getProperty ("famille", 0)));
            s.reglage = texteVersReglage (enfant["reglages"].toString(), static_cast<uint32_t> (static_cast<juce::int64> (enfant.getProperty ("graine", 1))));
            if (texteVersCourbe (enfant["points"].toString(), s.courbe))
            {
                if (static_cast<bool> (enfant.getProperty ("stockee", false)))
                    stockerSeed (t, s);
                else
                    setSeed (t, s);
            }
        }
        else if (enfant.hasType ("EVOLUTION"))
        {
            const auto solos = enfant["solos"].toString();
            for (int f = 0; f < courbes::nombreFamilles; ++f)
                soloPropre[static_cast<size_t> (t)][static_cast<size_t> (f)] = solos[f] == '1';
            visible[static_cast<size_t> (t)] = static_cast<bool> (enfant.getProperty ("visible", t == 0));
            const auto v = juce::StringArray::fromTokens (enfant["valeurs"].toString(), " ", "");
            for (int c = 0; c < evo::total && c < v.size(); ++c)
            {
                const float x = v[c].getFloatValue();
                if (std::isfinite (x))
                    setEvo (t, false, c, x);
            }
        }
    }
    for (int i = state.getNumChildren(); --i >= 0;)
        if (state.getChild (i).hasType ("COURBE") || state.getChild (i).hasType ("EVOLUTION") || state.getChild (i).hasType ("SEED"))
            state.removeChild (i, nullptr);
    apvts.replaceState (state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
