#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
// Monter ce numero a chaque changement de format de l'etat, et gerer l'ancien dans setStateInformation.
constexpr int stateVersion = 2;

// Le CC est reevalue tous les 32 echantillons (< 1 ms) et envoye seulement s'il change.
constexpr int pasEnvoi = 32;

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
} // namespace

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

PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ETAT", createLayout())
{
    pLongueur = apvts.getRawParameterValue ("longueur");
    pTernaire = apvts.getRawParameterValue ("ternaire");
    pPhase = apvts.getRawParameterValue ("phase");
    pCc = apvts.getRawParameterValue ("cc");
    pCanal = apvts.getRawParameterValue ("canal");
    pMin = apvts.getRawParameterValue ("min");
    pMax = apvts.getRawParameterValue ("max");
    pLissage = apvts.getRawParameterValue ("lissage");
    pEcoute = apvts.getRawParameterValue ("ecoute_on");
    pEvolution = apvts.getRawParameterValue ("evolution");
    pEvolCycles = apvts.getRawParameterValue ("evol_cycles");
    pGarder = apvts.getRawParameterValue ("p_garder");
    pRevenir = apvts.getRawParameterValue ("p_revenir");
    pVarier = apvts.getRawParameterValue ("p_varier");
    pChanger = apvts.getRawParameterValue ("p_changer");
    pIntensite = apvts.getRawParameterValue ("intensite");
    for (int f = 0; f < courbes::nombreFamilles; ++f)
        pPoids[static_cast<size_t> (f)] = apvts.getRawParameterValue ("poids_" + juce::String (f));

    reglageInterface = courbes::membre (0, 0);
    courbes::generer (0, reglageInterface, courbeInterface);
    courbePartagee = courbeBase = courbeAudio = courbeJoueeUI = courbeInterface;
    reglagePartage = reglageBase = reglageAudio = reglageInterface;
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    using Float = juce::AudioParameterFloat;
    using Attr = juce::AudioParameterFloatAttributes;

    // Interrupteurs : une liste Non / Oui plutot qu'AudioParameterBool, qui garde telle quelle une valeur
    // intermediaire (0.37) envoyee par l'hote et ne la retrouve pas apres sauvegarde (piege note dans PIEGES.md).
    auto bascule = [] (const char* id, const char* nom, bool defaut)
    { return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, nom, juce::StringArray { "Non", "Oui" }, defaut ? 1 : 0); };

    juce::StringArray nomsLongueurs, nomsGrilles;
    for (const auto& d : longueurs())
        nomsLongueurs.add (d.nom);
    for (const auto& d : grilles())
        nomsGrilles.add (d.nom);

    // Identifiants et numeros de version : NE CHANGENT JAMAIS apres une sortie (projets, automations).
    // ("vitesse" et "ecoute" de la v0.1 sont abandonnes : ne jamais les reutiliser.)
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "longueur", 1 }, "Longueur",
                                                              nomsLongueurs, longueurParDefaut));
    layout.add (bascule ("ternaire", "Longueurs ternaires", true));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "grille", 1 }, "Grille", nomsGrilles, 2));
    layout.add (bascule ("grille_ternaire", "Grille ternaire", false));
    layout.add (std::make_unique<Float> (juce::ParameterID { "phase", 1 }, "Decalage",
                                         juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, Attr().withLabel ("%")));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "cc", 1 }, "Numero de CC", 0, 127, 20));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "canal", 1 }, "Canal MIDI", 1, 16, 1));
    layout.add (std::make_unique<Float> (juce::ParameterID { "min", 1 }, "Minimum",
                                         juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, Attr().withLabel ("%")));
    layout.add (std::make_unique<Float> (juce::ParameterID { "max", 1 }, "Maximum",
                                         juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, Attr().withLabel ("%")));
    layout.add (std::make_unique<Float> (juce::ParameterID { "lissage", 1 }, "Lissage",
                                         juce::NormalisableRange<float> (0.0f, 200.0f, 0.1f, 0.5f), 2.0f, Attr().withLabel ("ms")));
    layout.add (bascule ("ecoute_on", "Ecoute", false));

    // Evolution : a chaque cycle (ou tous les N cycles), tirage au sort de ce que devient la courbe.
    auto pourcent = [&layout] (const char* id, const char* nom, float defaut)
    {
        layout.add (std::make_unique<Float> (juce::ParameterID { id, 1 }, nom,
                                             juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), defaut, Attr().withLabel ("%")));
    };
    layout.add (bascule ("evolution", "Evolution", false));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "evol_cycles", 1 }, "Evolution tous les", 1, 16, 1));
    pourcent ("p_garder", "Garder", 50.0f);
    pourcent ("p_revenir", "Revenir", 10.0f);
    pourcent ("p_varier", "Varier", 30.0f);
    pourcent ("p_changer", "Changer de famille", 10.0f);
    pourcent ("intensite", "Intensite", 40.0f);
    for (int f = 0; f < courbes::nombreFamilles; ++f)
    {
        const auto id = "poids_" + juce::String (f);
        const auto nom = "Poids " + juce::String::fromUTF8 (courbes::nomFamille (f));
        layout.add (std::make_unique<Float> (juce::ParameterID { id, 1 }, nom,
                                             juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, Attr().withLabel ("%")));
    }
    return layout;
}

void PluginProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    phaseLibre = 0.0;
    phaseTon = 0.0;
    sortie.reset (sr, pLissage->load());
    ecoute.reset (sr, 20.0f);
    decalage.reset (sr, 20.0f);
    bas.reset (sr, 20.0f);
    haut.reset (sr, 20.0f);
    ton.reset (sr, 1.5f);
    ecoute.snap (pEcoute->load() > 0.5f ? 1.0f : 0.0f);
    decalage.snap (pPhase->load() * 0.01f);
    bas.snap (pMin->load() * 0.01f);
    haut.snap (pMax->load() * 0.01f);
    sortie.snap (bas.current() + (haut.current() - bas.current()) * courbeAudio.valeur (decalage.current()));
    ton.snap (sortie.current());
    dernierCc = dernierNumero = dernierCanal = -1; // renvoyer la valeur au redemarrage
    dernierCycle = std::numeric_limits<int64_t>::min();
    compteurCycles = 0;
    compteurEnvoi = 0;
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    // Nouvelle courbe envoyee par l'interface ? On la prend si le verrou est libre, sinon au bloc suivant.
    if (versionAudio != versionCourbe.load (std::memory_order_acquire))
    {
        const juce::SpinLock::ScopedTryLockType essai (verrouPartage);
        if (essai.isLocked())
        {
            courbeBase = courbeAudio = courbePartagee;
            reglageBase = reglageAudio = reglagePartage;
            familleBase = familleAudio = famillePartagee;
            versionAudio = versionCourbe.load (std::memory_order_relaxed);
            compteurCycles = 0;
            joueeChangee = true;
        }
    }
    const bool evolution = pEvolution->load() > 0.5f;
    if (! evolution && joueeDifferente)
    {
        courbeAudio = courbeBase;
        reglageAudio = reglageBase;
        familleAudio = familleBase;
        joueeDifferente = false;
        joueeChangee = true;
    }

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
    const int index = longueurJouee (juce::roundToInt (pLongueur->load()), pTernaire->load() > 0.5f);
    const double cycle = juce::jmax (1.0e-3, enNoires (longueurs()[static_cast<size_t> (index)], npm));
    const double pasParEchantillon = bpm / 60.0 / sr / cycle;
    double position = joue ? ppq / cycle : phaseLibre;

    sortie.reset (sr, pLissage->load());
    ecoute.setTarget (pEcoute->load() > 0.5f ? 1.0f : 0.0f);
    decalage.setTarget (pPhase->load() * 0.01f);
    bas.setTarget (pMin->load() * 0.01f);
    haut.setTarget (pMax->load() * 0.01f);

    const int numero = juce::jlimit (0, 127, juce::roundToInt (pCc->load()));
    const int canal = juce::jlimit (1, 16, juce::roundToInt (pCanal->load()));
    if (numero != dernierNumero || canal != dernierCanal)
    {
        dernierNumero = numero;
        dernierCanal = canal;
        dernierCc = -1; // nouveau CC ou canal : renvoyer la valeur tout de suite
        compteurEnvoi = 0;
    }

    const double pasTon = 220.0 / sr;
    const int numChannels = buffer.getNumChannels();
    float valeur = sortie.current();
    for (int i = 0; i < numSamples; ++i)
    {
        const double p = position + static_cast<double> (decalage.next());
        const auto numeroCycle = static_cast<int64_t> (std::floor (p));
        if (numeroCycle != dernierCycle)
        {
            if (evolution)
            {
                if (dernierCycle != std::numeric_limits<int64_t>::min() && numeroCycle == dernierCycle + 1)
                {
                    if (++compteurCycles >= juce::jmax (1, juce::roundToInt (pEvolCycles->load())))
                    {
                        compteurCycles = 0;
                        evoluer (numeroCycle);
                    }
                }
                else if (joueeDifferente)
                {
                    // Saut dans le temps (lecture relancee ailleurs) : on repart de la courbe de base,
                    // pour que chaque lecture depuis le meme endroit donne la meme suite.
                    courbeAudio = courbeBase;
                    reglageAudio = reglageBase;
                    familleAudio = familleBase;
                    joueeDifferente = false;
                    joueeChangee = true;
                    compteurCycles = 0;
                }
            }
            dernierCycle = numeroCycle;
        }
        const float brut = courbeAudio.valeur (static_cast<float> (p - std::floor (p)));
        const float lo = bas.next(), hi = haut.next();
        sortie.setTarget (lo + (hi - lo) * brut);
        valeur = sortie.next();
        if (! std::isfinite (valeur))
        {
            valeur = 0.0f;
            sortie.snap (0.0f);
        }

        if (--compteurEnvoi <= 0)
        {
            compteurEnvoi = pasEnvoi;
            const int cc = juce::jlimit (0, 127, juce::roundToInt (valeur * 127.0f));
            if (cc != dernierCc)
            {
                midi.addEvent (juce::MidiMessage::controllerEvent (canal, numero, cc), i);
                dernierCc = cc;
            }
        }

        // Ecoute : un son test dont le volume suit la courbe (eteint par defaut : le plugin reste muet).
        ton.setTarget (valeur);
        const float volume = ton.next();
        const float e = ecoute.next();
        if (e > 0.0f)
        {
            const float s = static_cast<float> (std::sin (juce::MathConstants<double>::twoPi * phaseTon)) * 0.3f * e * volume;
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.getWritePointer (ch)[i] = s;
        }
        phaseTon += pasTon;
        if (phaseTon >= 1.0)
            phaseTon -= 1.0;
        position += pasParEchantillon;
    }

    if (joueeChangee)
        publierJouee();
    phaseLibre = position - std::floor (position);
    const double affiche = position + static_cast<double> (decalage.current());
    phaseAffichee.store (static_cast<float> (affiche - std::floor (affiche)), std::memory_order_relaxed);
    valeurAffichee.store (valeur, std::memory_order_relaxed);
    ccAffiche.store (dernierCc, std::memory_order_relaxed);
}

