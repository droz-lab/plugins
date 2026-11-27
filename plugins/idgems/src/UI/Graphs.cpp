#include "Graphs.h"

namespace idg::ui
{
namespace
{
float freqToX (float f, juce::Rectangle<float> r, float lo = 20.0f, float hi = 20000.0f)
{
    return r.getX() + r.getWidth() * std::log (f / lo) / std::log (hi / lo);
}

void freqGrid (juce::Graphics& g, juce::Rectangle<float> r, float lo, float hi, bool labels)
{
    g.setFont (Theme::mono (8.5f));
    for (float f : { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
    {
        if (f < lo || f > hi) continue;
        float x = freqToX (f, r, lo, hi);
        g.setColour (Theme::line.withAlpha (0.55f));
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        if (labels)
        {
            g.setColour (Theme::textDim.withAlpha (0.8f));
            g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), juce::Rectangle<float> (x + 2, r.getBottom() - 10, 26, 10), juce::Justification::left);
        }
    }
}
}

// ---------------------------------------------------------------------------
SpectrumView::SpectrumView()
{
    db.fill (-100.0f);
    peak.fill (-100.0f);
    setInterceptsMouseClicks (false, false);
}

void SpectrumView::push (const float* data, int n)
{
    for (int i = 0; i < n; ++i)
    {
        ring[(size_t) pos] = data[i];
        pos = (pos + 1) % size;
    }
    sinceFft += n;
    if (sinceFft < 512) return;
    sinceFft = 0;
    for (int i = 0; i < size; ++i) work[(size_t) i] = ring[(size_t) ((pos + i) % size)];
    window.multiplyWithWindowingTable (work.data(), (size_t) size);
    std::fill (work.begin() + size, work.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (work.data());
    for (int b = 0; b < bins; ++b)
    {
        float d = juce::Decibels::gainToDecibels (work[(size_t) b] * 4.0f / (float) size, -110.0f);
        db[(size_t) b] = d > db[(size_t) b] ? d : db[(size_t) b] * 0.8f + d * 0.2f;
        peak[(size_t) b] = d > peak[(size_t) b] ? d : peak[(size_t) b] - 0.25f;
    }
}

void SpectrumView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    if (showGrid) { g.setColour (Theme::bg.withAlpha (0.7f)); g.fillRect (r); }
    const float lo = 20.0f, hi = 20000.0f, minDb = -84.0f, maxDb = 6.0f;
    if (showGrid)
    {
        freqGrid (g, r, lo, hi, true);
        for (float d : { -72.0f, -48.0f, -24.0f, 0.0f })
        {
            float y = juce::jmap (d, minDb, maxDb, r.getBottom(), r.getY());
            g.setColour (Theme::line.withAlpha (0.4f));
            g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        }
    }
    auto valueAt = [&] (const std::array<float, bins>& arr, float f)
    {
        float binF = f * (float) size / (float) sampleRate;
        // moyenne sur la largeur du pixel (lissage dans les aigus)
        int b0 = juce::jlimit (1, bins - 2, (int) binF);
        float frac = binF - (float) b0;
        float v = arr[(size_t) b0] + (arr[(size_t) b0 + 1] - arr[(size_t) b0]) * juce::jlimit (0.0f, 1.0f, frac);
        // pente de +4.5 dB/octave autour de 1 kHz (lecture perceptive, comme sur un analyseur de mix)
        return v + 4.5f * std::log2 (f / 1000.0f);
    };
    auto build = [&] (const std::array<float, bins>& arr, bool closed)
    {
        juce::Path p;
        const int steps = juce::jmax (32, (int) r.getWidth() / 2);
        float prevMax = -200.0f;
        for (int i = 0; i <= steps; ++i)
        {
            float t = (float) i / steps;
            float f = lo * std::pow (hi / lo, t);
            float f2 = lo * std::pow (hi / lo, (float) (i + 1) / steps);
            // maximum des bins couverts par ce pas (évite les trous dans les aigus)
            float v = valueAt (arr, f);
            int b0 = (int) (f * size / sampleRate), b1 = (int) (f2 * size / sampleRate);
            for (int b = b0 + 1; b < b1 && b < bins; ++b) v = juce::jmax (v, arr[(size_t) b] + 4.5f * std::log2 (f / 1000.0f));
            v = 0.7f * v + 0.3f * (prevMax > -150.0f ? prevMax : v);
            prevMax = v;
            float x = r.getX() + t * r.getWidth();
            float y = juce::jlimit (r.getY(), r.getBottom(), juce::jmap (v, minDb, maxDb, r.getBottom(), r.getY()));
            if (i == 0) { if (closed) { p.startNewSubPath (x, r.getBottom()); p.lineTo (x, y); } else p.startNewSubPath (x, y); }
            else p.lineTo (x, y);
        }
        if (closed) { p.lineTo (r.getRight(), r.getBottom()); p.closeSubPath(); }
        return p;
    };
    auto fill = build (db, true);
    juce::ColourGradient grad (Theme::accent.withAlpha (fillAlpha), r.getX(), r.getY(), Theme::blue.withAlpha (fillAlpha * 0.25f), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillPath (fill);
    auto line = build (db, false);
    g.setColour (juce::Colour (0xffff1e3c).withAlpha (0.35f * lineAlpha));
    g.strokePath (line, juce::PathStrokeType (1.2f), juce::AffineTransform::translation (-2.0f, 0.0f));
    g.setColour (Theme::blue.withAlpha (0.35f * lineAlpha));
    g.strokePath (line, juce::PathStrokeType (1.2f), juce::AffineTransform::translation (2.0f, 0.0f));
    g.setColour (Theme::accent.withAlpha (lineAlpha));
    g.strokePath (line, juce::PathStrokeType (1.6f));
    auto pk = build (peak, false);
    juce::Path dashed;
    const float dash[] { 2.0f, 3.0f };
    juce::PathStrokeType (1.0f).createDashedStroke (dashed, pk, dash, 2);
    g.setColour (Theme::text.withAlpha (0.45f * lineAlpha));
    g.fillPath (dashed);
}

// ---------------------------------------------------------------------------
void ModuleBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    Theme::panelFrame (g, r, colour, 1.0f);
    Theme::sectionTitle (g, title, juce::Rectangle<float> (r.getX() + 6.0f, r.getY() + 3.0f, r.getWidth() - 12.0f, 14.0f), colour);
}

// ---------------------------------------------------------------------------
void OutputGraph::tick()
{
    auto& oc = proc.getOutputChain();
    const float c = oc.clipInPeak.exchange (0.0f);
    clipLevel = c > clipLevel ? c : clipLevel * 0.85f;
    clipPeak = c > clipPeak ? c : clipPeak * 0.97f;
    for (int j = 0; j < 3; ++j)
    {
        const float l = oc.ottLevelDb[j].exchange (-90.0f);
        ottLvl[j] = l > ottLvl[j] ? l : ottLvl[j] - 1.5f;
        const float gr = oc.ottGainDb[j].exchange (0.0f);
        ottGr[j] = ottGr[j] * 0.8f + gr * 0.2f;
    }
    grHist[(size_t) histPos] = juce::Decibels::gainToDecibels (juce::jmax (1.0e-4f, proc.meterGR.load()));
    histPos = (histPos + 1) % (int) grHist.size();
    repaint();
}

void OutputGraph::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Theme::bg.withAlpha (0.75f));
    g.fillRect (r);
    g.setColour (Theme::line);
    g.drawRect (r, 1.0f);
    r = r.reduced (3.0f);
    switch (kind)
    {
        case HighPass: paintHp (g, r); break;
        case Mono: paintMono (g, r); break;
        case Ott: paintOtt (g, r); break;
        case Clip: paintClip (g, r); break;
        case Limiter: paintLim (g, r); break;
    }
}

