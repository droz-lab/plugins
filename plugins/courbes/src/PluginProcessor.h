#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <droz/dsp.h>

#include "Courbe.h"

#include <atomic>

/** Courbes : joue une courbe en boucle, calee sur le tempo, et l'envoie en MIDI CC.
    Instrument (pour qu'Ableton et Bitwig laissent router sa sortie MIDI) ; il ne fait du son
    que si "Ecoute" est monte : un son test module par la courbe, pour entendre la forme. */
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
    std::atomic<int> editorWidth { 820 }, editorHeight { 520 };

    //== Cote interface (fil de l'interface uniquement) ==========================================
    courbes::Courbe getCourbe() const;
    /** Remplace la courbe jouee. `famille` / `membre` servent a l'affichage (membre -1 = generee). */
    void setCourbe (const courbes::Courbe&, int famille, int membre, uint32_t graine);
    int getFamille() const noexcept { return famille.load(); }
    int getMembre() const noexcept { return membre.load(); }
    uint32_t getGraine() const noexcept { return graine.load(); }
    /** Augmente a chaque changement de courbe (rechargement d'un projet compris). */
    int getVersionCourbe() const noexcept { return versionCourbe.load(); }

    // Pour l'affichage : ou en est la lecture, et la valeur envoyee.
    std::atomic<float> phaseAffichee { 0.0f }, valeurAffichee { 0.0f };
    std::atomic<int> ccAffiche { -1 };

    static const juce::StringArray& nomsVitesses();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    double dureeCycleEnTemps (int indexVitesse, const juce::AudioPlayHead::TimeSignature&) const;

    // Courbe : copie gardee par l'interface, copie partagee sous SpinLock, copie du fil audio.
    mutable juce::CriticalSection verrouInterface;
    courbes::Courbe courbeInterface;
    juce::SpinLock verrouPartage;
    courbes::Courbe courbePartagee;
    courbes::Courbe courbeAudio;
    std::atomic<int> versionCourbe { 1 };
    int versionAudio = 0;
    std::atomic<int> famille { 0 }, membre { 0 };
    std::atomic<uint32_t> graine { 0 };

    std::atomic<float>* pVitesse = nullptr;
    std::atomic<float>* pPhase = nullptr;
    std::atomic<float>* pCc = nullptr;
    std::atomic<float>* pCanal = nullptr;
    std::atomic<float>* pMin = nullptr;
    std::atomic<float>* pMax = nullptr;
    std::atomic<float>* pLissage = nullptr;
    std::atomic<float>* pEcoute = nullptr;

    double sr = 48000.0;
    double phaseLibre = 0.0;     // position quand le transport est arrete
    double phaseTon = 0.0;       // son test de l'ecoute
    droz::Smoother sortie, ecoute, decalage, bas, haut;
    int dernierCc = -1, dernierNumero = -1, dernierCanal = -1;
    int compteurEnvoi = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
