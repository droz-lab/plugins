#pragma once
#include "Theme.h"

namespace idg::ui
{
class IDGLookAndFeel : public juce::LookAndFeel_V4
{
public:
    IDGLookAndFeel();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return Theme::mono (11.0f); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return Theme::mono (juce::jmin (12.0f, h * 0.55f), true); }
    juce::Font getPopupMenuFont() override { return Theme::mono (13.0f); }
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minP, float maxP,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize, bool over, bool down) override;
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
};
} // namespace idg::ui
