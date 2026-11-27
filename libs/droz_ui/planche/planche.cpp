// Planche de l'habillage : tous les controles de droz::ui sur une image, sans DAW.
//   droz_ui_planche docs/habillage.png [--echelle 2]
// Sert a regler l'habillage et a le montrer. Aucun plugin n'en depend.

#include <droz/ui.h>

#include <cmath>
#include <cstdio>
#include <memory>

namespace
{
using namespace droz::ui;

// Exemple de visuel : une onde (la matiere, en blanc) et une tete de lecture (ce qui joue, en orange).
struct Onde final : Visual
{
    void draw (juce::Graphics& g, juce::Rectangle<float> area, double seconds) override
    {
        const int pas = 16;
        g.setColour (colours::line);
        for (int i = 1; i < pas; ++i)
            g.fillRect (area.getX() + area.getWidth() * (float) i / (float) pas, area.getY(), 1.0f, area.getHeight());

        juce::Path onde;
        for (int x = 0; x <= (int) area.getWidth(); x += 2)
        {
            const float t = (float) x / area.getWidth();
            const float y = area.getCentreY() - area.getHeight() * 0.36f * std::sin (t * 12.566f) * (0.25f + 0.75f * t);
            if (x == 0)
                onde.startNewSubPath (area.getX(), y);
            else
                onde.lineTo (area.getX() + (float) x, y);
        }
        g.setColour (colours::text);
        g.strokePath (onde, juce::PathStrokeType (metrics::stroke));

        const float tete = area.getX() + area.getWidth() * (float) std::fmod (0.3125 + seconds * 0.5, 1.0);
        g.setColour (colours::accent);
        g.fillRect (tete - 1.0f, area.getY(), metrics::stroke, area.getHeight());
    }
};

struct Planche final : juce::Component
{
    Planche()
    {
        for (auto* k : { &taux, &profondeur, &forme, &melange })
            addAndMakeVisible (*k);
        taux.slider.setRange (0.0, 1.0, 0.01);
        taux.slider.setValue (0.25);
        profondeur.slider.setRange (0.0, 100.0, 1.0);
        profondeur.slider.setValue (70.0);
        forme.slider.setRange (-1.0, 1.0, 0.01);
        forme.slider.setValue (-0.4);
        melange.slider.setRange (0.0, 100.0, 1.0);
        melange.slider.setValue (100.0);
        melange.slider.setEnabled (false);

        horizontal.setRange (0.0, 1.0);
        horizontal.setValue (0.62);
        vertical.setRange (0.0, 1.0);
        vertical.setValue (0.4);
        division.addItemList ({ "1/4", "1/8", "1/16", "1/16T", "1/32" }, 1);
        division.setSelectedId (3);
        synchro.setToggleState (true, juce::dontSendNotification);
        b.setToggleState (true, juce::dontSendNotification);
        for (auto* c : std::initializer_list<juce::Component*> { &horizontal, &vertical, &division, &synchro, &libre, &a, &b, &visuel })
            addAndMakeVisible (*c);

        setLookAndFeel (&look);
        setSize (640, 520);
    }

    ~Planche() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::background);
        auto area = drawHeader (g, getLocalBounds(), "Planche", "0.1.0");
        area = drawFooter (g, area, juce::String::fromUTF8 ("140 BPM  -  synchro hôte  -  pied : état, aide, tempo"));
        area.reduce (metrics::margin, metrics::margin);
        area.removeFromTop (120 + metrics::gap);
        area.removeFromTop (metrics::knobHeight + metrics::gap);
        drawPanel (g, area.removeFromLeft (300), juce::String::fromUTF8 ("Réglages"));
        area.removeFromLeft (metrics::gap);
        drawPanel (g, area, "Couleurs");
        const juce::Colour nuancier[] = { colours::background, colours::panel, colours::line, colours::dim, colours::text, colours::accent };
        const char* noms[] = { "background", "panel", "line", "dim", "text", "accent" };
        auto zone = area.reduced (metrics::gap).withTrimmedTop (18);
        for (int i = 0; i < 6; ++i)
        {
            auto ligne = zone.removeFromTop (20);
            const auto pastille = ligne.removeFromLeft (28).reduced (0, 3).toFloat();
            g.setColour (nuancier[i]);
            g.fillRoundedRectangle (pastille, 2.0f);
            g.setColour (colours::line);
            g.drawRoundedRectangle (pastille, 2.0f, 1.0f);
            g.setColour (colours::text);
            g.setFont (body (12.0f));
            g.drawText (juce::String (noms[i]) + "  #" + nuancier[i].toDisplayString (false), ligne.withTrimmedLeft (8), juce::Justification::centredLeft);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds();
        area.removeFromTop (metrics::header);
        area.removeFromBottom (metrics::footer);
        area.reduce (metrics::margin, metrics::margin);
        visuel.setBounds (area.removeFromTop (120));
        area.removeFromTop (metrics::gap);

        auto potards = area.removeFromTop (metrics::knobHeight);
        for (auto* k : { &taux, &profondeur, &forme, &melange })
        {
            k->setBounds (potards.removeFromLeft (metrics::knobWidth));
            potards.removeFromLeft (metrics::gap);
        }
        vertical.setBounds (potards.removeFromLeft (metrics::row));
        area.removeFromTop (metrics::gap);

        auto reglages = area.removeFromLeft (300).reduced (metrics::gap).withTrimmedTop (18);
        auto ligne = reglages.removeFromTop (metrics::row);
        division.setBounds (ligne.removeFromLeft (110));
        ligne.removeFromLeft (metrics::gap);
        a.setBounds (ligne.removeFromLeft (48));
        ligne.removeFromLeft (4);
        b.setBounds (ligne.removeFromLeft (48));
        reglages.removeFromTop (metrics::gap);
        ligne = reglages.removeFromTop (metrics::row);
        synchro.setBounds (ligne.removeFromLeft (130));
        libre.setBounds (ligne);
        reglages.removeFromTop (metrics::gap);
        horizontal.setBounds (reglages.removeFromTop (metrics::row));
    }

    LookAndFeel look;
    Onde visuel;
    Knob taux { "Taux", "", "" }, profondeur { "Profondeur", "", " %" }, forme { "Forme", "", "" }, melange { juce::String::fromUTF8 ("Mélange"), "", " %" };
    juce::Slider horizontal { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Slider vertical { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::ComboBox division;
    juce::ToggleButton synchro { "Synchro" }, libre { "Libre" };
    juce::TextButton a { "A" }, b { "B" };
};
} // namespace

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::printf ("usage : droz_ui_planche <image.png> [--echelle 2]\n");
        return 2;
    }
    float echelle = 2.0f;
    for (int i = 2; i + 1 < argc; ++i)
        if (juce::String (argv[i]) == "--echelle")
            echelle = juce::String (argv[i + 1]).getFloatValue();

    juce::ScopedJuceInitialiser_GUI gui;
    int code = 0;
    {
        Planche planche;
        const auto image = planche.createComponentSnapshot (planche.getLocalBounds(), true, echelle);
        juce::File fichier (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String::fromUTF8 (argv[1])));
        fichier.deleteFile();
        juce::FileOutputStream flux (fichier);
        if (! flux.openedOk() || ! juce::PNGImageFormat().writeImageToStream (image, flux))
        {
            std::printf ("ecriture impossible : %s\n", argv[1]);
            code = 1;
        }
    }
    return code;
}
