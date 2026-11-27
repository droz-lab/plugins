#pragma once

#include "PluginProcessor.h"

#include <functional>

/** Couleurs de Droz Courbes. */
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
const juce::Colour lecture { 0xffe9edf2 };

/** Couleur de chaque onglet. */
juce::Colour onglet (int t);
} // namespace palette

/** Dessine l'insecte de l'onglet `t` (8 insectes differents) dans `zone`. */
void dessinerInsecte (juce::Graphics&, int t, juce::Rectangle<float> zone, juce::Colour);

/** Style commun : sliders fins, boutons a bascule colores quand ils sont allumes. */
class Style final : public juce::LookAndFeel_V4
{
public:
    Style();
    void setAccent (juce::Colour);
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool survol, bool appui) override;
};

/** Bouton d'onglet : couleur, insecte, numero, CC, et un point quand l'onglet est actif. */
class BoutonOnglet final : public juce::Button
{
public:
    explicit BoutonOnglet (int index);
    void setInfos (bool actif, int cc, bool evolution, bool visible);
    void paintButton (juce::Graphics&, bool survol, bool appui) override;

private:
    int index;
    bool actif = false, evolution = false, visible = true;
    int cc = 0;
};

/** Fleche Annuler (gauche) ou Retablir (droite), dessinee. */
class BoutonFleche final : public juce::Button
{
public:
    explicit BoutonFleche (bool versLaDroite);
    void paintButton (juce::Graphics&, bool survol, bool appui) override;

private:
    bool droite;
};

/** Petite vue d'une courbe, sans edition (courbe jouee, prochaine courbe). */
class MiniVue final : public juce::Component
{
public:
    void setCourbe (const courbes::Courbe* c, juce::Colour couleur, const juce::String& titre);
    void paint (juce::Graphics&) override;

private:
    courbes::Courbe courbe;
    bool vide = true;
    juce::Colour couleur;
    juce::String titre;
};

/** Zone ou l'on voit et modifie les courbes : une courbe (onglet affiche) ou plusieurs superposees.
    Points selectionnables sur plusieurs courbes a la fois, deplacement et etirement de la selection. */
class VueCourbe final : public juce::Component, public juce::SettableTooltipClient
{
public:
    VueCourbe();

    void setCourbe (int onglet, const courbes::Courbe& c);
    const courbes::Courbe& getCourbe (int onglet) const noexcept { return calques[static_cast<size_t> (onglet)].courbe; }
    /** Visible : dessinee. Modifiable : ses points se selectionnent et se deplacent. */
    void setCalque (int onglet, bool visible, bool modifiable);
    /** Onglet principal : sa couleur, son remplissage, sa grille, les points ajoutes au double-clic. */
    void setActif (int onglet);
    /** Courbe jouee par l'evolution sur l'onglet principal (dessinee par-dessus), sinon nullptr. */
    void setJouee (const courbes::Courbe* c);
    void setLecture (int onglet, float phase);
    void setGrille (double cases, double casesParTemps, double casesParMesure);
    void setAimant (bool a) noexcept { aimant = a; }
    void setCrayon (bool c) noexcept { crayon = c; }
    void setInsectes (bool i) { insectes = i; repaint(); }
    bool enTrainDeModifier() const noexcept { return mode != Mode::rien; }
    int nombreChoisis() const noexcept;

    std::function<void()> onDebutModif;          // juste avant une modification (pour Annuler)
    std::function<void (uint32_t)> onModif;      // apres une modification : masque des onglets touches

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    enum class Mode
    {
        rien,
        deplacer,
        etirer,
        tension,
        cadre,
        dessin
    };
    enum Poignee : int
    {
        aucune = 0,
        gauche = 1,
        droite = 2,
        haut = 4,
        bas = 8
    };
    struct Ref
    {
        int t = -1, i = -1;
        bool valide() const noexcept { return t >= 0 && i >= 0; }
    };
    struct Calque
    {
        courbes::Courbe courbe;
        bool visible = false, modifiable = false;
        float lecture = 0.0f;
    };

