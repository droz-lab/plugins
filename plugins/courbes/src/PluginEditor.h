#pragma once

#include "PluginProcessor.h"

#include <functional>

/** Couleurs de Droz Courbes. */
namespace palette
{
const juce::Colour fond { 0xff111317 };
const juce::Colour panneau { 0xff1a1d23 };
const juce::Colour grille { 0xff2a2e37 };
const juce::Colour grilleForte { 0xff3a404c };
const juce::Colour texte { 0xffd9dce3 };
const juce::Colour texteDoux { 0xff8a909c };
const juce::Colour accent { 0xffe8a33d };
const juce::Colour accent2 { 0xff4fc3b8 };
} // namespace palette

/** Zone ou l'on voit et modifie la courbe. */
class VueCourbe final : public juce::Component, public juce::SettableTooltipClient
{
public:
    VueCourbe();

    void setCourbe (const courbes::Courbe& c);
    const courbes::Courbe& getCourbe() const noexcept { return courbe; }
    void setLecture (float phase, float valeur);
    bool enTrainDeModifier() const noexcept { return glisse != Glisse::rien; }

    std::function<void()> onModif;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    enum class Glisse
    {
        rien,
        point,
        tension
    };

    juce::Rectangle<float> zone() const;
    juce::Point<float> versEcran (float x, float y) const;
    juce::Point<float> versCourbe (juce::Point<float> ecran) const;
    int pointSous (juce::Point<float> ecran) const;
    int segmentSous (float xCourbe) const;
    void modifie();
    void menuPoint (int index);

    courbes::Courbe courbe;
    float lecture = 0.0f, valeur = 0.0f;
    int survole = -1, choisi = -1;
    Glisse glisse = Glisse::rien;
    float tensionDepart = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VueCourbe)
};

class PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Potard
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label nom;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attache;
    };

    void timerCallback() override;
    void preparerPotard (Potard&, const juce::String& id, const juce::String& nom, const juce::String& aide);
    void choisirMembre (int famille, int membre);
    void generer();
    void majTitreMembre();
    void envoyerCourbe();

    PluginProcessor& processor;
    bool ready = false; // vrai une fois la taille gardee restauree

    juce::LookAndFeel_V4 style;
    juce::TooltipWindow infobulles { this, 500 };

    juce::Label titre;
    juce::ComboBox familles;
    juce::TextButton precedent { "<" }, suivant { ">" }, boutonGenerer;
    juce::Label nomMembre;
    VueCourbe vue;
    juce::Label aide;

    juce::ComboBox vitesse;
    juce::Label nomVitesse;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attacheVitesse;
    Potard decalage, minimum, maximum, lissage, ecoute, cc, canal;
    juce::Label lectureCc;

    int famille = 0, membre = 0;
    uint32_t graine = 0;
    bool modifiee = false;
    int versionConnue = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
