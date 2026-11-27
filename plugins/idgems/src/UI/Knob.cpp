#include "Knob.h"

namespace idg::ui
{
namespace
{
constexpr float startAngle = juce::MathConstants<float>::pi * 1.25f;
constexpr float endAngle = juce::MathConstants<float>::pi * 2.75f;
}

Knob::Knob (EditorContext& c, int idx, juce::Colour col, const juce::String& lbl)
    : ctx (c), paramIndex (idx), colour (col), label (lbl)
{
    const auto& d = ctx.def (idx);
    if (label.isEmpty())
    {
        label = d.name;
        if (d.effect >= 0) label = label.fromFirstOccurrenceOf (" ", false, false);
    }
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (startAngle, endAngle, true);
    setMouseDragSensitivity (240);
    setVelocityBasedMode (false);
    setPopupMenuEnabled (false);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (ctx.proc.apvts, d.id, *this);
    if (auto* p = ctx.proc.param (idx)) setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    ctx.knobs.push_back (this);
    onDragStart = [this] { ctx.proc.pushUndo(); };
}

Knob::~Knob()
{
    auto& k = ctx.knobs;
    k.erase (std::remove (k.begin(), k.end(), this), k.end());
    if (ctx.hoveredParam == paramIndex) ctx.hoveredParam = -1;
}

juce::String Knob::valueText() const
{
    return ctx.def (paramIndex).toText ((float) getValue());
}

juce::String Knob::getTooltip()
{
    if (! Theme::tooltips) return {};
    const auto& d = ctx.def (paramIndex);
    juce::String t = d.name + " : " + valueText();
    if (d.help.isNotEmpty()) t << "\n" << d.help;
    t << IDG_T ("\n\nDouble-clic : valeur par défaut. Clic droit : cadenas, plage de random, modulations, MIDI learn.");
    t << IDG_T ("\nGlisser une source de modulation ici pour l'assigner. Alt + glisser : profondeur de la dernière modulation.");
    return t;
}

void Knob::tick()
{
    // scintillement du néon : plus la valeur est haute, plus il vacille souvent
    const float n = (float) ctx.proc.param (paramIndex)->getValue();
    if (n > 0.15f && rng.nextFloat() < 0.15f + 0.6f * n * n)
    {
        flicker = rng.nextFloat() < 0.2f * n ? 1.0f : rng.nextFloat() * 0.35f;
        repaint();
    }
    else if (flicker > 0.0f) { flicker = 0.0f; repaint(); }
    const float d = ctx.proc.bank[paramIndex].display01.load (std::memory_order_relaxed);
    int count = 0;
    for (const auto& s : ctx.proc.modShared.slots)
        if (s.src.load() >= 0 && s.dst.load() == paramIndex) ++count;
    if (std::abs (d - lastDisplay) > 0.002f || count != lastModCount)
    {
        lastDisplay = d;
        lastModCount = count;
        repaint();
    }
}

void Knob::paint (juce::Graphics& g)
{
    auto& proc = ctx.proc;
    const auto& d = ctx.def (paramIndex);
    auto* prm = proc.param (paramIndex);
    auto bounds = getLocalBounds().toFloat();
    const float labelH = big ? 16.0f : 13.0f;
    auto dialArea = bounds.withTrimmedBottom (labelH);
    const float size = juce::jmin (dialArea.getWidth(), dialArea.getHeight()) - 4.0f;
    auto dial = juce::Rectangle<float> (size, size).withCentre (dialArea.getCentre());
    const auto centre = dial.getCentre();
    const float radius = size * 0.5f;
    const bool enabled = isEnabled();
    const float alpha = enabled ? 1.0f : 0.35f;

    const float norm = prm->getValue();
    const float modNorm = proc.bank[paramIndex].display01.load();
    auto angleOf = [] (float n) { return startAngle + juce::jlimit (0.0f, 1.0f, n) * (endAngle - startAngle); };

    auto arc = [&] (float r, float a0, float a1, juce::Colour c, float thick)
    {
        juce::Path p;
        p.addCentredArc (centre.x, centre.y, r, r, 0.0f, juce::jmin (a0, a1), juce::jmax (a0, a1), true);
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    };

    if (dropHover)
    {
        g.setColour (Theme::accent.withAlpha (0.25f));
        g.fillEllipse (dial.expanded (3.0f));
    }

    const float ringR = radius - 3.0f;
    const float rmin = proc.bank[paramIndex].rmin.load(), rmax = proc.bank[paramIndex].rmax.load();
    const bool bipolar = d.type == PType::Float && d.min < 0.0f && d.max > 0.0f;
    const float zeroN = bipolar ? prm->convertTo0to1 (0.0f) : 0.0f;
    // rail éteint : fin et sombre
    arc (ringR, startAngle, endAngle, Theme::line.withAlpha (0.9f * alpha), 2.0f);
    // tube néon : plus la valeur est forte, plus il bloome et scintille
    {
        const float amount = bipolar ? std::abs (norm - zeroN) / juce::jmax (zeroN, 1.0f - zeroN) : norm;
        const float intensity = juce::jlimit (0.0f, 1.0f, amount);
        const float flick = 1.0f - flicker * (0.08f + 0.3f * intensity * intensity);
        const float bloom = (0.2f + 0.8f * intensity) * flick * alpha;
        const float a0 = bipolar ? angleOf (zeroN) : startAngle, a1 = angleOf (norm);
        if (std::abs (a1 - a0) > 0.01f)
        {
            // halo : couches de plus en plus larges et transparentes
            const float spread = 2.0f + 7.0f * intensity;
            for (int k = 4; k >= 1; --k)
                arc (ringR, a0, a1, colour.withAlpha (bloom * 0.11f * (5 - k) / 4.0f), 3.0f + spread * (float) k * 0.55f);
            // tube coloré puis cœur presque blanc
            arc (ringR, a0, a1, colour.withAlpha (juce::jmin (1.0f, 0.75f + 0.25f * bloom) * alpha), 3.2f);
            arc (ringR, a0, a1, colour.interpolatedWith (juce::Colours::white, 0.35f + 0.45f * intensity).withAlpha ((0.5f + 0.5f * bloom) * alpha), 1.2f);
            // éclat au bout du tube
            const float ae = a1 - juce::MathConstants<float>::halfPi;
            const juce::Point<float> tip { centre.x + std::cos (ae) * ringR, centre.y + std::sin (ae) * ringR };
            const float gs = 5.0f + 9.0f * intensity;
            g.setColour (colour.withAlpha (bloom * 0.35f));
            g.fillEllipse (juce::Rectangle<float> (gs, gs).withCentre (tip));
            g.setColour (juce::Colours::white.withAlpha ((0.6f + 0.4f * bloom) * alpha));
            g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre (tip));
        }
    }
    // plage de random personnalisée (trait bleu extérieur)
    if (std::abs (rmin - d.rmin) > 1.0e-3f || std::abs (rmax - d.rmax) > 1.0e-3f)
        arc (radius - 0.2f, angleOf (rmin), angleOf (rmax), Theme::blue.withAlpha (0.9f * alpha), 1.2f);