    juce::Rectangle<float> zone() const;
    juce::Point<float> versEcran (float x, float y) const;
    juce::Point<float> versCourbe (juce::Point<float> ecran) const;
    float aimanterX (float x, const juce::ModifierKeys&) const;
    float aimanterY (float y, const juce::ModifierKeys&) const;
    Ref pointSous (juce::Point<float> ecran) const;
    static int segmentSous (const courbes::Courbe&, float xCourbe);
    int courbeSous (juce::Point<float> ecran) const; // onglet dont la ligne passe sous la souris, ou -1
    juce::Rectangle<float> boiteSelection (const std::array<Calque, nombreOnglets>&) const; // coordonnees courbe
    juce::Rectangle<float> boiteEcran() const;
    int poigneeSous (juce::Point<float> ecran) const;
    uint32_t masqueChoisis() const noexcept;
    void toutDeselectionner();
    void modifie (uint32_t masque);
    void debut();
    void supprimerSelection();
    void menuPoint (Ref);
    void appliquerTrace();
    juce::Path chemin (const courbes::Courbe&) const;

    std::array<Calque, nombreOnglets> calques, depart;
    int actif = 0;
    courbes::Courbe jouee;
    bool afficheJouee = false;
    double cases = 16.0, casesParTemps = 4.0, casesParMesure = 16.0;
    bool aimant = true, crayon = false, insectes = true;
    std::vector<juce::Point<float>> trace; // trait du crayon, en coordonnees courbe
    Ref survole, saisi;
    Mode mode = Mode::rien;
    int poignee = aucune;
    juce::Point<float> debutSouris;
    juce::Rectangle<float> cadre;
    int calqueTension = -1, segmentTension = -1;
    float tensionDepart = 0.0f;
    bool dejaNote = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VueCourbe)
};

class PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    /** Un slider horizontal avec son nom (au-dessus ou a gauche). */
    struct Reglette
    {
        juce::Slider slider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        juce::Label nom;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attache;
    };
    /** Un reglage d'evolution (commun ou propre a l'onglet) : nom, slider, pourcentage reel. */
    struct LigneEvo
    {
        juce::Label nom, pourcent;
        juce::Slider slider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
        juce::TextButton seule; // familles : "ne garder que celle-ci"
    };
    struct Etat
    {
        courbes::Courbe courbe;
        int famille = 0, membre = 0;
        courbes::Reglage reglage;
        bool modifiee = false;
    };
    using Instantane = std::array<Etat, nombreOnglets>; // tous les onglets (la selection peut toucher plusieurs courbes)
    using AttacheBouton = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using AttacheSlider = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;
    void preparerNom (juce::Label&, const juce::String& texte);
    void preparerReglette (Reglette&, const juce::String& nom, const juce::String& aide, int largeurTexte);
    void preparerBascule (juce::TextButton&, const juce::String& texte, const juce::String& aide);
    void preparerBouton (juce::TextButton&, const juce::String& texte, const juce::String& aide, std::function<void()> action);
    void lierOnglet (int t);
    void attacher (Reglette&, const char* base);

    // Courbe de l'onglet affiche.
    Etat etatActuel() const;
    void appliquer (const Etat&, bool memoriserAvant);
    Etat etatOnglet (int t) const;
    Instantane instantane() const;
    void restaurer (const Instantane&);
    void majCalques();
    void memoriser();
    void annuler();
    void retablir();
    void choisirMembre (int famille, int membre);
    void generer (bool toutAuHasard);
    void regenerer();
    void transformer (std::function<bool (courbes::Courbe&)>);
    void afficherReglages();
    void majTitreMembre();
    void envoyerCourbe();
    void reprendreCourbe();
    void majLongueurs();
    void majGrille();
    void menuMesCourbes();
    void menuDesSeeds();
    PluginProcessor::Seed seedActuelle() const;
    bool evolutionAllumee (int t) const;
    juce::File fichierMesCourbes() const;

    // Evolution.
    bool evoCommune() const;
    void majEvolution (bool forcer);
    void appliquerStyle (int style);
    void reglerFamilles (std::function<float (int famille)>);

    static void placer (Reglette&, juce::Rectangle<int> zone, bool nomAGauche, int largeurNom);

    PluginProcessor& processor;
    bool ready = false; // vrai une fois la taille gardee restauree
    int onglet = 0;

    Style style;
    juce::TooltipWindow infobulles { this, 500 };

    juce::Label titre, version;
    juce::OwnedArray<BoutonOnglet> onglets;

    // Formes et familles.
    juce::ComboBox familles;
    juce::TextButton precedent { "<" }, suivant { ">" }, relancer, boutonGenerer, mesCourbes;
    juce::Label nomMembre;
    std::array<Reglette, courbes::nombreReglages> reglagesFamille;

    // Outils.
    BoutonFleche boutonAnnuler { false }, boutonRetablir { true };
    juce::TextButton boutonCrayon, boutonVisible, boutonInsectes, boutonPanneau, boutonInverser, boutonRetourner, boutonDoubler, boutonMoitie,
        boutonCopier, boutonColler, boutonActif, boutonMulti;
    std::unique_ptr<AttacheBouton> attacheActif;

    VueCourbe vue;
    juce::Label aide;

    // Temps.
    juce::Slider longueur { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label nomLongueur;
    std::unique_ptr<juce::ParameterAttachment> attacheLongueur;
    std::vector<int> longueursAutorisees;
    bool ternaireConnu = true;
    juce::TextButton ternaire, grilleTernaire, boutonAimant;
    std::unique_ptr<AttacheBouton> attacheTernaire, attacheGrilleTernaire;
    juce::ComboBox grille, depart;
    juce::Label nomGrille, nomDepart;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attacheGrille, attacheDepart;
    Reglette swing;

    // Sortie.
    Reglette decalage, minimum, maximum, lissage;
    juce::TextButton ecoute;
    std::unique_ptr<AttacheBouton> attacheEcoute;
    juce::Slider cc { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft }, canal { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    juce::Label nomCc, nomCanal, lectureCc;
    std::unique_ptr<AttacheSlider> attacheCc, attacheCanal;

    // Evolution.
    juce::TextButton evolution, lie, nouvelleSuite, suitePrecedente, revenirSeed, garderSeed, menuSeeds, aucunSolo;
    juce::ComboBox mesuresEvo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attacheMesures;
    std::unique_ptr<AttacheBouton> attacheEvolution, attacheLie;
    juce::Label nomSuite, titreReglages, titreIssues, titreFamilles, nomCycles;
    juce::ComboBox rythme;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attacheRythme;
    MiniVue vueJouee, vueProchaine;
    Reglette coherence;
    std::array<juce::TextButton, 3> styles;
    juce::Slider cycles { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    std::array<LigneEvo, 5> issues; // garder, revenir, varier, changer, intensite
    std::array<LigneEvo, courbes::nombreFamilles> poids;
    std::array<juce::Label, courbes::nombreGroupes> nomsGroupes;
    std::array<juce::TextButton, courbes::nombreGroupes> groupeTout, groupeRien;
    juce::Rectangle<int> zoneEvolution;
    std::vector<uint32_t> suitesPrecedentes;

    // Courbe affichee.
    int famille = 0, membre = 0;
    courbes::Reglage reglage;
    bool modifiee = false, majEnCours = false;
    int versionConnue = -1, versionLectureConnue = -1;
    std::vector<Instantane> historique, futur;
    std::array<int, nombreOnglets> versionsCalques {};
    bool multi = false;
    static std::unique_ptr<Etat> pressePapier;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
