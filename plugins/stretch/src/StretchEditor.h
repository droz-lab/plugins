#pragma once
#include "StretchProcessor.h"

namespace idg
{
/** Potards « arc néon » (même esprit que le rack IDGems). */
class StretchLookAndFeel : public juce::LookAndFeel_V4
{
public:
    StretchLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
};

/** Frise des pas : partie jouée (enregistrement de la frame), puis boucle figée. */
class StepStrip : public juce::Component
{
public:
    explicit StepStrip (StretchProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;

private:
    StretchProcessor& proc;
};

/** Filtre graphique : spectre de sortie + courbe du coupe-bas / coupe-haut.
    Glisser une poignée à gauche / à droite = fréquence ; vers le haut / le bas = résonance.
    Double-clic sur une poignée = remise à zéro. */
class FilterGraph : public juce::Component
{
public:
    explicit FilterGraph (StretchProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void updateSpectrum();

private:
    float xForFreq (float f) const;
    float freqForX (float x) const;
    float yForDb (float db) const;
    int handleAt (juce::Point<float>) const;
    juce::Point<float> handlePos (int which) const;

    StretchProcessor& proc;
    juce::RangedAudioParameter* params[3] {}; // hp, lp, reso
    juce::dsp::FFT fft { 12 };
    std::array<float, 8192> fftData {};
    std::array<float, 2048> spec {};
    int dragging = -1, hover = -1;
    float dragStartReso = 0.0f;
    float dragStartY = 0.0f;
};

class StretchEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit StretchEditor (StretchProcessor&);
    ~StretchEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateRateButtons();

    static constexpr int baseW = 760, baseH = 430;

    StretchProcessor& proc;
    StretchLookAndFeel lnf;

    struct Content : public juce::Component
    {
        void paint (juce::Graphics&) override;
        void resized() override;
        juce::OwnedArray<juce::TextButton> rateButtons;
        std::unique_ptr<juce::Slider> grain, reso;
        std::unique_ptr<StepStrip> strip;
        std::unique_ptr<FilterGraph> graph;
        juce::Rectangle<int> grainArea, resoArea, graphArea;
    } content;

    std::unique_ptr<juce::ParameterAttachment> rateAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> grainAtt, resoAtt;
    std::unique_ptr<juce::TooltipWindow> tooltip;
    int currentRate = -1;
    bool sizeReady = false;
};
} // namespace idg
