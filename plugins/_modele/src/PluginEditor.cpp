#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      gainAttachment (p.apvts, "gain", gain.slider)
{
    addAndMakeVisible (gain);

    // Apres avoir ajoute les controles : c'est ce qui leur fait reprendre l'habillage (zones de texte comprises).
    setLookAndFeel (&look);

    // Lire la taille gardee AVANT setResizeLimits : il redimensionne la fenetre (encore vide) au minimum,
    // et resized() ecraserait la taille gardee. Piege verifie par le test "interface".
    const int width = processor.editorWidth.load(), height = processor.editorHeight.load();
    setResizable (true, true);
    setResizeLimits (320, 200, 1600, 1200);
    setSize (width, height);
    ready = true;
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (droz::ui::colours::background);

    // Texte accentue : toujours passer par fromUTF8, jamais une chaine brute dans juce::String.
    auto name = juce::String::fromUTF8 ("Droz Modèle");
    if (name.startsWithIgnoreCase ("droz "))
        name = name.substring (5);
    auto area = droz::ui::drawHeader (g, getLocalBounds(), name, JucePlugin_VersionString);
    // Pied : la description de plugin.json. A remplacer par un etat utile (tempo, aide) quand le plugin en a un.
    droz::ui::drawFooter (g, area, juce::String::fromUTF8 (JucePlugin_Desc));
}

void PluginEditor::resized()
{
    if (ready)
    {
        processor.editorWidth = getWidth();
        processor.editorHeight = getHeight();
    }

    namespace m = droz::ui::metrics;
    auto area = getLocalBounds();
    area.removeFromTop (m::header);
    area.removeFromBottom (m::footer);
    gain.setBounds (area.reduced (m::margin).withSizeKeepingCentre (m::knobWidth, m::knobHeight));
}
