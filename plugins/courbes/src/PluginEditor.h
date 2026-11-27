#pragma once

#include "PluginProcessor.h"

#include <functional>

/** Couleurs de Courbes. */
namespace palette
{
const juce::Colour fond { 0xff0e1014 };
const juce::Colour panneau { 0xff181b21 };
const juce::Colour panneauClair { 0xff20242c };
const juce::Colour grille { 0xff252a33 };
const juce::Colour grilleForte { 0xff363c48 };
const juce::Colour grilleMesure { 0xff4a5262 };
const juce::Colour texte { 0xffdfe2e8 };
const juce::Colour texteDoux { 0xff8b919d };
const juce::Colour accent { 0xffeea63a };
const juce::Colour accent2 { 0xff45c7bb };
} // namespace palette

/** Style commun : sliders fins, boutons a bascule colores quand ils sont allumes. */
class Style final : public juce::LookAndFeel_V4
{
public:
    Style();
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool survol, bool appui) override;
};

/** Zone ou l'on voit et modifie la courbe. */
class VueCourbe final : public juce::Component, public juce::SettableTooltipClient
{
public:
    VueCourbe();

    void setCourbe (const courbes::Courbe& c);
    const courbes::Courbe& getCourbe() const noexcept { return courbe; }
    /** Courbe jouee quand l'evolution l'a changee (dessinee en fantome), sinon rien. */
    void setJouee (const courbes::Courbe* c);
    void setLecture (float phase, float valeur);
    /** Grille : nombre de cases dans le cycle, et combien de cases font un temps et une mesure. */
    void setGrille (double cases, double casesParTemps, double casesParMesure);
    void setAimant (bool a) noexcept { aimant = a; }
    bool enTrainDeModifier() const noexcept { return glisse != Glisse::rien; }

    std::function<void()> onModif;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    enum class Glisse
    {
        rien,
        point,
        tension
    };

    juce::Rectangle<float> zone() const;
    juce::Point<float> versEcran (float x, float y) const;
    juce::Point<float> versCourbe (juce::Point<float> ecran) const;
    juce::Point<float> aimanter (juce::Point<float> c, const juce::ModifierKeys&) const;
    int pointSous (juce::Point<float> ecran) const;
    int segmentSous (float xCourbe) const;
    void modifie();
    void menuPoint (int index);
    juce::Path chemin (const courbes::Courbe&) const;

    courbes::Courbe courbe, jouee;
    bool afficheJouee = false;
    float lecture = 0.0f, valeur = 0.0f;
    double cases = 16.0, casesParTemps = 4.0, casesParMesure = 16.0;
    bool aimant = true;
    int survole = -1, choisi = -1;
    Glisse glisse = Glisse::rien;
    float tensionDepart = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VueCourbe)
};

class PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    /** Un slider horizontal avec son nom (au-dessus ou a gauche). */
    struct Reglette
    {
        juce::Slider slider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        juce::Label nom;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attache;
    };
    using AttacheBouton = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void preparer (Reglette&, const juce::String& id, const juce::String& nom, const juce::String& aide, int largeurTexte);
    void preparerBascule (juce::TextButton&, std::unique_ptr<AttacheBouton>&, const juce::String& id, const juce::String& aide);
    void preparerNom (juce::Label&, const juce::String& texte);
    void choisirMembre (int famille, int membre);
    void generer (bool toutAuHasard);
    void regenerer();
    void afficherReglages();
    void majTitreMembre();
    void envoyerCourbe();
    void majLongueurs();
    void majGrille();
    static void placer (Reglette&, juce::Rectangle<int> zone, bool nomAGauche, int largeurNom);

    PluginProcessor& processor;
    bool ready = false; // vrai une fois la taille gardee restauree

    Style style;
    juce::TooltipWindow infobulles { this, 500 };

    // En-tete : familles et formes
    juce::Label titre;
    juce::ComboBox familles;
    juce::TextButton precedent { "<" }, suivant { ">" }, relancer, boutonGenerer;
    juce::Label nomMembre;
    std::array<Reglette, courbes::nombreReglages> reglagesFamille;

    VueCourbe vue;
    juce::Label aide;

    // Temps
    juce::Slider longueur { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label nomLongueur;
    std::unique_ptr<juce::ParameterAttachment> attacheLongueur;
    std::vector<int> longueursAutorisees;
    bool ternaireConnu = true;
    juce::TextButton ternaire, grilleTernaire, boutonAimant;
    std::unique_ptr<AttacheBouton> attacheTernaire, attacheGrilleTernaire;
    juce::ComboBox grille;
    juce::Label nomGrille;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attacheGrille;

    // Sortie
    Reglette decalage, minimum, maximum, lissage;
    juce::TextButton ecoute;
    std::unique_ptr<AttacheBouton> attacheEcoute;
    juce::Slider cc { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft }, canal { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    juce::Label nomCc, nomCanal, lectureCc;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attacheCc, attacheCanal;

    // Evolution
    juce::TextButton evolution;
    std::unique_ptr<AttacheBouton> attacheEvolution;
    juce::Slider evolCycles { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attacheEvolCycles;
    juce::Label nomEvolCycles, titreIssues, titrePoids, joue;
    Reglette garder, revenir, varier, changer, intensite;
    std::array<Reglette, courbes::nombreFamilles> poids;
    juce::Rectangle<int> zoneEvolution;

    int famille = 0, membre = 0;
    courbes::Reglage reglage;
    bool modifiee = false, majEnCours = false;
    int versionConnue = 0, versionJoueeConnue = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
