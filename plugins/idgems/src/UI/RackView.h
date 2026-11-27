#pragma once
#include "EditorContext.h"

namespace idg::ui
{
/** Colonne de gauche : dry/wet + solo par famille, puis la chaîne d'effets (ordre en glisser-déposer). */
class RackView : public juce::Component
{
public:
    explicit RackView (EditorContext&);
    ~RackView() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverRow = -1; repaint(); }
    void tick();

private:
    struct FamilyRow;
    int rowAt (int y) const;
    juce::Rectangle<int> rowBounds (int slot) const;
    void showFxMenu (int fx);

    EditorContext& ctx;
    juce::OwnedArray<FamilyRow> families;
    juce::TextButton seqAll { "SEQ" };
    int listTop = 0;
    static constexpr int rowH = 20;
    int dragSlot = -1, dropSlot = -1, hoverRow = -1;
    bool dragging = false;
    int downZone = -1;
    float act[NumFx] {};
};
} // namespace idg::ui
