#include "Visuals.h"

namespace idg::ui
{
namespace
{
float valueNoise (float x, float y, uint32_t seed)
{
    auto h = [seed] (int ix, int iy) { return idg::dsp::hash01 ((uint32_t) ix * 73856093u ^ (uint32_t) iy * 19349663u ^ seed); };
    int ix = (int) std::floor (x), iy = (int) std::floor (y);
    float fx = x - (float) ix, fy = y - (float) iy;
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    float a = h (ix, iy), b = h (ix + 1, iy), c = h (ix, iy + 1), d = h (ix + 1, iy + 1);
    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}

/** Fissure ramifiée (ligne brisée) qui part d'un point. */
void crack (juce::Path& p, juce::Random& r, juce::Point<float> start, float angle, float length, int depth)
{
    auto pt = start;
    p.startNewSubPath (pt);
    float travelled = 0.0f;
    while (travelled < length)
    {
        float step = 8.0f + r.nextFloat() * 22.0f;
        angle += (r.nextFloat() - 0.5f) * 0.9f;
        pt += { std::cos (angle) * step, std::sin (angle) * step };
        p.lineTo (pt);
        travelled += step;
        if (depth > 0 && r.nextFloat() < 0.18f)
        {
            juce::Path branch;
            crack (branch, r, pt, angle + (r.nextBool() ? 1.0f : -1.0f) * (0.6f + r.nextFloat() * 0.6f), length * 0.4f, depth - 1);
            p.addPath (branch);
            p.startNewSubPath (pt);
        }
    }
}
}

juce::Image makeBackgroundTexture (int w, int h, juce::Colour accent)
{
    juce::Image img (juce::Image::RGB, juce::jmax (1, w), juce::jmax (1, h), true);
    juce::Random r (1337);
    {
        // roche sombre : bruit multi-octaves, légèrement chaud
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                float n = 0.55f * valueNoise (x / 90.0f, y / 90.0f, 11u) + 0.3f * valueNoise (x / 23.0f, y / 23.0f, 29u)
                        + 0.15f * valueNoise (x / 5.0f, y / 5.0f, 47u);
                float grain = r.nextFloat() * 3.0f;
                int v = (int) (4.0f + n * 13.0f + grain);
                bd.setPixelColour (x, y, juce::Colour::fromRGB ((juce::uint8) (v + 1), (juce::uint8) v, (juce::uint8) juce::jmax (0, v - 1)));
            }
    }
    juce::Graphics g (img);
    // halo de lumière froide (comme l'éclat blanc derrière la roche)
    {
        juce::ColourGradient haze (juce::Colour (0xffe8e2e6).withAlpha (0.10f), (float) w * 0.92f, (float) h * 0.05f,
                                   juce::Colours::transparentBlack, (float) w * 0.45f, (float) h * 0.6f, true);
        g.setGradientFill (haze);
        g.fillRect (0, 0, w, h);
        juce::ColourGradient haze2 (juce::Colour (0xffe8e2e6).withAlpha (0.06f), 0.0f, (float) h,
                                    juce::Colours::transparentBlack, (float) w * 0.35f, (float) h * 0.55f, true);
        g.setGradientFill (haze2);
        g.fillRect (0, 0, w, h);
    }
    // fissures incandescentes : halo large + cœur brillant
    for (int i = 0; i < 7; ++i)
    {
        juce::Path p;
        juce::Point<float> st { r.nextFloat() * (float) w, r.nextFloat() * (float) h };
        crack (p, r, st, r.nextFloat() * juce::MathConstants<float>::twoPi, 120.0f + r.nextFloat() * 260.0f, 2);
        g.setColour (accent.withAlpha (0.05f));
        g.strokePath (p, juce::PathStrokeType (9.0f, juce::PathStrokeType::mitered));
        g.setColour (accent.withAlpha (0.10f));
        g.strokePath (p, juce::PathStrokeType (3.5f, juce::PathStrokeType::mitered));
        g.setColour (accent.brighter (0.4f).withAlpha (0.45f));
        g.strokePath (p, juce::PathStrokeType (1.0f, juce::PathStrokeType::mitered));
    }
    // poussière et braises (bokeh)
    for (int i = 0; i < 420; ++i)
    {
        float x = r.nextFloat() * (float) w, y = r.nextFloat() * (float) h;
        float sz = r.nextFloat() < 0.92f ? 0.8f + r.nextFloat() * 1.8f : 4.0f + r.nextFloat() * 10.0f;
        auto c = r.nextFloat() < 0.55f ? accent.brighter (0.2f) : juce::Colour (0xffd8d2cc);
        if (r.nextFloat() < 0.02f) c = juce::Colour (0xff4cff8a); // rares points verts, comme sur l'artwork
        g.setColour (c.withAlpha (sz > 3.0f ? 0.04f + r.nextFloat() * 0.05f : 0.15f + r.nextFloat() * 0.35f));
        g.fillEllipse (x, y, sz, sz);
    }
    // vignettage
    juce::ColourGradient vg (juce::Colours::transparentBlack, (float) w * 0.5f, (float) h * 0.45f,
                             juce::Colours::black.withAlpha (0.6f), 0.0f, 0.0f, true);
    g.setGradientFill (vg);
    g.fillRect (0, 0, w, h);
    return img;
}

