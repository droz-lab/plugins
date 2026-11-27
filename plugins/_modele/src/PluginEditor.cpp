#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      gainAttachment (p.apvts, "gain", gainSlider)
{
    // Texte accentue : toujours passer par fromUTF8, jamais une chaine brute dans juce::String.
    title.setText (juce::String::fromUTF8 ("Droz Modèle"), juce::dontSendNotification);
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
    gainSlider.setBounds (area.reduced (8));
}