void PluginProcessor::evoluer (int64_t cycle) noexcept
{
    courbes::Evolution e;
    e.garder = pGarder->load();
    e.revenir = pRevenir->load();
    e.varier = pVarier->load();
    e.changer = pChanger->load();
    e.intensite = pIntensite->load() * 0.01f;
    for (int f = 0; f < courbes::nombreFamilles; ++f)
        e.poids[static_cast<size_t> (f)] = pPoids[static_cast<size_t> (f)]->load();

    // Tirage seme par la position : meme morceau, meme endroit, meme suite (l'export sonne comme l'ecoute).
    courbes::Hasard h (graineEvolution.load (std::memory_order_relaxed) ^ static_cast<uint32_t> (static_cast<uint64_t> (cycle) * 2654435761ULL));
    auto issue = courbes::tirerIssue (e, h);
    int f = familleAudio;
    if (issue == courbes::Issue::changer)
    {
        f = courbes::tirerFamille (e, familleAudio, h);
        if (f < 0)
            issue = courbes::Issue::varier; // aucune autre famille autorisee
    }

    switch (issue)
    {
        case courbes::Issue::garder: return;
        case courbes::Issue::revenir:
            courbeAudio = courbeBase;
            reglageAudio = reglageBase;
            familleAudio = familleBase;
            joueeDifferente = false;
            break;
        case courbes::Issue::varier:
            reglageAudio = courbes::perturber (reglageAudio, e.intensite, h);
            courbes::generer (familleAudio, reglageAudio, courbeAudio);
            joueeDifferente = true;
            break;
        case courbes::Issue::changer:
            familleAudio = f;
            reglageAudio = courbes::perturber (courbes::membre (f, courbes::choix (h.suivant(), courbes::membresParFamille)), e.intensite, h);
            courbes::generer (familleAudio, reglageAudio, courbeAudio);
            joueeDifferente = true;
            break;
    }
    joueeChangee = true;
}

