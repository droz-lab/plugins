#pragma once

#include "PluginProcessor.h"
#include "Views.h"

#include <droz/ui.h>

/** All controls, laid out at a fixed base size (PluginProcessor::baseWidth x baseHeight).
    The editor scales it as a whole: the window can be resized, the ratio is kept. */
class MarkovContent final : public juce::Component
{
public:
    explicit MarkovContent (PluginProcessor&);
    ~MarkovContent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh(); // ~30 times a second, from the editor's timer

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const juce::String& label, const juce::String& tooltip);
    void attachKnob (Knob&, const juce::String& paramId);
    void setupCombo (juce::ComboBox&, juce::Label&, const juce::String& label, const juce::String& tooltip);
    void selectTrack (int t);
    void updateCustomNotes();

    PluginProcessor& processor;
    droz::ui::LookAndFeel look; // declared before the controls: they use it until they are destroyed

    // Scale and global
    juce::ComboBox scaleBox, rootBox, orderBox, rhythmBox;
    juce::Label scaleLabel, rootLabel, orderLabel, rhythmLabel;
    std::unique_ptr<ComboAttachment> scaleAttachment, rootAttachment, orderAttachment, rhythmAttachment;
    std::array<juce::TextButton, 12> customNotes;
    std::array<std::unique_ptr<ButtonAttachment>, 12> customAttachments;
    juce::Label tempoLabel;
    Knob seed, temperature, previewLevel;
    juce::TextButton regenerate;
    juce::ToggleButton preview;
    std::unique_ptr<ButtonAttachment> previewAttachment;

    // Learning
    juce::ToggleButton learn;
    std::unique_ptr<ButtonAttachment> learnAttachment;
    juce::Label learnStatus;
    juce::TextButton learnReset;

    // Tracks
    std::vector<std::unique_ptr<markovui::TrackRow>> rows;
    Knob lo, hi, steps, gate, afterHit, afterRest;
    juce::ComboBox divisionBox;
    juce::Label divisionLabel;
    std::unique_ptr<ComboAttachment> divisionAttachment;

    // Matrix and steps
    markovui::MatrixView matrix;
    juce::TextButton matrixDefault;
    markovui::StepView stepView;
    juce::TextButton stepsDefault, stepsFull;

    int track = 0;
    std::array<uint32_t, markov::numTracks> lastNotes {};
    std::array<float, markov::numTracks> leds {};
    int tick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MarkovContent)
};

class PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { content.refresh(); }

    PluginProcessor& processor;
    bool ready = false; // true once the kept size has been restored
    MarkovContent content;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
