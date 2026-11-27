#pragma once
#include "EditorContext.h"

namespace idg::ui
{
/** Spectre détaillé : courbe lissée, échelle log, crêtes maintenues, fantômes chromatiques. */
class SpectrumView : public juce::Component
{
public:
    SpectrumView();
    void push (const float* data, int n);
    void setSampleRate (double sr) { if (sr > 0) sampleRate = sr; }
    void paint (juce::Graphics&) override;
    bool showGrid = true;
    float fillAlpha = 0.35f;
    float lineAlpha = 1.0f;

private:
    static constexpr int order = 11, size = 1 << order, bins = size / 2;
    juce::dsp::FFT fft { order };
    juce::dsp::WindowingFunction<float> window { (size_t) size, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, size> ring {};
    std::array<float, size * 2> work {};
    std::array<float, bins> db {}, peak {};
    int pos = 0, sinceFft = 0;
    double sampleRate = 48000.0;
};

/** Boîte de module : cadre « mal imprimé », étiquette, zone graphique. */
class ModuleBox : public juce::Component
{
public:
    ModuleBox (const juce::String& title, juce::Colour c) : title (title), colour (c) { setInterceptsMouseClicks (false, true); }
    void paint (juce::Graphics&) override;
    juce::Rectangle<int> content() const { return getLocalBounds().reduced (6).withTrimmedTop (18); }
    juce::String title;
    juce::Colour colour;
};

/** Petites vues graphiques de la chaîne de sortie (lisent les paramètres et la télémétrie du processeur). */
class OutputGraph : public juce::Component
{
public:
    enum Kind { HighPass, Mono, Ott, Clip, Limiter };
    OutputGraph (IDGemsProcessor& p, Kind k) : proc (p), kind (k) { setInterceptsMouseClicks (false, false); }
    void tick();
    void paint (juce::Graphics&) override;

private:
    void paintHp (juce::Graphics&, juce::Rectangle<float>);
    void paintMono (juce::Graphics&, juce::Rectangle<float>);
    void paintOtt (juce::Graphics&, juce::Rectangle<float>);
    void paintClip (juce::Graphics&, juce::Rectangle<float>);
    void paintLim (juce::Graphics&, juce::Rectangle<float>);
    IDGemsProcessor& proc;
    Kind kind;
    float clipLevel = 0.0f, clipPeak = 0.0f;
    float ottLvl[3] { -90, -90, -90 }, ottGr[3] {};
    std::array<float, 120> grHist {};
    int histPos = 0;
};
} // namespace idg::ui
