#include "Views.h"

namespace markovui
{
namespace colours = droz::ui::colours;

//==============================================================================
TrackRow::TrackRow (PluginProcessor& p, int index, std::function<void (int)> onSelect)
    : processor (p),
      track (index),
      select (std::move (onSelect)),
      enabledAttachment (p.apvts, PluginProcessor::trackParam (index, "actif"), enabled)
{
    enabled.setButtonText ("active");
    enabled.setTooltip (fr ("Allume ou éteint la piste ") + juce::String (track + 1)
                        + fr (". Éteinte, elle ne joue rien et sa note en cours s'arrête."));
    addAndMakeVisible (enabled);
    setTooltip (fr ("Piste ") + juce::String (track + 1) + fr (" : notes envoyées sur le canal MIDI ") + juce::String (track + 1)
                + fr (". Cliquer pour l'afficher dans Matrice et Pas."));
}

void TrackRow::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    if (selected)
    {
        g.setColour (colours::panel);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (colours::dim);
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
    }
    const auto ledArea = juce::Rectangle<float> (10.0f, r.getCentreY() - 5.0f, 10.0f, 10.0f);
    g.setColour (colours::line);
    g.fillEllipse (ledArea);
    g.setColour (accent.withAlpha (juce::jlimit (0.0f, 1.0f, led)));
    g.fillEllipse (ledArea);

    g.setColour (colours::text);
    g.setFont (droz::ui::caps (12.0f));
    g.drawText ("PISTE " + juce::String (track + 1), 30, 0, 90, getHeight(), juce::Justification::centredLeft);
    g.setColour (colours::dim);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText ("canal " + juce::String (track + 1), 120, 0, 60, getHeight(), juce::Justification::centredLeft);
}

void TrackRow::resized()
{
    enabled.setBounds (getWidth() - 76, 0, 72, getHeight());
}

void TrackRow::mouseDown (const juce::MouseEvent&)
{
    if (select)
        select (track);
}

void TrackRow::setSelected (bool s)
{
    if (s != selected)
    {
        selected = s;
        repaint();
    }
}

void TrackRow::setLed (float level)
{
    if (std::abs (level - led) > 0.01f)
    {
        led = level;
        repaint();
    }
}

//==============================================================================
MatrixView::MatrixView (PluginProcessor& p) : processor (p)
{
    setTooltip (fr ("Matrice de transition : ligne = note jouée, colonne = note suivante, en % de chances. "
                    "Glisser vers le haut ou le bas sur une case pour la changer. Double-clic = valeur par défaut."));
}

juce::Rectangle<float> MatrixView::gridArea (int size) const
{
    auto area = getLocalBounds().toFloat().reduced (2.0f);
    area.removeFromLeft (44.0f);
    area.removeFromTop (22.0f);
    area.removeFromBottom (18.0f);
    const float cell = std::floor (std::min (area.getWidth(), area.getHeight()) / static_cast<float> (size));
    return area.withSize (cell * static_cast<float> (size), cell * static_cast<float> (size));
}

bool MatrixView::cellAt (juce::Point<float> pos, int size, int& row, int& col) const
{
    const auto grid = gridArea (size);
    if (! grid.contains (pos))
        return false;
    const float cell = grid.getWidth() / static_cast<float> (size);
    col = juce::jlimit (0, size - 1, static_cast<int> ((pos.x - grid.getX()) / cell));
    row = juce::jlimit (0, size - 1, static_cast<int> ((pos.y - grid.getY()) / cell));
    return true;
}

