#pragma once
#include "EditorContext.h"

namespace idg::ui
{
/** Potard lié à un paramètre : anneaux de modulation, plage de random, cadenas, MIDI learn, cible de glisser-déposer. */
class Knob : public juce::Slider, public juce::DragAndDropTarget
{
public:
    Knob (EditorContext& ctx, int paramIndex, juce::Colour colour, const juce::String& label = {});
    ~Knob() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    juce::String getTooltip() override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dropHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dropHover = false; repaint(); }
    void itemDropped (const SourceDetails&) override;

    /** Appelé par le timer de l'éditeur : redessine si la valeur modulée a bougé. */
    void tick();
    int getParamIndex() const { return paramIndex; }
    void setColour (juce::Colour c) { colour = c; repaint(); }
    void setBig (bool b) { big = b; }

private:
    void showMenu();
    juce::String valueText() const;

    EditorContext& ctx;
    int paramIndex;
    juce::Colour colour;
    juce::String label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    bool dropHover = false, hovering = false, big = false;
    float lastDisplay = -1.0f;
    int lastModCount = -1;
    int altSlot = -1;
    float altStartDepth = 0.0f;
    float flicker = 0.0f;
    juce::Random rng;
};
} // namespace idg::ui
