#pragma once
#include "../Core/ParamDefs.h"

namespace idg::ui
{
/** Direction artistique : noir texturé, accent orange, touches de bleu, aberration chromatique. */
struct Theme
{
    static inline juce::Colour accent { 0xffff6a00 };
    static inline juce::Colour blue { 0xff2fa8ff };
    static inline juce::Colour bg { 0xff0a0a0c };
    static inline juce::Colour panel { 0xff121216 };
    static inline juce::Colour panelHi { 0xff1b1b21 };
    static inline juce::Colour line { 0xff2a2a33 };
    static inline juce::Colour text { 0xffe8e6e3 };
    static inline juce::Colour textDim { 0xff8a8892 };
    static inline juce::Colour red { 0xffff2d55 };
    static inline bool tooltips = true;
    static inline float glitchAmount = 0.5f;

    static juce::Font mono (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, bold ? juce::Font::bold : juce::Font::plain));
    }
    static juce::Font sans (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }

    /** Texte avec aberration chromatique (rouge / bleu décalés). */
    static void glitchText (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, juce::Justification j,
                            juce::Colour c, float offset = 1.2f)
    {
        g.setColour (juce::Colour (0xffff1e3c).withAlpha (0.55f));
        g.drawText (s, r.translated (-offset, 0), j, false);
        g.setColour (blue.withAlpha (0.55f));
        g.drawText (s, r.translated (offset, 0.3f), j, false);
        g.setColour (c);
        g.drawText (s, r, j, false);
    }

    /** Cadre « mal imprimé » : fond, contour principal + deux contours décalés (rouge / bleu), encoche déchirée. */
    static void panelFrame (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour edge, float alpha = 1.0f)
    {
        g.setColour (panel.withAlpha (0.74f * alpha));
        g.fillRect (r);
        // trame de demi-teintes discrète dans le coin bas-droit
        g.setColour (edge.withAlpha (0.07f * alpha));
        for (float y = r.getBottom() - 30.0f; y < r.getBottom() - 2.0f; y += 4.0f)
            for (float x = r.getRight() - 60.0f + std::fmod (y, 8.0f) * 0.5f; x < r.getRight() - 2.0f; x += 4.0f)
                g.fillRect (x, y, 1.5f, 1.5f);
        g.setColour (juce::Colour (0xffff1e3c).withAlpha (0.14f * alpha));
        g.drawRect (r.translated (-1.5f, 0.5f), 1.0f);
        g.setColour (blue.withAlpha (0.14f * alpha));
        g.drawRect (r.translated (1.5f, -0.5f), 1.0f);
        g.setColour (edge.withAlpha (0.55f * alpha));
        g.drawRect (r, 1.0f);
        // encoche déchirée en haut à droite
        juce::Path tear;
        const float tx = r.getRight() - 46.0f, ty = r.getY();
        tear.startNewSubPath (tx, ty);
        tear.lineTo (tx + 8, ty + 5); tear.lineTo (tx + 15, ty + 1); tear.lineTo (tx + 24, ty + 6); tear.lineTo (tx + 31, ty);
        g.setColour (bg);
        g.strokePath (tear, juce::PathStrokeType (3.0f));
        g.setColour (accent.withAlpha (0.25f * alpha));
        g.strokePath (tear, juce::PathStrokeType (5.0f));
        g.setColour (accent.brighter (0.5f).withAlpha (0.95f * alpha));
        g.strokePath (tear, juce::PathStrokeType (1.0f));
        // repères d'impression
        g.setColour (edge.withAlpha (0.9f * alpha));
        g.fillRect (r.getX() - 1.0f, r.getBottom() - 10.0f, 2.0f, 10.0f);
        g.fillRect (r.getX() - 1.0f, r.getBottom() - 1.0f, 10.0f, 2.0f);
    }

    /** Étiquette inversée façon ruban adhésif : bloc de couleur, texte noir. */
    static float tapeLabel (juce::Graphics& g, const juce::String& s, float x, float y, juce::Colour c, float h = 13.0f)
    {
        auto f = mono (h * 0.78f, true);
        g.setFont (f);
        const float w = juce::GlyphArrangement::getStringWidth (f, s) + 10.0f;
        juce::Path p;
        p.startNewSubPath (x, y + 1.0f);
        p.lineTo (x + w, y);
        p.lineTo (x + w - 2.0f, y + h);
        p.lineTo (x + 1.5f, y + h - 0.5f);
        p.closeSubPath();
        g.setColour (blue.withAlpha (0.6f));
        g.fillPath (p, juce::AffineTransform::translation (2.0f, 1.5f));
        g.setColour (c);
        g.fillPath (p);
        g.setColour (juce::Colours::black);
        g.drawText (s, juce::Rectangle<float> (x + 5.0f, y, w - 8.0f, h), juce::Justification::centredLeft, false);
        return w;
    }

    /** Police des titres (large, futuriste). */
    static juce::Font title (float h);

    /** Texte « tranché » façon logo Altered Brother : lettres coupées en biais, moitiés décalées,
        rendu métal et halo orange optionnel. */
    static void cutText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float height,
                         juce::Colour colour, juce::Justification just = juce::Justification::centredLeft,
                         float glow = 0.0f, bool metal = true);

    static void sectionTitle (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, juce::Colour c)
    {
        auto t = s.startsWith ("// ") ? s.substring (3) : s;
        // petit trait incandescent devant le titre
        g.setColour (c);
        g.fillRect (r.getX(), r.getY() + 3.0f, 2.0f, 8.0f);
        g.setColour (c.withAlpha (0.35f));
        g.fillRect (r.getX() - 1.0f, r.getY() + 2.0f, 4.0f, 10.0f);
        cutText (g, t.toUpperCase(), r.withTrimmedLeft (7.0f).withHeight (14.0f), 10.5f, c.interpolatedWith (text, 0.45f),
                 juce::Justification::centredLeft, 0.0f, true);
    }
};
} // namespace idg::ui
