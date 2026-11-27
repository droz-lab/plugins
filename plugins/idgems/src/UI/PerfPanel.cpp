#include "PerfPanel.h"

namespace idg::ui
{
struct PerfPanel::XYPad : public juce::Component, public juce::SettableTooltipClient
{
    explicit XYPad (EditorContext& c) : ctx (c)
    {
        ix = c.proc.indexOf (G::morphX);
        iy = c.proc.indexOf (G::morphY);
        setTooltip (IDG_T ("Pad de morphing : déplacez le curseur entre les snapshots. Glissez un snapshot (point numéroté) pour le placer."));
    }
    juce::Point<float> toScreen (float x, float y) const
    {
        auto r = getLocalBounds().toFloat().reduced (8);
        return { r.getX() + x * r.getWidth(), r.getY() + (1.0f - y) * r.getHeight() };
    }
    juce::Point<float> fromScreen (juce::Point<float> p) const
    {
        auto r = getLocalBounds().toFloat().reduced (8);
        return { juce::jlimit (0.0f, 1.0f, (p.x - r.getX()) / r.getWidth()), juce::jlimit (0.0f, 1.0f, 1.0f - (p.y - r.getY()) / r.getHeight()) };
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Theme::bg.withAlpha (0.85f));
        g.fillRect (r);
        g.setColour (Theme::line);
        g.drawRect (r, 1.0f);
        for (int i = 1; i < 8; ++i)
        {
            g.setColour (Theme::line.withAlpha (0.4f));
            g.drawVerticalLine ((int) (r.getX() + r.getWidth() * i / 8), r.getY(), r.getBottom());
            g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * i / 8), r.getX(), r.getRight());
        }
        const float mx = ctx.proc.getNorm (ix), my = ctx.proc.getNorm (iy);
        auto puck = toScreen (mx, my);
        for (int i = 0; i < NumSnapshots; ++i)
        {
            const auto& s = ctx.proc.snapshots[i];
            auto p = toScreen (s.x, s.y);
            if (s.valid && ctx.proc.morphEnabled.load())
            {
                float d = p.getDistanceFrom (puck);
                g.setColour (Theme::accent.withAlpha (juce::jlimit (0.05f, 0.6f, 40.0f / (d + 1.0f))));
                g.drawLine (juce::Line<float> (p, puck), 1.0f);
            }
            g.setColour (s.valid ? Theme::accent : Theme::line);
            g.fillEllipse (juce::Rectangle<float> (18, 18).withCentre (p));
            g.setColour (s.valid ? juce::Colours::black : Theme::textDim);
            g.setFont (Theme::mono (11.0f, true));
            g.drawText (juce::String (i + 1), juce::Rectangle<float> (18, 18).withCentre (p), juce::Justification::centred);
        }
        g.setColour (Theme::blue.withAlpha (0.4f));
        g.fillEllipse (juce::Rectangle<float> (26, 26).withCentre (puck));
        g.setColour (Theme::text);
        g.drawEllipse (juce::Rectangle<float> (26, 26).withCentre (puck), 2.0f);
        g.drawLine (puck.x - 18, puck.y, puck.x + 18, puck.y, 1.0f);
        g.drawLine (puck.x, puck.y - 18, puck.x, puck.y + 18, 1.0f);
        if (! ctx.proc.morphEnabled.load())
        {
            g.setColour (Theme::textDim);
            g.setFont (Theme::mono (10.0f));
            g.drawText (IDG_T ("MORPH DÉSACTIVÉ"), r.reduced (6), juce::Justification::bottomRight);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        dragSnap = -1;
        for (int i = 0; i < NumSnapshots; ++i)
            if (toScreen (ctx.proc.snapshots[i].x, ctx.proc.snapshots[i].y).getDistanceFrom (e.position) < 10.0f) { dragSnap = i; break; }
        if (dragSnap < 0)
        {
            ctx.proc.pushUndo();
            ctx.proc.morphEnabled = true;
            ctx.proc.param (ix)->beginChangeGesture();
            ctx.proc.param (iy)->beginChangeGesture();
            mouseDrag (e);
        }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto v = fromScreen (e.position);
        if (dragSnap >= 0) { ctx.proc.snapshots[dragSnap].x = v.x; ctx.proc.snapshots[dragSnap].y = v.y; }
        else { ctx.proc.setNorm (ix, v.x); ctx.proc.setNorm (iy, v.y); }
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragSnap < 0) { ctx.proc.param (ix)->endChangeGesture(); ctx.proc.param (iy)->endChangeGesture(); }
        dragSnap = -1;
    }
    EditorContext& ctx;
    int ix, iy, dragSnap = -1;
};

