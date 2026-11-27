#pragma once
#include "EditorContext.h"

namespace idg::ui
{
/** Séquenceur on/off d'un effet : 32 pas, probabilité, ratchets, swing, longueur propre. */
class SeqEditor : public juce::Component
{
public:
    explicit SeqEditor (EditorContext&);
    void setEffect (int fx);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragStep = -1; }
    void tick();
    void refresh();

private:
    int stepAt (juce::Point<int>) const;
    juce::Rectangle<float> cell (int i) const;

    EditorContext& ctx;
    int fx = 0;
    juce::TextButton enable { "SEQ" }, rnd { "RND" }, lock { "LOCK" }, clear { "CLEAR" }, fill { "FILL" };
    juce::ComboBox rate;
    juce::Slider length, swing;
    juce::Label lengthLabel, swingLabel;
    juce::Rectangle<int> grid;
    int dragStep = -1;
    float dragStartProb = 1.0f;
    int lastCurrent = -2;
};
} // namespace idg::ui