// ---------------------------------------------------------------------------
void CorruptScope::push (const float* data, int n)
{
    for (int i = 0; i < n; ++i)
    {
        wave[(size_t) writePos] = data[i];
        writePos = (writePos + 1) % (int) wave.size();
    }
    // spectre grossier
    for (int i = 0; i < 512; ++i)
    {
        float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / 512.0f);
        fftData[(size_t) i] = wave[(size_t) ((writePos + 512 + i) % (int) wave.size())] * w;
    }
    std::fill (fftData.begin() + 512, fftData.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftData.data());
    for (int b = 0; b < 64; ++b)
    {
        int lo = (int) std::pow (2.0, b / 64.0 * 8.0), hi = juce::jmax (lo + 1, (int) std::pow (2.0, (b + 1) / 64.0 * 8.0));
        float m = 0;
        for (int k = lo; k < hi && k < 256; ++k) m = juce::jmax (m, fftData[(size_t) k]);
        float db = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (m / 64.0f) + 60.0f) / 60.0f);
        bands[(size_t) b] = juce::jmax (db, bands[(size_t) b] * 0.85f);
    }
}

void CorruptScope::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float h = r.getHeight(), w = r.getWidth();
    // forme d'onde, tranches décalées, aberration chromatique
    const int n = (int) wave.size();
    auto drawWave = [&] (juce::Colour c, float dx)
    {
        juce::Path p;
        int slice = 0;
        float off = 0;
        for (int i = 0; i < n; i += 2)
        {
            if (i / 128 != slice)
            {
                slice = i / 128;
                off = rng.nextFloat() < intensity * 0.35f ? (rng.nextFloat() - 0.5f) * h * 0.3f : 0.0f;
            }
            float x = r.getX() + (float) i / (float) n * w + dx;
            float y = r.getCentreY() - wave[(size_t) ((writePos + i) % n)] * h * 0.42f + off;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (1.2f));
    };
    drawWave (juce::Colour (0xffff1e3c).withAlpha (alpha * 0.35f), -2.0f - intensity * 3.0f);
    drawWave (Theme::blue.withAlpha (alpha * 0.35f), 2.0f + intensity * 3.0f);
    drawWave (Theme::accent.withAlpha (alpha * 0.8f), 0.0f);
}

// ---------------------------------------------------------------------------
void GlitchOverlay::setLevel (float l)
{
    level = l;
    ++frame;
    const float amt = Theme::glitchAmount;
    const float w = (float) getWidth(), h = (float) getHeight();
    // naissance de braises proportionnelle au niveau (et quelques-unes en permanence)
    const float rate = amt * (0.15f + level * 9.0f);
    int spawn = (int) rate + (rng.nextFloat() < rate - std::floor (rate) ? 1 : 0);
    for (auto& p : sparks)
    {
        if (spawn <= 0) break;
        if (p.life > 0.0f) continue;
        p.x = rng.nextFloat() * w;
        p.y = h * (0.5f + rng.nextFloat() * 0.6f);
        p.vx = (rng.nextFloat() - 0.5f) * 1.2f;
        p.vy = -(0.4f + rng.nextFloat() * (1.0f + level * 4.0f));
        p.size = 0.8f + rng.nextFloat() * (rng.nextFloat() < 0.1f ? 4.0f : 1.8f);
        p.life = 1.0f;
        p.decay = 0.008f + rng.nextFloat() * 0.025f;
        p.hot = rng.nextFloat() < 0.75f;
        --spawn;
    }
    bool any = false;
    for (auto& p : sparks)
    {
        if (p.life <= 0.0f) continue;
        p.x += p.vx + std::sin ((float) frame * 0.07f + p.y * 0.01f) * 0.3f;
        p.y += p.vy;
        p.vy *= 0.995f;
        p.life -= p.decay;
        any = true;
    }
    // éclat de fissure sur un pic
    if (level * amt > 0.35f && rng.nextFloat() < level * amt * 0.25f) flash = 1.0f;
    else flash *= 0.8f;
    if (any || flash > 0.02f || frame % 15 == 0) repaint();
}

