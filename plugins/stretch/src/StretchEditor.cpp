#include "StretchEditor.h"
#include "UI/Theme.h"

namespace idg
{
using ui::Theme;

namespace
{
const juce::Colour kGlitch { 0xffff6a00 }; // famille Glitch : orange
const juce::Colour kBlue { 0xff2fa8ff };
constexpr float kMinF = 20.0f, kMaxF = 20000.0f, kTopDb = 18.0f, kBotDb = -60.0f;
} // namespace


// ---------------------------------------------------------------------------
StretchLookAndFeel::StretchLookAndFeel()
{
    setColour (juce::TooltipWindow::backgroundColourId, Theme::panelHi);
    setColour (juce::TooltipWindow::textColourId, Theme::text);
    setColour (juce::TooltipWindow::outlineColourId, kGlitch.withAlpha (0.6f));
}

void StretchLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    // place pour le halo et la valeur affichée dessous
    r = r.withTrimmedBottom (24.0f);
    const float size = juce::jmin (r.getWidth(), r.getHeight()) * 0.84f;
    r = r.withSizeKeepingCentre (size, size);
    const auto c = r.getCentre();
    const float rad = size * 0.5f - 4.0f;
    const float thick = juce::jmax (3.0f, size * 0.07f);
    const float ang = a0 + pos * (a1 - a0);

    // corps
    g.setColour (Theme::panelHi);
    g.fillEllipse (r.reduced (thick + 3.0f));
    g.setColour (Theme::line);
    g.drawEllipse (r.reduced (thick + 3.0f), 1.0f);