void MatrixView::paint (juce::Graphics& g)
{
    const auto scale = processor.currentScale();
    const int size = scale.size;
    const auto data = processor.getTrack (track);
    const auto grid = gridArea (size);
    const float cell = grid.getWidth() / static_cast<float> (size);
    const bool playing = processor.hostPlaying.load();
    const int current = processor.currentDegree[(size_t) track].load() % size;

    g.setFont (juce::Font (juce::FontOptions (juce::jlimit (9.0f, 13.0f, cell * 0.3f))));
    for (int i = 0; i < size; ++i)
    {
        const auto name = noteName (scale.pitchClass (i), false);
        g.setColour (playing && i == current ? accent : colours::dim);
        g.drawText (name, juce::Rectangle<float> (grid.getX() - 44.0f, grid.getY() + i * cell, 40.0f, cell), juce::Justification::centredRight);
        g.setColour (colours::dim);
        g.drawText (name, juce::Rectangle<float> (grid.getX() + i * cell, grid.getY() - 20.0f, cell, 18.0f), juce::Justification::centred);
    }

    for (int i = 0; i < size; ++i)
    {
        float row[markov::maxDegrees], p[markov::maxDegrees];
        for (int j = 0; j < size; ++j)
            row[j] = markov::weight (data, size, i, j);
        markov::normaliseRow (row, size, 1.0f, p);
        for (int j = 0; j < size; ++j)
        {
            const auto r = juce::Rectangle<float> (grid.getX() + j * cell, grid.getY() + i * cell, cell, cell).reduced (1.0f);
            const float level = juce::jlimit (0.0f, 1.0f, p[j] * 2.5f);
            g.setColour (colours::panel);
            g.fillRect (r);
            g.setColour (colours::text.withAlpha (0.04f + 0.96f * level));
            g.fillRect (r);
            if (cell >= 30.0f)
            {
                g.setColour (level > 0.55f ? colours::background : colours::text);
                g.drawText (juce::String (juce::roundToInt (100.0f * p[j])), r, juce::Justification::centred);
            }
        }
    }
    if (playing)
    {
        g.setColour (accent);
        g.drawRect (juce::Rectangle<float> (grid.getX(), grid.getY() + current * cell, grid.getWidth(), cell), 1.5f);
    }

    bool any2 = false;
    for (int a = 0; a < size; ++a)
        for (int b = 0; b < size; ++b)
            any2 = any2 || data.has2[a][b];
    g.setColour (colours::dim);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText (any2 ? fr ("ordre 2 appris (utilisé en mode Ordre 2)") : (data.edited ? fr ("matrice modifiée") : fr ("matrice par défaut")),
                getLocalBounds().removeFromBottom (16), juce::Justification::centredRight);
}

void MatrixView::mouseDown (const juce::MouseEvent& e)
{
    const int size = processor.currentScale().size;
    dragRow = dragCol = -1;
    int row = 0, col = 0;
    if (! cellAt (e.position, size, row, col))
        return;
    dragRow = row;
    dragCol = col;
    const auto data = processor.getTrack (track);
    dragStart = markov::weight (data, size, row, col);
}

void MatrixView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragRow < 0)
        return;
    const int size = processor.currentScale().size;
    if (dragRow >= size || dragCol >= size)
        return;
    const float value = juce::jlimit (0.0f, 1.0f, dragStart - static_cast<float> (e.getDistanceFromDragStartY()) / 150.0f);
    const int r = dragRow, c = dragCol;
    processor.editTrack (track, [=] (markov::TrackData& d) {
        markov::materialise (d, size);
        d.w[r][c] = value;
    });
    repaint();
}

void MatrixView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int size = processor.currentScale().size;
    int row = 0, col = 0;
    if (! cellAt (e.position, size, row, col))
        return;
    processor.editTrack (track, [=] (markov::TrackData& d) {
        markov::materialise (d, size);
        d.w[row][col] = markov::defaultWeight (size, row, col);
    });
    repaint();
}

//==============================================================================
StepView::StepView (PluginProcessor& p) : processor (p)
{
    setTooltip (fr ("Pas de la piste. Haut : chance de jouer (0 à 100 %). Milieu : vélocité centrale. "
                    "Bas : écart de vélocité (tirage au hasard autour du centre). Cliquer ou glisser pour dessiner, "
                    "double-clic = valeur par défaut du pas."));
}

int StepView::numSteps() const
{
    return juce::jlimit (1, markov::maxSteps,
                         static_cast<int> (processor.apvts.getRawParameterValue (PluginProcessor::trackParam (track, "pas"))->load()));
}

juce::Rectangle<float> StepView::lane (int index) const
{
    auto area = getLocalBounds().toFloat();
    area.removeFromLeft (92.0f);
    const float gap = 8.0f;
    const float h = (area.getHeight() - 2.0f * gap) / 3.0f;
    return area.withY (area.getY() + static_cast<float> (index) * (h + gap)).withHeight (h);
}

