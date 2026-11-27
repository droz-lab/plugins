#pragma once
#include "Knob.h"
#include "Visuals.h"
#include "ModPanel.h"

namespace idg::ui
{
/** Colonne de droite : macro Chaos, 8 macros, système de random, vu-mètres. */
class RightPanel : public juce::Component
{
public:
    explicit RightPanel (EditorContext&);
    ~RightPanel() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    void tick();
    void refresh();
    void doRandom();

private:
    struct BigRandomButton;
    EditorContext& ctx;
    std::unique_ptr<Knob> chaos;
    juce::OwnedArray<Knob> macros;
    juce::OwnedArray<ModSourceButton> macroHandles;
    std::unique_ptr<BigRandomButton> bigRandom;
    juce::ComboBox scope, sync, mutateBars;
    juce::Slider amount, glide, driftAmt, driftSpeed, mutateAmt;
    juce::TextEditor seed;
    juce::TextButton back { "<" }, fwd { ">" }, star { "*" }, history { "HISTORIQUE" },
        drift { "DRIFT" }, mutate { "MUTATE" }, learn { "APPREND" }, order { "ORDRE" }, seqs { IDG_T ("SÉQ.") };
    Meters meters;
    juce::Rectangle<int> chaosArea, macroArea, randomArea;
    juce::String historyText;
};
} // namespace idg::ui