    juce::Path track;
    track.addCentredArc (c.x, c.y, rad, rad, 0.0f, a0, a1, true);
    g.setColour (Theme::line);
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (pos > 0.001f)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, rad, rad, 0.0f, a0, ang, true);
        // halo qui grossit avec la valeur
        g.setColour (kGlitch.withAlpha (0.10f + 0.20f * pos));
        g.strokePath (arc, juce::PathStrokeType (thick * (2.2f + 1.5f * pos), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (kGlitch);
        g.strokePath (arc, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white.withAlpha (0.35f + 0.4f * pos));
        g.strokePath (arc, juce::PathStrokeType (thick * 0.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // index
    const juce::Point<float> p1 = c.getPointOnCircumference (rad * 0.25f, ang), p2 = c.getPointOnCircumference (rad * 0.62f, ang);
    g.setColour (Theme::text);
    g.drawLine ({ p1, p2 }, 2.0f);

    // valeur au centre
    g.setColour (Theme::text);
    g.setFont (Theme::mono (14.0f, true));
    g.drawText (s.getTextFromValue (s.getValue()), juce::Rectangle<float> (c.x - 60.0f, r.getBottom() + 2.0f, 120.0f, 22.0f),
                juce::Justification::centred, false);
}

void StretchLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const bool on = b.getToggleState();
    if (on)
    {
        g.setColour (kGlitch.withAlpha (0.25f));
        g.fillRect (r.expanded (1.5f));
        g.setColour (kGlitch);
        g.fillRect (r);
    }
    else
    {
        g.setColour (over ? Theme::panelHi.brighter (0.1f) : Theme::panelHi);
        g.fillRect (r);
        g.setColour (Theme::line);
        g.drawRect (r, 1.0f);
    }
}

void StretchLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? juce::Colours::black : Theme::text);
    g.setFont (Theme::mono (juce::jmin (13.0f, b.getHeight() * 0.42f), true));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
void StepStrip::paint (juce::Graphics& g)
{
    // simple indicateur : 4 pas, le pas en cours s'allume, un trait par répétition du grain
    auto r = getLocalBounds().toFloat();
    g.setColour (Theme::bg.brighter (0.03f));
    g.fillRect (r);

    constexpr int steps = 4;
    const float phase = proc.uiPhase.load();
    const int cur = ((proc.uiStep.load() % steps) + steps) % steps;
    const float sw = r.getWidth() / steps;
    const float reps = proc.uiRepeats.load();

    for (int i = 0; i < steps; ++i)
    {
        auto s = juce::Rectangle<float> (r.getX() + i * sw, r.getY(), sw, r.getHeight()).reduced (3.0f, 5.0f);
        const bool active = i == cur;
        g.setColour (kGlitch.withAlpha (active ? 0.85f : 0.25f));
        g.fillRect (s);
        const float px = s.getWidth() / juce::jmax (1.0f, reps);
        if (px >= 4.0f)
        {
            g.setColour (juce::Colours::black.withAlpha (0.3f));
            for (float xx = s.getX() + px; xx < s.getRight() - 1.0f; xx += px)
                g.fillRect (xx, s.getY(), 1.5f, s.getHeight());
        }
        g.setColour (active ? kGlitch.brighter (0.3f) : Theme::line);
        g.drawRect (s, 1.0f);
        if (active)
        {
            g.setColour (juce::Colours::white);
            g.fillRect (s.getX() + s.getWidth() * phase - 1.0f, s.getY() - 4.0f, 2.0f, s.getHeight() + 8.0f);
        }
    }
}

// ---------------------------------------------------------------------------
FilterGraph::FilterGraph (StretchProcessor& p) : proc (p)
{
    params[0] = proc.apvts.getParameter ("hp");
    params[1] = proc.apvts.getParameter ("lp");
    params[2] = proc.apvts.getParameter ("reso");
    spec.fill (-120.0f);
    setRepaintsOnMouseActivity (true);
}

float FilterGraph::xForFreq (float f) const
{
    return (float) getWidth() * std::log (juce::jlimit (kMinF, kMaxF, f) / kMinF) / std::log (kMaxF / kMinF);
}

float FilterGraph::freqForX (float x) const
{
    return kMinF * std::pow (kMaxF / kMinF, juce::jlimit (0.0f, 1.0f, x / (float) juce::jmax (1, getWidth())));
}

float FilterGraph::yForDb (float db) const
{
    const float h = (float) getHeight();
    return juce::jmap (juce::jlimit (kBotDb, kTopDb, db), kTopDb, kBotDb, 4.0f, h - 2.0f);
}

juce::Point<float> FilterGraph::handlePos (int which) const
{
    const float f = params[which]->convertFrom0to1 (params[which]->getValue());
    const float reso = params[2]->convertFrom0to1 (params[2]->getValue());
    const float hp = params[0]->convertFrom0to1 (params[0]->getValue());
    const float lp = params[1]->convertFrom0to1 (params[1]->getValue());
    const float db = StretchEngine::responseDb (f, which == 0 ? hp : 20.0f, which == 1 ? lp : 20000.0f, reso);
    return { xForFreq (f), juce::jmin (yForDb (db), (float) getHeight() - 10.0f) };
}

int FilterGraph::handleAt (juce::Point<float> p) const
{
    int best = -1;
    float bestD = 18.0f;
    for (int i = 0; i < 2; ++i)
    {
        const float d = std::abs (handlePos (i).x - p.x);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

void FilterGraph::updateSpectrum()
{
    const int n = 4096;
    const int w = proc.scopeWrite.load (std::memory_order_acquire);
    for (int i = 0; i < n; ++i)
    {
        const float win = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (n - 1));
        fftData[(size_t) i] = proc.scope[(size_t) ((w + i) & (StretchProcessor::scopeSize - 1))] * win;
    }
    std::fill (fftData.begin() + n, fftData.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);
    for (int k = 0; k < 2048; ++k)
    {
        const float db = juce::Decibels::gainToDecibels (fftData[(size_t) k] / (float) n * 4.0f, -120.0f);
        spec[(size_t) k] = db > spec[(size_t) k] ? db : spec[(size_t) k] - 1.5f; // retombée douce
    }
    repaint();
}

void FilterGraph::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Theme::bg.brighter (0.03f));
    g.fillRect (r);

    // grille : octaves en fréquence, 12 dB en niveau
    g.setFont (Theme::mono (9.5f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xForFreq (f);
        g.setColour (Theme::line.withAlpha (0.6f));
        g.drawVerticalLine ((int) x, 0.0f, r.getBottom());
        g.setColour (Theme::textDim.withAlpha (0.7f));
        g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f),
                    juce::Rectangle<float> (x + 2.0f, r.getBottom() - 13.0f, 30.0f, 12.0f), juce::Justification::centredLeft, false);
    }
    for (float db = 12.0f; db > kBotDb; db -= 12.0f)
    {
        g.setColour (Theme::line.withAlpha (std::abs (db) < 0.1f ? 0.9f : 0.35f));
        g.drawHorizontalLine ((int) yForDb (db), 0.0f, r.getRight());
    }

    // spectre de sortie (bleu)
    {
        const double binHz = proc.getSampleRate() > 0 ? proc.getSampleRate() / 4096.0 : 44100.0 / 4096.0;
        juce::Path sp;
        bool started = false;
        for (int px = 0; px < getWidth(); px += 2)
        {
            const float f = freqForX ((float) px);
            const auto k = juce::jlimit (1, 2047, (int) std::lround (f / binHz));
            const float y = yForDb (spec[(size_t) k] + 6.0f);
            if (! started) { sp.startNewSubPath ((float) px, y); started = true; }
            else sp.lineTo ((float) px, y);
        }
        juce::Path fill (sp);
        fill.lineTo (r.getRight(), r.getBottom());
        fill.lineTo (0.0f, r.getBottom());
        fill.closeSubPath();
        g.setColour (kBlue.withAlpha (0.12f));
        g.fillPath (fill);
        g.setColour (kBlue.withAlpha (0.45f));
        g.strokePath (sp, juce::PathStrokeType (1.0f));
    }

    // courbe du filtre (orange)
    const float hp = params[0]->convertFrom0to1 (params[0]->getValue());
    const float lp = params[1]->convertFrom0to1 (params[1]->getValue());
    const float reso = params[2]->convertFrom0to1 (params[2]->getValue());
    juce::Path curve;
    for (int px = 0; px <= getWidth(); ++px)
    {
        const float y = yForDb (StretchEngine::responseDb (freqForX ((float) px), hp, lp, reso));
        if (px == 0) curve.startNewSubPath (0.0f, y); else curve.lineTo ((float) px, y);
    }
    juce::Path area (curve);
    area.lineTo (r.getRight(), r.getBottom());
    area.lineTo (0.0f, r.getBottom());
    area.closeSubPath();
    g.setColour (kGlitch.withAlpha (0.10f));
    g.fillPath (area);
    g.setColour (kGlitch.withAlpha (0.3f));
    g.strokePath (curve, juce::PathStrokeType (5.0f));
    g.setColour (kGlitch);
    g.strokePath (curve, juce::PathStrokeType (1.8f));

    // poignées
    const char* names[] { "BAS", "HAUT" };
    for (int i = 0; i < 2; ++i)
    {
        const auto p = handlePos (i);
        const bool on = dragging == i || hover == i;
        g.setColour (kGlitch.withAlpha (on ? 0.45f : 0.25f));
        g.fillEllipse (p.x - 11.0f, p.y - 11.0f, 22.0f, 22.0f);
        g.setColour (on ? juce::Colours::white : kGlitch);
        g.fillEllipse (p.x - 6.0f, p.y - 6.0f, 12.0f, 12.0f);
        const float f = params[i]->convertFrom0to1 (params[i]->getValue());
        g.setColour (Theme::text);
        g.setFont (Theme::mono (10.5f, true));
        auto txt = juce::String (names[i]) + " " + params[i]->getCurrentValueAsText();
        const float tx = juce::jlimit (2.0f, r.getRight() - 112.0f, i == 0 ? p.x + 10.0f : p.x - 120.0f);
        g.drawText (txt, juce::Rectangle<float> (tx, juce::jmax (2.0f, p.y - 26.0f), 110.0f, 14.0f),
                    i == 0 ? juce::Justification::centredLeft : juce::Justification::centredRight, false);
        juce::ignoreUnused (f);
    }
    g.setColour (Theme::line);
    g.drawRect (r, 1.0f);
}

