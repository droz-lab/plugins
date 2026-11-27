#pragma once
#include "Knob.h"
#include "Graphs.h"

namespace idg::ui
{
/** Onglet ENTRÉE : transitoires, sidechain, capture, feedback, gamme, latence. */
class InputPanel : public juce::Component
{
public:
    explicit InputPanel (EditorContext&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void tick();

private:
    EditorContext& ctx;
    juce::OwnedArray<juce::Component> comps;
    juce::TextButton panic { "PANIC" };
    juce::String captureText, latencyText;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> sections;
};

/** Onglet SORTIE : un module par boîte, avec retour graphique, + spectre détaillé. */
class OutputPanel : public juce::Component
{
public:
    explicit OutputPanel (EditorContext&);
    void resized() override;
    void tick();
    SpectrumView spectrum;

private:
    EditorContext& ctx;
    juce::OwnedArray<juce::Component> comps;
    juce::OwnedArray<ModuleBox> boxes;
    juce::OwnedArray<OutputGraph> graphs;
};

/** Onglet ENREG : rolling recorder, enregistrement manuel, glisser-déposer vers Ableton. */
class RecorderPanel : public juce::Component, private juce::Timer
{
public:
    explicit RecorderPanel (EditorContext&);
    ~RecorderPanel() override;
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    struct DragTile;
    void timerCallback() override;
    void setLastFile (const juce::File&);
    EditorContext& ctx;
    juce::Slider seconds;
    juce::TextButton saveLast { IDG_T ("SAUVER LES DERNIÈRES SECONDES") }, rec { "REC" }, openFolder { "OUVRIR LE DOSSIER" };
    std::unique_ptr<DragTile> tile;
    juce::Rectangle<int> rollArea, recArea, tileArea;
};
} // namespace idg::ui
