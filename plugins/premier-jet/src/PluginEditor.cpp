#include "PluginEditor.h"

#include <Equipe.h>

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      gainAttachment (p.apvts, "gain", gainSlider)
{
    // Texte accentue : toujours passer par fromUTF8, jamais une chaine brute dans juce::String.
    title.setText (juce::String::fromUTF8 ("Droz Premier Jet"), juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centred);
    title.setFont (juce::FontOptions (20.0f));
    addAndMakeVisible (title);

    gainSlider.setTextValueSuffix (" dB");
    addAndMakeVisible (gainSlider);

    // Lire la taille gardee AVANT setResizeLimits : il redimensionne la fenetre (encore vide) au minimum,
    // et resized() ecraserait la taille gardee. Piege verifie par le test "interface".
    const int width = processor.editorWidth.load(), height = processor.editorHeight.load();
    setResizable (true, true);
    setResizeLimits (320, 200, 1600, 1200);
    setSize (width, height);
    ready = true;
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff15171c));

    // Colonne de droite : une ligne par fichier de equipe/.
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (40);
    area = area.removeFromRight (area.getWidth() / 2);
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (juce::String::fromUTF8 ("Équipe"), area.removeFromTop (20), juce::Justification::centredLeft);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.setFont (juce::FontOptions (15.0f));
    for (int i = 0; equipe::lignes[i] != nullptr && area.getHeight() >= 22; ++i)
        g.drawText (juce::String::fromUTF8 (equipe::lignes[i]), area.removeFromTop (22), juce::Justification::centredLeft, true);
}

void PluginEditor::resized()
{
    if (ready)
    {
        processor.editorWidth = getWidth();
        processor.editorHeight = getHeight();
    }

    auto area = getLocalBounds().reduced (16);
    title.setBounds (area.removeFromTop (32));
    gainSlider.setBounds (area.removeFromLeft (area.getWidth() / 2).reduced (8));
}
