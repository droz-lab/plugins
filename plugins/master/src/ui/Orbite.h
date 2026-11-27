// La chaine en orbite : chaque module est une planete. On glisse une planete pour changer l'ordre,
// on clique pour l'afficher, on clique sur son point pour l'allumer ou l'eteindre.
#pragma once

#include "../PluginProcessor.h"
#include "Habillage.h"

namespace master::ui
{
class Orbite : public juce::Component, public juce::TooltipClient
{
public:
    explicit Orbite (PluginProcessor& p) : proc (p) { litOrdre(); }

    std::function<void (int)> quandSelection;

    void rafraichit()
    {
        if (glisse < 0)
            litOrdre();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace droz::ui;
        const auto b = getLocalBounds().toFloat();
        const float cy = b.getCentreY() + 4.0f;

        // L'orbite : un arc tres ouvert, du signal d'entree a la sortie.
        juce::Path arc;
        arc.startNewSubPath (b.getX() + 10.0f, cy + 10.0f);
        arc.quadraticTo (b.getCentreX(), cy - 26.0f, b.getRight() - 10.0f, cy + 10.0f);
        g.setColour (colours::line.brighter (0.2f));
        g.strokePath (arc, juce::PathStrokeType (1.0f), {});
        g.setColour (colours::dim);
        g.setFont (caps (9.0f));
        g.drawText ("IN", juce::Rectangle<float> (b.getX(), cy + 14.0f, 40.0f, 14.0f), juce::Justification::centredLeft);
        g.drawText ("OUT", juce::Rectangle<float> (b.getRight() - 40.0f, cy + 14.0f, 40.0f, 14.0f), juce::Justification::centredRight);

        for (int i = 0; i < nbModules; ++i)
        {
            const int m = vue[static_cast<size_t> (i)];
            if (m == glisse)
                continue;
            dessinePlanete (g, m, centre (i));
        }
        if (glisse >= 0)
            dessinePlanete (g, glisse, { xGlisse, centre (0).y - 6.0f });
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = planeteSous (e.position);
        if (i < 0)
            return;
        const int m = vue[static_cast<size_t> (i)];
        if (e.position.getDistanceFrom (point (centre (i))) < 9.0f)
        {
            basculeActif (m);
            return;
        }
        proc.selection = m;
        if (quandSelection)
            quandSelection (m);
        glisse = m;
        departGlisse = e.position.x;
        xGlisse = centre (i).x;
        decalage = e.position.x - xGlisse;
        bouge = false;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (glisse < 0)
            return;
        if (std::abs (e.position.x - departGlisse) > 4.0f)
            bouge = true;
        if (! bouge)
            return;
        xGlisse = e.position.x - decalage;
        // Place la planete a l'emplacement le plus proche.
        int cible = 0;
        float mieux = 1.0e9f;
        for (int i = 0; i < nbModules; ++i)
        {
            const float d = std::abs (centre (i).x - xGlisse);
            if (d < mieux)
            {
                mieux = d;
                cible = i;
            }
        }
        auto it = std::find (vue.begin(), vue.end(), glisse);
        const int actuel = static_cast<int> (it - vue.begin());
        if (cible != actuel)
        {
            vue.erase (it);
            vue.insert (vue.begin() + cible, glisse);
        }
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (glisse >= 0 && bouge)
        {
            PluginProcessor::Ordre o;
            std::copy (vue.begin(), vue.end(), o.begin());
            proc.demandeOrdre (o);
        }
        glisse = -1;
        rafraichit();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int i = planeteSous (e.position);
        if (i >= 0)
            basculeActif (vue[static_cast<size_t> (i)]);
    }

    juce::String getTooltip() override
    {
        const int i = planeteSous (getMouseXYRelative().toFloat());
        if (i < 0)
            return T ("La chaîne, de l'entrée (gauche) à la sortie (droite). Glisser une planète pour changer l'ordre.");
        const auto& d = modules()[static_cast<size_t> (vue[static_cast<size_t> (i)])];
        return d.nom + " : " + d.aide + T (" Clic : afficher. Glisser : déplacer. Point ou double-clic : allumer / éteindre.");
    }

private:
    void litOrdre()
    {
        const auto o = proc.ordre();
        vue.assign (o.begin(), o.end());
    }

    juce::Point<float> centre (int i) const
    {
        const auto b = getLocalBounds().toFloat().reduced (50.0f, 0.0f);
        const float t = (static_cast<float> (i) + 0.5f) / static_cast<float> (nbModules);
        const float x = b.getX() + t * b.getWidth();
        // Suit l'arc de l'orbite.
        const float u = 2.0f * t - 1.0f;
        const float y = b.getCentreY() + 4.0f - 8.0f * (1.0f - u * u);
        return { x, y };
    }