void FilterGraph::mouseMove (const juce::MouseEvent& e)
{
    const int h = handleAt (e.position);
    if (h != hover) { hover = h; repaint(); }
}

void FilterGraph::mouseDown (const juce::MouseEvent& e)
{
    dragging = handleAt (e.position);
    if (dragging < 0)
    {
        // clic hors poignée : on prend la plus proche du côté du clic
        dragging = e.position.x < (handlePos (0).x + handlePos (1).x) * 0.5f ? 0 : 1;
    }
    dragStartY = e.position.y;
    dragStartReso = params[2]->getValue();
    params[dragging]->beginChangeGesture();
    params[2]->beginChangeGesture();
    mouseDrag (e);
}

void FilterGraph::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    auto* p = params[dragging];
    p->setValueNotifyingHost (p->convertTo0to1 (freqForX (e.position.x)));
    // vertical : résonance (vers le haut = plus de résonance)
    const float dy = (dragStartY - e.position.y) / (float) juce::jmax (1, getHeight());
    params[2]->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, dragStartReso + dy * 1.5f));
    repaint();
}

void FilterGraph::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
    {
        params[dragging]->endChangeGesture();
        params[2]->endChangeGesture();
    }
    dragging = -1;
}

void FilterGraph::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int h = handleAt (e.position);
    if (h < 0) return;
    params[h]->beginChangeGesture();
    params[h]->setValueNotifyingHost (params[h]->getDefaultValue());
    params[h]->endChangeGesture();
    repaint();
}

