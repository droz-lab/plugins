#pragma once
#include "EditorContext.h"

namespace idg::ui
{
/** Navigateur de presets façon Vital (catégories, recherche, favoris, auteur, tags) + fenêtre de sauvegarde. */
class PresetBrowser : public juce::Component, private juce::ListBoxModel
{
public:
    explicit PresetBrowser (EditorContext&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void open();
    void openSave();
    std::function<void()> onClose;

private:
    int getNumRows() override { return (int) filtered.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void refilter();
    void close() { setVisible (false); if (onClose) onClose(); }

    EditorContext& ctx;
    juce::TextEditor search;
    juce::ListBox list { "presets", this };
    juce::OwnedArray<juce::TextButton> cats;
    juce::TextButton closeBtn { "FERMER" }, favOnly { "FAVORIS" }, saveBtn { "SAUVER" }, doSave { "ENREGISTRER" }, rescan { IDG_T ("RAFRAÎCHIR") };
    juce::TextEditor nameEd, authorEd, tagsEd;
    juce::ComboBox catBox;
    juce::String category;
    std::vector<int> filtered;
    bool saveMode = false;
    juce::Rectangle<int> panelR;
};
} // namespace idg::ui
