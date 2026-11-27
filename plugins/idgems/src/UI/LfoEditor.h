#pragma once
#include "EditorContext.h"

namespace idg::ui
{
/** Éditeur de forme de LFO : points + courbes, ou crayon sur une grille. */
class LfoEditor : public juce::Component
{
public:
    explicit LfoEditor (EditorContext&);
    void setLfo (int i) { lfo = i; repaint(); }
    void setPencil (bool p) { pencil = p; }
    void setGrid (int g) { grid = g; repaint(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Point<float> toScreen (float x, float y) const;
    juce::Point<float> fromScreen (juce::Point<float>) const;
    int hitPoint (juce::Point<float>) const;
    void publish (const LfoShape& s);
    void pencilAt (juce::Point<float>);

    EditorContext& ctx;
    int lfo = 0;
    bool pencil = false;
    int grid = 16;
    int dragPt = -1;
    int curveSeg = -1;
    float curveStart = 0.0f;
    LfoShape work;
};
} // namespace idg::ui
