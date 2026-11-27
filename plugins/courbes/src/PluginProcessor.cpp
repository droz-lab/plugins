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
    static const char* noms[nombreChamps] = { "actif", "longueur", "ternaire", "grille", "grille_ternaire", "phase", "cc", "canal",
                                              "min", "max", "lissage", "swing", "depart", "evolution", "lie", "coherence" };
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
        { "16 mesures", 0, 16, false },
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
        bascule ("actif", "Actif", t == 0);
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id (t, "longueur"), 1 }, nom ("Longueur"),
                                                                  nomsLongueurs, longueurParDefaut));
        bascule ("ternaire", "Longueurs ternaires", true);
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
    }

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "evol_rythme", 1 }, "Evolution : rythme",
                                                              juce::StringArray { "Chaque iteration", "Toutes les N mesures" }, 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "evol_mesures", 1 }, "Evolution : mesures", 1, 16, 1));
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
    for (int f = 0; f < courbes::nombreFamilles; ++f)
        e.poids[static_cast<size_t> (f)] = lit (evo::poids0 + f);
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
        v.jouee = v.base;
        v.rJouee = v.rBase;
        v.fJouee = v.fBase;
        v.joueeEstLaBase = true;
        v.prochaineValide = false;
        v.sortie.snap (v.bas.current() + (v.haut.current() - v.bas.current()) * v.jouee.valeur (v.decalage.current()));
        v.valeur = v.sortie.current();
        v.dernierCycle = std::numeric_limits<int64_t>::min();
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

void PluginProcessor::preparerProchaine (int t, int64_t apres, double cycle, double npm) noexcept
{
    auto& v = voix[static_cast<size_t> (t)];
    const bool lieAuxAutres = lire (t, lie) > 0.5f;
    int n = 1;
    const auto e = lireEvolution (t, lieAuxAutres, n);
    int64_t m = 0;
    if (pRythme->load() > 0.5f)
    {
        // Toutes les N mesures, pour toutes les courbes : premier debut de cycle a partir de la prochaine
        // frontiere de N mesures.
        const double bloc = juce::jmax (1.0, static_cast<double> (std::round (pMesures->load()))) * npm;
        const double debutCycle = static_cast<double> (apres) * cycle;
        const double frontiere = (std::floor (debutCycle / bloc + 1.0e-9) + 1.0) * bloc;
        m = static_cast<int64_t> (std::ceil (frontiere / cycle - 1.0e-9));
        if (m <= apres)
            m = apres + 1;
    }
    else
        m = (apres >= 0 ? apres / n : (apres - n + 1) / n) * n + n; // prochain multiple de n apres `apres`
    v.cycleProchaine = m;

    // Tirage propre a l'onglet (seme par onglet et numero de cycle) : meme endroit, meme suite.
    const uint32_t graine = graineEvolution.load (std::memory_order_relaxed);
    courbes::Hasard propre (melanger (graine ^ (0x51ED2700u + static_cast<uint32_t> (t) * 0x9E3779B9u), static_cast<uint64_t> (m)));
    auto issue = courbes::tirerIssue (e, propre);
    int f = -1;
    if (lieAuxAutres)
    {
        // Tirage commun, seme par la mesure ou tombe le changement : les onglets lies decident ensemble,
        // meme avec des longueurs differentes. La coherence dit a quel point chacun suit cette decision.
        const auto mesure = static_cast<int64_t> (std::floor (static_cast<double> (m) * cycle / npm + 1.0e-9));
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
            v.fProchaine = v.fJouee;
            v.rProchaine = courbes::perturber (v.rJouee, e.intensite, propre);
            courbes::generer (v.fProchaine, v.rProchaine, v.prochaine);
            break;
        case courbes::Issue::changer:
            v.fProchaine = f;
            v.rProchaine = courbes::perturber (courbes::membre (f, courbes::choix (propre.suivant(), courbes::membresParFamille)), e.intensite, propre);
            courbes::generer (v.fProchaine, v.rProchaine, v.prochaine);
            break;
    }
    v.prochaineValide = true;
    v.aPublier = true;
}

