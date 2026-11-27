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

/** Courbes : joue une courbe en boucle, calee sur le tempo, et l'envoie en MIDI CC.
    Instrument (pour qu'Ableton et Bitwig laissent router sa sortie MIDI) ; il ne fait du son
    que si "Ecoute" est allume : un son test module par la courbe, pour entendre la forme. */
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

    // Taille de la fenetre : gardee dans l'etat du plugin, sinon le DAW la perd a la reouverture.
    std::atomic<int> editorWidth { 1240 }, editorHeight { 780 };
    // Aimant de la grille dans l'editeur (garde avec le projet).
    std::atomic<bool> aimant { true };

    //== Durees ===================================================================================
    static const std::vector<Duree>& longueurs();   // choix du parametre "longueur"
    static const std::vector<Duree>& grilles();     // choix du parametre "grille" (binaire)
    static constexpr int longueurParDefaut = 10;    // 1 mesure
    /** Index de longueur reellement joue : sans le ternaire, une valeur ternaire passe a la suivante. */
    static int longueurJouee (int index, bool ternaireAutorise);
    static double enNoires (const Duree&, double noiresParMesure);

    //== Cote interface (fil de l'interface uniquement) ==========================================
    courbes::Courbe getCourbe() const;
    courbes::Reglage getReglage() const;
    /** Remplace la courbe jouee. `membre` -1 = reglee ou generee a la main. */
    void setCourbe (const courbes::Courbe&, int famille, int membre, const courbes::Reglage&);
    int getFamille() const noexcept { return famille.load(); }
    int getMembre() const noexcept { return membre.load(); }
    /** Augmente a chaque changement de courbe (rechargement d'un projet compris). */
    int getVersionCourbe() const noexcept { return versionCourbe.load(); }

    // Pour l'affichage : ou en est la lecture, la valeur envoyee, la signature du morceau.
    std::atomic<float> phaseAffichee { 0.0f }, valeurAffichee { 0.0f };
    std::atomic<int> ccAffiche { -1 };
    std::atomic<double> noiresParMesure { 4.0 };

    /** Courbe en train d'etre jouee (differente de la courbe de base quand l'evolution est active). */
    void getCourbeJouee (courbes::Courbe& c, int& familleJouee) const;
    int getVersionJouee() const noexcept { return versionJouee.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Courbe : copie gardee par l'interface, copie partagee sous SpinLock, copie du fil audio.
    mutable juce::CriticalSection verrouInterface;
    courbes::Courbe courbeInterface;
    courbes::Reglage reglageInterface;
    juce::SpinLock verrouPartage;
    courbes::Courbe courbePartagee;
    courbes::Reglage reglagePartage;
    int famillePartagee = 0;

    // Fil audio : courbe de base (celle de l'editeur) et courbe jouee (apres evolution).
    courbes::Courbe courbeBase, courbeAudio;
    courbes::Reglage reglageBase, reglageAudio;
    int familleBase = 0, familleAudio = 0;
    int64_t dernierCycle = std::numeric_limits<int64_t>::min();
    int compteurCycles = 0;
    bool joueeChangee = false;   // a publier vers l'interface
    bool joueeDifferente = false; // la courbe jouee n'est plus la courbe de base
    void evoluer (int64_t cycle) noexcept;
    void publierJouee() noexcept;

    // Retour vers l'interface de la courbe jouee.
    mutable juce::SpinLock verrouJoue;
    courbes::Courbe courbeJoueeUI;
    int familleJoueeUI = 0;
    std::atomic<int> versionJouee { 0 };
    std::atomic<uint32_t> graineEvolution { 12345 };
    std::atomic<int> versionCourbe { 1 };
    int versionAudio = 0;
    std::atomic<int> famille { 0 }, membre { 0 };

    std::atomic<float>* pLongueur = nullptr;
    std::atomic<float>* pTernaire = nullptr;
    std::atomic<float>* pPhase = nullptr;
    std::atomic<float>* pCc = nullptr;
    std::atomic<float>* pCanal = nullptr;
    std::atomic<float>* pMin = nullptr;
    std::atomic<float>* pMax = nullptr;
    std::atomic<float>* pLissage = nullptr;
    std::atomic<float>* pEcoute = nullptr;
    std::atomic<float>* pEvolution = nullptr;
    std::atomic<float>* pEvolCycles = nullptr;
    std::atomic<float>* pGarder = nullptr;
    std::atomic<float>* pRevenir = nullptr;
    std::atomic<float>* pVarier = nullptr;
    std::atomic<float>* pChanger = nullptr;
    std::atomic<float>* pIntensite = nullptr;
    std::array<std::atomic<float>*, courbes::nombreFamilles> pPoids {};

    double sr = 48000.0;
    double phaseLibre = 0.0; // position quand le transport est arrete
    double phaseTon = 0.0;   // son test de l'ecoute
    droz::Smoother sortie, ecoute, decalage, bas, haut, ton;
    int dernierCc = -1, dernierNumero = -1, dernierCanal = -1;
    int compteurEnvoi = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