void GlitchOverlay::paint (juce::Graphics& g)
{
    if (Theme::glitchAmount <= 0.0f) return;
    for (auto& p : sparks)
    {
        if (p.life <= 0.0f) continue;
        auto c = p.hot ? Theme::accent.brighter (0.3f) : juce::Colour (0xffe6e0da);
        g.setColour (c.withAlpha (0.12f * p.life));
        g.fillEllipse (p.x - p.size * 2.0f, p.y - p.size * 2.0f, p.size * 4.0f, p.size * 4.0f);
        g.setColour (c.withAlpha (0.8f * p.life));
        g.fillEllipse (p.x - p.size * 0.5f, p.y - p.size * 0.5f, p.size, p.size);
    }
    if (flash > 0.02f)
    {
        // fissure lumineuse fugitive
        juce::Random r ((juce::int64) frame / 3);
        juce::Path p;
        float x = r.nextFloat() * (float) getWidth(), y = r.nextFloat() * (float) getHeight();
        p.startNewSubPath (x, y);
        for (int i = 0; i < 9; ++i) { x += (r.nextFloat() - 0.3f) * 40.0f; y += (r.nextFloat() - 0.5f) * 30.0f; p.lineTo (x, y); }
        g.setColour (Theme::accent.withAlpha (0.18f * flash));
        g.strokePath (p, juce::PathStrokeType (6.0f));
        g.setColour (Theme::accent.brighter (0.5f).withAlpha (0.7f * flash));
        g.strokePath (p, juce::PathStrokeType (1.2f));
    }
}

// ---------------------------------------------------------------------------
void Meters::tick()
{
    auto fall = [] (float& v, float target) { v = target > v ? target : v * 0.86f; };
    for (int k = 0; k < 2; ++k)
    {
        fall (in[k], proc.meterIn[k].load());
        fall (out[k], proc.meterOut[k].load());
    }
    float g = 1.0f - proc.meterGR.load();
    gr = g > gr ? g : gr * 0.9f;
    float pk = juce::jmax (out[0], out[1]);
    if (pk >= peakOut) { peakOut = pk; peakHold = 45; }
    else if (--peakHold < 0) peakOut *= 0.95f;
    repaint();
}

void Meters::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    Theme::sectionTitle (g, "// NIVEAUX", r.removeFromTop (16), Theme::accent);
    r.removeFromTop (4);
    auto toY = [] (float lin) { float db = juce::Decibels::gainToDecibels (lin, -60.0f); return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f); };
    auto bar = [&] (juce::Rectangle<float> b, float v, juce::Colour c)
    {
        g.setColour (Theme::line);
        g.fillRect (b);
        float f = toY (v);
        auto fill = b.withTrimmedTop (b.getHeight() * (1.0f - f));
        g.setColour (c);
        g.fillRect (fill);
        if (v > 0.98f) { g.setColour (Theme::red); g.fillRect (b.withHeight (3.0f)); }
    };
    auto labels = r.removeFromBottom (14);
    const float bw = 9.0f;
    auto area = r.reduced (4.0f, 0.0f);
    float x = area.getX();
    bar ({ x, area.getY(), bw, area.getHeight() }, in[0], Theme::blue); x += bw + 2;
    bar ({ x, area.getY(), bw, area.getHeight() }, in[1], Theme::blue); x += bw + 16;
    bar ({ x, area.getY(), bw, area.getHeight() }, out[0], Theme::accent); x += bw + 2;
    bar ({ x, area.getY(), bw, area.getHeight() }, out[1], Theme::accent); x += bw + 16;
    // réduction de gain (vers le bas)
    {
        juce::Rectangle<float> b { x, area.getY(), bw, area.getHeight() };
        g.setColour (Theme::line);
        g.fillRect (b);
        float db = juce::jlimit (0.0f, 24.0f, -juce::Decibels::gainToDecibels (1.0f - gr, -60.0f));
        g.setColour (Theme::red);
        g.fillRect (b.withHeight (b.getHeight() * db / 24.0f));
    }
    // échelle
    g.setFont (Theme::mono (9.0f));
    g.setColour (Theme::textDim);
    float sx = x + bw + 6;
    for (int db : { 0, -6, -12, -24, -48 })
    {
        float y = area.getY() + area.getHeight() * (1.0f - (db + 60.0f) / 60.0f);
        g.drawText (juce::String (db), juce::Rectangle<float> (sx, y - 5, 30, 10), juce::Justification::centredLeft);
    }
    g.drawText ("IN", juce::Rectangle<float> (area.getX(), labels.getY(), 2 * bw + 2, 12), juce::Justification::centred);
    g.drawText ("OUT", juce::Rectangle<float> (area.getX() + 2 * bw + 18, labels.getY(), 2 * bw + 2, 12), juce::Justification::centred);
    g.drawText ("GR", juce::Rectangle<float> (x - 2, labels.getY(), bw + 4, 12), juce::Justification::centred);
    g.setColour (peakOut > 0.98f ? Theme::red : Theme::text);
    g.drawText (juce::String (juce::Decibels::gainToDecibels (peakOut, -60.0f), 1) + " dB", juce::Rectangle<float> (sx + 30, area.getY(), 70, 12), juce::Justification::centredLeft);
}
} // namespace idg::ui
