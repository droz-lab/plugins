// Habillage commun des plugins Droz.
// Direction : celle de l'artwork Droz Recordz (Drive, dossier Charte Graphique) : fond ardoise bleu-noir,
// logo blanc en capitales larges et espacees, coulees rouge-orange. Donc : sombre, texte blanc glace,
// traits fins, et le rouge-orange comme seule couleur vive (valeurs, tete de lecture, etat actif).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace droz::ui
{
namespace colours
{
inline const juce::Colour background { 0xff0b1219 }; // bleu-noir du fond de la toile
inline const juce::Colour panel { 0xff1a2732 };      // ardoise sombre
inline const juce::Colour line { 0xff283c47 };       // ardoise claire : filets, pistes des potards
inline const juce::Colour text { 0xffedf3f6 };       // blanc glace du logo
inline const juce::Colour dim { 0xff8fa1ad };
inline const juce::Colour accent { 0xfff0431f };     // rouge-orange des coulees
} // namespace colours

/** Capitales espacees, comme le logo. A utiliser pour les titres et les libelles courts. */
inline juce::Font caps (float size)
{
    return juce::Font (juce::FontOptions (size)).withExtraKerningFactor (0.28f);
}

/** Majuscules, accents compris (toUpperCase de JUCE laisse "e accent grave" en minuscule selon le systeme). */
inline juce::String upper (const juce::String& text)
{
    juce::String out;
    for (auto c : text)
    {
        if ((c >= 0xE0 && c <= 0xFE && c != 0xF7) || (c >= 'a' && c <= 'z'))
            c -= 32;
        out += c;
    }
    return out;
}

/** En-tete commun : "DROZ" a gauche, nom du plugin au centre, filet dessous. Renvoie la zone restante. */
inline juce::Rectangle<int> drawHeader (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& pluginName)
{
    auto header = area.removeFromTop (44);
    g.setColour (colours::dim);
    g.setFont (caps (12.0f));
    g.drawText ("DROZ", header.reduced (16, 0), juce::Justification::centredLeft);
    g.setColour (colours::text);
    g.setFont (caps (16.0f));
    g.drawText (upper (pluginName), header, juce::Justification::centred);
    g.setColour (colours::line);
    g.fillRect (header.removeFromBottom (1));
    return area;
}

/** Potards en arc fin (valeur en rouge-orange), zones de texte sans cadre. */
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, colours::background);
        setColour (juce::Label::textColourId, colours::text);
        setColour (juce::Slider::textBoxTextColourId, colours::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, colours::line);
        setColour (juce::TextEditor::backgroundColourId, colours::panel);
        setColour (juce::TextEditor::textColourId, colours::text);
        setColour (juce::TextEditor::outlineColourId, colours::line);
        setColour (juce::TextEditor::focusedOutlineColourId, colours::dim);
        setColour (juce::PopupMenu::backgroundColourId, colours::panel);
        setColour (juce::PopupMenu::textColourId, colours::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::line);
        setColour (juce::TooltipWindow::backgroundColourId, colours::panel);
        setColour (juce::TooltipWindow::textColourId, colours::text);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float position, float startAngle, float endAngle, juce::Slider&) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (8.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float angle = startAngle + position * (endAngle - startAngle);

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
        g.setColour (colours::line);
        g.strokePath (track, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path value;
        value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
        g.setColour (colours::accent);
        g.strokePath (value, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Repere : un trait fin du centre vers l'arc.
        const juce::Point<float> tip (centre.x + (radius - 6.0f) * std::sin (angle), centre.y - (radius - 6.0f) * std::cos (angle));
        const juce::Point<float> base (centre.x + radius * 0.45f * std::sin (angle), centre.y - radius * 0.45f * std::cos (angle));
        g.setColour (colours::text);
        g.drawLine ({ base, tip }, 2.0f);
    }

    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* box = juce::LookAndFeel_V4::createSliderTextBox (slider);
        box->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        box->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        return box;
    }

    juce::Font getLabelFont (juce::Label&) override { return juce::Font (juce::FontOptions (14.0f)); }
};
} // namespace droz::ui
