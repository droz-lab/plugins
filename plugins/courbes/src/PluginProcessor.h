#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <droz/dsp.h>

#include "Courbe.h"

#include <array>
#include <atomic>
#include <limits>
#include <vector>

/** Une longueur de cycle (ou de case de grille), en noires ou en mesures. */
struct Duree
{
    const char* nom;
    double noires;   // > 0 : duree fixe en noires
    double mesures;  // > 0 : duree en mesures (depend de la signature rythmique)
    bool ternaire;
};

constexpr int nombreOnglets = 8;

/** Reglages d'evolution : garder, revenir, varier, changer, intensite, cycles, puis un poids par famille. */
namespace evo
{
enum : int
{
    garder = 0,
    revenir,
    varier,
    changer,
    intensite,
    cycles,
    poids0,
    total = poids0 + courbes::nombreFamilles
};
}

/** Droz Courbes : 8 courbes en boucle (onglets), calees sur le tempo, chacune envoyee sur son CC.
    Instrument (pour qu'Ableton et Bitwig laissent router sa sortie MIDI) ; il ne fait du son
    que si "Ecoute" est allume : un son test module par la courbe de l'onglet affiche. */
class PluginProcessor final : public juce::AudioProcessor
{
public:
    PluginProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    /** Identifiant d'un parametre d'onglet : "o<numero>_<base>". */
    static juce::String id (int onglet, const char* base);
    juce::RangedAudioParameter* param (int onglet, const char* base) const;
    float valeur (int onglet, const char* base) const;

    // Interface : taille, aimant, onglet affiche (garde avec le projet).
    std::atomic<int> editorWidth { 1320 }, editorHeight { 860 };
    std::atomic<bool> aimant { true };
    std::atomic<int> ongletAffiche { 0 };
    std::array<std::atomic<bool>, nombreOnglets> visible {};
    std::atomic<bool> insectes { true }, panneauEvolution { true };

    //== Durees ===================================================================================
    static const std::vector<Duree>& longueurs();
    static const std::vector<Duree>& grilles();
    static constexpr int longueurParDefaut = 12; // 2 mesures
    static int longueurJouee (int index, bool ternaireAutorise);
    static double enNoires (const Duree&, double noiresParMesure);
    /** Longueur du cycle et taille d'une case de grille de l'onglet, en noires. */
    double cycleNoires (int onglet, double npm) const;
    double caseNoires (int onglet) const;

    //== Courbes (fil de l'interface) =============================================================
    /** Courbe en cours de l'onglet (celle qu'on modifie ; l'evolution repart d'elle). */
    courbes::Courbe getCourbe (int onglet) const;
    courbes::Reglage getReglage (int onglet) const;
    void setCourbe (int onglet, const courbes::Courbe&, int famille, int membre, const courbes::Reglage&);

    /** Seed : la courbe de depart de l'evolution (a la relecture, l'evolution repart d'elle). */
    struct Seed
    {
        courbes::Courbe courbe;
        int famille = 0;
        courbes::Reglage reglage;
        juce::String nom;
    };
    Seed getSeed (int onglet) const;
    void setSeed (int onglet, const Seed&);
    std::vector<Seed> getSeedsStockees (int onglet) const;
    void stockerSeed (int onglet, const Seed&);
    void supprimerSeed (int onglet, int index);
    int getFamille (int onglet) const noexcept { return famille[static_cast<size_t> (onglet)].load(); }
    int getMembre (int onglet) const noexcept { return membre[static_cast<size_t> (onglet)].load(); }
    int getVersionCourbe (int onglet) const noexcept { return versionCourbe[static_cast<size_t> (onglet)].load(); }

    /** Ce que joue l'evolution : courbe en cours, prochaine courbe, dans combien de cycles. */
    struct Lecture
    {
        courbes::Courbe jouee, prochaine;
        int familleJouee = 0, familleProchaine = 0;
        courbes::Reglage reglageJouee;
        int versionCourbe = 0; // version de la courbe en cours que l'audio a deja prise en compte
        bool prochaineValide = false, joueeEstLaBase = true;
        int cyclesAvant = 0;
    };
    void getLecture (int onglet, Lecture&) const;
    int getVersionLecture (int onglet) const noexcept { return versionLecture[static_cast<size_t> (onglet)].load(); }

