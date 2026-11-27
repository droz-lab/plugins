// Custom views of the Markov interface: track rows, transition matrix, step grid.
#pragma once

#include "PluginProcessor.h"

#include <droz/ui.h>

namespace markovui
{
/** The plugin's single own colour (LEDs, playhead, current state). Everything else is monochrome. */
inline const juce::Colour accent { 0xffffb347 };

inline juce::String fr (const char* utf8) { return juce::String::fromUTF8 (utf8); }

//==============================================================================
/** One track in the list: activity LED, name and channel, "active" switch. Click = select. */
class TrackRow final : public juce::Component, public juce::SettableTooltipClient
{
public:
    TrackRow (PluginProcessor& p, int index, std::function<void (int)> onSelect);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    void setSelected (bool s);
    void setLed (float level);

private:
    PluginProcessor& processor;
    const int track;
    std::function<void (int)> select;
    bool selected = false;
    float led = 0.0f;
    juce::ToggleButton enabled;
    juce::AudioProcessorValueTreeState::ButtonAttachment enabledAttachment;
};

//==============================================================================
/** Transition matrix of the selected track: row = current degree, column = next degree.
    Drag up or down on a cell to change its weight, double-click = default value. */
class MatrixView final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit MatrixView (PluginProcessor& p);

    void setTrack (int t) { track = t; repaint(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> gridArea (int size) const;
    bool cellAt (juce::Point<float>, int size, int& row, int& col) const;

    PluginProcessor& processor;
    int track = 0;
    int dragRow = -1, dragCol = -1;
    float dragStart = 0.0f;
};

//==============================================================================
/** Step grid of the selected track, three lanes: play probability, velocity, velocity spread.
    Click or drag to draw values, double-click = default value of that step. */
class StepView final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit StepView (PluginProcessor& p);

    void setTrack (int t) { track = t; repaint(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    static constexpr int maxShownSpread = 63;

private:
    int numSteps() const;
    juce::Rectangle<float> lane (int index) const;
    void edit (juce::Point<float>, bool toDefault);

    PluginProcessor& processor;
    int track = 0;
    int dragLane = -1;
};
} // namespace markovui
