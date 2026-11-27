#include "LfoEditor.h"

namespace idg::ui
{
LfoEditor::LfoEditor (EditorContext& c) : ctx (c) {}

juce::Point<float> LfoEditor::toScreen (float x, float y) const
{
    auto r = getLocalBounds().toFloat().reduced (6.0f);
    return { r.getX() + x * r.getWidth(), r.getBottom() - y * r.getHeight() };
}

juce::Point<float> LfoEditor::fromScreen (juce::Point<float> p) const
{
    auto r = getLocalBounds().toFloat().reduced (6.0f);
    return { juce::jlimit (0.0f, 1.0f, (p.x - r.getX()) / r.getWidth()), juce::jlimit (0.0f, 1.0f, (r.getBottom() - p.y) / r.getHeight()) };
}

int LfoEditor::hitPoint (juce::Point<float> p) const
{
    const auto& s = ctx.proc.modShared.shapes[lfo];
    for (int i = 0; i < (int) s.pts.size(); ++i)
        if (toScreen (s.pts[(size_t) i].x, s.pts[(size_t) i].y).getDistanceFrom (p) < 7.0f) return i;
    return -1;
}

void LfoEditor::publish (const LfoShape& s)
{
    ctx.proc.modShared.setShape (lfo, s);
    repaint();
}

void LfoEditor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Theme::bg.withAlpha (0.85f));
    g.fillRect (r);
    g.setColour (Theme::line);
    g.drawRect (r, 1.0f);
    auto in = r.reduced (6.0f);
    for (int i = 1; i < grid; ++i)
    {
        float x = in.getX() + in.getWidth() * i / grid;
        g.setColour (Theme::line.withAlpha (i % 4 == 0 ? 0.9f : 0.4f));
        g.drawVerticalLine ((int) x, in.getY(), in.getBottom());
    }
    g.drawHorizontalLine ((int) in.getCentreY(), in.getX(), in.getRight());

    const auto& s = ctx.proc.modShared.shapes[lfo];
    juce::Path p, fill;
    const int N = 200;
    for (int i = 0; i <= N; ++i)
    {
        float x = (float) i / N;
        auto pt = toScreen (x, s.valueAt (x));
        if (i == 0) { p.startNewSubPath (pt); fill.startNewSubPath (toScreen (0, 0)); fill.lineTo (pt); }
        else { p.lineTo (pt); fill.lineTo (pt); }
    }
    fill.lineTo (toScreen (1, 0));
    fill.closeSubPath();
    const auto c = sourceColour (SrcLfo0);
    g.setColour (c.withAlpha (0.12f));
    g.fillPath (fill);
    g.setColour (juce::Colour (0xffff1e3c).withAlpha (0.3f));
    g.strokePath (p, juce::PathStrokeType (1.5f), juce::AffineTransform::translation (-1.5f, 0));
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (2.0f));
    if (! pencil)
    {
        const float ps = s.pts.size() > 12 ? 5.0f : 9.0f;
        for (auto& pt : s.pts)
        {
            auto sp = toScreen (pt.x, pt.y);
            g.setColour (Theme::bg);
            g.fillEllipse (juce::Rectangle<float> (ps, ps).withCentre (sp));
            g.setColour (Theme::accent);
            g.drawEllipse (juce::Rectangle<float> (ps, ps).withCentre (sp), 1.2f);
        }
    }
    // tête de lecture
    const float ph = ctx.proc.modShared.lfoPhase[lfo].load();
    auto hp = toScreen (ph, s.valueAt (ph));
    g.setColour (Theme::text.withAlpha (0.35f));
    g.drawVerticalLine ((int) hp.x, in.getY(), in.getBottom());
    g.setColour (Theme::text);
    g.fillEllipse (juce::Rectangle<float> (6, 6).withCentre (hp));
    g.setColour (Theme::textDim);
    g.setFont (Theme::mono (9.0f));
    g.drawText (pencil ? "CRAYON : dessinez sur la grille" : IDG_T ("clic : ajouter  •  glisser : déplacer  •  double-clic : supprimer  •  Maj+glisser : courbe"),
                in.withTop (in.getBottom() - 11), juce::Justification::centredLeft);
}

void LfoEditor::pencilAt (juce::Point<float> sp)
{
    auto v = fromScreen (sp);
    const int cell = juce::jlimit (0, grid - 1, (int) (v.x * grid));
    // reconstruit une forme en marches à partir des valeurs actuelles de la grille
    std::vector<float> vals ((size_t) grid);
    for (int i = 0; i < grid; ++i) vals[(size_t) i] = work.valueAt (((float) i + 0.5f) / grid);
    vals[(size_t) cell] = v.y;
    LfoShape s;
    for (int i = 0; i < grid; ++i)
    {
        s.pts.push_back ({ (float) i / grid, vals[(size_t) i], 0.0f });
        s.pts.push_back ({ (float) (i + 1) / grid - 0.0001f, vals[(size_t) i], 0.0f });
    }
    s.pts.back().x = 1.0f;
    work = s;
    publish (s);
}

void LfoEditor::mouseDown (const juce::MouseEvent& e)
{
    ctx.proc.pushUndo();
    work = ctx.proc.modShared.shapes[lfo];
    const auto pos = e.position;
    if (pencil) { pencilAt (pos); return; }
    dragPt = hitPoint (pos);
    curveSeg = -1;
    if (dragPt < 0)
    {
        auto v = fromScreen (pos);
        if (e.mods.isShiftDown())
        {
            for (int i = 0; i + 1 < (int) work.pts.size(); ++i)
                if (v.x >= work.pts[(size_t) i].x && v.x <= work.pts[(size_t) i + 1].x) { curveSeg = i; curveStart = work.pts[(size_t) i].curve; break; }
            return;
        }
        work.pts.push_back ({ v.x, v.y, 0.0f });
        work.sortPoints();
        dragPt = hitPoint (pos);
        publish (work);
    }
}

void LfoEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (pencil) { pencilAt (e.position); return; }
    if (curveSeg >= 0)
    {
        work.pts[(size_t) curveSeg].curve = juce::jlimit (-1.0f, 1.0f, curveStart - (float) e.getDistanceFromDragStartY() / 80.0f);
        publish (work);
        return;
    }
    if (dragPt < 0) return;
    auto v = fromScreen (e.position);
    auto& pts = work.pts;
    const int n = (int) pts.size();
    if (dragPt == 0) v.x = 0.0f;
    else if (dragPt == n - 1) v.x = 1.0f;
    else v.x = juce::jlimit (pts[(size_t) dragPt - 1].x, pts[(size_t) dragPt + 1].x, v.x);
    if (e.mods.isCommandDown()) v.y = std::round (v.y * 8.0f) / 8.0f;
    pts[(size_t) dragPt].x = v.x;
    pts[(size_t) dragPt].y = v.y;
    publish (work);
}

void LfoEditor::mouseUp (const juce::MouseEvent&) { dragPt = -1; curveSeg = -1; }

void LfoEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (pencil) return;
    work = ctx.proc.modShared.shapes[lfo];
    const int i = hitPoint (e.position);
    if (i > 0 && i < (int) work.pts.size() - 1)
    {
        work.pts.erase (work.pts.begin() + i);
        publish (work);
    }
}
} // namespace idg::ui