void OutputGraph::paintHp (juce::Graphics& g, juce::Rectangle<float> r)
{
    const float fc = proc.getValue (G::hpFreq);
    const float lo = 10.0f, hi = 2000.0f;
    freqGrid (g, r, lo, hi, false);
    juce::Path p, f;
    for (int i = 0; i <= 80; ++i)
    {
        float t = (float) i / 80.0f;
        float fr = lo * std::pow (hi / lo, t);
        float w = fr / fc;
        float mag = (w * w) / std::sqrt ((1 - w * w) * (1 - w * w) + 2.0f * w * w); // Butterworth 2e ordre
        float dbv = juce::Decibels::gainToDecibels (mag, -48.0f);
        float y = juce::jmap (dbv, -36.0f, 6.0f, r.getBottom(), r.getY());
        float x = r.getX() + t * r.getWidth();
        if (i == 0) { p.startNewSubPath (x, y); f.startNewSubPath (x, r.getBottom()); f.lineTo (x, y); } else { p.lineTo (x, y); f.lineTo (x, y); }
    }
    f.lineTo (r.getRight(), r.getBottom()); f.closeSubPath();
    g.setColour (Theme::blue.withAlpha (0.18f));
    g.fillPath (f);
    g.setColour (Theme::blue);
    g.strokePath (p, juce::PathStrokeType (1.6f));
    float x = freqToX (fc, r, lo, hi);
    g.setColour (Theme::text.withAlpha (0.5f));
    g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
    g.setFont (Theme::mono (9.0f, true));
    g.setColour (Theme::text);
    g.drawText (juce::String (juce::roundToInt (fc)) + " Hz", r.reduced (2), juce::Justification::topRight);
}

