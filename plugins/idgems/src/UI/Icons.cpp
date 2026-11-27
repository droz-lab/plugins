#include "Icons.h"

namespace idg::ui
{
namespace
{
constexpr float twoPi = juce::MathConstants<float>::twoPi;

void wave (juce::Path& p, float x0, float x1, float yc, float amp, float cycles, float phase = 0.0f, int steps = 40)
{
    for (int i = 0; i <= steps; ++i)
    {
        float t = (float) i / steps;
        float x = x0 + (x1 - x0) * t;
        float y = yc - amp * std::sin (twoPi * (cycles * t + phase));
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
}

void arrowHead (juce::Path& p, float x, float y, float dx, float dy, float s = 0.12f)
{
    // pointe en V orientée selon (dx, dy)
    float len = std::sqrt (dx * dx + dy * dy);
    dx /= len; dy /= len;
    float px = -dy, py = dx;
    p.startNewSubPath (x - dx * s + px * s * 0.7f, y - dy * s + py * s * 0.7f);
    p.lineTo (x, y);
    p.lineTo (x - dx * s - px * s * 0.7f, y - dy * s - py * s * 0.7f);
}
}

juce::Path effectIcon (int fx)
{
    juce::Path p;
    switch (fx)
    {
        case FxStutter: // tranches répétées qui décroissent
            for (int i = 0; i < 5; ++i)
            {
                float x = 0.12f + i * 0.18f, h = 0.7f - i * 0.12f;
                p.addRectangle (x, 0.5f - h * 0.5f, 0.08f, h);
            }
            break;
        case FxBeatRepeat: // boucle + barres
            p.addCentredArc (0.5f, 0.5f, 0.36f, 0.36f, 0.0f, 0.4f, twoPi - 0.2f, true);
            arrowHead (p, 0.5f + 0.36f * std::sin (0.4f), 0.5f - 0.36f * std::cos (0.4f), 1.0f, 0.3f);
            p.addRectangle (0.36f, 0.4f, 0.06f, 0.2f);
            p.addRectangle (0.47f, 0.4f, 0.06f, 0.2f);
            p.addRectangle (0.58f, 0.4f, 0.06f, 0.2f);
            break;
        case FxReverse: // onde + flèche vers la gauche
            wave (p, 0.1f, 0.9f, 0.62f, 0.14f, 2.0f);
            p.startNewSubPath (0.85f, 0.22f); p.lineTo (0.15f, 0.22f);
            arrowHead (p, 0.15f, 0.22f, -1.0f, 0.0f);
            break;
        case FxTapeStop: // bobine + onde qui s'étire
            p.addEllipse (0.08f, 0.1f, 0.4f, 0.4f);
            p.addEllipse (0.24f, 0.26f, 0.08f, 0.08f);
            for (int i = 0; i <= 40; ++i)
            {
                float t = (float) i / 40.0f;
                float x = 0.1f + 0.85f * t;
                float y = 0.75f - 0.13f * std::sin (twoPi * 3.0f * std::sqrt (t)) * (1.0f - t);
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            break;
        case FxShuffle: // flèches croisées
            p.startNewSubPath (0.1f, 0.25f); p.cubicTo (0.5f, 0.25f, 0.5f, 0.75f, 0.88f, 0.75f);
            p.startNewSubPath (0.1f, 0.75f); p.cubicTo (0.5f, 0.75f, 0.5f, 0.25f, 0.88f, 0.25f);
            arrowHead (p, 0.9f, 0.75f, 1.0f, 0.0f);
            arrowHead (p, 0.9f, 0.25f, 1.0f, 0.0f);
            break;
        case FxFreeze: // flocon
            for (int i = 0; i < 6; ++i)
            {
                float a = twoPi * i / 6.0f;
                float cx = 0.5f, cy = 0.5f, ex = cx + 0.4f * std::cos (a), ey = cy + 0.4f * std::sin (a);
                p.startNewSubPath (cx, cy); p.lineTo (ex, ey);
                float mx = cx + 0.25f * std::cos (a), my = cy + 0.25f * std::sin (a);
                p.startNewSubPath (mx, my); p.lineTo (mx + 0.1f * std::cos (a + 0.8f), my + 0.1f * std::sin (a + 0.8f));
                p.startNewSubPath (mx, my); p.lineTo (mx + 0.1f * std::cos (a - 0.8f), my + 0.1f * std::sin (a - 0.8f));
            }
            break;
        case FxScratch: // vinyle + geste
            p.addEllipse (0.1f, 0.1f, 0.8f, 0.8f);
            p.addEllipse (0.42f, 0.42f, 0.16f, 0.16f);
            p.addCentredArc (0.5f, 0.5f, 0.28f, 0.28f, 0.0f, -1.2f, 0.3f, true);
            arrowHead (p, 0.5f + 0.28f * std::sin (0.3f), 0.5f - 0.28f * std::cos (0.3f), 1.0f, 0.4f, 0.09f);
            break;
        case FxBitcrush: // escalier
            p.startNewSubPath (0.1f, 0.85f);
            for (int i = 0; i < 5; ++i)
            {
                float x = 0.1f + i * 0.16f, y = 0.85f - i * 0.16f;
                p.lineTo (x, y - 0.16f); p.lineTo (x + 0.16f, y - 0.16f);
            }
            break;
        case FxDistortion: // sinus écrêté
            for (int i = 0; i <= 50; ++i)
            {
                float t = (float) i / 50.0f;
                float y = juce::jlimit (-0.22f, 0.22f, 0.4f * std::sin (twoPi * 1.5f * t));
                if (i == 0) p.startNewSubPath (0.08f + 0.84f * t, 0.5f - y); else p.lineTo (0.08f + 0.84f * t, 0.5f - y);
            }
            p.startNewSubPath (0.08f, 0.22f); p.lineTo (0.92f, 0.22f);
            p.startNewSubPath (0.08f, 0.78f); p.lineTo (0.92f, 0.78f);
            break;
        case FxRingMod: // anneaux entrelacés
            p.addEllipse (0.1f, 0.25f, 0.5f, 0.5f);
            p.addEllipse (0.4f, 0.25f, 0.5f, 0.5f);
            break;
        case FxFreqShift: // onde décalée
            wave (p, 0.08f, 0.6f, 0.4f, 0.14f, 2.0f);
            wave (p, 0.4f, 0.92f, 0.68f, 0.1f, 3.0f);
            p.startNewSubPath (0.1f, 0.85f); p.lineTo (0.9f, 0.85f);
            arrowHead (p, 0.9f, 0.85f, 1.0f, 0.0f, 0.08f);
            break;
        case FxCorruption: // blocs cassés
            p.addRectangle (0.1f, 0.15f, 0.35f, 0.2f);
            p.addRectangle (0.55f, 0.15f, 0.35f, 0.2f);
            p.addRectangle (0.22f, 0.42f, 0.5f, 0.16f);
            p.addRectangle (0.1f, 0.66f, 0.18f, 0.2f);
            p.addRectangle (0.44f, 0.68f, 0.46f, 0.16f);
            break;
        case FxPitchShift: // note + flèche montante
            p.addEllipse (0.12f, 0.62f, 0.26f, 0.2f);
            p.startNewSubPath (0.38f, 0.72f); p.lineTo (0.38f, 0.16f); p.lineTo (0.55f, 0.26f);
            p.startNewSubPath (0.78f, 0.85f); p.lineTo (0.78f, 0.2f);
            arrowHead (p, 0.78f, 0.2f, 0.0f, -1.0f);
            break;
        case FxFormant: // bouche / voyelle
            p.startNewSubPath (0.1f, 0.5f);
            p.cubicTo (0.3f, 0.18f, 0.7f, 0.18f, 0.9f, 0.5f);
            p.cubicTo (0.7f, 0.82f, 0.3f, 0.82f, 0.1f, 0.5f);
            p.startNewSubPath (0.25f, 0.5f); p.cubicTo (0.4f, 0.4f, 0.6f, 0.6f, 0.75f, 0.5f);
            break;
        case FxFilter: // courbe passe-bas résonante
            p.startNewSubPath (0.08f, 0.45f); p.lineTo (0.45f, 0.45f);
            p.cubicTo (0.58f, 0.45f, 0.58f, 0.2f, 0.64f, 0.25f);
            p.cubicTo (0.7f, 0.3f, 0.75f, 0.85f, 0.92f, 0.88f);
            break;
        case FxComb: // peigne
            p.startNewSubPath (0.1f, 0.2f); p.lineTo (0.9f, 0.2f);
            for (int i = 0; i < 7; ++i) { float x = 0.12f + i * 0.126f; p.startNewSubPath (x, 0.2f); p.lineTo (x, 0.82f); }
            break;
        case FxVocoder: // micro + bandes
            p.addRoundedRectangle (0.12f, 0.12f, 0.2f, 0.42f, 0.1f);
            p.startNewSubPath (0.06f, 0.42f); p.cubicTo (0.06f, 0.72f, 0.38f, 0.72f, 0.38f, 0.42f);
            p.startNewSubPath (0.22f, 0.66f); p.lineTo (0.22f, 0.86f);
            for (int i = 0; i < 4; ++i) { float h = 0.2f + 0.15f * ((i * 7) % 4); p.addRectangle (0.5f + i * 0.11f, 0.85f - h, 0.06f, h); }
            break;
        case FxPixelSort: // barres triées
            for (int i = 0; i < 6; ++i) { float h = 0.12f + i * 0.13f; p.addRectangle (0.1f + i * 0.14f, 0.88f - h, 0.09f, h); }
            break;
        case FxDelay: // échos qui s'éteignent
            for (int i = 0; i < 4; ++i) { float s = 0.3f - i * 0.06f; p.addEllipse (0.08f + i * 0.22f, 0.5f - s * 0.5f, s, s); }
            break;
        case FxReverb: // arcs concentriques
            for (int i = 0; i < 4; ++i) { float r = 0.12f + i * 0.12f; p.addCentredArc (0.2f, 0.5f, r, r, 0.0f, 0.4f, 2.74f, true); }
            break;
        case FxChorus: // trois ondes décalées
            for (int i = 0; i < 3; ++i) wave (p, 0.08f, 0.92f, 0.32f + i * 0.18f, 0.08f, 2.0f, i * 0.12f);
            break;
        case FxFlanger: // balayage en jet
            for (int i = 0; i <= 40; ++i)
            {
                float t = (float) i / 40.0f;
                float y = 0.5f - 0.32f * std::sin (twoPi * (1.0f + 3.0f * t) * t);
                if (i == 0) p.startNewSubPath (0.08f + 0.84f * t, y); else p.lineTo (0.08f + 0.84f * t, y);
            }
            break;
        case FxPhaser: // cercle de phase
            p.addEllipse (0.1f, 0.1f, 0.8f, 0.8f);
            wave (p, 0.1f, 0.9f, 0.5f, 0.2f, 1.0f);
            p.startNewSubPath (0.5f, 0.1f); p.lineTo (0.5f, 0.9f);
            break;
        case FxGate: // impulsions
            p.startNewSubPath (0.06f, 0.78f);
            for (int i = 0; i < 4; ++i)
            {
                float x = 0.06f + i * 0.22f;
                p.lineTo (x, 0.22f); p.lineTo (x + 0.11f, 0.22f); p.lineTo (x + 0.11f, 0.78f); p.lineTo (x + 0.22f, 0.78f);
            }
            break;
        case FxTremolo: // enveloppe ondulante
            for (int i = 0; i <= 60; ++i)
            {
                float t = (float) i / 60.0f;
                float env = 0.12f + 0.2f * (0.5f + 0.5f * std::sin (twoPi * 2.0f * t));
                float y = 0.5f - env * std::sin (twoPi * 9.0f * t);
                if (i == 0) p.startNewSubPath (0.06f + 0.88f * t, y); else p.lineTo (0.06f + 0.88f * t, y);
            }
            break;
        case FxAutoPan: // L <-> R
            p.startNewSubPath (0.12f, 0.5f); p.lineTo (0.88f, 0.5f);
            arrowHead (p, 0.12f, 0.5f, -1.0f, 0.0f);
            arrowHead (p, 0.88f, 0.5f, 1.0f, 0.0f);
            p.addEllipse (0.4f, 0.4f, 0.2f, 0.2f);
            break;
        case FxStereo: // M/S : deux cercles
            p.addEllipse (0.08f, 0.22f, 0.56f, 0.56f);
            p.addEllipse (0.36f, 0.22f, 0.56f, 0.56f);
            p.startNewSubPath (0.5f, 0.1f); p.lineTo (0.5f, 0.9f);
            break;
        default:
            p.addRectangle (0.2f, 0.2f, 0.6f, 0.6f);
            break;
    }
    return p;
}

void drawEffectIcon (juce::Graphics& g, int fx, juce::Rectangle<float> area, juce::Colour c, float thickness, bool glitch)
{
    auto p = effectIcon (fx);
    const float s = juce::jmin (area.getWidth(), area.getHeight());
    auto t = juce::AffineTransform::scale (s).translated (area.getCentreX() - s * 0.5f, area.getCentreY() - s * 0.5f);
    juce::PathStrokeType st (thickness, juce::PathStrokeType::mitered, juce::PathStrokeType::square);
    if (glitch)
    {
        const float o = juce::jmax (0.8f, s * 0.04f);
        g.setColour (juce::Colour (0xffff1e3c).withAlpha (0.5f * c.getFloatAlpha()));
        g.strokePath (p, st, t.translated (-o, 0.0f));
        g.setColour (Theme::blue.withAlpha (0.5f * c.getFloatAlpha()));
        g.strokePath (p, st, t.translated (o, 0.0f));
    }
    g.setColour (c);
    g.strokePath (p, st, t);
}
} // namespace idg::ui
