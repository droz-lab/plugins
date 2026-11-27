#pragma once
#include "EditorContext.h"

namespace idg::ui
{
/** Barre du haut : logo, presets, undo/redo, A/B, voyant sans latence, panic, taille, réglages. */
class TopBar : public juce::Component
{
public:
    explicit TopBar (EditorContext&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void tick();
    void flash (const juce::String& msg) { message = msg; messageTime = 60; repaint(); }

    std::function<void()> onOpenBrowser, onSave, onSettings;
    std::function<void (float)> onScale;

private:
    EditorContext& ctx;
    juce::TextButton prev { "<" }, next { ">" }, presetName, save { "SAVE" }, undo { "UNDO" }, redo { "REDO" },
        a { "A" }, b { "B" }, copyAB { "A>B" }, panic { "PANIC" }, settings { IDG_T ("RÉGLAGES") }, zl { "0-LAT" };
    juce::ComboBox size;
    juce::String message;
    int messageTime = 0;
    juce::Random rng;
    int glitchFrame = 0;
};
} // namespace idg::ui
