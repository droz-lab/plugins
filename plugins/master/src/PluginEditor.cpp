#include "PluginEditor.h"

using namespace master;

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      spectre (p),
      orbite (p),
      panneau (p),
      entree (p.apvts, globaux()[0]),
      sortie (p.apvts, globaux()[1])
{
    addAndMakeVisible (spectre);
    addAndMakeVisible (orbite);
    addAndMakeVisible (panneau);
    addAndMakeVisible (entree);
    addAndMakeVisible (sortie);
    orbite.quandSelection = [this] (int m) { selectionne (m); };
    selectionne (processor.selection.load());

    // Apres avoir ajoute les controles : c'est ce qui leur fait reprendre l'habillage (zones de texte comprises).
    setLookAndFeel (&look);

    // Lire la taille gardee AVANT setResizeLimits : il redimensionne la fenetre (encore vide) au minimum,
    // et resized() ecraserait la taille gardee. Piege verifie par le test "interface".
    const int width = processor.editorWidth.load(), height = processor.editorHeight.load();
    setResizable (true, true);
    setResizeLimits (1000, 700, 1920, 1280);
    setSize (width, height);
    ready = true;
    startTimerHz (30);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PluginEditor::selectionne (int m)
{
    processor.selection = m;
    spectre.selection = m;
    panneau.montre (m);
    orbite.repaint();
    spectre.repaint();
}

void PluginEditor::timerCallback()
{
    spectre.tick();
    spectre.repaint();
    orbite.rafraichit();
    repaint (zoneVumetres);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (droz::ui::colours::background);
    if (cosmos.isValid())
        g.drawImage (cosmos, getLocalBounds().toFloat());

    droz::ui::drawHeader (g, getLocalBounds(), "Master");

    auto r = zoneVumetres.toFloat();
    const float w = (r.getWidth() - 12.0f) / 2.0f;
    dessineVumetre (g, r.removeFromLeft (w), processor.creteEntree.load(), "IN");
    r.removeFromLeft (12.0f);
    dessineVumetre (g, r, processor.creteSortie.load(), "OUT");

    // Latence, utile pour caler un enregistrement.
    g.setColour (droz::ui::colours::dim);
    g.setFont (droz::ui::caps (8.5f));
    const double ms = 1000.0 * processor.getLatencySamples() / std::max (1.0, processor.frequence.load());
    g.drawText (T ("LATENCE"), zoneVumetres.withY (zoneVumetres.getBottom() + 4).withHeight (12).expanded (20, 0), juce::Justification::centred);
    g.drawText (juce::String (ms, 1) + " MS", zoneVumetres.withY (zoneVumetres.getBottom() + 16).withHeight (12).expanded (20, 0), juce::Justification::centred);
}

void PluginEditor::dessineVumetre (juce::Graphics& g, juce::Rectangle<float> r, float db, const juce::String& nom)
{
    using namespace droz::ui;
    g.setColour (colours::dim);
    g.setFont (caps (8.5f));
    g.drawText (nom, r.removeFromBottom (14.0f), juce::Justification::centred);
    r.removeFromBottom (4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (colours::line);
    g.drawRoundedRectangle (r, 2.0f, 1.0f);
    const float t = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    auto barre = r.reduced (2.0f);
    barre = barre.removeFromBottom (barre.getHeight() * t);
    g.setGradientFill (juce::ColourGradient (master::ui::nebuleuse.withAlpha (0.4f), 0, r.getBottom(), colours::text, 0, r.getY(), false));
    g.fillRect (barre);
    // Reperes : 0, -6, -12, -24 dB.
    for (float rep : { 0.0f, -6.0f, -12.0f, -24.0f })
    {
        const float y = r.getBottom() - r.getHeight() * (rep + 60.0f) / 60.0f;
        g.setColour (colours::line.brighter (0.4f));
        g.drawHorizontalLine (juce::roundToInt (y), r.getX(), r.getX() + 4.0f);
    }
}

void PluginEditor::resized()
{
    if (ready)
    {
        processor.editorWidth = getWidth();
        processor.editorHeight = getHeight();
    }
    cosmos = master::ui::dessineCosmos (getWidth(), getHeight(), 1.0f);

    auto area = getLocalBounds();
    area.removeFromTop (44); // en-tete commun
    area.reduce (14, 12);

    auto droite = area.removeFromRight (110);
    area.removeFromRight (12);
    entree.setBounds (droite.removeFromTop (100));
    droite.removeFromTop (8);
    sortie.setBounds (droite.removeFromTop (100));
    droite.removeFromTop (16);
    zoneVumetres = droite.withTrimmedBottom (32).reduced (18, 0);

    spectre.setBounds (area.removeFromTop (juce::roundToInt (area.getHeight() * 0.28f)));
    orbite.setBounds (area.removeFromTop (118));
    panneau.setBounds (area);
}