    // modulations (anneau intérieur)
    float innerR = ringR - 5.0f;
    for (const auto& s : proc.modShared.slots)
    {
        const int src = s.src.load();
        if (src < 0 || s.dst.load() != paramIndex) continue;
        const float depth = s.depth.load();
        auto c = sourceColour (src).withAlpha (0.9f * alpha);
        if (s.bipolar.load()) arc (innerR, angleOf (norm - depth), angleOf (norm + depth), c, 2.0f);
        else arc (innerR, angleOf (norm), angleOf (norm + depth), c, 2.0f);
        innerR -= 3.0f;
        if (innerR < radius * 0.45f) break;
    }

    // corps octogonal avec encoche
    auto body = dial.reduced (size * 0.25f);
    juce::Path oct;
    for (int i = 0; i < 8; ++i)
    {
        const float a = juce::MathConstants<float>::pi * (0.125f + i * 0.25f);
        juce::Point<float> pt { centre.x + std::cos (a) * body.getWidth() * 0.52f, centre.y + std::sin (a) * body.getWidth() * 0.52f };
        if (i == 0) oct.startNewSubPath (pt); else oct.lineTo (pt);
    }
    oct.closeSubPath();
    g.setColour (Theme::panelHi.withAlpha (alpha));
    g.fillPath (oct);
    g.setColour (Theme::line.brighter (0.35f).withAlpha (alpha));
    g.strokePath (oct, juce::PathStrokeType (1.0f));
    {
        const float a = angleOf (norm) - juce::MathConstants<float>::halfPi;
        const float r0 = body.getWidth() * 0.12f, r1 = body.getWidth() * 0.5f;
        g.setColour (colour.withAlpha (alpha));
        g.drawLine (centre.x + std::cos (a) * r0, centre.y + std::sin (a) * r0, centre.x + std::cos (a) * r1, centre.y + std::sin (a) * r1, 2.2f);
    }
    // point de valeur modulée
    if (std::abs (modNorm - norm) > 0.002f)
    {
        const float a = angleOf (modNorm) - juce::MathConstants<float>::halfPi;
        g.setColour (Theme::text.withAlpha (alpha));
        g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre ({ centre.x + std::cos (a) * ringR, centre.y + std::sin (a) * ringR }));
    }

    // cadenas
    if (proc.bank[paramIndex].locked.load())
    {
        auto lr = juce::Rectangle<float> (dial.getRight() - 9.0f, dial.getY(), 8.0f, 9.0f);
        g.setColour (Theme::red);
        g.fillRect (lr.withTrimmedTop (4.0f));
        g.drawRoundedRectangle (lr.withTrimmedBottom (4.0f).reduced (1.5f, 0.0f), 2.0f, 1.2f);
    }
    // MIDI
    const int cc = proc.ccForParam (paramIndex);
    if (cc >= 0 || proc.getMidiLearnTarget() == paramIndex)
    {
        g.setColour (proc.getMidiLearnTarget() == paramIndex ? Theme::accent : Theme::blue);
        g.setFont (Theme::mono (8.5f, true));
        g.drawText (proc.getMidiLearnTarget() == paramIndex ? "LEARN" : "CC" + juce::String (cc),
                    juce::Rectangle<float> (0, 0, 34.0f, 9.0f), juce::Justification::topLeft);
    }

    // libellé / valeur
    auto lab = bounds.removeFromBottom (labelH);
    g.setFont (Theme::mono (big ? 11.5f : 10.0f, isMouseButtonDown()));
    const bool showValue = hovering || isMouseButtonDown();
    g.setColour ((showValue ? colour : Theme::textDim).withAlpha (alpha));
    g.drawFittedText (showValue ? valueText() : label.toUpperCase(), lab.toNearestInt(), juce::Justification::centred, 1, 0.7f);
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showMenu(); return; }
    if (e.mods.isAltDown())
    {
        // Alt + glisser : profondeur de la dernière modulation
        altSlot = -1;
        for (int i = MaxModSlots - 1; i >= 0; --i)
            if (ctx.proc.modShared.slots[i].src.load() >= 0 && ctx.proc.modShared.slots[i].dst.load() == paramIndex) { altSlot = i; break; }
        if (altSlot >= 0) { altStartDepth = ctx.proc.modShared.slots[altSlot].depth.load(); return; }
    }
    altSlot = -1;
    juce::Slider::mouseDown (e);
}

