#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
// Monter ce numero a chaque changement de format de l'etat, et gerer l'ancien dans setStateInformation.
constexpr int stateVersion = 1;

// Duree d'un cycle, en noires. 0 = une mesure (depend de la signature rythmique).
struct Vitesse
{
    const char* nom;
    double noires;
    double mesures;
};

constexpr Vitesse vitesses[] = {
    { "1/32", 0.125, 0 }, { "1/16 T", 1.0 / 6.0, 0 }, { "1/16", 0.25, 0 }, { "1/8 T", 1.0 / 3.0, 0 },
    { "1/8", 0.5, 0 },    { "1/8 .", 0.75, 0 },       { "1/4 T", 2.0 / 3.0, 0 }, { "1/4", 1.0, 0 },
    { "1/4 .", 1.5, 0 },  { "1/2", 2.0, 0 },          { "1 mesure", 0, 1 },      { "2 mesures", 0, 2 },
    { "4 mesures", 0, 4 }, { "8 mesures", 0, 8 },     { "16 mesures", 0, 16 },
};
constexpr int vitesseParDefaut = 10; // 1 mesure

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
        const float x = v[0].getFloatValue(), y = v[1].getFloatValue(), t = v[2].getFloatValue();
        const int type = juce::jlimit (0, 2, v[3].getIntValue());
        if (std::isfinite (x) && std::isfinite (y) && std::isfinite (t))
            c.ajouter (x, y, t, static_cast<courbes::Segment> (type));
    }
    c.trier();
    return c.taille() >= 2;
}
} // namespace

const juce::StringArray& PluginProcessor::nomsVitesses()
{
    static const juce::StringArray noms = []
    {
        juce::StringArray s;
        for (const auto& v : vitesses)
            s.add (v.nom);
        return s;
    }();
    return noms;
}

PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ETAT", createLayout())
{
    pVitesse = apvts.getRawParameterValue ("vitesse");
    pPhase = apvts.getRawParameterValue ("phase");
    pCc = apvts.getRawParameterValue ("cc");
    pCanal = apvts.getRawParameterValue ("canal");
    pMin = apvts.getRawParameterValue ("min");
    pMax = apvts.getRawParameterValue ("max");
    pLissage = apvts.getRawParameterValue ("lissage");
    pEcoute = apvts.getRawParameterValue ("ecoute");

    courbes::generer (0, courbes::membre (0, 0), courbeInterface);
    courbePartagee = courbeInterface;
    courbeAudio = courbeInterface;
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    using Float = juce::AudioParameterFloat;
    using Attr = juce::AudioParameterFloatAttributes;

    // Identifiants et numeros de version : NE CHANGENT JAMAIS apres une sortie (projets, automations).
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "vitesse", 1 }, "Vitesse",
                                                              nomsVitesses(), vitesseParDefaut));
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
    layout.add (std::make_unique<Float> (juce::ParameterID { "ecoute", 1 }, "Ecoute",
                                         juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, Attr().withLabel ("%")));
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
    ecoute.snap (pEcoute->load() * 0.01f);
    decalage.snap (pPhase->load() * 0.01f);
    bas.snap (pMin->load() * 0.01f);
    haut.snap (pMax->load() * 0.01f);
    sortie.snap (bas.current() + (haut.current() - bas.current()) * courbeAudio.valeur (decalage.current()));
    dernierCc = dernierNumero = dernierCanal = -1; // renvoyer la valeur au redemarrage
    compteurEnvoi = 0;
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

