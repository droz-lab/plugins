#include "ModPanel.h"

namespace idg::ui
{
void ModSourceButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool sel = ctx.selectedSource == source;
    const auto c = sourceColour (source);
    g.setColour (sel ? Theme::panelHi.brighter (0.2f) : Theme::panelHi);
    g.fillRect (r);
    g.setColour (sel ? c : Theme::line);
    g.drawRect (r, 1.0f);
    const float v = ctx.proc.modShared.value[source].load();
    g.setColour (c.withAlpha (0.35f));
    g.fillRect (r.withTrimmedTop (r.getHeight() * (1.0f - v)).withWidth (3.0f));
    g.setColour (sel ? c : Theme::text);
    g.setFont (Theme::mono (10.0f, true));
    g.drawText (sourceName (source), r.withTrimmedLeft (4), juce::Justification::centredLeft);
    // poignée de glisser
    g.setColour (c);
    auto h = r.removeFromRight (12.0f).reduced (2.0f, 4.0f);
    for (int i = 0; i < 3; ++i) g.fillRect (h.getX(), h.getY() + i * h.getHeight() / 3.0f, h.getWidth(), 1.5f);
}

void ModSourceButton::mouseDown (const juce::MouseEvent&)
{
    if (ctx.selectSource) ctx.selectSource (source);
}

void ModSourceButton::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() < 4) return;
    if (auto* dnd = juce::DragAndDropContainer::findParentDragContainerFor (this))
        if (! dnd->isDragAndDropActive())
        {
            juce::Image img (juce::Image::ARGB, 70, 20, true);
            juce::Graphics g (img);
            g.setColour (sourceColour (source));
            g.fillRect (0, 0, 70, 20);
            g.setColour (juce::Colours::black);
            g.setFont (Theme::mono (11.0f, true));
            g.drawText (sourceName (source), 0, 0, 70, 20, juce::Justification::centred);
            dnd->startDragging ("mod:" + juce::String (source), this, juce::ScaledImage (img), true);
        }
}

void ModSourceButton::tick()
{
    const float v = ctx.proc.modShared.value[source].load();
    if (std::abs (v - lastV) > 0.01f) { lastV = v; repaint(); }
}

// ---------------------------------------------------------------------------
struct ModPanel::ConnRow : public juce::Component
{
    ConnRow (EditorContext& c, int s) : ctx (c), slot (s)
    {
        depth.setSliderStyle (juce::Slider::LinearBar);
        depth.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        depth.setRange (-1.0, 1.0, 0.001);
        depth.setValue (c.proc.modShared.slots[s].depth.load(), juce::dontSendNotification);
        depth.setDoubleClickReturnValue (true, 0.3);
        depth.setColour (juce::Slider::trackColourId, sourceColour (c.proc.modShared.slots[s].src.load()).withAlpha (0.7f));
        depth.onValueChange = [this] { ctx.proc.modShared.slots[slot].depth = (float) depth.getValue(); };
        depth.setTooltip ("Profondeur de modulation (double-clic : 30 %)");
        addAndMakeVisible (depth);
        bip.setButtonText (IDG_T ("±"));
        bip.setClickingTogglesState (true);
        bip.setToggleState (c.proc.modShared.slots[s].bipolar.load(), juce::dontSendNotification);
        bip.setColour (juce::TextButton::buttonOnColourId, Theme::blue);
        bip.setTooltip ("Bipolaire : module autour de la valeur du potard");
        bip.onClick = [this] { ctx.proc.modShared.slots[slot].bipolar = bip.getToggleState(); };
        addAndMakeVisible (bip);
        del.setButtonText ("X");
        del.setTooltip ("Supprimer cette modulation");
        del.onClick = [this] { ctx.proc.pushUndo(); ctx.proc.modShared.removeConnection (slot); };
        addAndMakeVisible (del);
    }
    void paint (juce::Graphics& g) override
    {
        const int dst = ctx.proc.modShared.slots[slot].dst.load();
        if (dst < 0) return;
        g.setColour (Theme::text);
        g.setFont (Theme::mono (10.0f));
        g.drawFittedText (ctx.def (dst).name, getLocalBounds().withWidth (getWidth() - 4).withHeight (13), juce::Justification::centredLeft, 1, 0.7f);
        g.setColour (Theme::textDim);
        g.drawText (juce::String (juce::roundToInt (depth.getValue() * 100)) + "%", getLocalBounds().withHeight (13), juce::Justification::centredRight);
    }
    void resized() override
    {
        auto r = getLocalBounds().withTrimmedTop (14).withHeight (16);
        del.setBounds (r.removeFromRight (20));
        r.removeFromRight (2);
        bip.setBounds (r.removeFromRight (22));
        r.removeFromRight (4);
        depth.setBounds (r);
    }
    EditorContext& ctx;
    int slot;
    juce::Slider depth;
    juce::TextButton bip, del;
};