void PluginProcessor::publierJouee() noexcept
{
    const juce::SpinLock::ScopedTryLockType essai (verrouJoue);
    if (! essai.isLocked())
        return; // l'interface lit en ce moment : on reessaie au bloc suivant
    courbeJoueeUI = courbeAudio;
    familleJoueeUI = familleAudio;
    joueeChangee = false;
    versionJouee.fetch_add (1, std::memory_order_release);
}

void PluginProcessor::getCourbeJouee (courbes::Courbe& c, int& f) const
{
    const juce::SpinLock::ScopedLockType sl (verrouJoue);
    c = courbeJoueeUI;
    f = familleJoueeUI;
}

courbes::Courbe PluginProcessor::getCourbe() const
{
    const juce::ScopedLock sl (verrouInterface);
    return courbeInterface;
}

courbes::Reglage PluginProcessor::getReglage() const
{
    const juce::ScopedLock sl (verrouInterface);
    return reglageInterface;
}

void PluginProcessor::setCourbe (const courbes::Courbe& c, int f, int m, const courbes::Reglage& r)
{
    {
        const juce::ScopedLock sl (verrouInterface);
        courbeInterface = c;
        reglageInterface = r;
    }
    {
        const juce::SpinLock::ScopedLockType sl (verrouPartage);
        courbePartagee = c;
        reglagePartage = r;
        famillePartagee = f;
    }
    famille = f;
    membre = m;
    versionCourbe.fetch_add (1, std::memory_order_release);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("version_etat", stateVersion, nullptr);
    state.setProperty ("largeur", editorWidth.load(), nullptr);
    state.setProperty ("hauteur", editorHeight.load(), nullptr);
    state.setProperty ("aimant", aimant.load(), nullptr);
    state.setProperty ("graine_evolution", static_cast<juce::int64> (graineEvolution.load()), nullptr);

    state.removeChild (state.getChildWithName ("COURBE"), nullptr);
    const auto r = getReglage();
    juce::ValueTree courbe ("COURBE");
    courbe.setProperty ("points", courbeVersTexte (getCourbe()), nullptr);
    courbe.setProperty ("famille", famille.load(), nullptr);
    courbe.setProperty ("membre", membre.load(), nullptr);
    courbe.setProperty ("graine", static_cast<juce::int64> (r.graine), nullptr);
    courbe.setProperty ("reglages", juce::String (r.v[0], 4) + " " + juce::String (r.v[1], 4) + " "
                                        + juce::String (r.v[2], 4) + " " + juce::String (r.v[3], 4), nullptr);
    state.appendChild (courbe, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return; // etat illisible : on garde les reglages courants plutot que de planter

    auto state = juce::ValueTree::fromXml (*xml);
    // v1 -> v2 : "vitesse" et "ecoute" (potard) remplaces par "longueur" et "ecoute_on" ; valeurs par defaut.
    editorWidth = juce::jlimit (900, 2200, static_cast<int> (state.getProperty ("largeur", 1240)));
    editorHeight = juce::jlimit (620, 1500, static_cast<int> (state.getProperty ("hauteur", 780)));
    aimant = static_cast<bool> (state.getProperty ("aimant", true));
    graineEvolution = static_cast<uint32_t> (static_cast<juce::int64> (state.getProperty ("graine_evolution", 12345)));

    const auto courbe = state.getChildWithName ("COURBE");
    courbes::Courbe c;
    if (courbe.isValid() && texteVersCourbe (courbe["points"].toString(), c))
    {
        courbes::Reglage r;
        r.graine = static_cast<uint32_t> (static_cast<juce::int64> (courbe.getProperty ("graine", 1)));
        const auto v = juce::StringArray::fromTokens (courbe["reglages"].toString(), " ", "");
        for (int i = 0; i < courbes::nombreReglages; ++i)
        {
            const float x = i < v.size() ? v[i].getFloatValue() : 0.5f;
            r.v[static_cast<size_t> (i)] = std::isfinite (x) ? juce::jlimit (0.0f, 1.0f, x) : 0.5f;
        }
        setCourbe (c,
                   juce::jlimit (0, courbes::nombreFamilles - 1, static_cast<int> (courbe.getProperty ("famille", 0))),
                   juce::jlimit (-1, courbes::membresParFamille - 1, static_cast<int> (courbe.getProperty ("membre", 0))),
                   r);
    }
    state.removeChild (courbe, nullptr);
    apvts.replaceState (state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