void PluginProcessor::surCycle (int t, int64_t cycle, bool suite, double cycleN, double npm) noexcept
{
    auto& v = voix[static_cast<size_t> (t)];
    if (lire (t, evolution) < 0.5f)
    {
        if (! v.joueeEstLaBase || v.prochaineValide)
        {
            v.jouee = v.base;
            v.rJouee = v.rBase;
            v.fJouee = v.fBase;
            v.joueeEstLaBase = true;
            v.prochaineValide = false;
            v.aPublier = true;
        }
        return;
    }

    if (! suite)
    {
        // Debut de lecture ou saut dans le temps : on repart de la courbe de base,
        // pour que chaque lecture depuis le meme endroit donne la meme suite.
        v.jouee = v.base;
        v.rJouee = v.rBase;
        v.fJouee = v.fBase;
        v.joueeEstLaBase = true;
        v.prochaineValide = false;
    }
    else if (v.prochaineValide && cycle >= v.cycleProchaine)
    {
        v.jouee = v.prochaine;
        v.rJouee = v.rProchaine;
        v.fJouee = v.fProchaine;
        v.joueeEstLaBase = v.prochaineEstLaBase;
        v.prochaineValide = false;
    }
    if (! v.prochaineValide)
        preparerProchaine (t, cycle, cycleN, npm);
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
    l.joueeEstLaBase = v.joueeEstLaBase;
    l.prochaineValide = v.prochaineValide;
    if (v.prochaineValide)
    {
        l.prochaine = v.prochaine;
        l.familleProchaine = v.fProchaine;
        l.cyclesAvant = static_cast<int> (juce::jlimit<int64_t> (0, 1000, v.cycleProchaine - (v.dernierCycle == std::numeric_limits<int64_t>::min() ? 0 : v.dernierCycle)));
    }
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

    // Position dans le temps : celle du DAW quand il joue, sinon une horloge interne au meme tempo.
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

    for (int t = 0; t < nombreOnglets; ++t)
    {
        auto& v = voix[static_cast<size_t> (t)];
        auto& version = versionCourbe[static_cast<size_t> (t)];

        // Nouvelle courbe de base envoyee par l'interface ? On la prend si le verrou est libre.
        if (v.versionAudio != version.load (std::memory_order_acquire))
        {
            const juce::SpinLock::ScopedTryLockType essai (verrouPartage);
            if (essai.isLocked())
            {
                v.base = courbePartagee[static_cast<size_t> (t)];
                v.rBase = reglagePartage[static_cast<size_t> (t)];
                v.fBase = famillePartagee[static_cast<size_t> (t)];
                v.jouee = v.base;
                v.rJouee = v.rBase;
                v.fJouee = v.fBase;
                v.joueeEstLaBase = true;
                v.prochaineValide = false;
                v.versionAudio = version.load (std::memory_order_relaxed);
                v.aPublier = true;
                if (v.dernierCycle != std::numeric_limits<int64_t>::min() && lire (t, evolution) > 0.5f)
                    preparerProchaine (t, v.dernierCycle, cycleNoires (t, npm), npm);
            }
        }

        const bool actifOnglet = lire (t, actif) > 0.5f;
        const bool modeNote = lire (t, depart) > 0.5f;
        const double cycle = cycleNoires (t, npm);
        const double pas = bpm / 60.0 / sr / cycle;
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
            if (numeroCycle != v.dernierCycle)
            {
                const bool suite = v.dernierCycle != std::numeric_limits<int64_t>::min() && numeroCycle == v.dernierCycle + 1;
                v.dernierCycle = numeroCycle;
                surCycle (t, numeroCycle, suite, cycle, npm);
            }

            phaseCourbe = courbes::swinguer (static_cast<float> (p - std::floor (p)), cases, swingOnglet);
            const float brut = v.jouee.valeur (phaseCourbe);
            const float lo = v.bas.next(), hi = v.haut.next();
            v.sortie.setTarget (lo + (hi - lo) * brut);
            v.valeur = v.sortie.next();
            if (! std::isfinite (v.valeur))
            {
                v.valeur = 0.0f;
                v.sortie.snap (0.0f);
            }

            if (actifOnglet && --v.compteurEnvoi <= 0)
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
                ton.setTarget (actifOnglet ? v.valeur : 0.0f);
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

        if (modeNote)
            v.positionNote = position;
        else
            v.phaseLibre = position - std::floor (position);
        if (v.aPublier)
            publier (t);
        phaseAffichee[static_cast<size_t> (t)].store (phaseCourbe, std::memory_order_relaxed);
        valeurAffichee[static_cast<size_t> (t)].store (v.valeur, std::memory_order_relaxed);
        ccAffiche[static_cast<size_t> (t)].store (actifOnglet ? v.dernierCc : -1, std::memory_order_relaxed);
    }
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

    for (int i = state.getNumChildren(); --i >= 0;)
        if (state.getChild (i).hasType ("COURBE") || state.getChild (i).hasType ("EVOLUTION"))
            state.removeChild (i, nullptr);

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
    editorWidth = juce::jlimit (1100, 2400, static_cast<int> (state.getProperty ("largeur", 1320)));
    editorHeight = juce::jlimit (760, 1600, static_cast<int> (state.getProperty ("hauteur", 860)));
    aimant = static_cast<bool> (state.getProperty ("aimant", true));
    ongletAffiche = juce::jlimit (0, nombreOnglets - 1, static_cast<int> (state.getProperty ("onglet_affiche", 0)));
    graineEvolution = static_cast<uint32_t> (static_cast<juce::int64> (state.getProperty ("graine_evolution", 12345)));

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
        else if (enfant.hasType ("EVOLUTION"))
        {
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
        if (state.getChild (i).hasType ("COURBE") || state.getChild (i).hasType ("EVOLUTION"))
            state.removeChild (i, nullptr);
    apvts.replaceState (state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
