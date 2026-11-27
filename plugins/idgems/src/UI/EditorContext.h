#pragma once
#include "../PluginProcessor.h"
#include "Theme.h"

namespace idg::ui
{
class Knob;

/** Contexte partagé entre les composants de l'interface. */
struct EditorContext
{
    explicit EditorContext (IDGemsProcessor& p) : proc (p) {}
    IDGemsProcessor& proc;

    int selectedFx = FxStutter;
    int selectedSource = SrcLfo0;
    int hoveredParam = -1;
    std::vector<Knob*> knobs;               // potards vivants (rafraîchis par le timer de l'éditeur)
    juce::ValueTree fxClipboard;
    juce::String lfoShapeClipboard;
    std::vector<std::tuple<int, float, bool>> modClipboard; // (destination, profondeur, bipolaire)

    std::function<void (int)> selectEffect;
    std::function<void (int)> selectSource;
    std::function<void()> refreshAll;
    std::function<void (const juce::String&)> flash; // message bref dans la barre du haut

    const ParamDef& def (int i) const { return registry().all[(size_t) i]; }
};

/** Bouton lié à un paramètre booléen. */
class ParamButton : public juce::TextButton
{
public:
    ParamButton (EditorContext& c, const juce::String& id, const juce::String& text, juce::Colour on = Theme::accent)
    {
        setButtonText (text);
        setClickingTogglesState (true);
        setColour (juce::TextButton::buttonOnColourId, on);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (c.proc.apvts, id, *this);
        const int i = c.proc.indexOf (id);
        if (i >= 0 && c.def (i).help.isNotEmpty()) setTooltip (c.def (i).help);
    }

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

/** Liste déroulante liée à un paramètre de choix. */
class ParamCombo : public juce::ComboBox
{
public:
    ParamCombo (EditorContext& c, const juce::String& id)
    {
        const int i = c.proc.indexOf (id);
        if (i >= 0) addItemList (c.def (i).choices, 1);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (c.proc.apvts, id, *this);
        if (i >= 0) setTooltip (c.def (i).name + (c.def (i).help.isNotEmpty() ? "\n" + c.def (i).help : juce::String()));
    }

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};
} // namespace idg::ui
