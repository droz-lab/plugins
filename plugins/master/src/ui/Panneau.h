// Panneau du module affiche : allumage, mode (stereo, mid-side, gauche-droite, transitoire-tonal),
// dose de chaque partie, puis tous ses reglages.
#pragma once

#include "../PluginProcessor.h"
#include "Controles.h"

namespace master::ui
{
class Panneau : public juce::Component
{
public:
    explicit Panneau (PluginProcessor& p) : proc (p)
    {
        const char* noms[] = { "Stéréo", "Mid / Side", "Gauche / Droite", "Transit. / Tonal" };
        const char* aides[] = {
            "Le module traite gauche et droite ensemble (image gardée).",
            "Le module traite le centre (mid) et les côtés (side) séparément. Dose de chaque partie à gauche.",
            "Le module traite gauche et droite séparément.",
            "Le signal est coupé en attaques (transitoires) et en partie tenue (tonal), traitées séparément.",
        };
        for (int i = 0; i < 4; ++i)
        {
            auto& b = modes[static_cast<size_t> (i)];
            b.setButtonText (T (noms[i]));
            b.setTooltip (T (aides[i]));
            b.setClickingTogglesState (false);
            b.onClick = [this, i] {
                if (attacheMode)
                    attacheMode->setValueAsCompleteGesture (static_cast<float> (i));
            };
            addAndMakeVisible (b);
        }
        actif.setButtonText (T ("Allumé"));
        actif.setTooltip (T ("Allume ou éteint ce module (fondu sans clic, sa place dans la chaîne est gardée)."));
        addAndMakeVisible (actif);
    }

    void montre (int m)
    {
        module = m;
        const auto& d = modules()[static_cast<size_t> (m)];
        groupes.clear();
        attacheActif.reset();
        attacheMode.reset();
        doseA.reset();
        doseB.reset();

        attacheActif = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, idActif (m), actif);
        if (auto* p = proc.apvts.getParameter (idMode (m)))
            attacheMode = std::make_unique<juce::ParameterAttachment> (*p, [this] (float v) { modeAffiche (juce::roundToInt (v)); });

        auto dose = [&] (const juce::String& id, const char* aide) {
            DescParam p;
            p.id = id;
            p.nom = "Dose";
            p.court = "Dose";
            p.min = 0;
            p.max = 100;
            p.defaut = 100;
            p.unite = "%";
            p.aide = T (aide);
            auto c = std::make_unique<Controle> (proc.apvts, p);
            addAndMakeVisible (*c);
            return c;
        };
        doseA = dose (idDoseA (m), "Part de l'effet sur la première partie (100 % = plein effet, 0 % = intact).");
        doseB = dose (idDoseB (m), "Part de l'effet sur la seconde partie (100 % = plein effet, 0 % = intact).");

        for (const auto& g : d.groupes)
        {
            auto gr = std::make_unique<Groupe> (proc.apvts, g);
            addAndMakeVisible (*gr);
            groupes.push_back (std::move (gr));
        }
        if (attacheMode)
            attacheMode->sendInitialUpdate();
        // Controles crees apres l'habillage : on leur redonne le style (zones de texte comprises).
        sendLookAndFeelChange();
        resized();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace droz::ui;
        const auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (b, 6.0f);
        g.setColour (colours::line);
        g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);

        const auto& d = modules()[static_cast<size_t> (module)];
        auto haut = getLocalBounds().reduced (16, 0).removeFromTop (40);
        g.setColour (colours::text);
        g.setFont (caps (14.0f));
        const auto titre = upper (d.nom);
        g.drawText (titre, haut, juce::Justification::centredLeft);
        g.setColour (colours::dim);
        g.setFont (juce::Font (juce::FontOptions (12.5f)));
        const int decal = juce::roundToInt (juce::GlyphArrangement::getStringWidth (caps (14.0f), titre)) + 18;
        g.drawText (d.aide, haut.withTrimmedLeft (decal).withTrimmedRight (120), juce::Justification::centredLeft);
        g.setColour (colours::line);
        g.fillRect (16, 40, getWidth() - 32, 1);
        g.fillRect (colonneGauche + 8, 52, 1, getHeight() - 64);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12, 0);
        actif.setBounds (r.removeFromTop (40).removeFromRight (110).withSizeKeepingCentre (110, 24));
        r.removeFromTop (10);
        r.removeFromBottom (10);

        auto gauche = r.removeFromLeft (colonneGauche - 12);
        for (auto& b : modes)
        {
            b.setBounds (gauche.removeFromTop (26).reduced (0, 1));
            gauche.removeFromTop (3);
        }
        gauche.removeFromTop (6);
        if (doseA != nullptr)
        {
            const int h = std::min (96, gauche.getHeight());
            auto rangee = gauche.removeFromTop (h);
            const bool deux = doseB != nullptr && doseB->isVisible();
            doseA->setBounds (deux ? rangee.removeFromLeft (rangee.getWidth() / 2) : rangee);
            if (deux)
                doseB->setBounds (rangee);
        }

        r.removeFromLeft (16);
        int cols = 0;
        for (auto& g : groupes)
            cols += g->colonnes();
        if (cols == 0)
            return;
        const int largeurCol = std::min (150, r.getWidth() / cols);
        for (auto& g : groupes)
            g->setBounds (r.removeFromLeft (largeurCol * g->colonnes()));
    }

    /** Appele par la minuterie de l'editeur. */
    void tick() {}

private:
    void modeAffiche (int mode)
    {
        for (int i = 0; i < 4; ++i)
            modes[static_cast<size_t> (i)].setToggleState (i == mode, juce::dontSendNotification);
        if (doseA != nullptr)
        {
            doseA->renomme (nomPartie (mode, false));
            doseB->renomme (nomPartie (mode, true));
            doseB->setVisible (mode != 0);
        }
        resized();
    }

    static constexpr int colonneGauche = 190;
    PluginProcessor& proc;
    int module = 0;
    juce::ToggleButton actif;
    std::array<juce::TextButton, 4> modes;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attacheActif;
    std::unique_ptr<juce::ParameterAttachment> attacheMode;
    std::unique_ptr<Controle> doseA, doseB;
    std::vector<std::unique_ptr<Groupe>> groupes;
};
} // namespace master::ui