struct PerfPanel::Pad : public juce::Component, public juce::SettableTooltipClient
{
    Pad (EditorContext& c, int i) : ctx (c), index (i)
    {
        setTooltip (IDG_T ("Pad momentané : maintenir = effet actif. Clic droit : choisir l'effet. Aussi jouable au clavier MIDI (C1 = premier effet)."));
    }
    void paint (juce::Graphics& g) override
    {
        const int fx = ctx.proc.perfPads[index];
        const auto& d = effectDefs()[(size_t) fx];
        const auto c = familyColour (d.family);
        auto r = getLocalBounds().toFloat().reduced (2);
        const bool on = ctx.proc.rackShared.momentary[fx].load();
        g.setColour (on ? c : Theme::panelHi);
        g.fillRect (r);
        g.setColour (c);
        g.drawRect (r, on ? 2.0f : 1.0f);
        g.setColour (on ? juce::Colours::black : Theme::text);
        g.setFont (Theme::mono (10.0f, true));
        const bool isThrow = (fx == FxDelay || fx == FxReverb);
        g.drawFittedText ((isThrow ? "THROW\n" : "") + d.shortName, r.toNearestInt(), juce::Justification::centred, 2);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int fx = ctx.proc.perfPads[index];
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu m;
            for (int f = 0; f < NumFx; ++f) m.addItem (f + 1, effectDefs()[(size_t) f].name, true, f == fx);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r) { if (r > 0) { ctx.proc.perfPads[index] = r - 1; repaint(); } });
            return;
        }
        // un throw met le delay / la reverb en mode throw
        if (fx == FxDelay || fx == FxReverb)
            ctx.proc.setValue (fxParamId (fx, "throw"), 1.0f);
        ctx.proc.setMomentary (fx, true);
        repaint();
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;
        ctx.proc.setMomentary (ctx.proc.perfPads[index], false);
        repaint();
    }
    EditorContext& ctx;
    int index;
};

struct PerfPanel::ArrangeView : public juce::Component, public juce::SettableTooltipClient
{
    explicit ArrangeView (EditorContext& c) : ctx (c)
    {
        setTooltip (IDG_T ("Arrangement : clic = choisir le snapshot de la case, molette / glisser vertical = durée en mesures, clic droit = vider"));
    }
    juce::Rectangle<float> cellR (int i) const
    {
        const float w = (float) getWidth() / NumArrangeSlots;
        return { i * w, 0.0f, w - 2.0f, (float) getHeight() };
    }
    void paint (juce::Graphics& g) override
    {
        const int cur = ctx.proc.arrangeCurrent.load();
        for (int i = 0; i < NumArrangeSlots; ++i)
        {
            auto r = cellR (i);
            const auto& a = ctx.proc.arrange[i];
            g.setColour (i == cur && ctx.proc.arrangePlaying.load() ? Theme::accent.withAlpha (0.5f) : Theme::panelHi);
            g.fillRect (r);
            g.setColour (Theme::line);
            g.drawRect (r, 1.0f);
            g.setFont (Theme::mono (10.0f, true));
            g.setColour (a.snapshot >= 0 ? Theme::text : Theme::textDim);
            g.drawText (a.snapshot >= 0 ? "S" + juce::String (a.snapshot + 1) : "-", r.withHeight (r.getHeight() * 0.55f), juce::Justification::centred);
            if (a.snapshot >= 0)
            {
                g.setColour (Theme::blue);
                g.setFont (Theme::mono (9.0f));
                g.drawText (juce::String (a.bars) + "m", r.withTrimmedTop (r.getHeight() * 0.5f), juce::Justification::centred);
            }
        }
    }
    int cellAt (juce::Point<int> p) const { return juce::jlimit (0, NumArrangeSlots - 1, p.x * NumArrangeSlots / juce::jmax (1, getWidth())); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = cellAt (e.getPosition());
        startBars = ctx.proc.arrange[i].bars;
        dragging = false;
        if (e.mods.isPopupMenu()) { ctx.proc.arrange[i] = {}; repaint(); return; }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const int i = cellAt (e.getMouseDownPosition());
        if (std::abs (e.getDistanceFromDragStartY()) > 4) dragging = true;
        if (dragging && ctx.proc.arrange[i].snapshot >= 0)
        {
            ctx.proc.arrange[i].bars = juce::jlimit (1, 64, startBars - e.getDistanceFromDragStartY() / 8);
            repaint();
        }
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (dragging || e.mods.isPopupMenu()) return;
        const int i = cellAt (e.getPosition());
        juce::PopupMenu m;
        for (int s = 0; s < NumSnapshots; ++s) m.addItem (s + 1, "Snapshot " + juce::String (s + 1), ctx.proc.snapshots[s].valid);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this, i] (int r) { if (r > 0) { ctx.proc.arrange[i].snapshot = r - 1; repaint(); } });
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const int i = cellAt (e.getPosition());
        if (ctx.proc.arrange[i].snapshot < 0) return;
        ctx.proc.arrange[i].bars = juce::jlimit (1, 64, ctx.proc.arrange[i].bars + (w.deltaY > 0 ? 1 : -1));
        repaint();
    }
    EditorContext& ctx;
    int startBars = 1;
    bool dragging = false;
};