ModPanel::ModPanel (EditorContext& c) : ctx (c), lfoEditor (c)
{
    for (int s = 0; s < NumSources; ++s) addAndMakeVisible (sources.add (new ModSourceButton (c, s)));
    addAndMakeVisible (lfoEditor);
    lfoPreset.addItemList (LfoShape::presetNames(), 1);
    lfoPreset.setTextWhenNothingSelected ("FORMES...");
    lfoPreset.setTooltip (IDG_T ("Formes prédéfinies"));
    lfoPreset.onChange = [this]
    {
        int i = lfoPreset.getSelectedItemIndex();
        if (i >= 0 && ctx.selectedSource < SrcRnd0)
        {
            ctx.proc.pushUndo();
            ctx.proc.modShared.setShape (ctx.selectedSource - SrcLfo0, LfoShape::preset (i));
            lfoEditor.repaint();
        }
        lfoPreset.setSelectedItemIndex (-1, juce::dontSendNotification);
    };
    gridBox.addItemList ({ "4", "8", "16", "32" }, 1);
    gridBox.setSelectedItemIndex (2, juce::dontSendNotification);
    gridBox.setTooltip ("Grille du crayon");
    gridBox.onChange = [this] { static const int g[] { 4, 8, 16, 32 }; lfoEditor.setGrid (g[juce::jmax (0, gridBox.getSelectedItemIndex())]); };
    pencilBtn.setClickingTogglesState (true);
    pencilBtn.setTooltip ("Outil crayon : dessinez la forme sur la grille");
    pencilBtn.onClick = [this] { lfoEditor.setPencil (pencilBtn.getToggleState()); lfoEditor.repaint(); };
    copyShape.onClick = [this] { if (ctx.selectedSource < SrcRnd0) ctx.lfoShapeClipboard = ctx.proc.modShared.shapes[ctx.selectedSource].toString(); };
    pasteShape.onClick = [this]
    {
        if (ctx.selectedSource < SrcRnd0 && ctx.lfoShapeClipboard.isNotEmpty())
        {
            ctx.proc.pushUndo();
            ctx.proc.modShared.setShape (ctx.selectedSource, LfoShape::fromString (ctx.lfoShapeClipboard));
            lfoEditor.repaint();
        }
    };
    copyMods.setTooltip ("Copie toutes les cibles de cette source");
    pasteMods.setTooltip (IDG_T ("Colle les cibles copiées sur cette source"));
    copyMods.onClick = [this]
    {
        ctx.modClipboard.clear();
        for (auto& s : ctx.proc.modShared.slots)
            if (s.src.load() == ctx.selectedSource && s.dst.load() >= 0) ctx.modClipboard.push_back ({ s.dst.load(), s.depth.load(), s.bipolar.load() });
        if (ctx.flash) ctx.flash (juce::String ((int) ctx.modClipboard.size()) + IDG_T (" modulation(s) copiée(s)"));
    };
    pasteMods.onClick = [this]
    {
        ctx.proc.pushUndo();
        for (auto& [dst, depth, bip] : ctx.modClipboard) ctx.proc.modShared.addConnection (ctx.selectedSource, dst, depth, bip);
    };
    clearMods.onClick = [this]
    {
        ctx.proc.pushUndo();
        for (int i = 0; i < MaxModSlots; ++i) if (ctx.proc.modShared.slots[i].src.load() == ctx.selectedSource) ctx.proc.modShared.removeConnection (i);
    };
    for (auto* b : { &pencilBtn, &copyShape, &pasteShape, &copyMods, &pasteMods, &clearMods }) addAndMakeVisible (b);
    addAndMakeVisible (lfoPreset);
    addAndMakeVisible (gridBox);
    lfoTrig = std::make_unique<ParamCombo> (ctx, G::lfoTrig);
    addAndMakeVisible (*lfoTrig);
    connView.setViewedComponent (&connHolder, false);
    connView.setScrollBarsShown (true, false);
    addAndMakeVisible (connView);
    selectSource (ctx.selectedSource);
}

