#pragma once
#include "Knob.h"
#include "LfoEditor.h"

namespace idg::ui
{
/** Source de modulation : clic = sélection, glisser = assigner sur un potard. */
class ModSourceButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    ModSourceButton (EditorContext& c, int src) : ctx (c), source (src)
    {
        setTooltip ("Glissez " + sourceName (src) + IDG_T (" sur n'importe quel potard pour le moduler. Clic : éditer."));
    }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void tick();

private:
    EditorContext& ctx;
    int source;
    float lastV = -1.0f;
};

/** Onglet MODULATION : sources, éditeur de la source, liste de ses cibles. */
class ModPanel : public juce::Component
{
public:
    explicit ModPanel (EditorContext&);
    ~ModPanel() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    void selectSource (int src);
    void tick();
    void refreshConnections();

private:
    struct ConnRow;
    EditorContext& ctx;
    juce::OwnedArray<ModSourceButton> sources;
    LfoEditor lfoEditor;
    juce::OwnedArray<juce::Component> knobs;
    juce::ComboBox lfoPreset, gridBox;
    juce::TextButton pencilBtn { "CRAYON" }, copyShape { "COPIER" }, pasteShape { "COLLER" },
        copyMods { "COPIER MODS" }, pasteMods { "COLLER MODS" }, clearMods { "TOUT EFFACER" };
    std::unique_ptr<ParamCombo> lfoTrig;
    juce::Viewport connView;
    juce::Component connHolder;
    juce::OwnedArray<ConnRow> rows;
    juce::Rectangle<int> srcArea, editArea, connArea;
    int lastConnHash = 0;
};
} // namespace idg::ui
