#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <droz/dsp.h>

#include "MarkovEngine.h"

#include <array>
#include <atomic>

/** MIDI generator: 8 tracks, each sent on its own MIDI channel (1 to 8) of the single MIDI output.
    No audio input. The audio output only carries the optional preview ("ecoute"). */
class PluginProcessor final : public juce::AudioProcessor, private juce::Timer
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;

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

    //==============================================================================
    // Message thread API (interface)
    static juce::String trackParam (int track, const char* name) { return "p" + juce::String (track + 1) + "_" + name; }

    /** Copy of one track's data (matrices, steps). */
    markov::TrackData getTrack (int track) const;
    /** Edits one track's data. `edit` runs under the lock, keep it short. */
    void editTrack (int track, const std::function<void (markov::TrackData&)>& edit);
    markov::Scale currentScale() const;

    void resetLearning();
    void applyLearning(); // called by the timer while learning; public for tests

    juce::AudioProcessorValueTreeState apvts;

    // Window size: kept in the plugin state, otherwise the DAW loses it when reopening.
    static constexpr int baseWidth = 1120, baseHeight = 760;
    std::atomic<int> editorWidth { 980 }, editorHeight { 665 };
    std::atomic<int> selectedTrack { 0 }; // shown in the interface, target of the learning

    // Audio -> interface (simple values only)
    std::atomic<double> hostBpm { 0.0 };
    std::atomic<bool> hostPlaying { false };
    std::array<std::atomic<int>, markov::numTracks> currentStep {};
    std::array<std::atomic<int>, markov::numTracks> currentDegree {};
    std::array<std::atomic<uint32_t>, markov::numTracks> notesPlayed {};
    std::atomic<uint32_t> learnedNotes { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override { applyLearning(); }

    struct TrackRuntime
    {
        markov::TrackSim sim;
        int64_t nextStep = 0;
        int activeNote = -1;
        double offPpq = 0.0;
        int division = -1;
        bool enabled = false;
    };

    void syncPattern();
    void readSettings();
    void resync (int track, double ppq);
    void allNotesOff (juce::MidiBuffer& midi, int sample);
    void renderPreview (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi);

    // Shared pattern: the message thread owns `master`, the audio thread copies it into `working`
    // with a try-lock only (keeps the old copy if the lock is taken).
    markov::Pattern master, working;
    mutable juce::SpinLock patternLock;
    std::atomic<uint32_t> patternVersion { 1 };
    uint32_t workingVersion = 0;

    // Learning: the audio thread counts, then publishes a copy under a try-lock.
    markov::Learner learner, learnedCopy, learnedApplied;
    juce::SpinLock learnLock;
    std::atomic<uint32_t> learnVersion { 0 };
    uint32_t learnAppliedVersion = 0;
    std::atomic<bool> learnResetRequest { false };

    // Audio thread state
    std::array<markov::ChainTables, markov::numTracks> tables;
    std::array<markov::TrackSettings, markov::numTracks> settings;
    std::array<TrackRuntime, markov::numTracks> runtime;
    markov::Scale scale;
    float temperature = 1.0f;
    uint32_t seed = 1;
    bool tablesDirty = true;
    bool wasPlaying = false;
    double expectedPpq = 0.0;
    double sampleRate = 48000.0;

    // Preview voices (one per track)
    struct Voice
    {
        double phase = 0.0, increment = 0.0;
        float env = 0.0f, peak = 0.0f;
        bool attack = false, held = false;
    };
    std::array<Voice, markov::numTracks> voices;
    droz::Smoother previewGain;
    float attackCoeff = 0.0f, decayCoeff = 0.0f, releaseCoeff = 0.0f;

    // Parameters
    std::atomic<float>* pScale = nullptr;
    std::atomic<float>* pRoot = nullptr;
    std::array<std::atomic<float>*, 12> pCustom {};
    std::atomic<float>* pSeed = nullptr;
    std::atomic<float>* pTemperature = nullptr;
    std::atomic<float>* pOrder = nullptr;
    std::atomic<float>* pRhythm = nullptr;
    std::atomic<float>* pLearn = nullptr;
    std::atomic<float>* pPreview = nullptr;
    std::atomic<float>* pPreviewLevel = nullptr;
    struct TrackParams
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* lo = nullptr;
        std::atomic<float>* hi = nullptr;
        std::atomic<float>* steps = nullptr;
        std::atomic<float>* division = nullptr;
        std::atomic<float>* gate = nullptr;
        std::atomic<float>* afterHit = nullptr;
        std::atomic<float>* afterRest = nullptr;
    };
    std::array<TrackParams, markov::numTracks> trackParams;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};

namespace markovui
{
/** Note names in French (Do, Ré...), with octave: 60 = Do4. */
juce::String noteName (int midiNote, bool withOctave = true);
const juce::StringArray& scaleNames();
const juce::StringArray& divisionNames();
double divisionPpq (int index);
} // namespace markovui