void OutputGraph::paintMono (juce::Graphics& g, juce::Rectangle<float> r)
{
    const bool on = proc.getValue (G::monoOn) > 0.5f;
    const float fc = proc.getValue (G::monoFreq);
    const float lo = 20.0f, hi = 2000.0f;
    float x = freqToX (fc, r, lo, hi);
    auto monoZone = r.withRight (x);
    // zone mono : une seule barre centrale ; zone stéréo : deux traces écartées
    g.setColour ((on ? Theme::blue : Theme::line).withAlpha (0.22f));
    g.fillRect (monoZone);
    g.setColour (on ? Theme::blue : Theme::textDim);
    g.fillRect (monoZone.withSizeKeepingCentre (monoZone.getWidth(), 3.0f));
    juce::Path st;
    st.startNewSubPath (x, r.getCentreY());
    st.cubicTo (x + 12, r.getCentreY(), x + 14, r.getY() + 6, r.getRight(), r.getY() + 6);
    st.startNewSubPath (x, r.getCentreY());
    st.cubicTo (x + 12, r.getCentreY(), x + 14, r.getBottom() - 6, r.getRight(), r.getBottom() - 6);
    g.setColour (Theme::accent.withAlpha (0.8f));
    g.strokePath (st, juce::PathStrokeType (1.5f));
    g.setColour (Theme::text);
    g.setFont (Theme::mono (9.0f, true));
    g.drawText (on ? "MONO" : "OFF", monoZone.reduced (2), juce::Justification::topLeft);
    g.drawText ("ST", r.withLeft (x).reduced (2), juce::Justification::topRight);
    g.drawText (juce::String (juce::roundToInt (fc)) + " Hz", r.reduced (2), juce::Justification::bottomRight);
}

void OutputGraph::paintOtt (juce::Graphics& g, juce::Rectangle<float> r)
{
    const float depth = proc.getValue (G::ottDepth);
    static const char* names[] { "LOW", "MID", "HIGH" };
    const float bw = r.getWidth() / 3.0f;
    auto yOf = [&] (float d) { return juce::jmap (juce::jlimit (-72.0f, 0.0f, d), -72.0f, 0.0f, r.getBottom() - 10.0f, r.getY()); };
    for (int j = 0; j < 3; ++j)
    {
        auto col = r.withX (r.getX() + j * bw).withWidth (bw).reduced (4.0f, 0.0f);
        auto bar = col.withWidth (col.getWidth() * 0.45f).withBottom (r.getBottom() - 10.0f);
        g.setColour (Theme::line.withAlpha (0.6f));
        g.fillRect (bar);
        float y = yOf (ottLvl[j]);
        g.setColour (Theme::accent.withAlpha (depth > 0.0f ? 0.85f : 0.3f));
        g.fillRect (bar.withTop (y));
        // seuils down (-30) et up (-40)
        g.setColour (Theme::red.withAlpha (0.8f));
        g.drawHorizontalLine ((int) yOf (-30.0f), bar.getX() - 2, bar.getRight() + 2);
        g.setColour (Theme::blue.withAlpha (0.8f));
        g.drawHorizontalLine ((int) yOf (-40.0f), bar.getX() - 2, bar.getRight() + 2);
        // gain appliqué (vers le haut = upward, vers le bas = downward)
        auto grCol = col.withLeft (bar.getRight() + 4).withBottom (r.getBottom() - 10.0f);
        float mid = grCol.getCentreY();
        float gr = juce::jlimit (-24.0f, 24.0f, ottGr[j]);
        float h = grCol.getHeight() * 0.5f * std::abs (gr) / 24.0f;
        g.setColour (gr >= 0 ? Theme::blue : Theme::red);
        g.fillRect (gr >= 0 ? grCol.withTop (mid - h).withBottom (mid) : grCol.withTop (mid).withHeight (h));
        g.setColour (Theme::line.brighter (0.4f));
        g.drawHorizontalLine ((int) mid, grCol.getX(), grCol.getRight());
        g.setColour (Theme::textDim);
        g.setFont (Theme::mono (8.5f, true));
        g.drawText (names[j], col.withTop (r.getBottom() - 10.0f), juce::Justification::centred);
    }
    if (depth <= 0.0f)
    {
        g.setColour (Theme::textDim);
        g.setFont (Theme::mono (9.0f, true));
        g.drawText ("PROFONDEUR = 0", r, juce::Justification::centred);
    }
}