ModPanel::~ModPanel() = default;

void ModPanel::selectSource (int src)
{
    ctx.selectedSource = src;
    knobs.clear();
    const bool isLfo = src < SrcRnd0;
    lfoEditor.setVisible (isLfo);
    for (auto* c : std::initializer_list<juce::Component*> { &lfoPreset, &gridBox, &pencilBtn, &copyShape, &pasteShape }) c->setVisible (isLfo);
    auto& proc = ctx.proc;
    const auto col = sourceColour (src);
    auto addKnob = [&] (const juce::String& id, const juce::String& label)
    {
        int i = proc.indexOf (id);
        auto* k = new Knob (ctx, i, col, label);
        knobs.add (k);
        addAndMakeVisible (k);
    };
    auto addBtn = [&] (const juce::String& id, const juce::String& label)
    {
        auto* b = new ParamButton (ctx, id, label, col);
        knobs.add (b);
        addAndMakeVisible (b);
    };
    auto addCombo = [&] (const juce::String& id)
    {
        auto* cb = new ParamCombo (ctx, id);
        knobs.add (cb);
        addAndMakeVisible (cb);
    };
    if (isLfo)
    {
        const int i = src - SrcLfo0;
        lfoEditor.setLfo (i);
        addCombo (lfoId (i, "mode"));
        addBtn (lfoId (i, "sync"), "SYNC");
        addKnob (lfoId (i, "rate"), "VITESSE");
        addKnob (lfoId (i, "div"), "DIVISION");
        addKnob (lfoId (i, "smooth"), "LISSAGE");
        addKnob (lfoId (i, "phase"), "PHASE");
        addKnob (lfoId (i, "delay"), IDG_T ("DÉLAI"));
        addKnob (lfoId (i, "stereo"), IDG_T ("STÉRÉO"));
    }
    else if (src < SrcEnv)
    {
        const int i = src - SrcRnd0;
        addCombo (rndId (i, "style"));
        addBtn (rndId (i, "sync"), "SYNC");
        addBtn (rndId (i, "stereo"), IDG_T ("STÉRÉO"));
        addKnob (rndId (i, "rate"), "VITESSE");
        addKnob (rndId (i, "div"), "DIVISION");
    }
    else if (src == SrcEnv)
    {
        addCombo (G::envSrc);
        addKnob (G::envAtt, "ATTAQUE");
        addKnob (G::envRel, "RELEASE");
        addKnob (G::envGain, "GAIN");
    }
    else
    {
        addKnob (G::macro (src - SrcMacro0), "MACRO " + juce::String (src - SrcMacro0 + 1));
    }
    for (auto* s : sources) s->repaint();
    lastConnHash = -1;
    refreshConnections();
    resized();
    repaint();
}

void ModPanel::refreshConnections()
{
    rows.clear();
    int y = 0;
    for (int i = 0; i < MaxModSlots; ++i)
    {
        const auto& s = ctx.proc.modShared.slots[i];
        if (s.src.load() != ctx.selectedSource || s.dst.load() < 0) continue;
        auto* r = rows.add (new ConnRow (ctx, i));
        connHolder.addAndMakeVisible (r);
        r->setBounds (0, y, connView.getWidth() - 8, 32);
        y += 34;
    }
    connHolder.setSize (juce::jmax (10, connView.getWidth() - 8), juce::jmax (10, y));
}

void ModPanel::tick()
{
    for (auto* s : sources) s->tick();
    if (lfoEditor.isVisible()) lfoEditor.repaint();
    int hash = 0;
    for (int i = 0; i < MaxModSlots; ++i)
    {
        const auto& s = ctx.proc.modShared.slots[i];
        if (s.src.load() == ctx.selectedSource && s.dst.load() >= 0) hash = hash * 31 + i * 7 + s.dst.load();
    }
    if (hash != lastConnHash) { lastConnHash = hash; refreshConnections(); repaint(); }
}

