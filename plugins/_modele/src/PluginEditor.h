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
    // Un potard = un droz::ui::Knob : libelle, valeur, info-bulle. Les autres controles : docs/HABILLAGE.md.
    droz::ui::Knob gain { "Gain", juce::String::fromUTF8 ("Niveau de sortie. Double-clic : valeur par défaut."), " dB" };
    juce::AudioProcessorValueTreeState::SliderAttachment gainAttachment;

    juce::TooltipWindow tooltips { this }; // affiche les info-bulles des controles

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
