#pragma once
#include "Knob.h"

namespace idg::ui
{
/** Onglet PERFORMANCE : pad XY de morphing entre 8 snapshots, arrangement, pads momentanés. */
class PerfPanel : public juce::Component
{
public:
    explicit PerfPanel (EditorContext&);
    ~PerfPanel() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    void tick();
    void refresh();

private:
    struct XYPad;
    struct Pad;
    struct ArrangeView;
    EditorContext& ctx;
    std::unique_ptr<XYPad> xy;
    std::unique_ptr<ArrangeView> arrange;
    juce::OwnedArray<Pad> pads;
    juce::OwnedArray<juce::TextButton> snapButtons;
    juce::TextButton storeMode { "STOCKER" }, morphOn { "MORPH" }, playArr { "LECTURE" };
    juce::Rectangle<int> xyArea, snapArea, arrArea, padArea;
};
} // namespace idg::ui