PerfPanel::PerfPanel (EditorContext& c) : ctx (c)
{
    xy = std::make_unique<XYPad> (c);
    addAndMakeVisible (*xy);
    arrange = std::make_unique<ArrangeView> (c);
    addAndMakeVisible (*arrange);
    for (int i = 0; i < 8; ++i) addAndMakeVisible (pads.add (new Pad (c, i)));
    for (int i = 0; i < NumSnapshots; ++i)
    {
        auto* b = snapButtons.add (new juce::TextButton (juce::String (i + 1)));
        b->setTooltip ("Snapshot " + juce::String (i + 1) + " : clic = rappeler, Maj + clic (ou mode STOCKER) = enregistrer, clic droit = effacer");
        b->onClick = [this, i, b]
        {
            const bool store = storeMode.getToggleState() || juce::ModifierKeys::currentModifiers.isShiftDown();
            if (juce::ModifierKeys::currentModifiers.isPopupMenu()) return;
            if (store || ! ctx.proc.snapshots[i].valid)
            {
                ctx.proc.storeSnapshot (i);
                storeMode.setToggleState (false, juce::dontSendNotification);
                if (ctx.flash) ctx.flash ("Snapshot " + juce::String (i + 1) + IDG_T (" enregistré"));
            }
            else ctx.proc.recallSnapshot (i);
            refresh();
            juce::ignoreUnused (b);
        };
        addAndMakeVisible (b);
    }
    storeMode.setClickingTogglesState (true);
    storeMode.setTooltip (IDG_T ("Le prochain clic sur un numéro enregistre l'état actuel dans ce snapshot"));
    morphOn.setClickingTogglesState (true);
    morphOn.setTooltip (IDG_T ("Active le morphing : la position du curseur XY (automatisable) mélange les snapshots"));
    morphOn.onClick = [this] { ctx.proc.morphEnabled = morphOn.getToggleState(); xy->repaint(); };
    playArr.setClickingTogglesState (true);
    playArr.setTooltip (IDG_T ("Joue l'arrangement de snapshots, calé sur les mesures de l'hôte"));
    playArr.onClick = [this] { ctx.proc.arrangePlaying = playArr.getToggleState(); };
    for (auto* b : { &storeMode, &morphOn, &playArr }) addAndMakeVisible (b);
    refresh();
}

PerfPanel::~PerfPanel() = default;

void PerfPanel::refresh()
{
    for (int i = 0; i < NumSnapshots; ++i)
    {
        auto* b = snapButtons[i];
        b->setColour (juce::TextButton::buttonColourId, ctx.proc.snapshots[i].valid ? Theme::accent.withAlpha (0.45f) : Theme::panelHi);
    }
    morphOn.setToggleState (ctx.proc.morphEnabled.load(), juce::dontSendNotification);
    playArr.setToggleState (ctx.proc.arrangePlaying.load(), juce::dontSendNotification);
    repaint();
}

void PerfPanel::tick()
{
    xy->repaint();
    arrange->repaint();
    for (auto* p : pads) p->repaint();
}

void PerfPanel::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    xyArea = r.removeFromLeft (r.getHeight() + 20);
    xy->setBounds (xyArea.withTrimmedTop (16));
    r.removeFromLeft (12);
    snapArea = r.removeFromTop (60);
    auto sb = snapArea.withTrimmedTop (16).removeFromTop (24);
    for (auto* b : snapButtons) { b->setBounds (sb.removeFromLeft (30)); sb.removeFromLeft (3); }
    sb.removeFromLeft (6);
    storeMode.setBounds (sb.removeFromLeft (70));
    sb.removeFromLeft (4);
    morphOn.setBounds (sb.removeFromLeft (60));
    r.removeFromTop (6);
    arrArea = r.removeFromTop (70);
    auto ar = arrArea.withTrimmedTop (16);
    playArr.setBounds (ar.removeFromRight (70).withHeight (24));
    ar.removeFromRight (6);
    arrange->setBounds (ar.withHeight (40));
    r.removeFromTop (6);
    padArea = r;
    auto pr = r.withTrimmedTop (16);
    const int pw = pr.getWidth() / 4, ph = juce::jmin (52, pr.getHeight() / 2);
    for (int i = 0; i < 8; ++i) pads[i]->setBounds (pr.getX() + (i % 4) * pw, pr.getY() + (i / 4) * ph, pw - 4, ph - 4);
}

void PerfPanel::paint (juce::Graphics& g)
{
    Theme::sectionTitle (g, "// MORPH XY", xyArea.toFloat().withHeight (14), Theme::accent);
    Theme::sectionTitle (g, "// SNAPSHOTS", snapArea.toFloat().withHeight (14), Theme::accent);
    Theme::sectionTitle (g, "// ARRANGEMENT (mesures)", arrArea.toFloat().withHeight (14), Theme::accent);
    Theme::sectionTitle (g, IDG_T ("// PADS MOMENTANÉS  (MIDI : C1 = Stutter, C#1 = Beat Repeat...)"), padArea.toFloat().withHeight (14), Theme::accent);
}
} // namespace idg::ui