void Knob::mouseDrag (const juce::MouseEvent& e)
{
    if (altSlot >= 0)
    {
        float dpt = juce::jlimit (-1.0f, 1.0f, altStartDepth - (float) e.getDistanceFromDragStartY() / 150.0f);
        ctx.proc.modShared.slots[altSlot].depth = dpt;
        repaint();
        return;
    }
    if (! e.mods.isPopupMenu()) juce::Slider::mouseDrag (e);
}

void Knob::mouseUp (const juce::MouseEvent& e)
{
    if (altSlot >= 0) { altSlot = -1; return; }
    if (! e.mods.isPopupMenu()) juce::Slider::mouseUp (e);
}

void Knob::mouseEnter (const juce::MouseEvent& e)
{
    hovering = true;
    ctx.hoveredParam = paramIndex;
    juce::Slider::mouseEnter (e);
    repaint();
}

void Knob::mouseExit (const juce::MouseEvent& e)
{
    hovering = false;
    if (ctx.hoveredParam == paramIndex) ctx.hoveredParam = -1;
    juce::Slider::mouseExit (e);
    repaint();
}

bool Knob::isInterestedInDragSource (const SourceDetails& d)
{
    return d.description.toString().startsWith ("mod:");
}

void Knob::itemDropped (const SourceDetails& d)
{
    dropHover = false;
    const int src = d.description.toString().fromFirstOccurrenceOf ("mod:", false, false).getIntValue();
    if (src >= 0 && src < NumSources)
    {
        ctx.proc.pushUndo();
        const bool bip = src < SrcRnd0 + NumRandMods; // LFO et random : bipolaire par défaut
        ctx.proc.modShared.addConnection (src, paramIndex, 0.3f, bip);
        if (ctx.flash) ctx.flash (sourceName (src) + " -> " + ctx.def (paramIndex).name);
    }
    repaint();
}

