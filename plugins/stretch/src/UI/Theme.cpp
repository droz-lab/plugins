#include "Theme.h"
#include "BinaryData.h"

namespace idg::ui
{
juce::Font Theme::title (float h)
{
    static juce::Typeface::Ptr face = juce::Typeface::createSystemTypefaceFor (BinaryData::MichromaRegular_ttf,
                                                                               (size_t) BinaryData::MichromaRegular_ttfSize);
    if (face == nullptr) return sans (h, true);
    return juce::Font (juce::FontOptions (face).withHeight (h));
}

void Theme::cutText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float height,
                     juce::Colour colour, juce::Justification just, float glow, bool metal)
{
    auto f = title (height);
    f.setExtraKerningFactor (0.08f);
    juce::GlyphArrangement ga;
    ga.addFittedText (f, text, area.getX(), area.getY(), area.getWidth(), area.getHeight(), just, 1, 0.6f);

    // halo (lueur orange) sous le texte
    if (glow > 0.0f)
    {
        juce::Path all;
        ga.createPath (all);
        juce::DropShadow (accent.withAlpha (juce::jlimit (0.0f, 1.0f, glow)), juce::roundToInt (height * 0.9f), {}).drawForPath (g, all);
    }

    const auto bounds = ga.getBoundingBox (0, -1, true);
    // faux gras : contour épaissi en plus du remplissage
    const juce::PathStrokeType bold (juce::jmax (0.6f, height * 0.045f), juce::PathStrokeType::mitered);
    auto paintGlyph = [&] (juce::Graphics& gg, const juce::Path& gp) { gg.fillPath (gp); gg.strokePath (gp, bold); };
    auto fillFor = [&] (juce::Graphics& gg)
    {
        if (metal)
        {
            juce::ColourGradient grad (colour.brighter (0.6f), 0.0f, bounds.getY(), colour.darker (0.25f), 0.0f, bounds.getBottom(), false);
            grad.addColour (0.48, colour.brighter (0.15f));
            grad.addColour (0.52, colour.darker (0.35f));
            gg.setGradientFill (grad);
        }
        else gg.setColour (colour);
    };

    for (int i = 0; i < ga.getNumGlyphs(); ++i)
    {
        const auto& glyph = ga.getGlyph (i);
        if (glyph.isWhitespace()) continue;
        juce::Path gp;
        glyph.createPath (gp);
        const auto gb = gp.getBounds();
        if (gb.isEmpty()) continue;

        // découpe pseudo-aléatoire mais stable (dépend de la lettre et de sa position)
        const auto ch = (uint32_t) glyph.getCharacter();
        uint32_t hsh = ch * 2654435761u ^ (uint32_t) (i * 40503u + text.length() * 977u);
        hsh ^= hsh >> 13; hsh *= 0x5bd1e995u; hsh ^= hsh >> 15;
        const float r1 = (float) (hsh & 0xffff) / 65535.0f, r2 = (float) ((hsh >> 16) & 0xffff) / 65535.0f;

        if (r1 < 0.3f) { fillFor (g); paintGlyph (g, gp); continue; }

        const float cutY = gb.getY() + gb.getHeight() * (0.35f + 0.3f * r2);
        const float slope = (r2 - 0.5f) * gb.getHeight() * 0.5f;
        const float gap = juce::jmax (0.9f, height * 0.1f);
        const float shift = (r1 > 0.7f ? 1.0f : -0.6f) * juce::jmax (0.7f, height * 0.09f);
        const float x0 = gb.getX() - 4.0f, x1 = gb.getRight() + 4.0f;

        juce::Path upper, lower;
        upper.startNewSubPath (x0, gb.getY() - 4.0f);
        upper.lineTo (x1, gb.getY() - 4.0f);
        upper.lineTo (x1, cutY - slope * 0.5f - gap * 0.5f);
        upper.lineTo (x0, cutY + slope * 0.5f - gap * 0.5f);
        upper.closeSubPath();
        lower.startNewSubPath (x0, cutY + slope * 0.5f + gap * 0.5f);
        lower.lineTo (x1, cutY - slope * 0.5f + gap * 0.5f);
        lower.lineTo (x1, gb.getBottom() + 4.0f);
        lower.lineTo (x0, gb.getBottom() + 4.0f);
        lower.closeSubPath();

        {
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (upper);
            fillFor (g);
            paintGlyph (g, gp);
        }
        {
            juce::Graphics::ScopedSaveState s (g);
            g.addTransform (juce::AffineTransform::translation (shift, 0.0f));
            g.reduceClipRegion (lower);
            fillFor (g);
            paintGlyph (g, gp);
        }
    }
}
} // namespace idg::ui