    //== Reglages d'evolution =====================================================================
    /** Reglages communs (parametres du DAW) ou propres a un onglet (gardes dans le projet). */
    float getEvo (int onglet, bool communs, int champ) const;
    void setEvo (int onglet, bool communs, int champ, float v);
    static juce::String idEvoCommun (int champ);
    /** Multi-solo des familles : si au moins une famille est en solo, seules celles-la sont tirees (a parts egales). */
    bool getSolo (int onglet, bool communs, int famille) const;
    void setSolo (int onglet, bool communs, int famille, bool solo);
    static const std::vector<std::pair<const char*, double>>& mesuresEvolution(); // nom, duree en mesures
    uint32_t getGraine() const noexcept { return graineEvolution.load(); }
    void setGraine (uint32_t g) noexcept { graineEvolution = g; }

    // Pour l'affichage.
    std::array<std::atomic<float>, nombreOnglets> phaseAffichee {}, valeurAffichee {};
    std::array<std::atomic<int>, nombreOnglets> ccAffiche {};
    std::atomic<double> noiresParMesure { 4.0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct Voix
    {
        courbes::Courbe base, jouee, prochaine;
        courbes::Reglage rBase, rJouee, rProchaine;
        int fBase = 0, fJouee = 0, fProchaine = 0;
        bool prochaineValide = false, prochaineEstLaBase = false, joueeEstLaBase = true;
        int64_t cycleProchaine = 0, dernierCycle = std::numeric_limits<int64_t>::min();
        bool aPublier = true;
        double phaseLibre = 0.0, positionNote = 0.0;
        droz::Smoother sortie, decalage, bas, haut;
        int dernierCc = -1, numero = -1, canal = -1, compteurEnvoi = 0;
        float valeur = 0.0f;
        int versionAudio = 0, versionSeedAudio = 0;
    };

    void surCycle (int onglet, int64_t cycle, bool suite, double cycleNoires, double npm) noexcept;
    void preparerProchaine (int onglet, int64_t apres, double cycleNoires, double npm) noexcept;
    courbes::Evolution lireEvolution (int onglet, bool communs, int& cycles) const noexcept;
    void publier (int onglet) noexcept;

    std::array<Voix, nombreOnglets> voix;

public:
    /** Parametres de chaque onglet, dans l'ordre de nomsParametresOnglet(). */
    enum Champ : int { actif = 0, longueur, ternaire, grille, grilleTernaire, phase, cc, canal, min, max, lissage,
                       swing, depart, evolution, lie, coherence, nombreChamps };
    static const char* nomParametreOnglet (int champ);

private:
    std::array<std::array<std::atomic<float>*, nombreChamps>, nombreOnglets> pOnglet {};
    float lire (int onglet, int champ) const noexcept { return pOnglet[static_cast<size_t> (onglet)][static_cast<size_t> (champ)]->load(); }

    // Courbe de base : copie de l'interface, copie partagee sous SpinLock.
    mutable juce::CriticalSection verrouInterface;
    std::array<courbes::Courbe, nombreOnglets> courbeInterface;
    std::array<courbes::Reglage, nombreOnglets> reglageInterface;
    juce::SpinLock verrouPartage;
    std::array<courbes::Courbe, nombreOnglets> courbePartagee;
    std::array<courbes::Reglage, nombreOnglets> reglagePartage;
    std::array<int, nombreOnglets> famillePartagee {};
    std::array<std::atomic<int>, nombreOnglets> versionCourbe {}, famille {}, membre {}, versionSeed {};
    std::array<Seed, nombreOnglets> seedInterface;
    std::array<std::vector<Seed>, nombreOnglets> seedsStockees;
    std::array<courbes::Courbe, nombreOnglets> seedPartagee;
    std::array<courbes::Reglage, nombreOnglets> reglageSeedPartage;
    std::array<int, nombreOnglets> familleSeedPartagee {};
    std::array<std::array<std::atomic<bool>, courbes::nombreFamilles>, nombreOnglets> soloPropre {};
    std::array<std::atomic<bool>, courbes::nombreFamilles> soloCommun {};

    // Retour vers l'interface de ce que joue l'evolution.
    mutable juce::SpinLock verrouLecture;
    std::array<Lecture, nombreOnglets> lectureUI;
    std::array<std::atomic<int>, nombreOnglets> versionLecture {};

    // Reglages d'evolution propres a chaque onglet (non automatisables).
    std::array<std::array<std::atomic<float>, evo::total>, nombreOnglets> evoPropre;
    std::array<std::atomic<float>*, evo::total> evoCommun {};
    std::atomic<uint32_t> graineEvolution { 12345 };

    std::atomic<float>* pEcoute = nullptr;
    std::atomic<float>* pRythme = nullptr;
    std::atomic<float>* pMesures = nullptr;
    double sr = 48000.0;
    double phaseTon = 0.0;
    droz::Smoother ecoute, ton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
