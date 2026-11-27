// Habillage commun des plugins Droz. La regle ecrite est dans docs/HABILLAGE.md, la planche dans docs/habillage.png.
// Direction : celle de l'artwork Droz Recordz (Drive, dossier Charte Graphique) : fond ardoise bleu-noir,
// logo blanc en capitales larges et espacees, coulees rouge-orange. Donc : sombre, texte blanc glace,
// traits fins, et le rouge-orange comme seule couleur vive (valeurs, tete de lecture, etat actif).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <DrozPolices.h>

#include <cmath>

namespace droz::ui
{
//==============================================================================
// Couleurs. Aucune autre couleur dans un plugin : si un besoin revient, il s'ajoute ici.
namespace colours
{
inline const juce::Colour background { 0xff0b1219 }; // bleu-noir du fond de la toile
inline const juce::Colour panel { 0xff1a2732 };      // ardoise sombre : cadres, menus, visuel
inline const juce::Colour line { 0xff283c47 };       // ardoise claire : filets, pistes, etat eteint
inline const juce::Colour text { 0xffedf3f6 };       // blanc glace du logo
inline const juce::Colour dim { 0xff8fa1ad };        // libelles, texte secondaire
inline const juce::Colour accent { 0xfff0431f };     // rouge-orange des coulees : la seule couleur vive
} // namespace colours

//==============================================================================
// Mesures, en pixels a l'echelle 1. Tout s'aligne sur une grille de 4.
namespace metrics
{
inline constexpr int header = 44;      // en-tete commun
inline constexpr int footer = 24;      // pied commun
inline constexpr int margin = 16;      // bord de la fenetre
inline constexpr int gap = 12;         // entre deux controles
inline constexpr int knobWidth = 84;   // un potard avec son libelle et sa valeur
inline constexpr int knobHeight = 108;
inline constexpr int row = 24;         // hauteur d'un bouton, d'un menu, d'un curseur horizontal
inline constexpr float stroke = 2.0f;  // epaisseur des arcs et des pistes
inline constexpr float corner = 3.0f;  // arrondi des cadres
} // namespace metrics

//==============================================================================
// Polices (SIL OFL, embarquees : meme rendu sur Windows, Mac et Linux).
inline juce::Typeface::Ptr titleTypeface()
{
    static const auto face = juce::Typeface::createSystemTypefaceFor (DrozPolices::MichromaRegular_ttf,
                                                                      DrozPolices::MichromaRegular_ttfSize);
    return face;
}

inline juce::Typeface::Ptr monoTypeface()
{
    static const auto face = juce::Typeface::createSystemTypefaceFor (DrozPolices::IBMPlexMonoRegular_ttf,
                                                                      DrozPolices::IBMPlexMonoRegular_ttfSize);
    return face;
}

/** Michroma, capitales larges et espacees comme le logo. Titres et libelles courts, toujours avec upper(). */
inline juce::Font caps (float size)
{
    return juce::Font (juce::FontOptions (titleTypeface()).withHeight (size)).withExtraKerningFactor (0.14f);
}

/** IBM Plex Mono. Valeurs, menus, textes : les chiffres ne bougent pas quand la valeur change. */
inline juce::Font mono (float size)
{
    return juce::Font (juce::FontOptions (monoTypeface()).withHeight (size));
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

/** Echelle d'une interface a ratio fixe dessinee a (baseWidth x baseHeight). Voir docs/PIEGES.md, "ratio fixe". */
inline float scaleFor (juce::Rectangle<int> bounds, int baseWidth, int baseHeight)
{
    return juce::jmin ((float) bounds.getWidth() / (float) baseWidth, (float) bounds.getHeight() / (float) baseHeight);
}

//==============================================================================
/** En-tete commun : "DROZ" a gauche (souligne d'une coulee), nom du plugin au centre, version a droite.
    Renvoie la zone restante. Le nom se passe sans le prefixe "Droz ". */
inline juce::Rectangle<int> drawHeader (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& pluginName,
                                        const juce::String& version = {})
{
    auto header = area.removeFromTop (metrics::header);
    const auto filet = header.removeFromBottom (1);
    const auto inner = header.reduced (metrics::margin, 0);

    const auto logoFont = caps (11.0f);
    g.setColour (colours::dim);
    g.setFont (logoFont);
    g.drawText ("DROZ", inner, juce::Justification::centredLeft);

    g.setColour (colours::text);
    g.setFont (caps (15.0f));
    g.drawText (upper (pluginName), header, juce::Justification::centred);

    if (version.isNotEmpty())
    {
        g.setColour (colours::dim);
        g.setFont (mono (11.0f));
        g.drawText ("v" + version, inner, juce::Justification::centredRight);
    }

    g.setColour (colours::line);
    g.fillRect (filet);
    g.setColour (colours::accent);
    g.fillRect (filet.withWidth (metrics::margin + juce::roundToInt (juce::GlyphArrangement::getStringWidth (logoFont, "DROZ"))));
    return area;
}

/** Pied commun : un filet et une ligne de texte discrete (etat, aide, tempo). Renvoie la zone restante. */
inline juce::Rectangle<int> drawFooter (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text)
{
    auto footer = area.removeFromBottom (metrics::footer);
    g.setColour (colours::line);
    g.fillRect (footer.removeFromTop (1));
    g.setColour (colours::dim);
    g.setFont (mono (11.0f));
    g.drawText (text, footer.reduced (metrics::margin, 0), juce::Justification::centredLeft, true);
    return area;
}

/** Cadre de section : fond ardoise, titre en capitales en haut a gauche. Renvoie l'interieur. */
inline juce::Rectangle<int> drawPanel (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title = {})
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (area.toFloat(), metrics::corner);
    auto inner = area.reduced (metrics::gap);
    if (title.isNotEmpty())
    {
        g.setColour (colours::dim);
        g.setFont (caps (9.0f));
        g.drawText (upper (title), inner.removeFromTop (14), juce::Justification::centredLeft);
        inner.removeFromTop (4);
    }
    return inner;
}

//==============================================================================
/** Habillage de tous les controles JUCE. Un seul par editeur, pose avec setLookAndFeel apres avoir ajoute
    les controles (voir docs/PIEGES.md), retire dans le destructeur. */
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        using namespace juce;
        setColour (ResizableWindow::backgroundColourId, colours::background);
        setColour (Label::textColourId, colours::text);
        setColour (Slider::textBoxTextColourId, colours::text);
        setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
        setColour (Slider::textBoxHighlightColourId, colours::line);
        setColour (TextEditor::backgroundColourId, colours::panel);
        setColour (TextEditor::textColourId, colours::text);
        setColour (TextEditor::highlightColourId, colours::line);
        setColour (TextEditor::highlightedTextColourId, colours::text);
        setColour (TextEditor::outlineColourId, colours::line);
        setColour (TextEditor::focusedOutlineColourId, colours::dim);
        setColour (CaretComponent::caretColourId, colours::accent);
        setColour (TextButton::textColourOffId, colours::text);
        setColour (TextButton::textColourOnId, colours::background);
        setColour (ToggleButton::textColourId, colours::text);
        setColour (ComboBox::textColourId, colours::text);
        setColour (ComboBox::backgroundColourId, colours::panel);
        setColour (ComboBox::outlineColourId, colours::line);
        setColour (ComboBox::arrowColourId, colours::dim);
        setColour (PopupMenu::backgroundColourId, colours::panel);
        setColour (PopupMenu::textColourId, colours::text);
        setColour (PopupMenu::highlightedBackgroundColourId, colours::line);
        setColour (PopupMenu::highlightedTextColourId, colours::text);
        setColour (TooltipWindow::backgroundColourId, colours::panel);
        setColour (TooltipWindow::textColourId, colours::text);
        setColour (TooltipWindow::outlineColourId, colours::line);
        setColour (ScrollBar::thumbColourId, colours::line);
    }

    //==========================================================================
    // Potard : arc fin, valeur en rouge-orange, repere blanc.
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float position, float startAngle, float endAngle, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (8.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float angle = startAngle + position * (endAngle - startAngle);
        const juce::PathStrokeType stroke (metrics::stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        const float alpha = slider.isEnabled() ? 1.0f : 0.35f;

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
        g.setColour (colours::line);
        g.strokePath (track, stroke);

        juce::Path value;
        value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
        g.setColour (colours::accent.withMultipliedAlpha (alpha));
        g.strokePath (value, stroke);

        // Repere : un trait fin du centre vers l'arc.
        const juce::Point<float> tip (centre.x + (radius - 6.0f) * std::sin (angle), centre.y - (radius - 6.0f) * std::cos (angle));
        const juce::Point<float> base (centre.x + radius * 0.45f * std::sin (angle), centre.y - radius * 0.45f * std::cos (angle));
        g.setColour (colours::text.withMultipliedAlpha (alpha));
        g.drawLine ({ base, tip }, metrics::stroke);
    }

    // Curseur droit : piste fine, valeur en rouge-orange, poignee = un trait blanc.
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                           float maxSliderPos, juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        const bool horizontal = style == juce::Slider::LinearHorizontal;
        if (! horizontal && style != juce::Slider::LinearVertical)
        {
            juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
            return;
        }

        const float alpha = slider.isEnabled() ? 1.0f : 0.35f;
        const auto area = juce::Rectangle<int> (x, y, width, height).toFloat();
        const float half = metrics::stroke * 0.5f;
        g.setColour (colours::line);
        if (horizontal)
        {
            const float cy = area.getCentreY();
            g.fillRect (juce::Rectangle<float> (area.getX(), cy - half, area.getWidth(), metrics::stroke));
            g.setColour (colours::accent.withMultipliedAlpha (alpha));
            g.fillRect (juce::Rectangle<float> (area.getX(), cy - half, sliderPos - area.getX(), metrics::stroke));
            g.setColour (colours::text.withMultipliedAlpha (alpha));
            g.fillRect (juce::Rectangle<float> (sliderPos - half, cy - 7.0f, metrics::stroke, 14.0f));
        }
        else
        {
            const float cx = area.getCentreX();
            g.fillRect (juce::Rectangle<float> (cx - half, area.getY(), metrics::stroke, area.getHeight()));
            g.setColour (colours::accent.withMultipliedAlpha (alpha));
            g.fillRect (juce::Rectangle<float> (cx - half, sliderPos, metrics::stroke, area.getBottom() - sliderPos));
            g.setColour (colours::text.withMultipliedAlpha (alpha));
            g.fillRect (juce::Rectangle<float> (cx - 7.0f, sliderPos - half, 14.0f, metrics::stroke));
        }
    }

    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* box = juce::LookAndFeel_V4::createSliderTextBox (slider);
        box->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        box->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        return box;
    }

    //==========================================================================
    // Bouton : cadre fin. Allume ou enfonce : plein rouge-orange, texte sombre.
    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&, bool highlighted, bool down) override
    {
        const auto area = button.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = button.getToggleState() || down;
        const float alpha = button.isEnabled() ? 1.0f : 0.35f;
        if (on)
        {
            g.setColour (colours::accent.withMultipliedAlpha (alpha));
            g.fillRoundedRectangle (area, metrics::corner);
            return;
        }
        g.setColour (highlighted ? colours::line : colours::panel);
        g.fillRoundedRectangle (area, metrics::corner);
        g.setColour ((highlighted ? colours::dim : colours::line).withMultipliedAlpha (alpha));
        g.drawRoundedRectangle (area, metrics::corner, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override { return caps (10.0f); }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool down) override
    {
        const bool on = button.getToggleState() || down;
        g.setFont (caps (10.0f));
        g.setColour ((on ? colours::background : colours::text).withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.35f));
        g.drawText (upper (button.getButtonText()), button.getLocalBounds().reduced (6, 0), juce::Justification::centred, true);
    }

    // Case : un carre. Allumee : plein rouge-orange.
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool) override
    {
        const float alpha = button.isEnabled() ? 1.0f : 0.35f;
        auto area = button.getLocalBounds();
        const auto box = area.removeFromLeft (14).withSizeKeepingCentre (14, 14).toFloat().reduced (0.5f);
        if (button.getToggleState())
        {
            g.setColour (colours::accent.withMultipliedAlpha (alpha));
            g.fillRoundedRectangle (box, 2.0f);
        }
        else
        {
            g.setColour ((highlighted ? colours::dim : colours::line).withMultipliedAlpha (alpha));
            g.drawRoundedRectangle (box, 2.0f, 1.5f);
        }
        g.setColour (colours::text.withMultipliedAlpha (alpha));
        g.setFont (mono (13.0f));
        g.drawText (button.getButtonText(), area.withTrimmedLeft (8), juce::Justification::centredLeft, true);
    }

    //==========================================================================
    // Menu deroulant : cadre ardoise, chevron fin.
    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        const auto area = juce::Rectangle<int> (width, height).toFloat().reduced (0.5f);
        g.setColour (colours::panel);
        g.fillRoundedRectangle (area, metrics::corner);
        g.setColour (box.hasKeyboardFocus (true) || box.isPopupActive() ? colours::dim : colours::line);
        g.drawRoundedRectangle (area, metrics::corner, 1.0f);

        const float cx = (float) width - 14.0f, cy = (float) height * 0.5f;
        juce::Path chevron;
        chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
        chevron.lineTo (cx, cy + 2.0f);
        chevron.lineTo (cx + 4.0f, cy - 2.0f);
        g.setColour (colours::dim.withMultipliedAlpha (box.isEnabled() ? 1.0f : 0.35f));
        g.strokePath (chevron, juce::PathStrokeType (1.5f));
    }

    // Coin de redimensionnement : trois traits discrets au lieu du blanc de JUCE.
    void drawCornerResizer (juce::Graphics& g, int w, int h, bool over, bool dragging) override
    {
        g.setColour (over || dragging ? colours::dim : colours::line);
        for (int i = 1; i <= 3; ++i)
        {
            const float d = (float) i * 4.0f;
            g.drawLine ((float) w - d, (float) h - 1.0f, (float) w - 1.0f, (float) h - d, 1.0f);
        }
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return mono (13.0f); }
    juce::Font getPopupMenuFont() override { return mono (13.0f); }
    juce::Font getLabelFont (juce::Label&) override { return mono (13.0f); }
};