void OutputGraph::paintClip (juce::Graphics& g, juce::Rectangle<float> r)
{
    // fonction de transfert : entrée 0..2 (linéaire, jusqu'à +6 dB) -> sortie 0..1
    auto clip = [] (float a) { return a > 0.7f ? 0.7f + 0.3f * std::tanh ((a - 0.7f) / 0.3f) : a; };
    const float maxIn = 2.0f;
    auto toPt = [&] (float in, float out) { return juce::Point<float> (r.getX() + in / maxIn * r.getWidth(), r.getBottom() - out * r.getHeight()); };
    // zone de saturation
    g.setColour (Theme::red.withAlpha (0.1f));
    g.fillRect (r.withLeft (toPt (0.7f, 0).x));
    g.setColour (Theme::line);
    g.drawLine (juce::Line<float> (toPt (0, 0), toPt (1.0f, 1.0f)), 1.0f); // diagonale = sans écrêtage
    g.drawHorizontalLine ((int) toPt (0, 1.0f).y, r.getX(), r.getRight());
    juce::Path p;
    for (int i = 0; i <= 60; ++i)
    {
        float a = maxIn * i / 60.0f;
        auto pt = toPt (a, clip (a));
        if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
    }
    g.setColour (Theme::red);
    g.strokePath (p, juce::PathStrokeType (1.8f));
    // position du signal
    const float lvl = juce::jmin (maxIn, clipLevel), pk = juce::jmin (maxIn, clipPeak);
    g.setColour (Theme::text.withAlpha (0.35f));
    g.drawVerticalLine ((int) toPt (pk, 0).x, r.getY(), r.getBottom());
    auto dot = toPt (lvl, clip (lvl));
    g.setColour (lvl > 0.7f ? Theme::red : Theme::text);
    g.fillEllipse (juce::Rectangle<float> (8, 8).withCentre (dot));
    g.setColour (Theme::bg);
    g.drawEllipse (juce::Rectangle<float> (8, 8).withCentre (dot), 1.0f);
    // taux d'écrêtage
    const float red = lvl > 0.7f ? juce::Decibels::gainToDecibels (clip (lvl) / lvl) : 0.0f;
    g.setFont (Theme::mono (9.0f, true));
    g.setColour (red < -0.5f ? Theme::red : Theme::textDim);
    g.drawText (red < -0.1f ? juce::String (red, 1) + " dB" : "PROPRE", r.reduced (2), juce::Justification::topLeft);
}

void OutputGraph::paintLim (juce::Graphics& g, juce::Rectangle<float> r)
{
    const int n = (int) grHist.size();
    juce::Path p;
    p.startNewSubPath (r.getX(), r.getY());
    for (int i = 0; i < n; ++i)
    {
        float d = juce::jlimit (-12.0f, 0.0f, grHist[(size_t) ((histPos + i) % n)]);
        p.lineTo (r.getX() + r.getWidth() * i / (n - 1), r.getY() + (-d / 12.0f) * r.getHeight());
    }
    p.lineTo (r.getRight(), r.getY());
    p.closeSubPath();
    g.setColour (Theme::red.withAlpha (0.35f));
    g.fillPath (p);
    g.setColour (Theme::red);
    g.strokePath (p, juce::PathStrokeType (1.2f));
    for (int d : { -3, -6, -9 })
    {
        float y = r.getY() + (-d / 12.0f) * r.getHeight();
        g.setColour (Theme::line.withAlpha (0.6f));
        g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
    }
    const float cur = grHist[(size_t) ((histPos + n - 1) % n)];
    g.setFont (Theme::mono (9.0f, true));
    g.setColour (Theme::text);
    g.drawText ("GR " + juce::String (cur, 1) + " dB", r.reduced (2), juce::Justification::bottomLeft);
    g.drawText ("PLAFOND " + juce::String (proc.getValue (G::ceiling), 1), r.reduced (2), juce::Justification::bottomRight);
}
} // namespace idg::ui
