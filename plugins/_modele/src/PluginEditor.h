#pragma once

#include "PluginProcessor.h"

#include <droz/ui.h>

class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PluginProcessor& processor;
    bool ready = false; // vrai une fois la taille gardee restauree

    droz::ui::LookAndFeel look; // declare avant les controles : ils s'en servent jusqu'a leur destruction
    juce::Slider gainSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::AudioProcessorValueTreeState::SliderAttachment gainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
