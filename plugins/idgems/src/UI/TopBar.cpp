#include "TopBar.h"

namespace idg::ui
{
TopBar::TopBar (EditorContext& c) : ctx (c)
{
    for (auto* bt : { &prev, &next, &presetName, &save, &undo, &redo, &a, &b, &copyAB, &panic, &settings, &zl }) addAndMakeVisible (bt);
    addAndMakeVisible (size);
    prev.onClick = [this] { if (ctx.proc.presets) ctx.proc.presets->next (-1); };
    next.onClick = [this] { if (ctx.proc.presets) ctx.proc.presets->next (1); };
    presetName.onClick = [this] { if (onOpenBrowser) onOpenBrowser(); };
    presetName.setTooltip ("Ouvrir le navigateur de presets");
    save.onClick = [this] { if (onSave) onSave(); };
    save.setTooltip ("Sauvegarder le preset");
    undo.onClick = [this] { ctx.proc.undo(); };
    redo.onClick = [this] { ctx.proc.redo(); };
    undo.setTooltip ("Annuler (Ctrl+Z)");
    redo.setTooltip (IDG_T ("Rétablir (Ctrl+Y)"));
    a.setClickingTogglesState (false);
    a.onClick = [this] { ctx.proc.switchAB (0); };
    b.onClick = [this] { ctx.proc.switchAB (1); };
    copyAB.onClick = [this] { ctx.proc.copyAtoB(); flash (IDG_T ("A copié dans B")); };
    a.setTooltip (IDG_T ("Comparaison A/B : réglage A"));
    b.setTooltip (IDG_T ("Comparaison A/B : réglage B"));
    copyAB.setTooltip (IDG_T ("Copie le réglage courant dans A et B"));
    panic.setColour (juce::TextButton::buttonColourId, Theme::red.withAlpha (0.75f));
    panic.setTooltip (IDG_T ("PANIC : coupe instantanément les feedbacks et vide les buffers"));
    panic.onClick = [this] { ctx.proc.panic(); flash ("PANIC"); };
    zl.setClickingTogglesState (true);
    zl.setColour (juce::TextButton::buttonOnColourId, Theme::blue);
    zl.onClick = [this] { ctx.proc.setValue (G::zeroLat, zl.getToggleState() ? 1.0f : 0.0f); };
    settings.onClick = [this] { if (onSettings) onSettings(); };
    size.addItemList ({ "70 %", "85 %", "100 %", "125 %", "150 %", "200 %" }, 1);
    size.setTooltip ("Taille de l'interface (vous pouvez aussi glisser le coin)");
    size.onChange = [this]
    {
        static const float s[] { 0.7f, 0.85f, 1.0f, 1.25f, 1.5f, 2.0f };
        if (onScale && size.getSelectedItemIndex() >= 0) onScale (s[size.getSelectedItemIndex()]);
    };
    size.setTextWhenNothingSelected ("TAILLE");
}

void TopBar::resized()
{
    auto r = getLocalBounds().reduced (8, 8);
    r.removeFromLeft (196);
    auto place = [&r] (juce::Component& c, int w) { c.setBounds (r.removeFromLeft (w)); r.removeFromLeft (4); };
    place (prev, 24);
    place (presetName, 230);
    place (next, 24);
    place (save, 50);
    r.removeFromLeft (10);
    place (undo, 50);
    place (redo, 50);
    r.removeFromLeft (10);
    place (a, 26);
    place (b, 26);
    place (copyAB, 40);
    auto right = r;
    settings.setBounds (right.removeFromRight (80)); right.removeFromRight (4);
    size.setBounds (right.removeFromRight (74)); right.removeFromRight (8);
    panic.setBounds (right.removeFromRight (64)); right.removeFromRight (4);
    zl.setBounds (right.removeFromRight (56));
}

void TopBar::tick()
{
    auto name = ctx.proc.presets ? ctx.proc.presets->currentName() : juce::String ("Init");
    if (presetName.getButtonText() != name) presetName.setButtonText (name);
    undo.setEnabled (ctx.proc.canUndo());
    redo.setEnabled (ctx.proc.canRedo());
    a.setToggleState (ctx.proc.getABSlot() == 0, juce::dontSendNotification);
    b.setToggleState (ctx.proc.getABSlot() == 1, juce::dontSendNotification);
    const bool z = ctx.proc.zeroLatency();
    zl.setToggleState (z, juce::dontSendNotification);
    zl.setTooltip (z ? IDG_T ("MODE SANS LATENCE ACTIF\nModules dégradés : Pitch Shift et Formant (algo simplifié), Pixel Sort (coupé), limiteur sans anticipation (écrêtage plus dur)")
                     : IDG_T ("Mode sans latence (désactivé). Latence actuelle compensée par Ableton."));
    if (messageTime > 0 && --messageTime == 0) repaint();
    if (++glitchFrame % 4 == 0) repaint (juce::Rectangle<int> (0, 0, 200, getHeight()));
}

void TopBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Theme::bg.withAlpha (0.85f));
    g.fillRect (r);
    g.setColour (Theme::accent.withAlpha (0.5f));
    g.fillRect (r.getX(), r.getBottom() - 1.0f, r.getWidth(), 1.0f);
    // logo
    auto logo = juce::Rectangle<float> (12, 4, 180, r.getHeight() - 8);
    const float jitter = rng.nextFloat() < 0.06f ? (rng.nextFloat() - 0.5f) * 4.0f : 0.0f;
    Theme::cutText (g, "IDGEMS", logo.withHeight (30.0f).translated (jitter, -2.0f), 23.0f, juce::Colour (0xffeeeae6),
                    juce::Justification::centredLeft, 0.55f, true);
    if (rng.nextFloat() < 0.2f)
    {
        g.setColour (Theme::accent.withAlpha (0.6f));
        g.fillRect (logo.getX() + rng.nextFloat() * 100.0f, logo.getY() + rng.nextFloat() * logo.getHeight(), 30.0f + rng.nextFloat() * 50.0f, 2.0f);
    }
    g.setFont (Theme::mono (9.0f, true));
    g.setColour (Theme::accent);
    g.drawText ("GLITCH MACHINE", logo.withTrimmedTop (26).withHeight (10), juce::Justification::centredLeft);
    if (messageTime > 0)
    {
        g.setFont (Theme::mono (11.0f, true));
        g.setColour (Theme::accent);
        g.drawText (message, juce::Rectangle<float> (r.getWidth() * 0.5f + 60, 0, 300, r.getHeight()), juce::Justification::centredLeft);
    }
    if (ctx.proc.zeroLatency())
    {
        g.setColour (Theme::blue);
        g.fillEllipse (zl.getBounds().toFloat().withSizeKeepingCentre (6, 6).withX ((float) zl.getX() - 10.0f));
    }
}
} // namespace idg::ui
