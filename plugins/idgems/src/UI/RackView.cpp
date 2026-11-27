#include "RackView.h"
#include "Icons.h"

namespace idg::ui
{
struct RackView::FamilyRow : public juce::Component
{
    FamilyRow (EditorContext& c, int f) : ctx (c), fam (f)
    {
        mix.setSliderStyle (juce::Slider::LinearBar);
        mix.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        mix.setColour (juce::Slider::trackColourId, familyColour (f).withAlpha (0.75f));
        mix.setPopupMenuEnabled (false);
        mix.setTooltip ("Dry/wet de la famille " + familyNames()[f] + IDG_T (" : dose tous ses effets, où qu'ils soient dans la chaîne"));
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (c.proc.apvts, G::famMix (f), mix);
        addAndMakeVisible (mix);
        solo.setButtonText ("S");
        solo.setClickingTogglesState (true);
        solo.setColour (juce::TextButton::buttonOnColourId, familyColour (f));
        solo.setTooltip ("Solo de la famille");
        solo.onClick = [this] { ctx.proc.rackShared.famSolo[fam] = solo.getToggleState(); };
        addAndMakeVisible (solo);
        rnd.setButtonText ("?");
        rnd.setTooltip ("Random de la famille " + familyNames()[f]);
        rnd.onClick = [this] { ctx.proc.random.request (RandomEngine::Scope::Family, fam); };
        addAndMakeVisible (rnd);
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (familyColour (fam));
        g.fillRect (r.removeFromLeft (3.0f).reduced (0, 2));
        g.setFont (Theme::mono (10.0f, true));
        g.drawText (familyNames()[fam], r.withWidth (82).withTrimmedLeft (5), juce::Justification::centredLeft);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (0, 2);
        r.removeFromLeft (88);
        rnd.setBounds (r.removeFromRight (20));
        r.removeFromRight (2);
        solo.setBounds (r.removeFromRight (20));
        r.removeFromRight (4);
        mix.setBounds (r);
    }
    EditorContext& ctx;
    int fam;
    juce::Slider mix;
    juce::TextButton solo, rnd;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

RackView::RackView (EditorContext& c) : ctx (c)
{
    for (int f = 0; f < NumFamilies; ++f) addAndMakeVisible (families.add (new FamilyRow (c, f)));
    seqAll.setClickingTogglesState (true);
    seqAll.setToggleState (c.proc.seqMaster.load(), juce::dontSendNotification);
    seqAll.setTooltip (IDG_T ("Interrupteur général de tous les séquenceurs (chaque séquenceur garde son réglage)"));
    seqAll.onClick = [this] { ctx.proc.seqMaster = seqAll.getToggleState(); repaint(); };
    addAndMakeVisible (seqAll);
}

RackView::~RackView() = default;

void RackView::resized()
{
    auto r = getLocalBounds().reduced (8, 0);
    r.removeFromTop (22);
    for (auto* f : families) f->setBounds (r.removeFromTop (23));
    listTop = r.getY() + 26;
    seqAll.setBounds (getWidth() - 8 - 64, listTop - 23, 64, 19);
}

juce::Rectangle<int> RackView::rowBounds (int slot) const
{
    return { 8, listTop + slot * rowH, getWidth() - 16, rowH - 1 };
}

int RackView::rowAt (int y) const
{
    if (y < listTop) return -1;
    int s = (y - listTop) / rowH;
    return s < NumFx ? s : -1;
}

void RackView::tick()
{
    bool changed = false;
    for (int i = 0; i < NumFx; ++i)
    {
        float a = juce::jmin (1.0f, ctx.proc.rackShared.activity[i].load() * 3.0f);
        if (std::abs (a - act[i]) > 0.02f) { act[i] = a; changed = true; }
    }
    for (int f = 0; f < NumFamilies; ++f)
        families[f]->solo.setToggleState (ctx.proc.rackShared.famSolo[f].load(), juce::dontSendNotification);
    seqAll.setToggleState (ctx.proc.seqMaster.load(), juce::dontSendNotification);
    seqAll.setButtonText (ctx.proc.seqMaster.load() ? IDG_T ("SÉQ ON") : IDG_T ("SÉQ OFF"));
    if (changed) repaint();
}

void RackView::paint (juce::Graphics& g)
{
    auto& proc = ctx.proc;
    const auto& reg = registry();
    const auto& defs = effectDefs();
    Theme::sectionTitle (g, "// FAMILLES", juce::Rectangle<float> (10, 4, 200, 16), Theme::accent);
    Theme::sectionTitle (g, IDG_T ("// CHAÎNE"), juce::Rectangle<float> (10, (float) listTop - 20, 140, 16), Theme::accent);

    const auto order = proc.rackShared.getOrder();
    const bool zl = proc.zeroLatency();
    for (int s = 0; s < NumFx; ++s)
    {
        const int fx = order[(size_t) s];
        const auto& d = defs[(size_t) fx];
        auto r = rowBounds (s).toFloat();
        const bool on = proc.getNorm (reg.fxOn[fx]) > 0.5f;
        const bool sel = fx == ctx.selectedFx;
        const bool solo = proc.rackShared.solo[fx].load();
        const bool unavailable = zl && d.noZeroLatency;
        const auto fc = familyColour (d.family);
        const float a = unavailable ? 0.3f : 1.0f;

        if (dragging && s == dragSlot) g.setColour (Theme::accent.withAlpha (0.15f));
        else g.setColour (sel ? Theme::panelHi.brighter (0.15f) : (s == hoverRow ? Theme::panelHi : Theme::panel.withAlpha (0.6f)));
        g.fillRect (r);
        if (sel) { g.setColour (Theme::accent); g.drawRect (r, 1.0f); }

        // indentation des effets en parallèle
        float x = r.getX();
        g.setColour (fc.withAlpha ((on ? 1.0f : 0.35f) * a));
        g.fillRect (x, r.getY(), 3.0f, r.getHeight());
        // activité (barre qui scintille)
        if (on && act[fx] > 0.01f)
        {
            g.setColour (fc.withAlpha (0.25f));
            g.fillRect (r.withX (x + 4).withWidth ((r.getWidth() - 64.0f) * act[fx]).reduced (0, 3));
        }
        drawEffectIcon (g, fx, juce::Rectangle<float> (x + 6, r.getY() + 3, r.getHeight() - 6, r.getHeight() - 6),
                        (on ? fc : Theme::textDim).withAlpha (a), 1.2f, on);
        g.setFont (Theme::mono (11.0f, on));
        g.setColour ((on ? Theme::text : Theme::textDim).withAlpha (a));
        g.drawText (d.shortName, juce::Rectangle<float> (x + 24, r.getY(), 80, r.getHeight()), juce::Justification::centredLeft);

        // zones : ON, S (solo), # (séquenceur)
        auto z = r.withTrimmedLeft (r.getWidth() - 60);
        auto led = z.removeFromLeft (20).reduced (5);
        g.setColour (on ? fc : Theme::line);
        g.fillEllipse (led);
        if (on) { g.setColour (fc.withAlpha (0.3f)); g.fillEllipse (led.expanded (3)); }
        auto tag = [&] (juce::Rectangle<float> zr, const juce::String& t, bool active, juce::Colour c)
        {
            g.setColour (active ? c : Theme::line.brighter (0.3f));
            g.setFont (Theme::mono (10.0f, true));
            g.drawText (t, zr, juce::Justification::centred);
        };
        tag (z.removeFromLeft (18), "S", solo, Theme::accent);
        const bool seq = proc.seqData[fx].enabled.load();
        tag (z, "#", seq && proc.seqMaster.load(), seq ? fc : Theme::line);
        if (proc.rackShared.fxLocked[fx].load())
        {
            g.setColour (Theme::red);
            g.fillRect (r.getRight() - 3.0f, r.getY(), 3.0f, r.getHeight());
        }
    }
    if (dragging && dropSlot >= 0)
    {
        auto r = rowBounds (dropSlot).toFloat();
        float y = dropSlot > dragSlot ? r.getBottom() : r.getY();
        g.setColour (Theme::accent);
        g.fillRect (r.getX(), y - 1.0f, r.getWidth(), 3.0f);
    }
}

void RackView::mouseMove (const juce::MouseEvent& e)
{
    int r = rowAt (e.y);
    if (r != hoverRow)
    {
        hoverRow = r;
        repaint();
        if (r >= 0)
        {
            const auto& d = effectDefs()[(size_t) ctx.proc.rackShared.getOrder()[(size_t) r]];
            juce::ignoreUnused (d);
        }
    }
}

void RackView::mouseDown (const juce::MouseEvent& e)
{
    const int s = rowAt (e.y);
    if (s < 0) return;
    const int fx = ctx.proc.rackShared.getOrder()[(size_t) s];
    if (e.mods.isPopupMenu()) { showFxMenu (fx); return; }
    auto r = rowBounds (s);
    const int fromRight = r.getRight() - e.x;
    downZone = fromRight > 60 ? 0 : fromRight > 40 ? 1 : fromRight > 22 ? 2 : 5;
    dragSlot = s;
    dropSlot = s;
    dragging = false;
    const auto& reg = registry();
    auto& proc = ctx.proc;
    switch (downZone)
    {
        case 1: proc.pushUndo(); proc.setNormGesture (reg.fxOn[fx], proc.getNorm (reg.fxOn[fx]) > 0.5f ? 0.0f : 1.0f); break;
        case 2: proc.rackShared.solo[fx] = ! proc.rackShared.solo[fx].load(); break;
        case 5: proc.seqData[fx].enabled = ! proc.seqData[fx].enabled.load(); break;
        default: if (ctx.selectEffect) ctx.selectEffect (fx); break;
    }
    repaint();
}

void RackView::mouseDrag (const juce::MouseEvent& e)
{
    if (downZone != 0 || dragSlot < 0) return;
    if (! dragging && std::abs (e.getDistanceFromDragStartY()) > 4) dragging = true;
    if (dragging)
    {
        int s = rowAt (juce::jlimit (listTop, listTop + NumFx * rowH - 1, e.y));
        if (s != dropSlot) { dropSlot = s; repaint(); }
    }
}

void RackView::mouseUp (const juce::MouseEvent&)
{
    if (dragging && dropSlot >= 0 && dropSlot != dragSlot)
    {
        ctx.proc.pushUndo();
        auto order = ctx.proc.rackShared.getOrder();
        const int fx = order[(size_t) dragSlot];
        std::vector<int> v (order.begin(), order.end());
        v.erase (v.begin() + dragSlot);
        v.insert (v.begin() + dropSlot, fx);
        std::array<int, NumFx> o {};
        for (int i = 0; i < NumFx; ++i) o[(size_t) i] = v[(size_t) i];
        ctx.proc.rackShared.setOrder (o);
    }
    dragging = false;
    dragSlot = dropSlot = -1;
    downZone = -1;
    repaint();
}

void RackView::showFxMenu (int fx)
{
    auto& proc = ctx.proc;
    const auto& d = effectDefs()[(size_t) fx];
    juce::PopupMenu m;
    m.addSectionHeader (d.name);
    m.addItem (1, "Random de l'effet");
    m.addItem (2, IDG_T ("Init (valeurs par défaut)"));
    m.addItem (3, IDG_T ("Copier les réglages"));
    m.addItem (4, IDG_T ("Coller les réglages"), ctx.fxClipboard.isValid());
    m.addItem (5, proc.rackShared.fxLocked[fx].load() ? IDG_T ("Déverrouiller l'effet") : IDG_T ("Verrouiller l'effet (jamais randomisé)"));
    if (d.triggerable) m.addItem (6, "Relance sur les transitoires", true, proc.rackShared.transTrig[fx].load());
    m.addItem (7, IDG_T ("Note MIDI de déclenchement : ") + juce::MidiMessage::getMidiNoteName (36 + fx, true, true, 3), false);
    m.showMenuAsync (juce::PopupMenu::Options(), [this, fx] (int r)
    {
        auto& p = ctx.proc;
        if (r == 1) p.random.request (RandomEngine::Scope::Effect, fx);
        else if (r == 2) { p.pushUndo(); p.initEffect (fx); }
        else if (r == 3) ctx.fxClipboard = p.copyEffect (fx);
        else if (r == 4) { p.pushUndo(); p.pasteEffect (fx, ctx.fxClipboard); }
        else if (r == 5) p.rackShared.fxLocked[fx] = ! p.rackShared.fxLocked[fx].load();
        else if (r == 6) p.rackShared.transTrig[fx] = ! p.rackShared.transTrig[fx].load();
        if (ctx.refreshAll) ctx.refreshAll();
    });
}
} // namespace idg::ui
