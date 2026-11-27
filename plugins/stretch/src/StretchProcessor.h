#pragma once
#include "Core/JuceIncludes.h"
#include "Effects/StretchEngine.h"

namespace idg
{
/** Petit VST3 autonome « IDGems Stretch » : l'effet seul, pour le régler à l'oreille
    avant de le réintégrer dans le rack IDGems. */
class StretchProcessor : public juce::AudioProcessor
{
public:
    static const juce::StringArray& rateNames();
    static double rateBeats (int index);

    StretchProcessor();

    void prepareToPlay (double sampleRate, int maxBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "IDGems Stretch"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> editorScale { 0.0f };
    std::atomic<int> editorW { 0 }, editorH { 0 }; // taille exacte en pixels (0 = inconnue)

    // pour l'affichage : position dans le pas courant (0..1) et pas courant
    std::atomic<float> uiPhase { 0.0f };
    std::atomic<int> uiStep { 0 };
    std::atomic<float> uiLevel { 0.0f };
    std::atomic<float> uiCapture { 0.25f }; // part du pas jouée avant le gel
    std::atomic<float> uiRepeats { 5.0f };  // répétitions du grain par pas

    // copie de la sortie (mono) pour le spectre de l'interface : écrite par l'audio, lue par l'UI
    static constexpr int scopeSize = 4096;
    std::array<float, scopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };

private:
    StretchEngine engine;
    std::atomic<float>* pRate = nullptr, *pGrain = nullptr, *pHp = nullptr, *pLp = nullptr, *pReso = nullptr;
    double sr = 44100.0;
    double freeBeat = 0.0; // horloge interne quand le transport est arrêté
};
} // namespace idg