    float rayon() const { return juce::jlimit (18.0f, 34.0f, std::min (getHeight() * 0.30f, static_cast<float> (getWidth()) / (static_cast<float> (nbModules) * 3.2f))); }
    juce::Point<float> point (juce::Point<float> c) const { return c.translated (rayon() * 0.72f, -rayon() * 0.72f); }

    int planeteSous (juce::Point<float> pos) const
    {
        for (int i = 0; i < nbModules; ++i)
            if (pos.getDistanceFrom (centre (i)) < rayon() + 6.0f)
                return i;
        return -1;
    }

    bool actif (int m) const { return proc.apvts.getRawParameterValue (idActif (m))->load() > 0.5f; }

    void basculeActif (int m)
    {
        if (auto* p = proc.apvts.getParameter (idActif (m)))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (actif (m) ? 0.0f : 1.0f);
            p->endChangeGesture();
        }
        repaint();
    }

    void dessinePlanete (juce::Graphics& g, int m, juce::Point<float> c)
    {
        using namespace droz::ui;
        const float r = rayon();
        const bool on = actif (m);
        const bool choisi = proc.selection.load() == m;
        const float act = on ? juce::jlimit (0.0f, 1.0f, proc.module (m).activite.load() / 12.0f) : 0.0f;

        // Halo d'activite : plus le module travaille, plus la planete rayonne.
        if (act > 0.01f)
        {
            juce::ColourGradient halo (nebuleuse.withAlpha (0.55f * act), c.x, c.y, nebuleuse.withAlpha (0.0f), c.x + r * 2.0f, c.y, true);
            g.setGradientFill (halo);
            g.fillEllipse (c.x - r * 2.0f, c.y - r * 2.0f, r * 4.0f, r * 4.0f);
        }

        // Corps : sphere eclairee en haut a gauche.
        juce::ColourGradient corps (on ? juce::Colour (0xff2a2838) : juce::Colour (0xff15151b), c.x - r * 0.4f, c.y - r * 0.5f,
                                    juce::Colour (0xff050508), c.x + r, c.y + r, true);
        g.setGradientFill (corps);
        g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
        g.setColour (choisi ? nebuleuse : (on ? colours::text.withAlpha (0.8f) : colours::line.brighter (0.3f)));
        g.drawEllipse (c.x - r, c.y - r, 2 * r, 2 * r, choisi ? 2.0f : 1.0f);
        if (choisi)
        {
            // Anneau de selection, incline comme celui d'une planete.
            g.setColour (nebuleuse.withAlpha (0.7f));
            juce::Path anneau;
            anneau.addEllipse (c.x - r * 1.6f, c.y - r * 0.38f, r * 3.2f, r * 0.76f);
            anneau.applyTransform (juce::AffineTransform::rotation (-0.25f, c.x, c.y));
            g.strokePath (anneau, juce::PathStrokeType (1.0f));
        }

        const auto& d = modules()[static_cast<size_t> (m)];
        g.setColour (on ? colours::text : colours::dim);
        g.setFont (caps (r > 26.0f ? 10.0f : 8.5f));
        g.drawText (d.court, juce::Rectangle<float> (c.x - r * 1.5f, c.y - 7.0f, r * 3.0f, 14.0f), juce::Justification::centred);

        // Mode en petit sous la planete.
        static const char* modes[] = { "ST", "M/S", "G/D", "T/T" };
        const int mode = juce::jlimit (0, 3, juce::roundToInt (proc.apvts.getRawParameterValue (idMode (m))->load()));
        g.setColour (colours::dim);
        g.setFont (caps (8.5f));
        g.drawText (modes[mode], juce::Rectangle<float> (c.x - 30.0f, c.y + r + 3.0f, 60.0f, 12.0f), juce::Justification::centred);

        // Point d'allumage.
        const auto p = point (c);
        g.setColour (juce::Colour (0xff050508));
        g.fillEllipse (p.x - 6.0f, p.y - 6.0f, 12.0f, 12.0f);
        g.setColour (on ? nebuleuse : colours::dim);
        if (on)
            g.fillEllipse (p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f);
        else
            g.drawEllipse (p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f, 1.0f);
    }

    PluginProcessor& proc;
    std::vector<int> vue = std::vector<int> (nbModules);
    int glisse = -1;
    float xGlisse = 0, departGlisse = 0, decalage = 0;
    bool bouge = false;
};
} // namespace master::ui