void Knob::showMenu()
{
    auto& proc = ctx.proc;
    auto& bp = proc.bank[paramIndex];
    juce::PopupMenu m;
    m.addSectionHeader (ctx.def (paramIndex).name);
    m.addItem (1, bp.locked.load() ? IDG_T ("Déverrouiller (L)") : IDG_T ("Verrouiller : jamais randomisé (L)"));
    juce::PopupMenu range;
    range.addItem (10, "Min du random = valeur actuelle");
    range.addItem (11, "Max du random = valeur actuelle");
    range.addItem (12, IDG_T ("Plage par défaut"));
    range.addItem (13, IDG_T ("Plage complète"));
    m.addSubMenu ("Plage de random", range);

    juce::PopupMenu mods;
    int n = 0;
    for (int i = 0; i < MaxModSlots; ++i)
    {
        const auto& s = proc.modShared.slots[i];
        if (s.src.load() < 0 || s.dst.load() != paramIndex) continue;
        juce::PopupMenu sm;
        sm.addItem (1000 + i, "Supprimer");
        sm.addItem (2000 + i, "Inverser la profondeur");
        sm.addItem (3000 + i, "Bipolaire", true, s.bipolar.load());
        mods.addSubMenu (sourceName (s.src.load()) + " (" + juce::String (juce::roundToInt (s.depth.load() * 100)) + " %)", sm);
        ++n;
    }
    if (n > 0) { mods.addSeparator(); mods.addItem (20, "Supprimer toutes les modulations"); }
    m.addSubMenu ("Modulations", mods, n > 0);

    m.addSeparator();
    const int cc = proc.ccForParam (paramIndex);
    m.addItem (30, IDG_T ("MIDI learn (bouger un contrôleur)"));
    if (cc >= 0) m.addItem (31, "Oublier le CC " + juce::String (cc));
    m.addSeparator();
    m.addItem (40, IDG_T ("Valeur par défaut"));

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r)
    {
        auto& pr = ctx.proc;
        auto& b = pr.bank[paramIndex];
        if (r == 1) b.locked = ! b.locked.load();
        else if (r == 10) b.rmin = pr.getNorm (paramIndex);
        else if (r == 11) b.rmax = pr.getNorm (paramIndex);
        else if (r == 12) { b.rmin = ctx.def (paramIndex).rmin; b.rmax = ctx.def (paramIndex).rmax; }
        else if (r == 13) { b.rmin = 0.0f; b.rmax = 1.0f; }
        else if (r == 20) { pr.pushUndo(); pr.modShared.removeConnectionsFor (paramIndex); }
        else if (r == 30) pr.startMidiLearn (paramIndex);
        else if (r == 31) pr.clearMidiLearn (paramIndex);
        else if (r == 40) { pr.pushUndo(); pr.setNormGesture (paramIndex, pr.param (paramIndex)->getDefaultValue()); }
        else if (r >= 1000 && r < 2000) { pr.pushUndo(); pr.modShared.removeConnection (r - 1000); }
        else if (r >= 2000 && r < 3000) { auto& s = pr.modShared.slots[r - 2000]; s.depth = -s.depth.load(); }
        else if (r >= 3000 && r < 4000) { auto& s = pr.modShared.slots[r - 3000]; s.bipolar = ! s.bipolar.load(); }
        repaint();
    });
}
} // namespace idg::ui