// ---------------------------------------------------------------------------
void StretchEditor::Content::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.fillAll (Theme::bg);
    juce::ColourGradient glow (kGlitch.withAlpha (0.16f), r.getRight(), r.getBottom(), Theme::bg.withAlpha (0.0f), r.getCentreX(), r.getY(), true);
    g.setGradientFill (glow);
    g.fillRect (r);
    juce::Path crack;
    crack.startNewSubPath (r.getRight() - 250.0f, r.getBottom());
    crack.lineTo (r.getRight() - 205.0f, r.getBottom() - 7.0f);
    crack.lineTo (r.getRight() - 150.0f, r.getBottom() - 3.0f);
    crack.lineTo (r.getRight() - 90.0f, r.getBottom() - 10.0f);
    crack.lineTo (r.getRight(), r.getBottom() - 6.0f);
    g.setColour (kGlitch.withAlpha (0.12f));
    g.strokePath (crack, juce::PathStrokeType (5.0f));
    g.setColour (kGlitch.withAlpha (0.45f));
    g.strokePath (crack, juce::PathStrokeType (1.2f));

    Theme::cutText (g, "STRETCH", { 22.0f, 14.0f, 360.0f, 40.0f }, 30.0f, Theme::text, juce::Justification::centredLeft, 0.5f, true);
    g.setFont (Theme::mono (11.0f));
    g.setColour (Theme::textDim);
    g.drawText (IDG_T ("IDGEMS · GEL RYTHMIQUE DE LA FRAME"), juce::Rectangle<float> (24.0f, 54.0f, 400.0f, 14.0f), juce::Justification::centredLeft, false);

    g.drawText ("RATE", juce::Rectangle<float> (24.0f, 78.0f, 80.0f, 14.0f), juce::Justification::centredLeft, false);
    auto label = [&] (juce::Rectangle<int> a, const char* t)
    {
        g.setFont (Theme::mono (12.0f, true));
        g.setColour (Theme::textDim);
        g.drawText (IDG_T (t), a.withHeight (16).toFloat(), juce::Justification::centred, false);
    };
    label (grainArea, "GRAIN");
    label (resoArea, "RÉSO");
    g.setFont (Theme::mono (12.0f, true));
    g.setColour (Theme::textDim);
    g.drawText (IDG_T ("FILTRE  ·  glisser = fréquence, haut/bas = résonance"), graphArea.withHeight (16).toFloat(),
                juce::Justification::centredLeft, false);

    // durée du grain sous le potard
    if (grain != nullptr)
    {
        const double hz = juce::jmax (1.0, grain->getValue());
        const double ms = 1000.0 / hz;
        // note la plus proche + écart en cents (la boucle sonne à cette hauteur)
        const double midi = 69.0 + 12.0 * std::log2 (hz / 440.0);
        const int nn = (int) std::lround (midi);
        const int cents = (int) std::lround ((midi - nn) * 100.0);
        const auto note = juce::MidiMessage::getMidiNoteName (nn, true, true, 3);
        g.setFont (Theme::title (15.0f));
        g.setColour (kGlitch.brighter (0.2f));
        g.drawText (note, grainArea.withTop (grainArea.getBottom() - 36).withHeight (20).toFloat(), juce::Justification::centred, false);
        g.setFont (Theme::mono (11.0f));
        g.setColour (Theme::textDim);
        g.drawText ((cents >= 0 ? "+" : "") + juce::String (cents) + " ct  |  " + juce::String (ms, ms < 10.0 ? 1 : 0) + " ms",
                    grainArea.withTop (grainArea.getBottom() - 16).toFloat(), juce::Justification::centred, false);
    }
}