//==============================================================================
/** Potard complet : libelle en capitales au-dessus, valeur dessous, info-bulle. A relier au parametre avec un
    SliderAttachment sur `slider` (c'est lui qui regle le double-clic = valeur par defaut).
    Taille conseillee : metrics::knobWidth x metrics::knobHeight. */
class Knob : public juce::Component
{
public:
    Knob (const juce::String& labelText, const juce::String& tooltip, const juce::String& suffix = {})
        : label (upper (labelText))
    {
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, metrics::knobWidth, 18);
        slider.setTextValueSuffix (suffix);
        slider.setTooltip (tooltip);
        addAndMakeVisible (slider);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (colours::dim);
        g.setFont (caps (9.0f));
        g.drawText (label, getLocalBounds().removeFromTop (14), juce::Justification::centred, true);
    }

    void resized() override { slider.setBounds (getLocalBounds().withTrimmedTop (14)); }

    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };

private:
    juce::String label;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
};

//==============================================================================
/** Base du visuel anime d'un plugin : cadre ardoise, 30 images par seconde, a l'arret quand il est cache.
    Le plugin derive et dessine dans draw(). Regles : traits fins, blanc et gris pour la matiere, rouge-orange
    pour ce qui bouge ou joue (tete de lecture, crete). Aucune lecture bloquante : valeurs par std::atomic. */
class Visual : public juce::Component, private juce::Timer
{
public:
    Visual() { setOpaque (false); }

    /** area : interieur du cadre. seconds : temps ecoule depuis l'affichage (0 a la capture). */
    virtual void draw (juce::Graphics& g, juce::Rectangle<float> area, double seconds) = 0;

    void paint (juce::Graphics& g) override
    {
        g.setColour (colours::panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), metrics::corner);
        juce::Graphics::ScopedSaveState state (g);
        const auto inner = getLocalBounds().reduced (1);
        g.reduceClipRegion (inner);
        draw (g, inner.toFloat(), seconds);
    }

    void visibilityChanged() override { updateTimer(); }
    void parentHierarchyChanged() override { updateTimer(); }

private:
    void updateTimer()
    {
        if (isShowing())
            startTimerHz (30);
        else
            stopTimer();
    }

    void timerCallback() override
    {
        seconds += 1.0 / 30.0;
        repaint();
    }

    double seconds = 0.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Visual)
};
} // namespace droz::ui
