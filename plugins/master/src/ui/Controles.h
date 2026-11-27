// Controles generes a partir de la description des parametres (Parametres.h).
#pragma once

#include "../Parametres.h"
#include "Habillage.h"

namespace master::ui
{
/** Un parametre a l'ecran : potard, liste ou interrupteur, avec son nom, son info-bulle et son attache. */
class Controle : public juce::Component
{
public:
    Controle (juce::AudioProcessorValueTreeState& etat, const DescParam& d) : desc (d)
    {
        const auto bulle = d.nom + " : " + d.aide;
        if (d.genre == Genre::continu)
        {
            nom = droz::ui::upper (d.court);
            potard.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            potard.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
            potard.setTooltip (bulle);
            addAndMakeVisible (potard);
            attachePotard = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (etat, d.id, potard);
            potard.setDoubleClickReturnValue (true, d.defaut); // double-clic = valeur par defaut
        }
        else if (d.genre == Genre::liste)
        {
            liste.addItemList (d.choix, 1);
            liste.setTooltip (bulle);
            addAndMakeVisible (liste);
            attacheListe = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (etat, d.id, liste);
        }
        else
        {
            bouton.setButtonText (d.court);
            bouton.setTooltip (bulle);
            addAndMakeVisible (bouton);
            attacheBouton = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (etat, d.id, bouton);
        }
    }

    bool estPotard() const { return desc.genre == Genre::continu; }

    /** Nom affiche au-dessus du potard (change avec le mode pour les doses). */
    void renomme (const juce::String& n)
    {
        nom = droz::ui::upper (n);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        if (! estPotard())
            return;
        // Dessine ici plutot qu'avec un juce::Label : l'habillage commun impose sa police aux Labels.
        g.setColour (droz::ui::colours::dim);
        g.setFont (droz::ui::caps (9.5f));
        g.drawText (nom, getLocalBounds().removeFromTop (14), juce::Justification::centred);
    }
    int hauteurFixe() const { return desc.genre == Genre::liste ? 28 : 24; }

    void resized() override
    {
        auto r = getLocalBounds();
        if (estPotard())
        {
            r.removeFromTop (15);
            potard.setBounds (r.withTrimmedBottom (4));
        }
        else if (desc.genre == Genre::liste)
            liste.setBounds (r.reduced (2, 2));
        else
            bouton.setBounds (r);
    }

    juce::Slider& lePotard() { return potard; }

private:
    DescParam desc;
    juce::String nom;
    juce::Slider potard;
    juce::ComboBox liste;
    juce::ToggleButton bouton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachePotard;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attacheListe;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attacheBouton;
};

/** Une colonne de reglages avec son titre : listes et interrupteurs en haut, potards en grille dessous. */
class Groupe : public juce::Component
{
public:
    Groupe (juce::AudioProcessorValueTreeState& etat, const DescGroupe& g) : titre (g.titre)
    {
        for (const auto& p : g.params)
        {
            auto c = std::make_unique<Controle> (etat, p);
            addAndMakeVisible (*c);
            (c->estPotard() ? potards : fixes).push_back (std::move (c));
        }
    }

    /** Nombre de colonnes de potards (pour repartir la largeur entre groupes). */
    int colonnes() const { return potards.size() > 4 ? 2 : 1; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().reduced (4, 0);
        g.setColour (droz::ui::colours::dim);
        g.setFont (droz::ui::caps (10.0f));
        g.drawText (droz::ui::upper (titre), r.removeFromTop (18), juce::Justification::centred);
        g.setColour (droz::ui::colours::line);
        g.fillRect (r.getX() + 8, 18, r.getWidth() - 16, 1);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6, 0);
        r.removeFromTop (24);
        for (auto& f : fixes)
        {
            f->setBounds (r.removeFromTop (f->hauteurFixe()));
            r.removeFromTop (4);
        }
        if (potards.empty())
            return;
        const int cols = colonnes();
        const int rangees = (static_cast<int> (potards.size()) + cols - 1) / cols;
        const int h = std::min (104, r.getHeight() / rangees);
        const int w = r.getWidth() / cols;
        for (size_t i = 0; i < potards.size(); ++i)
        {
            const int col = static_cast<int> (i) % cols, row = static_cast<int> (i) / cols;
            potards[i]->setBounds (r.getX() + col * w, r.getY() + row * h, w, h);
        }
    }

private:
    juce::String titre;
    std::vector<std::unique_ptr<Controle>> fixes, potards;
};
} // namespace master::ui
