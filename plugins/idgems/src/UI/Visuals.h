#pragma once
#include "EditorContext.h"

namespace idg::ui
{
/** Fond noir texturé (bruit, rayures, trame) généré une fois. */
juce::Image makeBackgroundTexture (int w, int h, juce::Colour accent);

/** Oscilloscope + spectre « corrompus » (données fournies par l'éditeur). */
class CorruptScope : public juce::Component
{
public:
    void push (const float* data, int n);
    void paint (juce::Graphics&) override;
    void setIntensity (float i) { intensity = i; }
    float alpha = 0.55f;

private:
    std::array<float, 1024> wave {};
    std::array<float, 64> bands {};
    int writePos = 0;
    float intensity = 0.0f;
    juce::Random rng;
    juce::dsp::FFT fft { 9 };
    std::array<float, 1024> fftData {};
};

/** Calque de braises / particules qui réagit au son (sans intercepter la souris). */
class GlitchOverlay : public juce::Component
{
public:
    GlitchOverlay() { setInterceptsMouseClicks (false, false); }
    void setLevel (float l);
    void paint (juce::Graphics&) override;

private:
    struct Spark { float x = 0, y = 0, vx = 0, vy = 0, size = 1, life = 0, decay = 0.01f; bool hot = true; };
    std::array<Spark, 140> sparks;
    float level = 0.0f, flash = 0.0f;
    juce::Random rng;
    int frame = 0;
};

/** Vu-mètres entrée / sortie + réduction de gain. */
class Meters : public juce::Component
{
public:
    explicit Meters (IDGemsProcessor& p) : proc (p) {}
    void tick();
    void paint (juce::Graphics&) override;

private:
    IDGemsProcessor& proc;
    float in[2] {}, out[2] {}, gr = 0, peakOut = 0;
    int peakHold = 0;
};
} // namespace idg::ui