void StepView::paint (juce::Graphics& g)
{
    const auto data = processor.getTrack (track);
    const int n = numSteps();
    const bool playing = processor.hostPlaying.load()
                         && processor.apvts.getRawParameterValue (PluginProcessor::trackParam (track, "actif"))->load() > 0.5f;
    const int current = processor.currentStep[(size_t) track].load();
    static const char* names[] = { "Chance", "Vélocité", "Écart" };
    static const char* units[] = { "%", "1-127", "+/-" };

    for (int l = 0; l < 3; ++l)
    {
        const auto r = lane (l);
        g.setColour (colours::dim);
        g.setFont (droz::ui::caps (11.0f));
        g.drawText (droz::ui::upper (fr (names[l])), juce::Rectangle<float> (0.0f, r.getCentreY() - 18.0f, 90.0f, 16.0f), juce::Justification::centredLeft);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (units[l], juce::Rectangle<float> (0.0f, r.getCentreY() + 2.0f, 90.0f, 16.0f), juce::Justification::centredLeft);

        const float w = r.getWidth() / static_cast<float> (n);
        for (int s = 0; s < n; ++s)
        {
            auto col = juce::Rectangle<float> (r.getX() + s * w, r.getY(), w, r.getHeight()).reduced (1.5f, 0.0f);
            g.setColour (s % 4 == 0 ? colours::panel.brighter (0.12f) : colours::panel);
            g.fillRect (col);
            if (playing && s == current)
            {
                g.setColour (accent.withAlpha (0.25f));
                g.fillRect (col);
            }
            g.setColour (colours::text);
            if (l == 0)
            {
                const float v = data.prob[s];
                g.fillRect (col.withTop (col.getBottom() - col.getHeight() * v));
                if (w >= 26.0f)
                {
                    g.setColour (v > 0.85f ? colours::background : colours::dim);
                    g.setFont (juce::Font (juce::FontOptions (10.0f)));
                    g.drawText (juce::String (juce::roundToInt (100.0f * v)), col.withHeight (14.0f), juce::Justification::centred);
                }
            }
            else if (l == 1)
            {
                const float c = data.velocity[s] / 127.0f;
                const float lo = juce::jmax (1, data.velocity[s] - data.spread[s]) / 127.0f;
                const float hi = juce::jmin (127, data.velocity[s] + data.spread[s]) / 127.0f;
                g.setColour (colours::text.withAlpha (0.55f));
                g.fillRect (col.withTop (col.getBottom() - col.getHeight() * c));
                // Spread: the range the velocity is drawn from, around the centre.
                g.setColour (colours::text.withAlpha (0.18f));
                g.fillRect (col.withTop (col.getBottom() - col.getHeight() * hi).withBottom (col.getBottom() - col.getHeight() * lo));
                g.setColour (colours::text);
                g.fillRect (col.withTop (col.getBottom() - col.getHeight() * c).withHeight (2.0f));
            }
            else
            {
                const float v = juce::jmin (1.0f, data.spread[s] / static_cast<float> (maxShownSpread));
                g.setColour (colours::dim);
                g.fillRect (col.withTop (col.getBottom() - col.getHeight() * v));
            }
        }
    }
}

void StepView::edit (juce::Point<float> pos, bool toDefault)
{
    if (dragLane < 0)
        return;
    const auto r = lane (dragLane);
    const int n = numSteps();
    const int s = juce::jlimit (0, n - 1, static_cast<int> ((pos.x - r.getX()) / (r.getWidth() / static_cast<float> (n))));
    const float v = juce::jlimit (0.0f, 1.0f, (r.getBottom() - pos.y) / r.getHeight());
    const int l = dragLane, t = track;
    processor.editTrack (track, [=] (markov::TrackData& d) {
        if (l == 0)
            d.prob[s] = toDefault ? markov::defaultProb (t, s) : std::round (v * 100.0f) / 100.0f;
        else if (l == 1)
            d.velocity[s] = toDefault ? markov::defaultVelocity (s) : static_cast<uint8_t> (juce::jlimit (1, 127, juce::roundToInt (v * 127.0f)));
        else
            d.spread[s] = toDefault ? markov::defaultSpread : static_cast<uint8_t> (juce::roundToInt (v * maxShownSpread));
    });
    repaint();
}

void StepView::mouseDown (const juce::MouseEvent& e)
{
    dragLane = -1;
    for (int l = 0; l < 3; ++l)
        if (lane (l).contains (e.position))
            dragLane = l;
    edit (e.position, false);
}

void StepView::mouseDrag (const juce::MouseEvent& e)
{
    edit (e.position, false);
}

void StepView::mouseDoubleClick (const juce::MouseEvent& e)
{
    for (int l = 0; l < 3; ++l)
        if (lane (l).contains (e.position))
            dragLane = l;
    edit (e.position, true);
}
} // namespace markovui