void StretchEditor::Content::resized()
{
    auto r = getLocalBounds().reduced (24, 0);
    r.removeFromTop (94);
    auto rateRow = r.removeFromTop (30);
    const int bw = rateRow.getWidth() / juce::jmax (1, rateButtons.size());
    for (auto* b : rateButtons) b->setBounds (rateRow.removeFromLeft (bw));
    r.removeFromTop (8);
    if (strip) strip->setBounds (r.removeFromTop (36));
    r.removeFromTop (12);
    r.removeFromBottom (14);

    grainArea = r.removeFromLeft (190);
    if (grain) grain->setBounds (grainArea.withTrimmedTop (16).withTrimmedBottom (36));
    r.removeFromLeft (14);
    resoArea = r.removeFromRight (96);
    if (reso) reso->setBounds (resoArea.withTrimmedTop (16).withSizeKeepingCentre (96, 110));
    r.removeFromRight (10);
    graphArea = r;
    if (graph) graph->setBounds (r.withTrimmedTop (20));
}

StretchEditor::StretchEditor (StretchProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    content.strip = std::make_unique<StepStrip> (p);
    content.addAndMakeVisible (*content.strip);
    content.graph = std::make_unique<FilterGraph> (p);
    content.addAndMakeVisible (*content.graph);

    const auto& names = StretchProcessor::rateNames();
    for (int i = 0; i < names.size(); ++i)
    {
        auto* b = content.rateButtons.add (new juce::TextButton (names[i]));
        b->onClick = [this, i] { if (rateAtt) rateAtt->setValueAsCompleteGesture ((float) i); };
        b->setTooltip (IDG_T ("Période de répétition : à chaque pas, le son joue le temps d'un grain puis ce grain tourne en boucle."));
        content.addAndMakeVisible (b);
    }
    rateAtt = std::make_unique<juce::ParameterAttachment> (
        *proc.apvts.getParameter ("rate"), [this] (float v) { currentRate = (int) std::lround (v); updateRateButtons(); });
    rateAtt->sendInitialUpdate();

    auto makeKnob = [this] (const char* /*id*/, const char* help)
    {
        auto s = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
        s->setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        s->setTooltip (IDG_T (help));
        content.addAndMakeVisible (*s);
        return s;
    };
    content.grain = makeKnob ("grain_hz", "Fréquence de répétition du grain (40 Hz = 25 ms rejouées 40 fois par seconde). La note affichée est la "
                                          "hauteur que prend la boucle. Automatisable : le grain change en direct, même pendant le gel.");
    content.reso = makeKnob ("reso", "Résonance commune du coupe-bas et du coupe-haut.");
    grainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "grain_hz", *content.grain);
    resoAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "reso", *content.reso);
    content.grain->setDoubleClickReturnValue (true, 40.0);
    content.reso->setDoubleClickReturnValue (true, 0.0);
    content.grain->onValueChange = [this] { content.repaint(); };

    addAndMakeVisible (content);
    content.setSize (baseW, baseH);
    tooltip = std::make_unique<juce::TooltipWindow> (this, 600);

    // taille mémorisée lue AVANT setResizable (qui déclenche resized())
    const float remembered = proc.editorScale.load();
    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) baseW / baseH);
        c->setSizeLimits (baseW / 2, baseH / 2, baseW * 2, baseH * 2);
    }
    const float scale = juce::jlimit (0.5f, 2.0f, remembered > 0.0f ? remembered : 1.0f);
    setSize ((int) (baseW * scale), (int) (baseH * scale));
    sizeReady = true;
    startTimerHz (30);
}

StretchEditor::~StretchEditor()
{
    stopTimer();
    tooltip.reset();
    setLookAndFeel (nullptr);
}

void StretchEditor::updateRateButtons()
{
    for (int i = 0; i < content.rateButtons.size(); ++i)
        content.rateButtons[i]->setToggleState (i == currentRate, juce::dontSendNotification);
}

void StretchEditor::paint (juce::Graphics& g) { g.fillAll (Theme::bg); }

void StretchEditor::resized()
{
    const float s = (float) getWidth() / (float) baseW;
    content.setTransform (juce::AffineTransform::scale (s));
    if (sizeReady) proc.editorScale = s;
}

void StretchEditor::timerCallback()
{
    content.strip->repaint();
    content.graph->updateSpectrum();
}
} // namespace idg
