// Habillage de Droz Master : droz::ui (monochrome) + une seule couleur a lui, la "nebuleuse".
#pragma once

#include <droz/ui.h>

namespace master::ui
{
/** La couleur propre au plugin : lavande de nebuleuse. Seule couleur hors noir et blanc. */
inline const juce::Colour nebuleuse { 0xff9d8cff };

class Look : public droz::ui::LookAndFeel
{
public:
    Look()
    {
        using namespace droz::ui;
        setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::outlineColourId, colours::line);
        setColour (juce::ComboBox::textColourId, colours::text);
        setColour (juce::ComboBox::arrowColourId, colours::dim);
        setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        setColour (juce::TextButton::buttonOnColourId, nebuleuse.withAlpha (0.22f));
        setColour (juce::TextButton::textColourOffId, colours::dim);
        setColour (juce::TextButton::textColourOnId, colours::text);
        setColour (juce::ToggleButton::textColourId, colours::text);
        setColour (juce::ToggleButton::tickColourId, nebuleuse);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool survol, bool) override
    {
        using namespace droz::ui;
        auto r = b.getLocalBounds().toFloat();
        const auto capsule = r.removeFromLeft (30.0f).withSizeKeepingCentre (26.0f, 14.0f);
        const bool on = b.getToggleState();
        g.setColour (on ? nebuleuse.withAlpha (0.35f) : colours::panel);
        g.fillRoundedRectangle (capsule, 7.0f);
        g.setColour (on ? nebuleuse : (survol ? colours::dim : colours::line));
        g.drawRoundedRectangle (capsule, 7.0f, 1.0f);
        const float x = on ? capsule.getRight() - 12.0f : capsule.getX() + 2.0f;
        g.setColour (on ? colours::text : colours::dim);
        g.fillEllipse (x, capsule.getY() + 2.0f, 10.0f, 10.0f);
        if (b.getButtonText().isNotEmpty())
        {
            g.setColour (on ? colours::text : colours::dim);
            g.setFont (caps (10.5f));
            g.drawText (upper (b.getButtonText()), r.reduced (4.0f, 0.0f), juce::Justification::centredLeft);
        }
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool survol, bool) override
    {
        using namespace droz::ui;
        const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        if (b.getToggleState())
        {
            g.setColour (nebuleuse.withAlpha (0.20f));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (nebuleuse);
        }
        else
            g.setColour (survol ? colours::dim : colours::line);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override { return droz::ui::caps (10.0f); }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace droz::ui;
        const auto r = juce::Rectangle<float> (0, 0, static_cast<float> (w), static_cast<float> (h)).reduced (1.0f);
        g.setColour (box.isMouseOver (true) ? colours::dim : colours::line);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        juce::Path fleche;
        const float cx = r.getRight() - 10.0f, cy = r.getCentreY();
        fleche.addTriangle (cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
        g.setColour (colours::dim);
        g.fillPath (fleche);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (12.5f)); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (4, 1, box.getWidth() - 20, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }
};

/** Fond cosmique : degrade profond, nebuleuse, champ d'etoiles. Seme : toujours le meme ciel. */
inline juce::Image dessineCosmos (int w, int h, float echelle)
{
    using namespace droz::ui;
    juce::Image img (juce::Image::ARGB, std::max (1, juce::roundToInt (w * echelle)), std::max (1, juce::roundToInt (h * echelle)), true);
    juce::Graphics g (img);
    g.addTransform (juce::AffineTransform::scale (echelle));
    const float W = static_cast<float> (w), H = static_cast<float> (h);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff07060f), 0, 0, colours::background, 0, H, false));
    g.fillAll();

    // Nebuleuse : nappes douces de la couleur du plugin.
    struct Nappe { float x, y, r, a; };
    const Nappe nappes[] = { { 0.16f, 0.30f, 0.50f, 0.24f }, { 0.80f, 0.16f, 0.42f, 0.16f }, { 0.60f, 0.62f, 0.55f, 0.12f }, { 0.28f, 0.88f, 0.32f, 0.10f } };
    for (const auto& n : nappes)
    {
        const float rr = n.r * std::max (W, H);
        juce::ColourGradient grad (nebuleuse.withAlpha (n.a), n.x * W, n.y * H, nebuleuse.withAlpha (0.0f), n.x * W + rr, n.y * H, true);
        g.setGradientFill (grad);
        g.fillEllipse (n.x * W - rr, n.y * H - rr, 2 * rr, 2 * rr);
    }

    // Etoiles.
    juce::Random rnd (20261127);
    const int nb = static_cast<int> (W * H / 2200.0f);
    for (int i = 0; i < nb; ++i)
    {
        const float x = rnd.nextFloat() * W, y = rnd.nextFloat() * H;
        const float t = rnd.nextFloat();
        const float taille = 0.6f + 1.4f * t * t * t;
        g.setColour (juce::Colours::white.withAlpha (0.15f + 0.75f * rnd.nextFloat() * (0.4f + 0.6f * t)));
        g.fillEllipse (x - taille * 0.5f, y - taille * 0.5f, taille, taille);
        if (t > 0.93f)
        {
            // Quelques etoiles plus brillantes, avec un halo et une croix fine.
            juce::ColourGradient halo (juce::Colours::white.withAlpha (0.25f), x, y, juce::Colours::white.withAlpha (0.0f), x + 7.0f, y, true);
            g.setGradientFill (halo);
            g.fillEllipse (x - 7.0f, y - 7.0f, 14.0f, 14.0f);
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.drawLine (x - 6.0f, y, x + 6.0f, y, 0.6f);
            g.drawLine (x, y - 6.0f, x, y + 6.0f, 0.6f);
        }
    }
    return img;
}
} // namespace master::ui