void ModPanel::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    srcArea = r.removeFromLeft (148);
    r.removeFromLeft (8);
    connArea = r.removeFromRight (178);
    r.removeFromRight (8);
    editArea = r;

    // sources : LFO (2 x 5), random, env, macros
    auto s = srcArea.withTrimmedTop (16);
    const int bw = s.getWidth() / 2 - 2, bh = 20;
    for (int i = 0; i < NumSources; ++i)
    {
        int col = i % 2, row = i / 2;
        sources[i]->setBounds (s.getX() + col * (bw + 4), s.getY() + row * (bh + 3), bw, bh);
    }

    auto e = editArea.withTrimmedTop (16);
    const bool isLfo = ctx.selectedSource < SrcRnd0;
    if (isLfo)
    {
        auto tools = e.removeFromTop (22);
        lfoPreset.setBounds (tools.removeFromLeft (92)); tools.removeFromLeft (4);
        pencilBtn.setBounds (tools.removeFromLeft (66)); tools.removeFromLeft (4);
        gridBox.setBounds (tools.removeFromLeft (52)); tools.removeFromLeft (4);
        pasteShape.setBounds (tools.removeFromRight (62)); tools.removeFromRight (4);
        copyShape.setBounds (tools.removeFromRight (62));
        e.removeFromTop (4);
        lfoEditor.setBounds (e.removeFromTop (e.getHeight() - 92));
        e.removeFromTop (4);
    }
    // contrôles : listes / boutons empilés à gauche, potards à droite
    auto row = e;
    auto side = row.removeFromLeft (78);
    row.removeFromLeft (4);
    int x = row.getX();
    const bool single = ctx.selectedSource >= SrcMacro0;
    const int nKnobs = (int) std::count_if (knobs.begin(), knobs.end(), [] (juce::Component* c) { return dynamic_cast<Knob*> (c) != nullptr; });
    const int kw = single ? 110 : juce::jlimit (44, 70, row.getWidth() / juce::jmax (1, nKnobs));
    for (auto* k : knobs)
    {
        if (auto* kb = dynamic_cast<Knob*> (k))
        {
            kb->setBounds (x, row.getY(), kw, juce::jmin (single ? 130 : 80, row.getHeight()));
            x += kw;
        }
        else
        {
            k->setBounds (side.removeFromTop (22));
            side.removeFromTop (4);
        }
    }

    auto cr = connArea.withTrimmedTop (16);
    auto btns = cr.removeFromBottom (48);
    auto b1 = btns.removeFromTop (22);
    copyMods.setBounds (b1.removeFromLeft (b1.getWidth() / 2 - 2));
    b1.removeFromLeft (4);
    pasteMods.setBounds (b1);
    btns.removeFromTop (4);
    clearMods.setBounds (btns.removeFromLeft (cr.getWidth() / 2 - 2));
    btns.removeFromLeft (4);
    lfoTrig->setBounds (btns);
    lfoTrig->setTooltip ("Ce qui relance les LFO en mode Trigger / Enveloppe");
    connView.setBounds (cr.withTrimmedBottom (4));
    refreshConnections();
}

void ModPanel::paint (juce::Graphics& g)
{
    Theme::sectionTitle (g, "// SOURCES", srcArea.toFloat().withHeight (14), Theme::accent);
    Theme::sectionTitle (g, "// " + sourceName (ctx.selectedSource), editArea.toFloat().withHeight (14), sourceColour (ctx.selectedSource));
    Theme::sectionTitle (g, "// CIBLES (" + juce::String (rows.size()) + ")", connArea.toFloat().withHeight (14), Theme::accent);
    if (rows.isEmpty())
    {
        g.setColour (Theme::textDim);
        g.setFont (Theme::sans (12.0f));
        g.drawFittedText (IDG_T ("Glissez la source (poignée ≡) sur un potard pour l'assigner.\nAlt + glisser sur le potard règle la profondeur."),
                          connArea.withTrimmedTop (22).withHeight (60), juce::Justification::topLeft, 4);
    }
}
} // namespace idg::ui
