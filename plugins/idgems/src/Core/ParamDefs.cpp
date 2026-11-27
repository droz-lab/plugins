#include "ParamDefs.h"

namespace idg
{
// ---------------------------------------------------------------------------
// Divisions
// ---------------------------------------------------------------------------
const juce::StringArray& divisionNames()
{
    static const juce::StringArray d { "1/64", "1/32T", "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8",
                                       "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1/1", "2/1", "4/1" };
    return d;
}

double divisionBeats (int i)
{
    static const double b[] { 0.0625, 0.125 * 2.0 / 3.0, 0.125, 0.25 * 2.0 / 3.0, 0.25, 0.375, 0.5 * 2.0 / 3.0, 0.5,
                              0.75, 2.0 / 3.0, 1.0, 1.5, 2.0, 4.0, 8.0, 16.0 };
    return b[juce::jlimit (0, 15, i)];
}

int divisionIndex (const juce::String& name) { return juce::jmax (0, divisionNames().indexOf (name)); }

const juce::StringArray& seqRateNames()
{
    static const juce::StringArray d { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    return d;
}

double seqRateBeats (int i)
{
    static const double b[] { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
    return b[juce::jlimit (0, 5, i)];
}

const juce::StringArray& familyNames()
{
    static const juce::StringArray f { "GLITCH", "DESTRUCTION", "PITCH", "FILTRES", "ESPACE", "MOUVEMENT" };
    return f;
}

juce::Colour familyColour (int f)
{
    static const juce::Colour c[] {
        juce::Colour (0xffff6a00),  // glitch : orange
        juce::Colour (0xffff2d55),  // destruction : rouge
        juce::Colour (0xffc86bff),  // pitch : violet
        juce::Colour (0xff3fb8ff),  // filtres : bleu
        juce::Colour (0xff33e0c0),  // espace : turquoise
        juce::Colour (0xffffd23f)   // mouvement : jaune
    };
    return c[juce::jlimit (0, 5, f)];
}

const juce::StringArray& modeNames()
{
    static const juce::StringArray m { "Stereo", "Mid", "Side", "L", "R" };
    return m;
}

// ---------------------------------------------------------------------------
// ParamDef
// ---------------------------------------------------------------------------
juce::NormalisableRange<float> ParamDef::range() const
{
    if (type == PType::Bool) return { 0.0f, 1.0f, 1.0f };
    if (type == PType::Choice) return { 0.0f, (float) juce::jmax (1, choices.size() - 1), 1.0f };
    return { min, max, 0.0f, skew, symSkew };
}

juce::String ParamDef::toText (float v) const
{
    if (type == PType::Bool) return v > 0.5f ? "ON" : "OFF";
    if (type == PType::Choice) return choices[juce::jlimit (0, choices.size() - 1, (int) std::lround (v))];
    if (unit == "Hz")
        return std::abs (v) >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (v, std::abs (v) < 10 ? 2 : 0) + " Hz";
    if (unit == "ms")
        return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String (v, v < 10 ? 1 : 0) + " ms";
    if (unit == "%") return juce::String (juce::roundToInt (v * 100.0f)) + " %";
    if (unit == "st") return (v > 0 ? "+" : "") + juce::String (v, 1) + " st";
    if (unit == "dB") return juce::String (v, 1) + " dB";
    if (unit == "s") return juce::String (v, 2) + " s";
    if (unit == "note") return juce::MidiMessage::getMidiNoteName (juce::roundToInt (v), true, true, 3);
    if (unit == "ct") return juce::String (juce::roundToInt (v)) + " ct";
    if (unit == "x") return "x" + juce::String (v, 1);
    return juce::String (v, 2) + (unit.isNotEmpty() ? " " + unit : juce::String());
}

// ---------------------------------------------------------------------------
// Petits constructeurs
// ---------------------------------------------------------------------------
namespace
{
ParamDef F (const char* key, const juce::String& name, float mn, float mx, float def, const juce::String& unit = {},
            float skew = 1.0f, float rmin = 0.0f, float rmax = 1.0f, const juce::String& help = {})
{
    ParamDef p;
    p.id = key; p.name = name; p.min = mn; p.max = mx; p.def = def; p.unit = unit; p.skew = skew;
    p.rmin = rmin; p.rmax = rmax; p.help = help; p.randomizable = true;
    return p;
}

ParamDef C (const char* key, const juce::String& name, juce::StringArray choices, int def, float rmin = 0.0f, float rmax = 1.0f,
            const juce::String& help = {})
{
    ParamDef p;
    p.id = key; p.name = name; p.type = PType::Choice; p.choices = choices; p.def = (float) def;
    p.min = 0; p.max = (float) (choices.size() - 1); p.rmin = rmin; p.rmax = rmax; p.help = help; p.randomizable = true;
    return p;
}

ParamDef B (const char* key, const juce::String& name, bool def, bool randomizable = true, const juce::String& help = {})
{
    ParamDef p;
    p.id = key; p.name = name; p.type = PType::Bool; p.def = def ? 1.0f : 0.0f; p.help = help; p.randomizable = randomizable;
    return p;
}

ParamDef Div (const char* key, const juce::String& name, const char* def, const char* rminName = "1/32", const char* rmaxName = "1/2")
{
    auto& d = divisionNames();
    auto n = (float) (d.size() - 1);
    return C (key, name, d, divisionIndex (def), (float) divisionIndex (rminName) / n, (float) divisionIndex (rmaxName) / n,
              IDG_T ("Division rythmique calée sur le tempo"));
}

ParamDef pitched (ParamDef p, PitchKind k) { p.pitch = k; return p; }
ParamDef sym (ParamDef p) { p.symSkew = true; return p; }

std::vector<EffectDef> buildEffects()
{
    std::vector<EffectDef> e;
    auto add = [&] (const char* key, const juce::String& name, const char* shortName, Family fam, std::vector<ParamDef> ps,
                    float defMix, bool trig, const juce::String& help)
    {
        EffectDef d;
        d.key = key; d.name = name; d.shortName = shortName; d.family = fam; d.params = std::move (ps);
        d.defMix = defMix; d.triggerable = trig; d.help = help;
        e.push_back (d);
        return (int) e.size() - 1;
    };

    // ---------------- GLITCH TEMPOREL ----------------
    add ("stutter", "Stutter / Retrigger", "STUTTER", FamGlitch, {
            Div ("div", "Taille", "1/16", "1/64", "1/4"),
            C ("retrig", "Relance", { "Trigger", "1/4", "1/2", "1 temps", "2 temps", "1 mesure" }, 3, 0.0f, 1.0f,
               IDG_T ("Relance automatique de la capture (Trigger = seulement séquenceur/MIDI/transitoires)")),
            F ("gate", "Gate", 0.05f, 1.0f, 1.0f, "%", 1.0f, 0.3f, 1.0f, IDG_T ("Longueur jouée de chaque répétition")),
            F ("decay", "Decay", 0.0f, 1.0f, 0.0f, "%", 1.0f, 0.0f, 0.6f, IDG_T ("Atténuation à chaque répétition")),
            pitched (F ("pitch", IDG_T ("Pitch/rép."), -12.0f, 12.0f, 0.0f, "st", 1.0f, 0.3f, 0.7f, IDG_T ("Décalage de hauteur à chaque répétition")), PitchKind::Semis)
         }, 1.0f, true, IDG_T ("Capture une tranche et la répète en boucle."));

    add ("beatrepeat", "Beat Repeat", "B.REPEAT", FamGlitch, {
            C ("interval", "Intervalle", { "1 temps", "2 temps", "1 mesure", "2 mesures" }, 2),
            Div ("grid", "Grille", "1/16", "1/64", "1/4"),
            F ("chance", "Chance", 0.0f, 1.0f, 0.5f, "%", 1.0f, 0.2f, 1.0f, IDG_T ("Probabilité de répétition à chaque intervalle")),
            F ("length", IDG_T ("Durée"), 0.1f, 1.0f, 0.5f, "%", 1.0f, 0.2f, 1.0f, IDG_T ("Durée de la répétition (part de l'intervalle)")),
            F ("pdecay", "Pitch decay", 0.0f, 1.0f, 0.0f, "%", 1.0f, 0.0f, 0.6f, IDG_T ("Ralentissement / descente à chaque répétition")),
            F ("vdecay", "Vol decay", 0.0f, 1.0f, 0.1f, "%", 1.0f, 0.0f, 0.5f)
         }, 1.0f, true, IDG_T ("Répète la grille à chaque intervalle, façon Beat Repeat."));

    add ("reverse", "Reverse", "REVERSE", FamGlitch, {
            Div ("size", "Taille", "1/8", "1/32", "1/1"),
            F ("chance", "Chance", 0.0f, 1.0f, 1.0f, "%", 1.0f, 0.3f, 1.0f, IDG_T ("Probabilité d'inverser chaque tranche")),
            F ("smooth", "Lissage", 0.0f, 1.0f, 0.2f, "%", 1.0f, 0.05f, 0.6f)
         }, 1.0f, true, IDG_T ("Joue les tranches précédentes à l'envers."));

    add ("tapestop", "Tape Stop", "TAPESTOP", FamGlitch, {
            F ("stop", IDG_T ("Arrêt"), 30.0f, 4000.0f, 600.0f, "ms", 0.4f, 0.1f, 0.7f),
            F ("start", IDG_T ("Redémarrage"), 10.0f, 2000.0f, 150.0f, "ms", 0.4f, 0.0f, 0.6f),
            F ("curve", "Courbe", 0.0f, 1.0f, 0.5f, "%"),
            C ("auto", "Mode", { "Gate", "1/2 mesure", "1 mesure", "2 mesures" }, 2, 0.0f, 1.0f,
               IDG_T ("Gate : arrêt quand l'effet est actif. Sinon : arrêt automatique à la fin de chaque période"))
         }, 1.0f, true, IDG_T ("Ralentit la bande jusqu'à l'arrêt."));

    add ("shuffle", "Buffer Shuffle", "SHUFFLE", FamGlitch, {
            Div ("div", "Tranche", "1/16", "1/32", "1/4"),
            F ("chance", "Chance", 0.0f, 1.0f, 0.5f, "%", 1.0f, 0.2f, 0.9f),
            F ("reverse", "Reverse", 0.0f, 1.0f, 0.15f, "%", 1.0f, 0.0f, 0.5f),
            C ("window", IDG_T ("Mémoire"), { "1 temps", "2 temps", "1 mesure", "2 mesures" }, 2)
         }, 1.0f, true, IDG_T ("Remélange les tranches de la dernière mesure."));

    add ("freeze", "Freeze granulaire", "FREEZE", FamGlitch, {
            F ("size", "Grain", 10.0f, 500.0f, 90.0f, "ms", 0.5f, 0.1f, 0.8f),
            F ("density", IDG_T ("Densité"), 1.0f, 16.0f, 4.0f, "x", 0.6f, 0.1f, 0.7f),
            F ("spray", "Spray", 0.0f, 1.0f, 0.3f, "%"),
            pitched (F ("pitch", "Pitch", -24.0f, 24.0f, 0.0f, "st", 1.0f, 0.25f, 0.75f), PitchKind::Semis),
            F ("spread", IDG_T ("Stéréo"), 0.0f, 1.0f, 0.5f, "%"),
            F ("length", "Capture", 50.0f, 2000.0f, 400.0f, "ms", 0.5f)
         }, 1.0f, true, "Fige le son et le fait vivre en nuage de grains.");

    add ("scratch", "Scratch", "SCRATCH", FamGlitch, {
            F ("pos", "Position", 0.0f, 1.0f, 0.0f, "%", 1.0f, 0.0f, 1.0f, IDG_T ("Position du disque (à bouger à la souris ou à moduler)")),
            B ("auto", "Auto", true),
            Div ("rate", "Vitesse", "1/4", "1/16", "1/1"),
            F ("depth", "Amplitude", 0.0f, 1.0f, 0.5f, "%", 1.0f, 0.1f, 1.0f),
            F ("smooth", "Inertie", 0.0f, 1.0f, 0.3f, "%")
         }, 1.0f, true, IDG_T ("Scratch du buffer : à la souris, en auto ou par modulation."));

    // ---------------- DESTRUCTION ----------------
    add ("bitcrush", "Bitcrush / Downsample", "BITCRUSH", FamDestruction, {
            F ("bits", "Bits", 1.0f, 16.0f, 8.0f, "", 1.0f, 0.15f, 0.8f),
            F ("rate", "Downsample", 1.0f, 64.0f, 4.0f, "x", 0.35f, 0.0f, 0.7f),
            F ("jitter", "Jitter", 0.0f, 1.0f, 0.0f, "%", 1.0f, 0.0f, 0.6f)
         }, 1.0f, false, IDG_T ("Réduction de résolution et de fréquence d'échantillonnage."));

    add ("distortion", "Distorsion / Waveshaper", "DISTO", FamDestruction, {
            F ("drive", "Drive", 0.0f, 48.0f, 12.0f, "dB", 1.0f, 0.1f, 0.7f),
            C ("curve", "Courbe", { "Tanh", "Dure", "Fold", "Asym", "Sinus", "Rectif" }, 0),
            F ("bias", "Bias", -1.0f, 1.0f, 0.0f, "", 1.0f, 0.3f, 0.7f),
            F ("tone", "Tone", 500.0f, 20000.0f, 12000.0f, "Hz", 0.3f, 0.3f, 1.0f),
            F ("out", "Sortie", -24.0f, 12.0f, -6.0f, "dB", 1.0f, 0.4f, 0.7f)
         }, 1.0f, false, "Saturation multi-courbes.");

    add ("ringmod", "Ring Mod", "RINGMOD", FamDestruction, {
            pitched (F ("freq", IDG_T ("Fréquence"), 1.0f, 5000.0f, 220.0f, "Hz", 0.3f, 0.05f, 0.8f), PitchKind::Hz),
            C ("wave", "Onde", { "Sinus", "Triangle", IDG_T ("Carré") }, 0),
            F ("spread", IDG_T ("Stéréo"), 0.0f, 1.0f, 0.0f, "%", 1.0f, 0.0f, 0.6f)
         }, 0.6f, false, "Modulation en anneau.");

    add ("freqshift", "Frequency Shifter", "FREQSHFT", FamDestruction, {
            sym (F ("shift", IDG_T ("Décalage"), -2000.0f, 2000.0f, 50.0f, "Hz", 0.35f, 0.25f, 0.75f)),
            F ("fb", "Feedback", 0.0f, 0.95f, 0.0f, "%", 1.0f, 0.0f, 0.7f),
            F ("spread", IDG_T ("Stéréo"), 0.0f, 1.0f, 0.0f, "%")
         }, 0.7f, false, IDG_T ("Décale toutes les fréquences (inharmonique)."));

    add ("corruption", "Corruption", "CORRUPT", FamDestruction, {
            F ("rate", "Taux", 0.1f, 30.0f, 4.0f, "Hz", 0.5f, 0.05f, 0.7f, "Nombre d'accidents par seconde"),
            F ("size", "Taille", 5.0f, 500.0f, 60.0f, "ms", 0.5f, 0.05f, 0.7f, IDG_T ("Durée de chaque accident")),
            C ("type", "Type", { "Mix", "Coupures", "Paquets", "CD qui saute", IDG_T ("Décrochage") }, 0),
            F ("jitter", "Jitter", 0.0f, 1.0f, 0.5f, "%")
         }, 1.0f, true, IDG_T ("Micro-coupures, paquets perdus, CD qui saute, décrochages."));

    // ---------------- PITCH ----------------
    add ("pitchshift", "Pitch Shift", "PITCH", FamPitch, {
            pitched (F ("semi", "Demi-tons", -24.0f, 24.0f, 7.0f, "st", 1.0f, 0.25f, 0.75f), PitchKind::Semis),
            F ("fine", "Fin", -100.0f, 100.0f, 0.0f, "ct", 1.0f, 0.4f, 0.6f),
            F ("fb", "Feedback", 0.0f, 0.9f, 0.0f, "%", 1.0f, 0.0f, 0.6f, IDG_T ("Réinjection : spirales de hauteur"))
         }, 0.5f, false, IDG_T ("Transposition (algorithme spectral, simplifié en mode sans latence)."));
    e.back().stft = true;

    add ("formant", "Formant", "FORMANT", FamPitch, {
            F ("shift", "Formants", -12.0f, 12.0f, 4.0f, "st", 1.0f, 0.2f, 0.8f, IDG_T ("Déplace les formants sans changer la hauteur"))
         }, 1.0f, false, IDG_T ("Déplacement des formants (voix de géant / de schtroumpf)."));
    e.back().stft = true;

    // ---------------- FILTRES ----------------
    add ("filter", "Filtre multimode", "FILTER", FamFilters, {
            C ("type", "Type", { "LP 12", "LP 24", "HP 12", "HP 24", "BP", "Notch" }, 1),
            F ("cutoff", "Cutoff", 20.0f, 20000.0f, 2000.0f, "Hz", 0.25f, 0.2f, 0.95f),
            F ("reso", IDG_T ("Résonance"), 0.0f, 1.0f, 0.3f, "%", 1.0f, 0.0f, 0.85f),
            F ("drive", "Drive", 0.0f, 24.0f, 0.0f, "dB", 1.0f, 0.0f, 0.5f)
         }, 1.0f, false, IDG_T ("Filtre multimode résonant."));

    add ("comb", IDG_T ("Comb / Résonateur"), "COMB", FamFilters, {
            pitched (F ("freq", IDG_T ("Fréquence"), 20.0f, 5000.0f, 220.0f, "Hz", 0.3f, 0.15f, 0.7f), PitchKind::Hz),
            F ("fb", "Feedback", -0.99f, 0.99f, 0.8f, "", 1.0f, 0.1f, 0.95f),
            F ("damp", "Amorti", 0.0f, 1.0f, 0.3f, "%"),
            C ("chord", "Accord", { "Simple", "Octave", "Quinte", "Triade", "4 sons" }, 0)
         }, 0.7f, false, IDG_T ("Résonateur accordé (suit la gamme globale)."));

    add ("vocoder", "Vocoder", "VOCODER", FamFilters, {
            C ("carrier", "Porteuse", { "Saw", IDG_T ("Carré"), "Bruit", "Accord" }, 3),
            pitched (F ("note", "Note", 24.0f, 96.0f, 48.0f, "note", 1.0f, 0.2f, 0.7f), PitchKind::Midi),
            C ("bands", "Bandes", { "8", "12", "16", "24" }, 2),
            F ("release", "Release", 5.0f, 500.0f, 40.0f, "ms", 0.5f, 0.1f, 0.6f),
            F ("shift", IDG_T ("Décalage"), -12.0f, 12.0f, 0.0f, "st", 1.0f, 0.3f, 0.7f, IDG_T ("Décale les bandes de la porteuse"))
         }, 1.0f, false, IDG_T ("Vocoder à porteuse interne (suit la gamme globale)."));

    add ("pixelsort", "Pixel Sort audio", "PIXSORT", FamFilters, {
            F ("thresh", "Seuil", 0.0f, 1.0f, 0.2f, "%", 1.0f, 0.0f, 0.7f, IDG_T ("Seules les fréquences au-dessus du seuil sont triées")),
            C ("dir", "Sens", { "Montant", "Descendant", IDG_T ("Aléatoire") }, 0),
            C ("segment", "Segment", { "8", "16", "32", "64", "128" }, 2),
            F ("smear", "Smear", 0.0f, 1.0f, 0.0f, "%", 1.0f, 0.0f, 0.8f)
         }, 1.0f, false, IDG_T ("Tri spectral, façon pixel sorting appliqué au son."));
    e.back().stft = true;
    e.back().noZeroLatency = true;

    // ---------------- ESPACE ----------------
    add ("delay", "Delay", "DELAY", FamSpace, {
            F ("time", "Temps", 1.0f, 2000.0f, 375.0f, "ms", 0.4f),
            B ("sync", "Sync", true),
            Div ("div", "Division", "1/8D", "1/32", "1/2"),
            F ("fb", "Feedback", 0.0f, 1.0f, 0.45f, "%", 1.0f, 0.1f, 0.8f),
            B ("pingpong", "Ping-pong", false),
            F ("tone", "Tone", 500.0f, 20000.0f, 8000.0f, "Hz", 0.3f),
            F ("lowcut", "Coupe-bas", 20.0f, 2000.0f, 120.0f, "Hz", 0.3f, 0.0f, 0.6f),
            F ("wow", "Wow", 0.0f, 1.0f, 0.0f, "%", 1.0f, 0.0f, 0.5f),
            B ("throw", "Throw", false, false, IDG_T ("Mode throw : seul le pad / séquenceur / MIDI envoie dans le delay, la queue reste"))
         }, 0.35f, true, "Delay synchronisable avec ping-pong et throws.");

    add ("reverb", "Reverb", "REVERB", FamSpace, {
            F ("size", "Taille", 0.0f, 1.0f, 0.6f, "%"),
            F ("decay", "Decay", 0.1f, 20.0f, 3.0f, "s", 0.4f, 0.0f, 0.6f),
            F ("damp", "Amorti", 0.0f, 1.0f, 0.4f, "%"),
            F ("predelay", IDG_T ("Pré-delay"), 0.0f, 250.0f, 10.0f, "ms", 0.5f),
            F ("width", "Largeur", 0.0f, 1.0f, 1.0f, "%"),
            F ("mod", "Modulation", 0.0f, 1.0f, 0.3f, "%"),
            B ("throw", "Throw", false, false, IDG_T ("Mode throw : seul le pad / séquenceur / MIDI envoie dans la reverb, la queue reste"))
         }, 0.3f, true, "Reverb FDN dense.");

    add ("chorus", "Chorus", "CHORUS", FamSpace, {
            F ("rate", "Vitesse", 0.01f, 5.0f, 0.6f, "Hz", 0.5f),
            F ("depth", "Profondeur", 0.0f, 1.0f, 0.5f, "%"),
            C ("voices", "Voix", { "2", "3", "4" }, 1),
            F ("spread", IDG_T ("Stéréo"), 0.0f, 1.0f, 0.8f, "%"),
            F ("fb", "Feedback", 0.0f, 0.7f, 0.0f, "%", 1.0f, 0.0f, 0.6f)
         }, 0.5f, false, "Chorus multi-voix.");

    add ("flanger", "Flanger", "FLANGER", FamSpace, {
            F ("rate", "Vitesse", 0.01f, 10.0f, 0.2f, "Hz", 0.4f),
            F ("depth", "Profondeur", 0.0f, 1.0f, 0.7f, "%"),
            F ("fb", "Feedback", -0.95f, 0.95f, 0.6f, "", 1.0f, 0.1f, 0.9f),
            F ("manual", "Manuel", 0.1f, 10.0f, 2.0f, "ms", 0.5f)
         }, 1.0f, false, IDG_T ("Flanger à feedback bipolaire."));

    add ("phaser", "Phaser", "PHASER", FamSpace, {
            F ("rate", "Vitesse", 0.01f, 10.0f, 0.4f, "Hz", 0.4f),
            F ("depth", "Profondeur", 0.0f, 1.0f, 0.8f, "%"),
            C ("stages", IDG_T ("Étages"), { "4", "6", "8", "12" }, 1),
            F ("fb", "Feedback", -0.9f, 0.9f, 0.5f, "", 1.0f, 0.1f, 0.9f),
            F ("center", "Centre", 100.0f, 8000.0f, 800.0f, "Hz", 0.3f)
         }, 1.0f, false, IDG_T ("Phaser multi-étages."));

    // ---------------- MOUVEMENT ----------------
    add ("gate", "Gate rythmique", "GATE", FamMovement, {
            Div ("div", "Division", "1/16", "1/32", "1/4"),
            F ("duty", "Ouverture", 0.05f, 1.0f, 0.5f, "%", 1.0f, 0.2f, 0.9f),
            F ("smooth", "Lissage", 0.0f, 1.0f, 0.1f, "%"),
            C ("pattern", "Motif", { "Droit", "Euclide 3/8", "Euclide 5/8", "Euclide 7/16", IDG_T ("Aléatoire") }, 0),
            F ("depth", "Profondeur", 0.0f, 1.0f, 1.0f, "%", 1.0f, 0.4f, 1.0f)
         }, 1.0f, false, "Trance gate rythmique.");

    add ("tremolo", "Tremolo", "TREMOLO", FamMovement, {
            F ("rate", "Vitesse", 0.1f, 30.0f, 5.0f, "Hz", 0.4f),
            B ("sync", "Sync", true),
            Div ("div", "Division", "1/8", "1/32", "1/2"),
            F ("depth", "Profondeur", 0.0f, 1.0f, 0.7f, "%"),
            C ("shape", "Forme", { "Sinus", "Triangle", IDG_T ("Carré"), "Dent de scie" }, 0),
            F ("stereo", IDG_T ("Stéréo"), 0.0f, 1.0f, 0.0f, "%")
         }, 1.0f, false, "Modulation d'amplitude.");

    add ("autopan", "Autopan", "AUTOPAN", FamMovement, {
            F ("rate", "Vitesse", 0.05f, 20.0f, 1.0f, "Hz", 0.4f),
            B ("sync", "Sync", true),
            Div ("div", "Division", "1/4", "1/16", "4/1"),
            F ("depth", "Profondeur", 0.0f, 1.0f, 0.8f, "%"),
            C ("shape", "Forme", { "Sinus", "Triangle", IDG_T ("Carré"), "Dent de scie" }, 0)
         }, 1.0f, false, "Panoramique automatique.");

    add ("stereo", IDG_T ("Stéréo / Mid-Side"), "STEREO", FamMovement, {
            F ("width", "Largeur", 0.0f, 2.0f, 1.0f, "%", 1.0f, 0.2f, 0.8f),
            F ("mid", "Mid", -24.0f, 12.0f, 0.0f, "dB", 1.0f, 0.5f, 0.8f),
            F ("side", "Side", -24.0f, 12.0f, 0.0f, "dB", 1.0f, 0.5f, 0.85f),
            F ("haas", "Haas", 0.0f, 30.0f, 0.0f, "ms", 0.5f, 0.0f, 0.6f),
            F ("rotate", "Rotation", -1.0f, 1.0f, 0.0f, "", 1.0f, 0.3f, 0.7f)
         }, 1.0f, false, IDG_T ("Largeur stéréo, mid/side et effet Haas."));

    jassert ((int) e.size() == NumFx);
    return e;
}

Registry buildRegistry()
{
    Registry r;
    auto push = [&r] (ParamDef p)
    {
        r.index[p.id] = (int) r.all.size();
        r.all.push_back (std::move (p));
        return (int) r.all.size() - 1;
    };
    auto global = [] (ParamDef p) { p.randomizable = false; return p; };

    // 1. Contrôles prioritaires (automation Ableton)
    for (int i = 0; i < NumMacros; ++i)
        push (global (F (G::macro (i).toRawUTF8(), ("Macro " + juce::String (i + 1)).toRawUTF8(), 0.0f, 1.0f, 0.0f, "%")));
    push (global (F (G::chaos, "Chaos", 0.0f, 1.0f, 0.5f, "%", 1.0f, 0, 1, IDG_T ("Du sage au carnage : dosage global, densité des séquenceurs, profondeur des modulations"))));
    push (global (F (G::globalMix, "Dry/Wet global", 0.0f, 1.0f, 1.0f, "%")));
    push (global (F (G::inGain, IDG_T ("Gain entrée"), -24.0f, 24.0f, 0.0f, "dB")));
    push (global (F (G::outGain, "Gain sortie", -24.0f, 24.0f, 0.0f, "dB")));
    for (int f = 0; f < NumFamilies; ++f)
        push (global (F (G::famMix (f).toRawUTF8(), ("Dry/Wet " + familyNames()[f]).toRawUTF8(), 0.0f, 1.0f, 1.0f, "%")));
    push (global (F (G::morphX, "Morph X", 0.0f, 1.0f, 0.5f, "%")));
    push (global (F (G::morphY, "Morph Y", 0.0f, 1.0f, 0.5f, "%")));
    push (global (F (G::fbAmount, "Feedback global", 0.0f, 0.95f, 0.0f, "%")));
    push (global (F (G::fbTime, "Feedback temps", 1.0f, 2000.0f, 250.0f, "ms", 0.4f)));

    // 2. Effets
    const auto& fx = effectDefs();
    for (int i = 0; i < NumFx; ++i)
    {
        const auto& d = fx[(size_t) i];
        auto common = [&] (ParamDef p) { p.common = true; p.effect = i; p.family = d.family; return p; };

        auto on = B ("on", "On", false, false);
        on.id = fxParamId (i, "on"); on.name = d.shortName + " On";
        r.fxOn[i] = push (common (on));

        const bool space = d.family == FamSpace;
        auto mix = F ("mix", "Mix", 0.0f, 1.0f, space ? d.defMix : 1.0f, "%", 1.0f, 0.25f, 1.0f, IDG_T ("Dry/wet propre à l'effet"));
        mix.randomizable = space; // le glitch s'assume à 100 % : seuls les effets d'espace ont un mix randomisé
        mix.id = fxParamId (i, "mix"); mix.name = d.shortName + " Mix";
        r.fxMix[i] = push (common (mix));

        r.fxFirst[i] = (int) r.all.size();
        r.fxCount[i] = (int) d.params.size();
        int local = 0;
        for (auto p : d.params)
        {
            p.name = d.shortName + " " + p.name;
            p.id = fxParamId (i, p.id);
            p.effect = i;
            p.family = d.family;
            p.localIndex = local++;
            push (p);
        }
    }

    // 3. Entrées / sortie / globaux
    push (global (F (G::ceiling, "Plafond limiteur", -24.0f, 0.0f, -0.3f, "dB")));
    push (global (F (G::limRelease, "Release limiteur", 10.0f, 500.0f, 80.0f, "ms", 0.5f)));
    push (global (F (G::hpFreq, "Passe-haut", 10.0f, 300.0f, 20.0f, "Hz", 0.5f)));
    push (global (B (G::monoOn, "Grave mono", true, false)));
    push (global (F (G::monoFreq, IDG_T ("Fréquence mono"), 40.0f, 400.0f, 120.0f, "Hz", 0.5f)));
    push (global (F (G::ottDepth, "OTT profondeur", 0.0f, 1.0f, 0.0f, "%")));
    push (global (F (G::ottTime, "OTT temps", 0.1f, 10.0f, 1.0f, "x", 0.4f)));
    push (global (F (G::ottUp, "OTT upward", 0.0f, 1.0f, 1.0f, "%")));
    push (global (F (G::ottDown, "OTT downward", 0.0f, 1.0f, 1.0f, "%")));
    push (global (F (G::ottGain, "OTT gain", -12.0f, 12.0f, 0.0f, "dB")));
    push (global (F (G::clipDrive, "Soft clip drive", 0.0f, 24.0f, 0.0f, "dB")));
    push (global (B (G::zeroLat, "Mode sans latence", false, false)));
    push (global (C (G::destOS, IDG_T ("Suréchantillonnage destruction"), { "1x", "2x", "4x" }, 1)));
    push (global (B (G::transOn, IDG_T ("Détection transitoires"), false, false)));
    push (global (F (G::transSens, IDG_T ("Sensibilité transitoires"), 0.0f, 1.0f, 0.5f, "%")));
    push (global (C (G::transSrc, "Source transitoires", { IDG_T ("Entrée"), "Sidechain" }, 0)));
    push (global (B (G::capture, "Capture", false, false)));
    push (global (C (G::captureLen, IDG_T ("Durée capture"), { "1 temps", "2 temps", "1 mesure", "2 mesures" }, 2)));
    {
        juce::StringArray keys { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        push (global (B (G::scaleOn, "Gamme active", false, false)));
        push (global (C (G::scaleKey, IDG_T ("Tonalité"), keys, 0)));
        push (global (C (G::scaleType, "Gamme", { "Majeure", "Mineure", "Dorien", "Phrygien", "Lydien", "Mixolydien",
                                                  "Mineure harm.", "Penta maj.", "Penta min.", "Blues", "Par tons", "Chromatique" }, 1)));
        push (global (B (G::pitchSnap, "Pitch snapping", true, false)));
    }
    push (global (F (G::envAtt, "Env follower attaque", 0.1f, 200.0f, 5.0f, "ms", 0.4f)));
    push (global (F (G::envRel, "Env follower release", 5.0f, 2000.0f, 150.0f, "ms", 0.4f)));
    push (global (F (G::envGain, "Env follower gain", -24.0f, 24.0f, 0.0f, "dB")));
    push (global (C (G::envSrc, "Env follower source", { IDG_T ("Entrée"), "Sidechain" }, 0)));
    push (global (C (G::lfoTrig, IDG_T ("Déclenchement LFO"), { "MIDI", "Transitoires", "MIDI + transitoires" }, 0)));

    // 4. Modulateurs
    for (int i = 0; i < NumLfos; ++i)
    {
        auto n = "LFO " + juce::String (i + 1) + " ";
        auto id = [&] (const char* k) { return lfoId (i, k); };
        auto mk = [&] (ParamDef p, const char* key) { p.id = id (key); p.name = n + p.name; p.randomizable = false; return p; };
        push (mk (F ("x", "Vitesse", 0.01f, 30.0f, 1.0f, "Hz", 0.3f), "rate"));
        push (mk (B ("x", "Sync", true), "sync"));
        push (mk (C ("x", "Division", divisionNames(), divisionIndex ("1/4")), "div"));
        push (mk (C ("x", "Mode", { "Libre", "Sync", "Trigger", "Enveloppe" }, 1), "mode"));
        push (mk (F ("x", "Lissage", 0.0f, 1.0f, 0.0f, "%"), "smooth"));
        push (mk (F ("x", "Phase", 0.0f, 1.0f, 0.0f, "%"), "phase"));
        push (mk (F ("x", IDG_T ("Délai"), 0.0f, 4.0f, 0.0f, "s", 0.5f), "delay"));
        push (mk (F ("x", IDG_T ("Stéréo"), 0.0f, 0.5f, 0.0f, "%"), "stereo"));
    }
    for (int i = 0; i < NumRandMods; ++i)
    {
        auto n = "Random " + juce::String (i + 1) + " ";
        auto mk = [&] (ParamDef p, const char* key) { p.id = rndId (i, key); p.name = n + p.name; p.randomizable = false; return p; };
        push (mk (F ("x", "Vitesse", 0.01f, 30.0f, 2.0f, "Hz", 0.3f), "rate"));
        push (mk (B ("x", "Sync", false), "sync"));
        push (mk (C ("x", "Division", divisionNames(), divisionIndex ("1/8")), "div"));
        push (mk (C ("x", "Style", { "Perlin", "Sample & Hold", "Sinus", "Lorenz" }, 0), "style"));
        push (mk (B ("x", IDG_T ("Stéréo"), false), "stereo"));
    }
    return r;
}
} // namespace

const std::vector<EffectDef>& effectDefs()
{
    static const std::vector<EffectDef> e = buildEffects();
    return e;
}

const Registry& registry()
{
    static const Registry r = buildRegistry();
    return r;
}

namespace
{
    // Interrupteur arrondi a 0 ou 1 des qu'il recoit une valeur : ce que l'hote lit
    // est ce qui sera sauve et recharge (JUCE garderait sinon 0.37 tel quel).
    struct SnappedBool final : juce::RangedAudioParameter
    {
        SnappedBool (const juce::ParameterID& id, const juce::String& name, bool def)
            : juce::RangedAudioParameter (id, name), defaultValue (def ? 1.0f : 0.0f), value (defaultValue) {}

        float getValue() const override { return value.load(); }
        void setValue (float v) override { value.store (v >= 0.5f ? 1.0f : 0.0f); }
        float getDefaultValue() const override { return defaultValue; }
        int getNumSteps() const override { return 2; }
        bool isDiscrete() const override { return true; }
        bool isBoolean() const override { return true; }
        juce::String getText (float v, int maxLen) const override { return juce::String (v >= 0.5f ? "On" : "Off").substring (0, juce::jmax (0, maxLen)); }
        float getValueForText (const juce::String& t) const override
        {
            const auto s = t.trim().toLowerCase();
            return (s == "on" || s == "1" || s == "true" || s == "yes") ? 1.0f : 0.0f;
        }
        const juce::NormalisableRange<float>& getNormalisableRange() const override { return range; }

        const float defaultValue;
        std::atomic<float> value;
        const juce::NormalisableRange<float> range { 0.0f, 1.0f, 1.0f };
    };
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto& reg = registry();
    int version = 1;
    for (const auto& p : reg.all)
    {
        juce::ParameterID pid { p.id, version };
        if (p.type == PType::Bool)
            layout.add (std::make_unique<SnappedBool> (pid, p.name, p.def > 0.5f));
        else if (p.type == PType::Choice)
            layout.add (std::make_unique<juce::AudioParameterChoice> (pid, p.name, p.choices, (int) p.def));
        else
        {
            auto def = p;
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                pid, p.name, p.range(), p.def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction ([def] (float v, int) { return def.toText (v); })));
        }
    }
    return layout;
}
} // namespace idg
