#pragma once
#include "Core/ParamBank.h"
#include "Effects/Rack.h"
#include "Modulation/ModEngine.h"
#include "Engine/Sequencer.h"
#include "Engine/OutputChain.h"
#include "Engine/Recorder.h"
#include "State/RandomEngine.h"
#include "State/Snapshots.h"
#include "State/PresetManager.h"

/** Processeur principal d'IDGems. */
class IDGemsProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    IDGemsProcessor();
    ~IDGemsProcessor() override;

    // ---- AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "IDGems"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- État complet (paramètres + état étendu)
    juce::ValueTree captureState (bool forPreset = false) const;
    void restoreState (const juce::ValueTree& state, bool fromPreset = false);
    void resetToInit();

    // ---- Paramètres (thread interface)
    juce::RangedAudioParameter* param (int index) const { return params[(size_t) index]; }
    float getNorm (int index) const { return params[(size_t) index]->getValue(); }
    void setNorm (int index, float v);
    void setNormGesture (int index, float v);
    float getValue (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    void setValue (const juce::String& id, float realValue);
    int indexOf (const juce::String& id) const { return idg::registry().find (id); }

    // ---- Effets
    void initEffect (int fx);
    void randomEffectOrder (juce::Random& r, float amount);
    juce::ValueTree copyEffect (int fx) const;
    void pasteEffect (int fx, const juce::ValueTree& t);

    // ---- Undo / redo / A-B
    void pushUndo();
    void undo();
    void redo();
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }
    void switchAB (int slot);
    void copyAtoB();
    int getABSlot() const { return abSlot; }

    // ---- Snapshots / morph
    void storeSnapshot (int i);
    void recallSnapshot (int i);
    void clearSnapshot (int i);
    void applyMorph (float x, float y);
    idg::Snapshot snapshots[idg::NumSnapshots];
    idg::ArrangeSlot arrange[idg::NumArrangeSlots];
    std::atomic<bool> morphEnabled { false };
    std::atomic<bool> seqMaster { true };   // interrupteur général des séquenceurs
    std::atomic<float> editorScale { 0.0f }; // taille de la fenêtre (0 = jamais ouverte), sauvée avec le projet
    std::atomic<bool> arrangePlaying { false };
    std::atomic<int> arrangeCurrent { -1 };

    // ---- Divers
    void panic();
    void setMomentary (int fx, bool down);
    double getBpm() const { return uiBpm.load(); }
    double getPpq() const { return uiPpq.load(); }
    int64_t getBeatCounter() const { return beatCounter.load(); }
    int64_t getBarCounter() const { return barCounter.load(); }
    int getCaptureState() const { return captureState_.load(); }
    bool zeroLatency() const { return getValue (idg::G::zeroLat) > 0.5f; }

    // MIDI learn
    void startMidiLearn (int paramIndex) { learnTarget = paramIndex; }
    int getMidiLearnTarget() const { return learnTarget; }
    void clearMidiLearn (int paramIndex);
    int ccForParam (int paramIndex) const;

    // ---- Composants publics (partagés avec l'interface)
    juce::AudioProcessorValueTreeState apvts;
    idg::ParamBank bank;
    idg::RackShared rackShared;
    idg::ModShared modShared;
    idg::SeqData seqData[idg::NumFx];
    idg::Recorder recorder;
    idg::RandomEngine random { *this };
    std::unique_ptr<idg::PresetManager> presets;

    std::atomic<float> meterIn[2], meterOut[2], meterGR { 1.0f }, outLevel { 0.0f };
    juce::AbstractFifo scopeFifo { 4096 };
    std::vector<float> scopeData = std::vector<float> (4096, 0.0f);

    std::atomic<int> ccToParam[128];
    juce::AbstractFifo ccFifo { 256 };
    std::array<std::pair<int, int>, 256> ccEvents {};

    int perfPads[8] { idg::FxStutter, idg::FxReverse, idg::FxTapeStop, idg::FxFreeze,
                      idg::FxBeatRepeat, idg::FxShuffle, idg::FxDelay, idg::FxReverb };

    // Réglages de l'interface (communs à toutes les instances)
    juce::ApplicationProperties appProps;
    juce::Colour getAccent();
    void setAccent (juce::Colour c);

    std::function<void()> onStateRestored; // l'interface se rafraîchit
    idg::OutputChain& getOutputChain() { return output; }

private:
    void timerCallback() override;
    void updateTransport (int numSamples);
    void processChunk (float* L, float* R, const float* scL, const float* scR, int n, bool hasSidechain);

    std::vector<juce::RangedAudioParameter*> params;
    idg::Rack rack;
    idg::ModEngine modEngine;
    idg::SeqRuntime seqRt[idg::NumFx];
    idg::OutputChain output;
    idg::dsp::Rng audioRng;
    idg::Scale scale;

    double sr = 44100.0;
    int maxBlock = 512;
    // transport
    double ppq = 0.0, bpm = 120.0;
    bool playing = false;
    int beatsPerBar = 4;
    double lastHostPpq = -1.0;
    std::atomic<double> uiBpm { 120.0 }, uiPpq { 0.0 };
    std::atomic<int64_t> beatCounter { 0 }, barCounter { 0 };

    // MIDI / déclenchements
    std::atomic<bool> midiHeld[idg::NumFx];
    bool pendingTrig[idg::NumFx] {};
    bool lastMomentary[idg::NumFx] {};
    bool midiLfoTrigger = false;

    // transitoires
    float trFast = 0, trSlow = 0;
    int trHold = 0;

    // capture d'entrée
    std::atomic<int> captureState_ { 0 }; // 0 off, 1 armée, 2 enregistre, 3 boucle
    std::vector<float> capBuf[2];
    int capLen = 0, capPos = 0, capTarget = 0;
    float capMix = 0.0f;
    int64_t capArmBeat = 0;

    // feedback global
    idg::dsp::RingBuffer fbBuf;
    idg::dsp::SVF fbHp;

    // dry/wet global
    idg::dsp::LatencyDelay dryDelay;
    std::vector<float> dryL, dryR;
    juce::SmoothedValue<float> inGainSm, globalMixSm;

    std::atomic<bool> panicFlag { false };
    int panicMute = 0;
    int lastLatency = -1;
    bool lastZeroLat = false;

    struct GlobalIdx
    {
        int capture, captureLen, transOn, transSens, transSrc, envSrc, lfoTrig, scaleOn, scaleKey, scaleType, pitchSnap, destOS,
            hpFreq, monoOn, monoFreq, ottDepth, ottTime, ottUp, ottDown, ottGain, clipDrive, ceiling, limRelease, zeroLat;
    } gi {};
    inline float rv (int i) const { return bank[i].raw->load (std::memory_order_relaxed); }
    double driftTime = 0.0;

    // indices fréquents
    int idxChaos, idxGlobalMix, idxInGain, idxOutGain, idxFbAmount, idxFbTime, idxMorphX, idxMorphY;

    // undo
    std::vector<juce::ValueTree> undoStack, redoStack;
    juce::ValueTree abStates[2];
    int abSlot = 0;
    int learnTarget = -1;
    float lastMorphX = -1, lastMorphY = -1;
    int64_t arrangeStartBar = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IDGemsProcessor)
};
