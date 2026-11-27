#pragma once
#include "../Core/JuceIncludes.h"

namespace idg
{
/** Enregistreur : rolling recorder (dernières secondes en permanence) + enregistrement manuel sur disque. */
class Recorder
{
public:
    Recorder();
    ~Recorder();

    void prepare (double sampleRate);
    /** Thread audio. */
    void push (const float* L, const float* R, int n);

    // Thread interface
    juce::File saveRolling (float seconds, double bpm);
    bool startRecording (double bpm);
    juce::File stopRecording();
    bool isRecording() const { return activeWriter.load() != nullptr; }
    double recordedSeconds() const { return sr > 0 ? (double) recordedSamples.load() / sr : 0.0; }
    static juce::File folder();
    static juce::File makeFileName (double bpm, const juce::String& suffix);

    static constexpr int maxRollingSeconds = 60;

private:
    double sr = 44100.0;
    std::vector<float> ring[2];
    std::atomic<int> writePos { 0 };
    std::atomic<int64_t> totalWritten { 0 };
    std::atomic<int64_t> recordedSamples { 0 };

    juce::TimeSliceThread thread { "IDGems Recorder" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threaded;
    juce::CriticalSection writerLock;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeWriter { nullptr };
    juce::File currentFile;
};
} // namespace idg
