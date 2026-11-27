#pragma once
#include "PluginProcessor.h"
#include "UI/LookAndFeel.h"
#include "UI/EditorContext.h"

namespace idg::ui
{
class RackView;
class EffectPanel;
class ModPanel;
class PerfPanel;
class InputPanel;
class OutputPanel;
class RecorderPanel;
class RightPanel;
class TopBar;
class PresetBrowser;
class GlitchOverlay;
}

/** Fenêtre d'IDGems : 1280 x 800 virtuels, mise à l'échelle (ratio conservé). */
class IDGemsEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer
{
public:
    explicit IDGemsEditor (IDGemsProcessor&);
    ~IDGemsEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    static constexpr int baseW = 1280, baseH = 800;
    void debugSelectTab (int t) { selectTab (t); }
    void debugTick() { timerCallback(); }

private:
    struct Content;
    void timerCallback() override;
    void selectEffect (int fx);
    void selectTab (int t);
    void refreshAll();
    void showSettings();
    void applyAccent (juce::Colour c);
    void setScale (float s);

    IDGemsProcessor& proc;
    idg::ui::IDGLookAndFeel lnf;
    idg::ui::EditorContext ctx;
    std::unique_ptr<Content> content;
    std::unique_ptr<juce::TooltipWindow> tooltip;
    std::vector<float> scopeTmp = std::vector<float> (4096);
    bool sizeReady = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IDGemsEditor)
};