double PluginProcessor::dureeCycleEnTemps (int index, const juce::AudioPlayHead::TimeSignature& sig) const
{
    const auto& v = vitesses[juce::jlimit (0, static_cast<int> (std::size (vitesses)) - 1, index)];
    if (v.mesures <= 0.0)
        return v.noires;
    const double noiresParMesure = sig.numerator > 0 && sig.denominator > 0
                                       ? 4.0 * static_cast<double> (sig.numerator) / static_cast<double> (sig.denominator)
                                       : 4.0;
    return v.mesures * noiresParMesure;
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
            courbeAudio = courbePartagee;
            versionAudio = versionCourbe.load (std::memory_order_relaxed);
        }
    }

    // Position dans le temps : celle du DAW quand il joue, sinon une horloge interne au meme tempo.
    double bpm = 120.0;
    double ppq = 0.0;
    bool joue = false;
    juce::AudioPlayHead::TimeSignature sig;
    if (auto* head = getPlayHead())
    {
        if (const auto pos = head->getPosition())
        {
            if (const auto b = pos->getBpm())
                bpm = juce::jlimit (10.0, 999.0, *b);
            if (const auto s = pos->getTimeSignature())
                sig = *s;
            if (const auto q = pos->getPpqPosition(); q && pos->getIsPlaying())
            {
                ppq = *q;
                joue = std::isfinite (ppq);
            }
        }
    }
    const double cycle = dureeCycleEnTemps (juce::roundToInt (pVitesse->load()), sig);
    const double pasParEchantillon = bpm / 60.0 / sr / cycle;
    double position = joue ? ppq / cycle : phaseLibre;

    sortie.reset (sr, pLissage->load());
    ecoute.setTarget (pEcoute->load() * 0.01f);
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

        // Ecoute : un son test dont le volume suit la courbe (0 par defaut : le plugin reste muet).
        const float e = ecoute.next();
        if (e > 0.0f)
        {
            const float s = static_cast<float> (std::sin (juce::MathConstants<double>::twoPi * phaseTon)) * 0.3f * e * valeur;
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.getWritePointer (ch)[i] = s;
        }
        phaseTon += pasTon;
        if (phaseTon >= 1.0)
            phaseTon -= 1.0;
        position += pasParEchantillon;
    }

    phaseLibre = position - std::floor (position);
    const double affiche = position + static_cast<double> (decalage.current());
    phaseAffichee.store (static_cast<float> (affiche - std::floor (affiche)), std::memory_order_relaxed);
    valeurAffichee.store (valeur, std::memory_order_relaxed);
    ccAffiche.store (dernierCc, std::memory_order_relaxed);
}

courbes::Courbe PluginProcessor::getCourbe() const
{
    const juce::ScopedLock sl (verrouInterface);
    return courbeInterface;
}

void PluginProcessor::setCourbe (const courbes::Courbe& c, int f, int m, uint32_t g)
{
    {
        const juce::ScopedLock sl (verrouInterface);
        courbeInterface = c;
    }
    {
        const juce::SpinLock::ScopedLockType sl (verrouPartage);
        courbePartagee = c;
    }
    famille = f;
    membre = m;
    graine = g;
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

    state.removeChild (state.getChildWithName ("COURBE"), nullptr);
    juce::ValueTree courbe ("COURBE");
    courbe.setProperty ("points", courbeVersTexte (getCourbe()), nullptr);
    courbe.setProperty ("famille", famille.load(), nullptr);
    courbe.setProperty ("membre", membre.load(), nullptr);
    courbe.setProperty ("graine", static_cast<juce::int64> (graine.load()), nullptr);
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
    // Ici : convertir les anciens formats si state["version_etat"] < stateVersion.
    editorWidth = juce::jlimit (480, 2000, static_cast<int> (state.getProperty ("largeur", 820)));
    editorHeight = juce::jlimit (320, 1400, static_cast<int> (state.getProperty ("hauteur", 520)));

    const auto courbe = state.getChildWithName ("COURBE");
    courbes::Courbe c;
    if (courbe.isValid() && texteVersCourbe (courbe["points"].toString(), c))
        setCourbe (c,
                   juce::jlimit (0, courbes::nombreFamilles - 1, static_cast<int> (courbe.getProperty ("famille", 0))),
                   juce::jlimit (-1, courbes::membresParFamille - 1, static_cast<int> (courbe.getProperty ("membre", 0))),
                   static_cast<uint32_t> (static_cast<juce::int64> (courbe.getProperty ("graine", 0))));
    state.removeChild (courbe, nullptr);
    apvts.replaceState (state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
