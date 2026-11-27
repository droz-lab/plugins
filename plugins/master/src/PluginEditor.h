#pragma once

#include "PluginProcessor.h"
#include "ui/Controles.h"
#include "ui/Orbite.h"
#include "ui/Panneau.h"
#include "ui/Spectre.h"

class PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void selectionne (int m);
    void dessineVumetre (juce::Graphics& g, juce::Rectangle<float> r, float db, const juce::String& nom);

    PluginProcessor& processor;
    bool ready = false; // vrai une fois la taille gardee restauree

    master::ui::Look look; // declare avant les controles : ils s'en servent jusqu'a leur destruction
    juce::TooltipWindow bulles { this, 600 };
    juce::Image cosmos;
    master::ui::Spectre spectre;
    master::ui::Orbite orbite;
    master::ui::Panneau panneau;
    master::ui::Controle entree, sortie;
    juce::Rectangle<int> zoneVumetres;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
