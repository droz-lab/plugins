#include "PluginEditor.h"

namespace
{
// Texte accentue : toujours passer par fromUTF8, jamais une chaine brute dans juce::String.
juce::String T (const char* utf8) { return juce::String::fromUTF8 (utf8); }

constexpr float rayonPoint = 5.0f;
constexpr float rayonPrise = 9.0f;
constexpr float taillePoignee = 8.0f;
constexpr size_t maxHistorique = 100;
} // namespace

std::unique_ptr<PluginEditor::Etat> PluginEditor::pressePapier;

//==============================================================================
juce::Colour palette::onglet (int t)
{
    static const juce::uint32 couleurs[nombreOnglets] = { 0xffeea63a, 0xff45c7bb, 0xffef6f6c, 0xffa98bff,
                                                          0xff9ad65b, 0xff5aa9f0, 0xffef7fc0, 0xffe8d44d };
    return juce::Colour (couleurs[juce::jlimit (0, nombreOnglets - 1, t)]);
}

void dessinerInsecte (juce::Graphics& g, int t, juce::Rectangle<float> r, juce::Colour c)
{
    // Insectes dessines a la main, en traits simples (originaux).
    const auto centre = r.getCentre();
    const float s = juce::jmin (r.getWidth(), r.getHeight());
    const auto rel = [&] (float x, float y) { return juce::Point<float> (centre.x + x * s, centre.y + y * s); };
    const auto ovale = [&] (float x, float y, float w, float h) { return juce::Rectangle<float> (w * s, h * s).withCentre (rel (x, y)); };
    const auto trait = [&] (float x1, float y1, float x2, float y2, float e = 1.2f)
    { g.drawLine ({ rel (x1, y1), rel (x2, y2) }, e); };
    g.setColour (c);

    switch (t)
    {
        case 0: // fourmi
            g.fillEllipse (ovale (-0.3f, 0.0f, 0.22f, 0.18f));
            g.fillEllipse (ovale (0.0f, 0.0f, 0.18f, 0.14f));
            g.fillEllipse (ovale (0.3f, 0.02f, 0.3f, 0.22f));
            for (float x : { -0.06f, 0.0f, 0.06f })
            {
                trait (x, 0.0f, x * 3.0f - 0.08f, -0.28f);
                trait (x, 0.0f, x * 3.0f - 0.08f, 0.28f);
            }
            trait (-0.38f, -0.05f, -0.5f, -0.25f);
            trait (-0.36f, -0.06f, -0.42f, -0.3f);
            break;
        case 1: // abeille
            g.setColour (c.withAlpha (0.45f));
            g.fillEllipse (ovale (-0.08f, -0.22f, 0.3f, 0.22f));
            g.fillEllipse (ovale (0.12f, -0.22f, 0.3f, 0.22f));
            g.setColour (c);
            g.fillEllipse (ovale (0.05f, 0.05f, 0.62f, 0.36f));
            g.setColour (palette::panneau);
            for (float x : { -0.05f, 0.08f, 0.21f })
                trait (x, -0.08f, x, 0.18f, 2.0f);
            g.setColour (c);
            g.fillEllipse (ovale (-0.33f, 0.03f, 0.2f, 0.2f));
            break;
        case 2: // coccinelle
            g.fillEllipse (ovale (0.0f, -0.33f, 0.24f, 0.16f));
            g.fillEllipse (ovale (0.0f, 0.04f, 0.62f, 0.62f));
            g.setColour (palette::panneau);
            trait (0.0f, -0.24f, 0.0f, 0.34f, 1.5f);
            for (auto p : { juce::Point<float> (-0.15f, -0.08f), juce::Point<float> (0.15f, -0.08f),
                            juce::Point<float> (-0.14f, 0.16f), juce::Point<float> (0.14f, 0.16f) })
                g.fillEllipse (ovale (p.x, p.y, 0.1f, 0.1f));
            break;
        case 3: // libellule
            g.setColour (c.withAlpha (0.5f));
            for (float y : { -0.12f, 0.12f })
            {
                g.fillEllipse (ovale (-0.12f, y, 0.16f, 0.5f));
                g.fillEllipse (ovale (0.08f, y, 0.14f, 0.44f));
            }
            g.setColour (c);
            trait (-0.4f, 0.0f, 0.45f, 0.0f, 2.2f);
            g.fillEllipse (ovale (-0.42f, 0.0f, 0.16f, 0.16f));
            break;
        case 4: // papillon
            g.fillEllipse (ovale (-0.18f, -0.14f, 0.34f, 0.36f));
            g.fillEllipse (ovale (0.18f, -0.14f, 0.34f, 0.36f));
            g.fillEllipse (ovale (-0.13f, 0.18f, 0.24f, 0.26f));
            g.fillEllipse (ovale (0.13f, 0.18f, 0.24f, 0.26f));
            g.setColour (palette::panneau);
            trait (0.0f, -0.28f, 0.0f, 0.32f, 2.4f);
            g.setColour (c);
            trait (0.0f, -0.28f, -0.12f, -0.44f);
            trait (0.0f, -0.28f, 0.12f, -0.44f);
            break;
        case 5: // scarabee
            for (float y : { -0.1f, 0.08f, 0.24f })
            {
                trait (0.0f, y, -0.34f, y + 0.08f);
                trait (0.0f, y, 0.34f, y + 0.08f);
            }
            g.fillEllipse (ovale (0.0f, 0.1f, 0.48f, 0.58f));
            g.fillEllipse (ovale (0.0f, -0.28f, 0.26f, 0.18f));
            trait (0.0f, -0.34f, 0.0f, -0.46f, 2.0f);
            g.setColour (palette::panneau);
            trait (0.0f, -0.16f, 0.0f, 0.38f, 1.2f);
            break;
        case 6: // sauterelle
            g.fillEllipse (ovale (0.02f, 0.0f, 0.62f, 0.18f));
            g.fillEllipse (ovale (-0.32f, -0.02f, 0.16f, 0.16f));
            trait (0.12f, 0.02f, 0.38f, -0.24f, 2.0f);
            trait (0.38f, -0.24f, 0.48f, 0.22f, 1.6f);
            trait (-0.1f, 0.06f, -0.16f, 0.24f);
            trait (0.0f, 0.06f, -0.02f, 0.24f);
            trait (-0.36f, -0.08f, -0.2f, -0.42f);
            break;
        default: // luciole
        {
            juce::ColourGradient lueur (c.withAlpha (0.7f), rel (0.2f, 0.08f), c.withAlpha (0.0f), rel (0.2f, 0.42f), true);
            g.setGradientFill (lueur);
            g.fillEllipse (ovale (0.2f, 0.08f, 0.66f, 0.66f));
            g.setColour (c);
            g.fillEllipse (ovale (0.2f, 0.08f, 0.24f, 0.2f));
            g.fillEllipse (ovale (-0.08f, 0.0f, 0.34f, 0.2f));
            g.fillEllipse (ovale (-0.3f, -0.02f, 0.14f, 0.14f));
            g.setColour (c.withAlpha (0.5f));
            g.fillEllipse (ovale (-0.08f, -0.14f, 0.26f, 0.14f));
            break;
        }
    }
}

//==============================================================================
Style::Style()
{
    setColour (juce::Slider::textBoxTextColourId, palette::texte);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::backgroundColourId, palette::grilleForte);
    setColour (juce::Slider::thumbColourId, palette::texte);
    setColour (juce::Label::textColourId, palette::texte);
    setColour (juce::ComboBox::backgroundColourId, palette::panneauClair);
    setColour (juce::ComboBox::outlineColourId, palette::grilleForte);
    setColour (juce::ComboBox::textColourId, palette::texte);
    setColour (juce::TextButton::buttonColourId, palette::panneauClair);
    setColour (juce::TextButton::textColourOffId, palette::texte);
    setColour (juce::TextButton::textColourOnId, palette::fond);
    setColour (juce::PopupMenu::backgroundColourId, palette::panneauClair);
    setColour (juce::PopupMenu::textColourId, palette::texte);
    setColour (juce::TooltipWindow::backgroundColourId, palette::panneauClair.brighter (0.1f));
    setColour (juce::TooltipWindow::textColourId, palette::texte);
    setColour (juce::TooltipWindow::outlineColourId, palette::grilleForte);
    setColour (juce::AlertWindow::backgroundColourId, palette::panneau);
    setColour (juce::AlertWindow::textColourId, palette::texte);
    setColour (juce::TextEditor::backgroundColourId, palette::panneauClair);
    setColour (juce::TextEditor::textColourId, palette::texte);
    setAccent (palette::onglet (0));
}

void Style::setAccent (juce::Colour c)
{
    setColour (juce::Slider::trackColourId, c);
    setColour (juce::ComboBox::arrowColourId, c);
    setColour (juce::TextButton::buttonOnColourId, c);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, c.withAlpha (0.3f));
}

void Style::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                              juce::Slider::SliderStyle, juce::Slider& s)
{
    const float cy = static_cast<float> (y) + static_cast<float> (h) * 0.5f;
    const auto piste = juce::Rectangle<float> (static_cast<float> (x), cy - 2.0f, static_cast<float> (w), 4.0f);
    g.setColour (s.findColour (juce::Slider::backgroundColourId));
    g.fillRoundedRectangle (piste, 2.0f);
    g.setColour (s.findColour (juce::Slider::trackColourId).withAlpha (s.isEnabled() ? 1.0f : 0.35f));
    g.fillRoundedRectangle (piste.withRight (pos), 2.0f);
    g.setColour (s.findColour (juce::Slider::thumbColourId).withAlpha (s.isEnabled() ? 1.0f : 0.4f));
    g.fillEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre ({ pos, cy }));
}

void Style::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool survol, bool appui)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool allume = b.getToggleState();
    auto c = allume ? b.findColour (juce::TextButton::buttonOnColourId) : b.findColour (juce::TextButton::buttonColourId);
    if (survol)
        c = c.brighter (0.12f);
    if (appui)
        c = c.darker (0.15f);
    if (! b.isEnabled())
        c = c.withAlpha (0.4f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (allume ? c : palette::grilleForte);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

//==============================================================================
BoutonOnglet::BoutonOnglet (int i) : juce::Button ("Onglet " + juce::String (i + 1)), index (i)
{
    setClickingTogglesState (false);
}

void BoutonOnglet::setInfos (bool a, int c, bool e, bool v, float val)
{
    if (a != actif || c != cc || e != evolution || v != visible || std::abs (val - valeur) > 0.004f)
    {
        actif = a;
        cc = c;
        evolution = e;
        visible = v;
        valeur = val;
        repaint();
    }
}

void LogoDroz::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float h = r.getHeight();
    // Quelques etoiles.
    g.setColour (palette::texteDoux.withAlpha (0.8f));
    for (auto p : { juce::Point<float> (0.02f, 0.15f), juce::Point<float> (0.47f, 0.08f), juce::Point<float> (0.93f, 0.2f),
                    juce::Point<float> (0.6f, 0.9f), juce::Point<float> (0.33f, 0.95f) })
        g.fillEllipse (juce::Rectangle<float> (2.0f, 2.0f).withCentre ({ r.getX() + p.x * r.getWidth(), r.getY() + p.y * h }));

    juce::Font gras (juce::FontOptions (h * 0.78f, juce::Font::bold));
    g.setFont (gras);
    g.setColour (couleur);
    float x = r.getX() + 6.0f;
    const auto lettre = [&] (const juce::String& l)
    {
        const float w = juce::GlyphArrangement::getStringWidth (gras, l);
        g.drawText (l, juce::Rectangle<float> (x, r.getY(), w + 2.0f, h), juce::Justification::centredLeft);
        x += w;
    };
    lettre ("DR");
    // Le O : une planete avec son anneau.
    const float d = h * 0.56f;
    x += d * 0.25f;
    const juce::Point<float> c (x + d * 0.5f, r.getCentreY());
    g.fillEllipse (juce::Rectangle<float> (d, d).withCentre (c));
    g.setColour (palette::fond);
    g.fillEllipse (juce::Rectangle<float> (d * 0.32f, d * 0.32f).withCentre (c.translated (-d * 0.12f, -d * 0.1f)));
    g.setColour (couleur);
    juce::Path anneau;
    anneau.addCentredArc (c.x, c.y, d * 0.95f, d * 0.28f, -0.35f, 0.0f, juce::MathConstants<float>::twoPi, true);
    g.strokePath (anneau, juce::PathStrokeType (1.6f));
    x += d * 1.25f;
    lettre ("Z");

    // COURBES, plus fin, avec un petit insecte qui marche sur une vague.
    x += 10.0f;
    juce::Font fin (juce::FontOptions (h * 0.46f));
    g.setFont (fin);
    g.setColour (palette::texte);
    const juce::String mot ("COURBES");
    const float wMot = juce::GlyphArrangement::getStringWidth (fin, mot) + 8.0f;
    g.drawText (mot, juce::Rectangle<float> (x, r.getY(), wMot, h * 0.62f), juce::Justification::bottomLeft);
    juce::Path vague;
    const float y0 = r.getY() + h * 0.82f;
    vague.startNewSubPath (x, y0);
    for (int i = 1; i <= 30; ++i)
    {
        const float u = static_cast<float> (i) / 30.0f;
        vague.lineTo (x + u * wMot, y0 - std::sin (u * 6.2831853f) * h * 0.06f);
    }
    g.setColour (couleur.withAlpha (0.7f));
    g.strokePath (vague, juce::PathStrokeType (1.2f));
    dessinerInsecte (g, 0, juce::Rectangle<float> (h * 0.42f, h * 0.42f).withCentre ({ x + wMot * 0.78f, y0 - h * 0.14f }), couleur);
}

BoutonFleche::BoutonFleche (bool versLaDroite) : juce::Button (versLaDroite ? "Retablir" : "Annuler"), droite (versLaDroite) {}

void BoutonFleche::paintButton (juce::Graphics& g, bool survol, bool appui)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    auto fond = palette::panneauClair;
    if (survol)
        fond = fond.brighter (0.12f);
    if (appui)
        fond = fond.darker (0.15f);
    g.setColour (isEnabled() ? fond : fond.withAlpha (0.4f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (palette::grilleForte);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    // Arc avec une pointe, vers la gauche (annuler) ou la droite (retablir).
    const auto c = r.getCentre();
    const float rayon = juce::jmin (r.getWidth(), r.getHeight()) * 0.26f;
    const float sens = droite ? 1.0f : -1.0f;
    juce::Path arc;
    arc.addCentredArc (c.x, c.y + rayon * 0.35f, rayon, rayon, 0.0f, -sens * 1.9f, sens * 0.9f, true);
    g.setColour (isEnabled() ? palette::texte : palette::texteDoux.withAlpha (0.5f));
    g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const auto bout = arc.getPointAlongPath (0.0f);
    juce::Path pointe;
    pointe.addTriangle (bout.x - sens * 5.0f, bout.y, bout.x + sens * 2.0f, bout.y - 5.0f, bout.x + sens * 2.0f, bout.y + 5.0f);
    g.fillPath (pointe);
}

void BoutonOnglet::paintButton (juce::Graphics& g, bool survol, bool)
{
    const auto couleur = palette::onglet (index);
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool choisi = getToggleState();
    g.setColour (choisi ? couleur.withAlpha (0.22f) : (survol ? palette::panneauClair : palette::panneau));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (choisi ? couleur : palette::grilleForte);
    g.drawRoundedRectangle (r, 7.0f, choisi ? 2.0f : 1.0f);

    auto contenu = r.reduced (6.0f, 4.0f);
    contenu.removeFromRight (22.0f); // colonne V / S / M
    contenu.removeFromBottom (6.0f); // mini-barre
    const auto icone = contenu.removeFromLeft (contenu.getHeight());
    dessinerInsecte (g, index, icone, actif ? couleur : couleur.withAlpha (0.35f));
    if (! visible)
    {
        g.setColour (palette::texteDoux);
        g.drawLine (icone.getX(), icone.getBottom(), icone.getRight(), icone.getY(), 1.5f); // barre : cache dans l'affichage multicourbe
    }
    contenu.removeFromLeft (6.0f);
    g.setColour (actif ? palette::texte : palette::texteDoux);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText (juce::String (index + 1), contenu.removeFromTop (contenu.getHeight() * 0.55f), juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (11.0f));
    g.setColour (palette::texteDoux);
    g.drawText (actif ? "CC " + juce::String (cc) : T ("muet"), contenu, juce::Justification::centredLeft);
    if (valeur >= 0.0f)
    {
        // Mini-barre : la valeur envoyee en ce moment.
        auto barre = r.reduced (8.0f, 0.0f).removeFromBottom (5.0f).translated (0.0f, -3.0f);
        barre.setRight (getWidth() - 26.0f);
        g.setColour (palette::grilleForte);
        g.fillRoundedRectangle (barre, 1.5f);
        g.setColour (couleur);
        g.fillRoundedRectangle (barre.withWidth (barre.getWidth() * valeur), 1.5f);
    }
    if (evolution && actif)
    {
        g.setColour (couleur);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ r.getRight() - 32.0f, r.getY() + 9.0f }));
    }
}

//==============================================================================
void MiniVue::setCourbe (const courbes::Courbe* c, juce::Colour col, const juce::String& t)
{
    vide = c == nullptr;
    if (c != nullptr)
        courbe = *c;
    couleur = col;
    titre = t;
    repaint();
}

void MiniVue::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (palette::panneauClair);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (palette::texteDoux);
    g.setFont (juce::FontOptions (11.0f));
    if (titre.isNotEmpty())
        g.drawText (titre, r.removeFromTop (15.0f).reduced (6.0f, 0.0f), juce::Justification::centredLeft);
    const auto z = r.reduced (6.0f, 4.0f);
    if (vide)
    {
        g.drawText ("-", z, juce::Justification::centred);
        return;
    }
    juce::Path p;
    const int n = juce::jmax (2, juce::roundToInt (z.getWidth()));
    for (int i = 0; i <= n; ++i)
    {
        const float x = static_cast<float> (i) / static_cast<float> (n);
        const juce::Point<float> e { z.getX() + x * z.getWidth(), z.getBottom() - courbe.valeur (juce::jmin (x, 0.99999f)) * z.getHeight() };
        if (i == 0)
            p.startNewSubPath (e);
        else
            p.lineTo (e);
    }
    g.setColour (couleur);
    g.strokePath (p, juce::PathStrokeType (1.6f));
}

//==============================================================================
VueCourbe::VueCourbe()
{
    setWantsKeyboardFocus (true);
    setTooltip (T ("Glisser un point : le déplacer. Glisser sur une ligne : la courber. Glisser dans le vide : sélectionner.\n"
                   "Sélection : glisser pour déplacer, tirer les poignées du cadre pour étirer. Suppr : effacer. Ctrl+A : tout.\n"
                   "Double-clic : ajouter ou supprimer un point. Clic droit sur un point : forme du segment. Maj : sans aimant."));
    calques[0].visible = calques[0].modifiable = true;
}

void VueCourbe::setCourbe (int t, const courbes::Courbe& c)
{
    calques[static_cast<size_t> (t)].courbe = c;
    survole = saisi = {};
    repaint();
}

void VueCourbe::setCalque (int t, bool visible, bool modifiable)
{
    auto& c = calques[static_cast<size_t> (t)];
    if (c.visible == visible && c.modifiable == (visible && modifiable))
        return;
    c.visible = visible;
    c.modifiable = visible && modifiable;
    if (! c.modifiable)
        for (int i = 0; i < c.courbe.taille(); ++i)
            c.courbe.point (i).choisi = false;
    repaint();
}

void VueCourbe::setActif (int t)
{
    actif = juce::jlimit (0, nombreOnglets - 1, t);
    repaint();
}

void VueCourbe::setJouee (const courbes::Courbe* c)
{
    afficheJouee = c != nullptr;
    if (c != nullptr)
        jouee = *c;
    repaint();
}

void VueCourbe::setLecture (int t, float phase)
{
    calques[static_cast<size_t> (t)].lecture = phase;
    repaint();
}

void VueCourbe::setGrille (double c, double parTemps, double parMesure)
{
    cases = juce::jlimit (1.0, 512.0, c);
    casesParTemps = juce::jmax (1.0e-3, parTemps);
    casesParMesure = juce::jmax (1.0e-3, parMesure);
    repaint();
}

int VueCourbe::nombreChoisis() const noexcept
{
    int n = 0;
    for (const auto& c : calques)
        for (int i = 0; i < c.courbe.taille(); ++i)
            n += c.courbe.point (i).choisi ? 1 : 0;
    return n;
}

uint32_t VueCourbe::masqueChoisis() const noexcept
{
    uint32_t m = 0;
    for (int t = 0; t < nombreOnglets; ++t)
    {
        const auto& c = calques[static_cast<size_t> (t)].courbe;
        for (int i = 0; i < c.taille(); ++i)
            if (c.point (i).choisi)
                m |= 1u << t;
    }
    return m;
}

void VueCourbe::toutDeselectionner()
{
    for (auto& c : calques)
        for (int i = 0; i < c.courbe.taille(); ++i)
            c.courbe.point (i).choisi = false;
}

juce::Rectangle<float> VueCourbe::zone() const
{
    return getLocalBounds().toFloat().reduced (14.0f, 16.0f);
}

juce::Point<float> VueCourbe::versEcran (float x, float y) const
{
    const auto z = zone();
    return { z.getX() + x * z.getWidth(), z.getBottom() - y * z.getHeight() };
}

juce::Point<float> VueCourbe::versCourbe (juce::Point<float> e) const
{
    const auto z = zone();
    return { (e.x - z.getX()) / z.getWidth(), (z.getBottom() - e.y) / z.getHeight() };
}

float VueCourbe::aimanterX (float x, const juce::ModifierKeys& mods) const
{
    if (aimant == mods.isShiftDown())
        return x;
    const float n = static_cast<float> (cases);
    return std::round (x * n) / n;
}

float VueCourbe::aimanterY (float y, const juce::ModifierKeys& mods) const
{
    if (aimant == mods.isShiftDown())
        return y;
    return std::round (y * 16.0f) / 16.0f;
}

VueCourbe::Ref VueCourbe::pointSous (juce::Point<float> e) const
{
    Ref meilleur;
    float distance = rayonPrise;
    // L'onglet principal passe en premier a distance egale.
    for (int k = 0; k < nombreOnglets; ++k)
    {
        const int t = (actif + k) % nombreOnglets;
        const auto& c = calques[static_cast<size_t> (t)];
        if (! c.modifiable)
            continue;
        for (int i = 0; i < c.courbe.taille(); ++i)
        {
            const auto& p = c.courbe.point (i);
            const float d = versEcran (p.x, p.y).getDistanceFrom (e);
            if (d < distance - (meilleur.valide() ? 0.01f : 0.0f))
            {
                distance = d;
                meilleur = { t, i };
            }
        }
    }
    return meilleur;
}

int VueCourbe::segmentSous (const courbes::Courbe& c, float x)
{
    int i = c.taille() - 1;
    for (int k = 0; k < c.taille(); ++k)
        if (c.point (k).x > x)
        {
            i = k - 1;
            break;
        }
    return i < 0 ? c.taille() - 1 : i;
}

int VueCourbe::courbeSous (juce::Point<float> e) const
{
    const float x = juce::jlimit (0.0f, 0.99999f, versCourbe (e).x);
    int meilleur = -1;
    float distance = 7.0f;
    for (int k = 0; k < nombreOnglets; ++k)
    {
        const int t = (actif + k) % nombreOnglets;
        const auto& c = calques[static_cast<size_t> (t)];
        if (! c.modifiable || c.courbe.taille() < 2)
            continue;
        const float d = std::abs (versEcran (x, c.courbe.valeur (x)).y - e.y);
        if (d < distance)
        {
            distance = d;
            meilleur = t;
        }
    }
    return meilleur;
}

juce::Rectangle<float> VueCourbe::boiteSelection (const std::array<Calque, nombreOnglets>& cs) const
{
    float x0 = 2.0f, x1 = -1.0f, y0 = 2.0f, y1 = -1.0f;
    for (const auto& c : cs)
        for (int i = 0; i < c.courbe.taille(); ++i)
            if (c.courbe.point (i).choisi)
            {
                const auto& p = c.courbe.point (i);
                x0 = juce::jmin (x0, p.x);
                x1 = juce::jmax (x1, p.x);
                y0 = juce::jmin (y0, p.y);
                y1 = juce::jmax (y1, p.y);
            }
    if (x1 < x0)
        return {};
    return { x0, y0, x1 - x0, y1 - y0 };
}

juce::Rectangle<float> VueCourbe::boiteEcran() const
{
    if (nombreChoisis() < 2)
        return {};
    const auto b = boiteSelection (calques);
    const auto a = versEcran (b.getX(), b.getBottom()), c = versEcran (b.getRight(), b.getY());
    return juce::Rectangle<float> (a, c).expanded (8.0f);
}

int VueCourbe::poigneeSous (juce::Point<float> e) const
{
    const auto b = boiteEcran();
    if (b.isEmpty() || ! b.expanded (taillePoignee).contains (e))
        return aucune;
    int p = aucune;
    if (std::abs (e.x - b.getX()) < taillePoignee) p |= gauche;
    if (std::abs (e.x - b.getRight()) < taillePoignee) p |= droite;
    if (std::abs (e.y - b.getY()) < taillePoignee) p |= haut;
    if (std::abs (e.y - b.getBottom()) < taillePoignee) p |= bas;
    return p;
}

void VueCourbe::debut()
{
    if (! dejaNote && onDebutModif)
        onDebutModif();
    dejaNote = true;
}

void VueCourbe::modifie (uint32_t masque)
{
    repaint();
    if (onModif && masque != 0)
        onModif (masque);
}

void VueCourbe::supprimerSelection()
{
    const auto masque = masqueChoisis();
    if (masque == 0)
        return;
    debut();
    for (auto& c : calques)
        for (int i = c.courbe.taille(); --i >= 0;)
            if (c.courbe.point (i).choisi && c.courbe.taille() > 2)
                c.courbe.retirer (i);
    dejaNote = false;
    modifie (masque);
}

juce::Path VueCourbe::chemin (const courbes::Courbe& c) const
{
    juce::Path trait;
    const auto z = zone();
    const int largeur = juce::jmax (2, juce::roundToInt (z.getWidth()));
    for (int px = 0; px <= largeur; ++px)
    {
        const float x = static_cast<float> (px) / static_cast<float> (largeur);
        const auto e = versEcran (x, c.valeur (juce::jmin (x, 0.99999f)));
        if (px == 0)
            trait.startNewSubPath (e);
        else
            trait.lineTo (e);
    }
    return trait;
}

void VueCourbe::paint (juce::Graphics& g)
{
    const auto z = zone();
    const auto couleur = palette::onglet (actif);
    g.setColour (palette::panneau);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);

    // Grille de l'onglet principal : cases fines, temps plus marques, mesures encore plus.
    const int nb = juce::jmin (256, static_cast<int> (std::ceil (cases - 1.0e-6)));
    for (int i = 0; i <= nb; ++i)
    {
        const double k = static_cast<double> (i);
        if (k > cases + 1.0e-6)
            break;
        const auto multiple = [k] (double m) { const double q = k / m; return std::abs (q - std::round (q)) < 1.0e-4; };
        const float x = z.getX() + z.getWidth() * static_cast<float> (k / cases);
        g.setColour (multiple (casesParMesure) ? palette::grilleMesure : multiple (casesParTemps) ? palette::grilleForte : palette::grille);
        g.drawVerticalLine (juce::roundToInt (x), z.getY(), z.getBottom());
    }
    for (int i = 0; i <= 4; ++i)
    {
        const float y = z.getY() + z.getHeight() * static_cast<float> (i) / 4.0f;
        g.setColour (i % 2 == 0 ? palette::grilleForte : palette::grille);
        g.drawHorizontalLine (juce::roundToInt (y), z.getX(), z.getRight());
    }

    // Autres courbes visibles (affichage multicourbe) : traits fins dans leur couleur.
    for (int t = 0; t < nombreOnglets; ++t)
    {
        const auto& c = calques[static_cast<size_t> (t)];
        if (t == actif || ! c.visible)
            continue;
        const auto col = palette::onglet (t);
        g.setColour (col.withAlpha (0.75f));
        g.strokePath (chemin (c.courbe), juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const auto tete = versEcran (c.lecture, c.courbe.valeur (c.lecture));
        if (insectes)
            dessinerInsecte (g, t, juce::Rectangle<float> (22.0f, 22.0f).withCentre (tete.translated (0.0f, -9.0f)), col);
        else
            g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (tete));
    }

    // Courbe principale, remplie.
    const auto& principale = calques[static_cast<size_t> (actif)].courbe;
    const auto trait = chemin (principale);
    juce::Path remplissage (trait);
    remplissage.lineTo (z.getRight(), z.getBottom());
    remplissage.lineTo (z.getX(), z.getBottom());
    remplissage.closeSubPath();
    g.setGradientFill (juce::ColourGradient (couleur.withAlpha (afficheJouee ? 0.12f : 0.3f), z.getX(), z.getY(),
                                             couleur.withAlpha (0.02f), z.getX(), z.getBottom(), false));
    g.fillPath (remplissage);
    g.setColour (couleur.withAlpha (afficheJouee ? 0.45f : 1.0f));
    g.strokePath (trait, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (afficheJouee)
    {
        g.setColour (palette::lecture.withAlpha (0.9f));
        g.strokePath (chemin (jouee), juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Tete de lecture de l'onglet principal.
    const float lecture = calques[static_cast<size_t> (actif)].lecture;
    const auto& lue = afficheJouee ? jouee : principale;
    const auto tete = versEcran (lecture, lue.valeur (lecture));
    g.setColour (palette::lecture.withAlpha (0.35f));
    g.drawVerticalLine (juce::roundToInt (tete.x), z.getY(), z.getBottom());
    if (insectes)
        dessinerInsecte (g, actif, juce::Rectangle<float> (28.0f, 28.0f).withCentre (tete.translated (0.0f, -12.0f)), palette::lecture);
    else
    {
        g.setColour (palette::lecture);
        g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (tete));
    }

    // Points des courbes modifiables (le principal par-dessus).
    for (int k = nombreOnglets; --k >= 0;)
    {
        const int t = (actif + k) % nombreOnglets;
        const auto& c = calques[static_cast<size_t> (t)];
        if (! c.modifiable)
            continue;
        const auto col = palette::onglet (t);
        const float base = t == actif ? rayonPoint : rayonPoint - 1.0f;
        for (int i = 0; i < c.courbe.taille(); ++i)
        {
            const auto& p = c.courbe.point (i);
            const auto e = versEcran (p.x, p.y);
            const bool actifPoint = (survole.t == t && survole.i == i) || (saisi.t == t && saisi.i == i);
            const float r = actifPoint ? base + 2.0f : base;
            g.setColour (p.choisi ? col : palette::fond);
            g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (e));
            g.setColour (actifPoint ? palette::texte : col);
            if (p.type == courbes::Segment::marche)
                g.drawRect (juce::Rectangle<float> (r * 1.8f, r * 1.8f).withCentre (e), 2.0f);
            else
                g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (e), 2.0f);
        }
    }

    // Cadre de selection avec poignees.
    const auto b = boiteEcran();
    if (! b.isEmpty())
    {
        g.setColour (palette::texte.withAlpha (0.6f));
        const float tirets[] = { 4.0f, 3.0f };
        g.drawDashedLine ({ b.getTopLeft(), b.getTopRight() }, tirets, 2);
        g.drawDashedLine ({ b.getBottomLeft(), b.getBottomRight() }, tirets, 2);
        g.drawDashedLine ({ b.getTopLeft(), b.getBottomLeft() }, tirets, 2);
        g.drawDashedLine ({ b.getTopRight(), b.getBottomRight() }, tirets, 2);
        g.setColour (palette::texte);
        for (auto p : { b.getTopLeft(), b.getTopRight(), b.getBottomLeft(), b.getBottomRight(),
                        juce::Point<float> (b.getCentreX(), b.getY()), juce::Point<float> (b.getCentreX(), b.getBottom()),
                        juce::Point<float> (b.getX(), b.getCentreY()), juce::Point<float> (b.getRight(), b.getCentreY()) })
            g.fillRect (juce::Rectangle<float> (taillePoignee, taillePoignee).withCentre (p));
    }
    if (mode == Mode::dessin && trace.size() > 1)
    {
        juce::Path p;
        p.startNewSubPath (versEcran (trace.front().x, trace.front().y));
        for (const auto& q : trace)
            p.lineTo (versEcran (q.x, q.y));
        g.setColour (palette::lecture);
        g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    if (mode == Mode::cadre)
    {
        g.setColour (palette::texte.withAlpha (0.08f));
        g.fillRect (cadre);
        g.setColour (palette::texte.withAlpha (0.6f));
        g.drawRect (cadre, 1.0f);
    }
}

void VueCourbe::mouseMove (const juce::MouseEvent& e)
{
    const auto s = pointSous (e.position);
    if (s.t != survole.t || s.i != survole.i)
    {
        survole = s;
        repaint();
    }
    const int p = poigneeSous (e.position);
    if ((p & (gauche | droite)) && (p & (haut | bas)))
        setMouseCursor (juce::MouseCursor::TopLeftCornerResizeCursor);
    else if (p & (gauche | droite))
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    else if (p & (haut | bas))
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    else if (crayon)
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
    else if (s.valide())
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    else if (courbeSous (e.position) >= 0)
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    else
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

void VueCourbe::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    dejaNote = false;
    debutSouris = e.position;
    saisi = pointSous (e.position);

    if (e.mods.isPopupMenu())
    {
        if (saisi.valide())
            menuPoint (saisi);
        return;
    }

    if (crayon)
    {
        debut();
        const auto c = versCourbe (e.position);
        trace = { { juce::jlimit (0.0f, 0.9999f, c.x), juce::jlimit (0.0f, 1.0f, c.y) } };
        mode = Mode::dessin;
        return;
    }

    poignee = poigneeSous (e.position);
    if (poignee != aucune && ! saisi.valide())
    {
        depart = calques;
        mode = Mode::etirer;
        return;
    }

    if (saisi.valide())
    {
        auto& p = calques[static_cast<size_t> (saisi.t)].courbe.point (saisi.i);
        if (e.mods.isCommandDown())
            p.choisi = ! p.choisi;
        else if (! p.choisi)
        {
            toutDeselectionner();
            p.choisi = true;
        }
        mode = Mode::deplacer;
    }
    else if (nombreChoisis() >= 2 && boiteEcran().contains (e.position) && courbeSous (e.position) < 0)
    {
        mode = Mode::deplacer; // glisser dans le cadre : deplace toute la selection
    }
    else if (const int t = courbeSous (e.position); t >= 0)
    {
        const auto& c = calques[static_cast<size_t> (t)].courbe;
        calqueTension = t;
        segmentTension = segmentSous (c, juce::jlimit (0.0f, 0.9999f, versCourbe (e.position).x));
        tensionDepart = c.point (segmentTension).tension;
        mode = Mode::tension;
    }
    else
    {
        if (! e.mods.isShiftDown() && ! e.mods.isCommandDown())
            toutDeselectionner();
        cadre = { e.position, e.position };
        mode = Mode::cadre;
    }
    depart = calques;
    repaint();
}

void VueCourbe::mouseDrag (const juce::MouseEvent& e)
{
    switch (mode)
    {
        case Mode::rien: return;

        case Mode::dessin:
        {
            const auto c = versCourbe (e.position);
            trace.push_back ({ juce::jlimit (0.0f, 0.9999f, c.x), juce::jlimit (0.0f, 1.0f, c.y) });
            repaint();
            return;
        }

        case Mode::cadre:
        {
            cadre = juce::Rectangle<float> (debutSouris, e.position);
            calques = depart;
            for (auto& c : calques)
            {
                if (! c.modifiable)
                    continue;
                for (int i = 0; i < c.courbe.taille(); ++i)
                {
                    auto& p = c.courbe.point (i);
                    if (cadre.contains (versEcran (p.x, p.y)))
                        p.choisi = true;
                }
            }
            repaint();
            return;
        }

        case Mode::deplacer:
        {
            if (e.getDistanceFromDragStart() < 2)
                return;
            debut();
            auto delta = versCourbe (e.position) - versCourbe (debutSouris);
            if (saisi.valide())
            {
                // L'aimant s'applique au point tenu ; les autres suivent du meme ecart.
                const auto& tenu = depart[static_cast<size_t> (saisi.t)].courbe.point (saisi.i);
                delta.x = aimanterX (tenu.x + delta.x, e.mods) - tenu.x;
                delta.y = aimanterY (tenu.y + delta.y, e.mods) - tenu.y;
            }
            calques = depart;
            for (auto& c : calques)
            {
                for (int i = 0; i < c.courbe.taille(); ++i)
                {
                    auto& p = c.courbe.point (i);
                    if (! p.choisi)
                        continue;
                    p.x = juce::jlimit (0.0f, 0.9999f, p.x + delta.x);
                    p.y = juce::jlimit (0.0f, 1.0f, p.y + delta.y);
                }
                c.courbe.trier();
            }
            survole = {};
            modifie (masqueChoisis());
            return;
        }

        case Mode::etirer:
        {
            debut();
            const auto b = boiteSelection (depart);
            const auto souris = versCourbe (e.position);
            float x0 = b.getX(), x1 = b.getRight(), y0 = b.getY(), y1 = b.getBottom();
            if (poignee & gauche) x0 = juce::jmin (aimanterX (souris.x, e.mods), x1 - 1.0e-3f);
            if (poignee & droite) x1 = juce::jmax (aimanterX (souris.x, e.mods), x0 + 1.0e-3f);
            if (poignee & bas) y0 = aimanterY (souris.y, e.mods);
            if (poignee & haut) y1 = aimanterY (souris.y, e.mods);
            calques = depart;
            for (auto& c : calques)
            {
                for (int i = 0; i < c.courbe.taille(); ++i)
                {
                    auto& p = c.courbe.point (i);
                    if (! p.choisi)
                        continue;
                    const float u = b.getWidth() > 1.0e-6f ? (p.x - b.getX()) / b.getWidth() : 0.0f;
                    const float v = b.getHeight() > 1.0e-6f ? (p.y - b.getY()) / b.getHeight() : 0.5f;
                    p.x = juce::jlimit (0.0f, 0.9999f, x0 + u * (x1 - x0));
                    p.y = juce::jlimit (0.0f, 1.0f, y0 + v * (y1 - y0));
                }
                c.courbe.trier();
            }
            modifie (masqueChoisis());
            return;
        }

        case Mode::tension:
        {
            if (calqueTension < 0)
                return;
            auto& c = calques[static_cast<size_t> (calqueTension)].courbe;
            if (segmentTension < 0 || segmentTension >= c.taille())
                return;
            debut();
            auto& p = c.point (segmentTension);
            // Le sens suit la pente : tirer vers le haut bombe toujours la courbe vers le haut.
            const auto& suivantPoint = c.point ((segmentTension + 1) % c.taille());
            const float sens = suivantPoint.y >= p.y ? 1.0f : -1.0f;
            const float dy = static_cast<float> (e.getDistanceFromDragStartY()) / 150.0f;
            p.tension = juce::jlimit (-1.0f, 1.0f, tensionDepart + dy * sens);
            modifie (1u << calqueTension);
            return;
        }
    }
}

void VueCourbe::mouseUp (const juce::MouseEvent& e)
{
    if (mode == Mode::dessin)
    {
        appliquerTrace();
        trace.clear();
        mode = Mode::rien;
        dejaNote = false;
        repaint();
        return;
    }
    // Clic simple sur un point deja choisi, sans glisser : il devient le seul choisi.
    if (mode == Mode::deplacer && saisi.valide() && e.getDistanceFromDragStart() < 2 && ! e.mods.isCommandDown())
    {
        toutDeselectionner();
        calques[static_cast<size_t> (saisi.t)].courbe.point (saisi.i).choisi = true;
    }
    mode = Mode::rien;
    saisi = {};
    calqueTension = segmentTension = -1;
    dejaNote = false;
    repaint();
}

namespace
{
/** Douglas-Peucker : garde les points qui s'ecartent de plus de `tolerance` de la ligne simplifiee. */
std::vector<juce::Point<float>> simplifier (const std::vector<juce::Point<float>>& p, float tolerance)
{
    const size_t n = p.size();
    if (n < 3)
        return p;
    std::vector<bool> garde (n, false);
    garde.front() = garde.back() = true;
    std::vector<std::pair<size_t, size_t>> pile { { 0, n - 1 } };
    while (! pile.empty())
    {
        const auto [a, b] = pile.back();
        pile.pop_back();
        float pire = 0.0f;
        size_t indexPire = a;
        const juce::Line<float> ligne (p[a], p[b]);
        for (size_t i = a + 1; i < b; ++i)
        {
            juce::Point<float> proche;
            const float d = ligne.getDistanceFromPoint (p[i], proche);
            if (d > pire)
            {
                pire = d;
                indexPire = i;
            }
        }
        if (pire > tolerance)
        {
            garde[indexPire] = true;
            pile.push_back ({ a, indexPire });
            pile.push_back ({ indexPire, b });
        }
    }
    std::vector<juce::Point<float>> r;
    for (size_t i = 0; i < n; ++i)
        if (garde[i])
            r.push_back (p[i]);
    return r;
}
} // namespace

/** Le trait du crayon remplace la courbe sur la zone dessinee, avec le moins de points possible. */
void VueCourbe::appliquerTrace()
{
    if (trace.size() < 2)
        return;
    // Une valeur par petite tranche de temps (la derniere dessinee gagne) : le trait devient une fonction du temps.
    constexpr int tranches = 512;
    std::array<float, tranches> y {};
    std::array<bool, tranches> vu {};
    for (size_t i = 1; i < trace.size(); ++i)
    {
        const auto a = trace[i - 1], b = trace[i];
        const int ia = juce::jlimit (0, tranches - 1, static_cast<int> (a.x * tranches));
        const int ib = juce::jlimit (0, tranches - 1, static_cast<int> (b.x * tranches));
        const int pas = ib >= ia ? 1 : -1;
        for (int k = ia;; k += pas)
        {
            const float u = ia == ib ? 1.0f : static_cast<float> (k - ia) / static_cast<float> (ib - ia);
            y[static_cast<size_t> (k)] = a.y + (b.y - a.y) * u;
            vu[static_cast<size_t> (k)] = true;
            if (k == ib)
                break;
        }
    }
    std::vector<juce::Point<float>> points;
    for (int k = 0; k < tranches; ++k)
        if (vu[static_cast<size_t> (k)])
            points.push_back ({ static_cast<float> (k) / tranches, y[static_cast<size_t> (k)] });
    if (points.size() < 2)
        return;
    const float x0 = points.front().x, x1 = points.back().x;

    auto& c = calques[static_cast<size_t> (actif)].courbe;
    int gardes = 0;
    for (int i = 0; i < c.taille(); ++i)
        if (c.point (i).x < x0 || c.point (i).x > x1)
            ++gardes;
    // Simplification : on monte la tolerance jusqu'a tenir dans la limite de points.
    std::vector<juce::Point<float>> simples;
    for (float tolerance = 0.008f;; tolerance *= 1.5f)
    {
        simples = simplifier (points, tolerance);
        if (gardes + static_cast<int> (simples.size()) <= courbes::maxPoints || tolerance > 0.5f)
            break;
    }
    for (int i = c.taille(); --i >= 0;)
        if (c.point (i).x >= x0 && c.point (i).x <= x1)
            c.retirer (i);
    for (const auto& q : simples)
        c.ajouter (q.x, q.y);
    c.trier();
    modifie (1u << actif);
}

void VueCourbe::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (crayon)
        return;
    const auto s = pointSous (e.position);
    if (s.valide())
    {
        auto& c = calques[static_cast<size_t> (s.t)].courbe;
        if (c.point (s.i).choisi && nombreChoisis() > 1)
        {
            supprimerSelection();
            return;
        }
        if (c.taille() > 2)
        {
            debut();
            c.retirer (s.i);
            survole = {};
            dejaNote = false;
            modifie (1u << s.t);
        }
        return;
    }
    const auto p = versCourbe (e.position);
    if (p.x < 0.0f || p.x > 1.0f)
        return;
    // Le point s'ajoute a la courbe sous la souris, sinon a l'onglet principal.
    const int cible = courbeSous (e.position) >= 0 ? courbeSous (e.position) : actif;
    auto& c = calques[static_cast<size_t> (cible)].courbe;
    const float x = aimanterX (p.x, e.mods), y = aimanterY (p.y, e.mods);
    const auto type = c.taille() > 0 ? c.point (segmentSous (c, x)).type : courbes::Segment::courbe;
    debut();
    if (c.ajouter (x, y, 0.0f, type))
    {
        c.trier();
        dejaNote = false;
        modifie (1u << cible);
    }
}

bool VueCourbe::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey)
    {
        supprimerSelection();
        return true;
    }
    if (k == juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 0))
    {
        for (auto& c : calques)
            if (c.modifiable)
                for (int i = 0; i < c.courbe.taille(); ++i)
                    c.courbe.point (i).choisi = true;
        repaint();
        return true;
    }
    if (k == juce::KeyPress::escapeKey)
    {
        toutDeselectionner();
        repaint();
        return true;
    }
    return false; // Annuler / Retablir : remonte a l'editeur
}

void VueCourbe::menuPoint (Ref r)
{
    const auto type = calques[static_cast<size_t> (r.t)].courbe.point (r.i).type;
    juce::PopupMenu m;
    m.addSectionHeader (T ("Segment qui part de ce point"));
    m.addItem (1, T ("Courbe"), true, type == courbes::Segment::courbe);
    m.addItem (2, T ("En S"), true, type == courbes::Segment::s);
    m.addItem (3, T ("Marche (tient puis saute)"), true, type == courbes::Segment::marche);
    m.addItem (4, T ("Remettre droit"), type != courbes::Segment::marche);
    m.addSeparator();
    m.addItem (5, T ("Supprimer le point"), calques[static_cast<size_t> (r.t)].courbe.taille() > 2);
    if (nombreChoisis() > 1)
        m.addItem (6, T ("Appliquer la forme à toute la sélection"), true);

    juce::Component::SafePointer<VueCourbe> moi (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [moi, r] (int choix)
                     {
                         if (moi == nullptr || choix == 0)
                             return;
                         auto& c = moi->calques[static_cast<size_t> (r.t)].courbe;
                         if (r.i >= c.taille())
                             return;
                         moi->dejaNote = false;
                         moi->debut();
                         auto masque = 1u << r.t;
                         auto& p = c.point (r.i);
                         if (choix == 1) p.type = courbes::Segment::courbe;
                         if (choix == 2) p.type = courbes::Segment::s;
                         if (choix == 3) p.type = courbes::Segment::marche;
                         if (choix == 4) p.tension = 0.0f;
                         if (choix == 6)
                         {
                             const auto modele = p;
                             for (auto& autre : moi->calques)
                                 for (int i = 0; i < autre.courbe.taille(); ++i)
                                     if (autre.courbe.point (i).choisi)
                                     {
                                         autre.courbe.point (i).type = modele.type;
                                         autre.courbe.point (i).tension = modele.tension;
                                     }
                             masque |= moi->masqueChoisis();
                         }
                         if (choix == 5) c.retirer (r.i);
                         moi->dejaNote = false;
                         moi->saisi = moi->survole = {};
                         moi->modifie (masque);
                     });
}

//==============================================================================
PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p)
{
    setLookAndFeel (&style);
    setWantsKeyboardFocus (true);

    titre.setText ("DROZ COURBES", juce::dontSendNotification); // remplace par le logo dessine
    titre.setVisible (false);
    titre.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    version.setText ("v" JucePlugin_VersionString, juce::dontSendNotification);
    version.setFont (juce::FontOptions (12.0f));
    version.setColour (juce::Label::textColourId, palette::texteDoux);
    addAndMakeVisible (version);
    preparerBascule (boutonInsectes, T ("Insectes"), T ("Les insectes se promènent sur leur courbe pendant la lecture. Éteint : un simple point."));
    boutonInsectes.setToggleState (processor.insectes.load(), juce::dontSendNotification);
    boutonInsectes.onClick = [this]
    {
        processor.insectes = boutonInsectes.getToggleState();
        vue.setInsectes (processor.insectes.load());
    };
    vue.setInsectes (processor.insectes.load());
    preparerBascule (boutonPanneau, T ("Évolution"), T ("Afficher ou masquer le panneau Évolution."));
    boutonPanneau.setToggleState (processor.panneauEvolution.load(), juce::dontSendNotification);
    boutonPanneau.onClick = [this]
    {
        processor.panneauEvolution = boutonPanneau.getToggleState();
        resized();
        repaint();
    };

    for (int t = 0; t < nombreOnglets; ++t)
    {
        auto* b = onglets.add (new BoutonOnglet (t));
        b->setTooltip (T ("Onglet ") + juce::String (t + 1) + T (" : une courbe et son CC. Le point coloré indique que l'évolution est allumée."));
        b->onClick = [this, t] { lierOnglet (t); };
        addAndMakeVisible (b);
    }

    // Formes et familles.
    for (int f = 0; f < courbes::nombreFamilles; ++f)
        familles.addItem (T (courbes::nomFamille (f)), f + 1);
    familles.onChange = [this]
    {
        const int f = familles.getSelectedId() - 1;
        if (f >= 0 && f != famille)
            choisirMembre (f, 0);
    };
    addAndMakeVisible (familles);
    precedent.setTooltip (T ("Forme précédente de la famille"));
    suivant.setTooltip (T ("Forme suivante de la famille"));
    precedent.onClick = [this] { choisirMembre (famille, (membre < 0 ? 0 : membre + courbes::membresParFamille - 1) % courbes::membresParFamille); };
    suivant.onClick = [this] { choisirMembre (famille, (membre < 0 ? 0 : membre + 1) % courbes::membresParFamille); };
    addAndMakeVisible (precedent);
    addAndMakeVisible (suivant);
    preparerBouton (mesCourbes, T ("Mes courbes"), T ("Enregistrer la courbe pour la retrouver dans tous tes projets, ou en recharger une."),
                    [this] { menuMesCourbes(); });
    preparerBouton (relancer, T ("Relancer"), T ("Seulement pour les familles Hasard et Gates : nouveau tirage au sort avec les mêmes réglages."), [this] { generer (false); });
    preparerBouton (boutonGenerer, T ("Générer"), T ("Invente une nouvelle forme dans cette famille : tous les réglages au hasard."), [this] { generer (true); });
    nomMembre.setColour (juce::Label::textColourId, palette::texteDoux);
    addAndMakeVisible (nomMembre);

    for (auto& r : reglagesFamille)
    {
        r.slider.setRange (0.0, 1.0);
        r.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        r.slider.setDoubleClickReturnValue (true, 0.5);
        r.slider.onDragStart = [this] { memoriser(); };
        r.slider.onValueChange = [this] { regenerer(); };
        addAndMakeVisible (r.slider);
        preparerNom (r.nom, {});
    }

    // Outils.
    boutonAnnuler.setTooltip (T ("Annuler la dernière modification (Ctrl+Z)."));
    boutonAnnuler.onClick = [this] { annuler(); };
    addAndMakeVisible (boutonAnnuler);
    boutonRetablir.setTooltip (T ("Refaire ce qui a été annulé (Ctrl+Maj+Z)."));
    boutonRetablir.onClick = [this] { retablir(); };
    addAndMakeVisible (boutonRetablir);
    preparerBascule (boutonCrayon, T ("Crayon"), T ("Dessine la courbe à main levée. Les points sont ensuite simplifiés automatiquement."));
    boutonCrayon.onClick = [this] { vue.setCrayon (boutonCrayon.getToggleState()); };
    preparerBouton (boutonInverser, T ("Inverser"), T ("Retourne la courbe de haut en bas."),
                    [this] { transformer ([] (courbes::Courbe& c) { courbes::inverser (c); return true; }); });
    preparerBouton (boutonRetourner, T ("Retourner"), T ("Joue la courbe à l'envers (de droite à gauche)."),
                    [this] { transformer ([] (courbes::Courbe& c) { courbes::retourner (c); return true; }); });
    preparerBouton (boutonDoubler, T ("×2"), T ("La forme deux fois plus vite, répétée deux fois."),
                    [this] { transformer ([] (courbes::Courbe& c) { return courbes::doubler (c); }); });
    preparerBouton (boutonMoitie, T ("÷2"), T ("Garde la première moitié de la forme, étirée sur tout le cycle."),
                    [this] { transformer ([] (courbes::Courbe& c) { courbes::moitie (c); return true; }); });
    preparerBouton (boutonCopier, T ("Copier"), T ("Copie la courbe de cet onglet."), [this] { pressePapier = std::make_unique<Etat> (etatActuel()); });
    preparerBouton (boutonColler, T ("Coller"), T ("Colle la courbe copiée dans cet onglet."), [this]
                    {
                        if (pressePapier != nullptr)
                            appliquer (*pressePapier, true);
                    });
    // Visible / Solo / Mute dans chaque onglet.
    for (int t = 0; t < nombreOnglets; ++t)
    {
        auto& v = boutonsVisible[static_cast<size_t> (t)];
        preparerBascule (v, "V", T ("Visible : afficher cette courbe dans « Toutes les courbes »."));
        v.setToggleState (processor.visible[static_cast<size_t> (t)].load(), juce::dontSendNotification);
        v.onClick = [this, t]
        {
            processor.visible[static_cast<size_t> (t)] = boutonsVisible[static_cast<size_t> (t)].getToggleState();
            majCalques();
        };
        auto& so = boutonsSolo[static_cast<size_t> (t)];
        preparerBascule (so, "S", T ("Solo : seuls les onglets en solo envoient leur CC."));
        so.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe8d44d));
        attachesSolo[static_cast<size_t> (t)] = std::make_unique<AttacheBouton> (processor.apvts, PluginProcessor::id (t, "solo"), so);
        auto& mu = boutonsMute[static_cast<size_t> (t)];
        preparerBascule (mu, "M", T ("Mute : cet onglet n'envoie plus son CC."));
        mu.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffef6f6c));
        attachesMute[static_cast<size_t> (t)] = std::make_unique<AttacheBouton> (processor.apvts, PluginProcessor::id (t, "mute"), mu);
        mu.onClick = [this, t]
        {
            // Dé-muter un onglet le rend aussi visible (on peut ensuite le masquer avec V).
            if (! boutonsMute[static_cast<size_t> (t)].getToggleState())
            {
                processor.visible[static_cast<size_t> (t)] = true;
                boutonsVisible[static_cast<size_t> (t)].setToggleState (true, juce::dontSendNotification);
                majCalques();
            }
        };
        for (auto* b : { &v, &so, &mu })
            b->setConnectedEdges (juce::Button::ConnectedOnTop | juce::Button::ConnectedOnBottom);
    }
    preparerBouton (toutDemute, T ("Dé-mute"), T ("Enlève tous les mutes."), [this]
                    {
                        for (int t = 0; t < nombreOnglets; ++t)
                            if (auto* q = processor.param (t, "mute"))
                                q->setValueNotifyingHost (0.0f);
                    });
    preparerBouton (toutDesolo, T ("Dé-solo"), T ("Enlève tous les solos."), [this]
                    {
                        for (int t = 0; t < nombreOnglets; ++t)
                            if (auto* q = processor.param (t, "solo"))
                                q->setValueNotifyingHost (0.0f);
                    });
    addAndMakeVisible (logo);
    preparerBascule (boutonMulti, T ("Toutes les courbes"), T ("Affiche toutes les courbes allumées en même temps. Leurs points se sélectionnent et se déplacent ensemble."));
    boutonMulti.onClick = [this]
    {
        multi = boutonMulti.getToggleState();
        majCalques();
    };

    vue.onDebutModif = [this] { memoriser(); };
    vue.onModif = [this] (uint32_t masque)
    {
        for (int t = 0; t < nombreOnglets; ++t)
        {
            if ((masque & (1u << t)) == 0)
                continue;
            if (t == onglet)
            {
                modifiee = true;
                envoyerCourbe();
            }
            else
            {
                processor.setCourbe (t, vue.getCourbe (t), processor.getFamille (t), -1, processor.getReglage (t));
                versionsCalques[static_cast<size_t> (t)] = processor.getVersionCourbe (t);
                if (! evolutionAllumee (t))
                    processor.setSeed (t, { vue.getCourbe (t), processor.getFamille (t), processor.getReglage (t), {} });
            }
        }
        majTitreMembre();
    };
    addAndMakeVisible (vue);

    aide.setText (T ("Glisser un point : déplacer  ·  Sur la ligne : courber  ·  Dans le vide : sélectionner  ·  Double-clic : ajouter / supprimer  ·  Clic droit : forme  ·  Maj : sans aimant  ·  Crayon : dessiner"),
                  juce::dontSendNotification);
    aide.setFont (juce::FontOptions (12.0f));
    aide.setColour (juce::Label::textColourId, palette::texteDoux);
    addAndMakeVisible (aide);

    // Temps.
    preparerNom (nomLongueur, T ("Longueur"));
    longueur.setTooltip (T ("Durée d'un cycle de la courbe, calée sur le tempo."));
    longueur.setTextBoxStyle (juce::Slider::TextBoxRight, false, 96, 20);
    longueur.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    longueur.textFromValueFunction = [this] (double v)
    {
        if (longueursAutorisees.empty())
            return juce::String();
        const int i = juce::jlimit (0, static_cast<int> (longueursAutorisees.size()) - 1, juce::roundToInt (v));
        return juce::String (PluginProcessor::longueurs()[static_cast<size_t> (longueursAutorisees[static_cast<size_t> (i)])].nom);
    };
    longueur.onValueChange = [this]
    {
        if (majEnCours || attacheLongueur == nullptr || longueursAutorisees.empty())
            return;
        const int i = juce::jlimit (0, static_cast<int> (longueursAutorisees.size()) - 1, juce::roundToInt (longueur.getValue()));
        attacheLongueur->setValueAsCompleteGesture (static_cast<float> (longueursAutorisees[static_cast<size_t> (i)]));
        majGrille();
    };
    addAndMakeVisible (longueur);
    preparerBascule (ternaire, T ("Ternaire"), T ("Inclure les longueurs ternaires (T) dans le slider Longueur."));
    ternaire.onClick = [this] { majLongueurs(); };

    preparerNom (nomGrille, T ("Grille"));
    for (int i = 0; i < static_cast<int> (PluginProcessor::grilles().size()); ++i)
        grille.addItem (PluginProcessor::grilles()[static_cast<size_t> (i)].nom, i + 1);
    grille.setTooltip (T ("Taille d'une case de la grille (aimant, repères, swing)."));
    grille.onChange = [this] { majGrille(); };
    addAndMakeVisible (grille);
    preparerBascule (grilleTernaire, T ("Ternaire"), T ("Grille en ternaire : chaque case dure 2/3 de la valeur choisie."));
    grilleTernaire.onClick = [this] { majGrille(); };
    preparerBascule (boutonAimant, T ("Aimant"), T ("Les points se collent à la grille. Maj enfoncée : l'inverse."));
    boutonAimant.setToggleState (processor.aimant.load(), juce::dontSendNotification);
    boutonAimant.onClick = [this]
    {
        processor.aimant = boutonAimant.getToggleState();
        vue.setAimant (processor.aimant.load());
    };
    vue.setAimant (processor.aimant.load());
    preparerReglette (swing, T ("Swing"), T ("Retarde une case sur deux de la grille, comme le groove d'une boîte à rythmes."), 52);

    preparerNom (nomDepart, T ("Départ"));
    depart.addItemList ({ "Tempo", "Note" }, 1);
    depart.setTooltip (T ("Tempo : la courbe suit la mesure. Note : elle repart du début à chaque note MIDI reçue."));
    addAndMakeVisible (depart);

    // Sortie.
    preparerReglette (decalage, T ("Décalage"), T ("Décale la courbe dans le temps (100 % = un cycle entier)."), 56);
    preparerReglette (minimum, T ("Minimum"), T ("Valeur envoyée quand la courbe est en bas."), 56);
    preparerReglette (maximum, T ("Maximum"), T ("Valeur envoyée quand la courbe est en haut. Plus petit que le minimum : la courbe s'inverse."), 56);
    preparerReglette (lissage, T ("Lissage"), T ("Adoucit les sauts de la courbe (marches, gates)."), 64);

    preparerBascule (ecoute, T ("Écoute"), T ("Son test dont le volume suit la courbe de l'onglet affiché. Éteint = plugin muet."));
    attacheEcoute = std::make_unique<AttacheBouton> (processor.apvts, "ecoute_on", ecoute);
    for (auto* s : { &cc, &canal })
    {
        s->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 40, 22);
        s->setColour (juce::Slider::textBoxOutlineColourId, palette::grilleForte);
        addAndMakeVisible (*s);
    }
    cc.setTooltip (T ("Numéro du contrôleur MIDI envoyé par cet onglet (0 à 127)."));
    canal.setTooltip (T ("Canal MIDI de cet onglet (1 à 16)."));
    preparerNom (nomCc, "CC");
    preparerNom (nomCanal, T ("Canal"));
    lectureCc.setJustificationType (juce::Justification::centredRight);
    lectureCc.setFont (juce::FontOptions (16.0f));
    lectureCc.setTooltip (T ("Dernière valeur envoyée (0 à 127)."));
    addAndMakeVisible (lectureCc);

    // Evolution.
    preparerBascule (evolution, T ("Évolution"), T ("Allumé : à chaque tirage, la courbe peut rester, revenir à ta courbe, varier ou changer de famille."));
    preparerBascule (lie, T ("Lié aux autres"), T ("Allumé : cet onglet suit les réglages communs et bouge avec les autres onglets liés. Éteint : ses propres réglages."));
    preparerReglette (coherence, T ("Cohérence"), T ("À quel point cet onglet suit les décisions communes (même moment, même famille). 100 % = toujours, 0 % = jamais. Chaque onglet garde sa propre variante."), 48);
    preparerBascule (cadenas, T ("Cadenas"), T ("Fige la courbe de cet onglet : l'évolution ne la change plus (les autres onglets continuent)."));
    cadenas.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffef6f6c));
    preparerBascule (seedDepart, T ("Seed à chaque lecture"), T ("Allumé : à chaque fois que tu lances la lecture, l'évolution repart de la seed (même suite à chaque fois)."));
    attacheSeedDepart = std::make_unique<AttacheBouton> (processor.apvts, "seed_depart", seedDepart);
    preparerReglette (fondu, T ("Fondu"), T ("Durée du passage en douceur d'une courbe à la suivante, en % d'un cycle. 0 = changement d'un coup."), 52);
    preparerNom (titreHistorique, T ("Historique (clic : reprendre · Maj+clic : garder comme seed)"));
    for (int i = 0; i < PluginProcessor::Lecture::tailleHistorique; ++i)
    {
        auto& h = historiqueVues[static_cast<size_t> (i)];
        h.setTooltip (T ("Une courbe déjà jouée. Clic : la reprendre. Maj+clic : en faire la seed."));
        h.onClic = [this, i] (const juce::ModifierKeys& mods)
        {
            if (i >= derniereLecture.nbHistorique)
                return;
            PluginProcessor::Seed sd;
            sd.courbe = derniereLecture.historique[static_cast<size_t> (i)];
            sd.famille = derniereLecture.famillesHistorique[static_cast<size_t> (i)];
            sd.reglage = derniereLecture.reglagesHistorique[static_cast<size_t> (i)];
            if (mods.isShiftDown())
            {
                processor.setSeed (onglet, sd);
                majEvolution (true);
                return;
            }
            Etat e;
            e.courbe = sd.courbe;
            e.famille = sd.famille;
            e.membre = -1;
            e.reglage = sd.reglage;
            appliquer (e, true);
        };
        addAndMakeVisible (h);
    }
    preparerNom (nomSuite, {});
    preparerBouton (nouvelleSuite, T ("Suite"), T ("Numéro de la suite de tirages. Clic : nouvelle suite (change tous les tirages au sort de l'évolution)."), [this]
                    {
                        suitesPrecedentes.push_back (processor.getGraine());
                        processor.setGraine (static_cast<uint32_t> (juce::Random::getSystemRandom().nextInt (1 << 30)) + 1u);
                        majEvolution (true);
                    });
    preparerBouton (suitePrecedente, T ("◀"), T ("Revenir à la suite d'avant."), [this]
                    {
                        if (suitesPrecedentes.empty())
                            return;
                        processor.setGraine (suitesPrecedentes.back());
                        suitesPrecedentes.pop_back();
                        majEvolution (true);
                    });
    addAndMakeVisible (vueJouee); // la seed
    preparerBouton (revenirSeed, T ("Revenir à la seed"), T ("Remet la seed comme courbe en cours (l'évolution repart d'elle)."), [this]
                    {
                        const auto sd = processor.getSeed (onglet);
                        Etat e;
                        e.courbe = sd.courbe;
                        e.famille = sd.famille;
                        e.membre = -1;
                        e.reglage = sd.reglage;
                        appliquer (e, true);
                    });
    preparerBouton (garderSeed, T ("Garder comme seed"), T ("La courbe en cours devient la seed : à chaque relecture, l'évolution repart d'elle."),
                    [this] { processor.setSeed (onglet, seedActuelle()); majEvolution (true); });
    preparerBouton (menuSeeds, T ("Seeds"), T ("Stocker la seed, ou reprendre une seed stockée."), [this] { menuDesSeeds(); });
    preparerBouton (fullRandom, T ("Full random"), T ("Remet toutes les familles à chances égales (et enlève les solos)."), [this]
                    {
                        for (int f = 0; f < courbes::nombreFamilles; ++f)
                            processor.setSolo (onglet, evoCommune(), f, false);
                        reglerFamilles ([] (int) { return 50.0f; });
                    });
    preparerBouton (aucunSolo, T ("Aucun solo"), T ("Enlève tous les solos : les chances reviennent aux sliders."), [this]
                    {
                        for (int f = 0; f < courbes::nombreFamilles; ++f)
                            processor.setSolo (onglet, evoCommune(), f, false);
                        majEvolution (true);
                    });
    mesuresEvo.setTooltip (T ("Toutes les courbes changent ensemble, à cet intervalle."));
    for (int i = 0; i < static_cast<int> (PluginProcessor::mesuresEvolution().size()); ++i)
        mesuresEvo.addItem (T (PluginProcessor::mesuresEvolution()[static_cast<size_t> (i)].first), i + 1);
    attacheMesures = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, "evol_mesures", mesuresEvo);
    addAndMakeVisible (mesuresEvo);
    addAndMakeVisible (vueProchaine);
    preparerNom (titreReglages, {});
    titreReglages.setColour (juce::Label::textColourId, palette::texte);
    for (int i = 0; i < 3; ++i)
        preparerBouton (styles[static_cast<size_t> (i)], T (courbes::styleEvolution (i).nom),
                        T ("Règle toutes les chances d'un coup. Tu peux affiner ensuite."), [this, i] { appliquerStyle (i); });
    preparerNom (nomCycles, {});
    rythme.addItemList ({ T ("Chaque itération"), T ("Toutes les N mesures") }, 1);
    rythme.setTooltip (T ("Quand l'évolution tire au sort. Chaque itération : à la fin de chaque cycle de la courbe, quelle que soit sa longueur. "
                          "N mesures : toutes les courbes changent au même moment, toutes les N mesures."));
    attacheRythme = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, "evol_rythme", rythme);
    rythme.onChange = [this] { majEvolution (true); };
    addAndMakeVisible (rythme);
    cycles.setRange (1.0, 16.0, 1.0);
    cycles.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 36, 22);
    cycles.setColour (juce::Slider::textBoxOutlineColourId, palette::grilleForte);
    cycles.onValueChange = [this]
    {
        if (majEnCours)
            return;
        if (processor.apvts.getRawParameterValue ("evol_rythme")->load() > 0.5f)
        {
            if (auto* mesures = processor.apvts.getParameter ("evol_mesures"))
                mesures->setValueNotifyingHost (mesures->convertTo0to1 (static_cast<float> (cycles.getValue())));
        }
        else
            processor.setEvo (onglet, evoCommune(), evo::cycles, static_cast<float> (cycles.getValue()));
    };
    addAndMakeVisible (cycles);
    preparerNom (titreIssues, T ("À chaque tirage, chances de…"));
    titreIssues.setColour (juce::Label::textColourId, palette::texte);
    static const char* nomsIssues[5] = { "Garder", "Revenir", "Varier", "Changer", "Intensité" };
    static const char* aidesIssues[5] = { "Garder la courbe en cours.", "Revenir à ta courbe (celle de l'éditeur).",
                                          "Inventer une variante dans la même famille.", "Passer à une autre famille (choisie avec les chances par famille).",
                                          "À quel point les variantes s'éloignent : 0 = même réglages, nouveau tirage ; 100 = tout au hasard." };
    for (int i = 0; i < 5; ++i)
    {
        auto& l = issues[static_cast<size_t> (i)];
        preparerNom (l.nom, T (nomsIssues[i]));
        preparerNom (l.pourcent, {});
        l.pourcent.setJustificationType (juce::Justification::centredRight);
        l.slider.setRange (0.0, 100.0, 0.1);
        l.slider.setTooltip (T (aidesIssues[i]));
        l.slider.setDoubleClickReturnValue (true, 50.0);
        l.slider.onValueChange = [this, i]
        {
            if (! majEnCours)
            {
                processor.setEvo (onglet, evoCommune(), i, static_cast<float> (issues[static_cast<size_t> (i)].slider.getValue()));
                majEvolution (true);
            }
        };
        addAndMakeVisible (l.slider);
    }
    preparerNom (titreFamilles, T ("Chances par famille"));
    titreFamilles.setColour (juce::Label::textColourId, palette::texte);
    for (int g = 0; g < courbes::nombreGroupes; ++g)
    {
        preparerNom (nomsGroupes[static_cast<size_t> (g)], T (courbes::nomGroupe (g)));
        nomsGroupes[static_cast<size_t> (g)].setColour (juce::Label::textColourId, palette::texte);
        preparerBouton (groupeTout[static_cast<size_t> (g)], T ("Tout"), T ("Met toutes les familles de ce groupe à 50 %."),
                        [this, g] { reglerFamilles ([this, g] (int f) { return courbes::groupeFamille (f) == g ? 50.0f : processor.getEvo (onglet, evoCommune(), evo::poids0 + f); }); });
        preparerBouton (groupeRien[static_cast<size_t> (g)], T ("Rien"), T ("Met toutes les familles de ce groupe à 0 (jamais choisies)."),
                        [this, g] { reglerFamilles ([this, g] (int f) { return courbes::groupeFamille (f) == g ? 0.0f : processor.getEvo (onglet, evoCommune(), evo::poids0 + f); }); });
    }
    for (int f = 0; f < courbes::nombreFamilles; ++f)
    {
        auto& l = poids[static_cast<size_t> (f)];
        preparerNom (l.nom, T (courbes::nomFamille (f)));
        preparerNom (l.pourcent, {});
        l.pourcent.setJustificationType (juce::Justification::centredRight);
        l.slider.setRange (0.0, 100.0, 0.1);
        l.slider.setDoubleClickReturnValue (true, 50.0);
        l.slider.setTooltip (T ("Chance de choisir cette famille quand l'évolution change de famille. 0 = jamais."));
        l.slider.onValueChange = [this, f]
        {
            if (! majEnCours)
            {
                processor.setEvo (onglet, evoCommune(), evo::poids0 + f, static_cast<float> (poids[static_cast<size_t> (f)].slider.getValue()));
                majEvolution (true);
            }
        };
        addAndMakeVisible (l.slider);
        preparerBouton (l.seule, "S", T ("Solo, comme sur une table de mixage : seules les familles en solo sont tirées, à parts égales "
                                         "(une : 100 %, deux : 50 % chacune…)."),
                        [this, f]
                        {
                            processor.setSolo (onglet, evoCommune(), f, ! processor.getSolo (onglet, evoCommune(), f));
                            majEvolution (true);
                        });
    }

    // Lire la taille gardee AVANT setResizeLimits : il redimensionne la fenetre (encore vide) au minimum,
    // et resized() ecraserait la taille gardee. Piege verifie par le test "interface".
    const int width = processor.editorWidth.load(), height = processor.editorHeight.load();
    lierOnglet (processor.ongletAffiche.load());
    setResizable (true, true);
    setResizeLimits (1200, 880, 2400, 1600);
    setSize (width, height);
    ready = true;
    timerCallback(); // premier affichage complet, sans attendre le minuteur
    startTimerHz (30);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//== Preparation des composants =================================================
void PluginEditor::preparerNom (juce::Label& l, const juce::String& texte)
{
    l.setText (texte, juce::dontSendNotification);
    l.setFont (juce::FontOptions (13.0f));
    l.setColour (juce::Label::textColourId, palette::texteDoux);
    l.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (l);
}

void PluginEditor::preparerReglette (Reglette& r, const juce::String& nom, const juce::String& aideTexte, int largeurTexte)
{
    r.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, largeurTexte, 20);
    r.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    r.slider.setTooltip (aideTexte + T ("\nDouble-clic : valeur par défaut."));
    addAndMakeVisible (r.slider);
    preparerNom (r.nom, nom);
}

void PluginEditor::preparerBascule (juce::TextButton& b, const juce::String& texte, const juce::String& aideTexte)
{
    b.setButtonText (texte);
    b.setClickingTogglesState (true);
    b.setTooltip (aideTexte);
    addAndMakeVisible (b);
}

void PluginEditor::preparerBouton (juce::TextButton& b, const juce::String& texte, const juce::String& aideTexte, std::function<void()> action)
{
    b.setButtonText (texte);
    b.setTooltip (aideTexte);
    b.onClick = std::move (action);
    addAndMakeVisible (b);
}

void PluginEditor::attacher (Reglette& r, const char* base)
{
    r.attache.reset();
    r.attache = std::make_unique<AttacheSlider> (processor.apvts, PluginProcessor::id (onglet, base), r.slider);
    if (auto* param = processor.param (onglet, base))
        r.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

/** Affiche l'onglet `t` : rattache tous les reglages a ses parametres, reprend sa courbe. */
void PluginEditor::lierOnglet (int t)
{
    onglet = juce::jlimit (0, nombreOnglets - 1, t);
    processor.ongletAffiche = onglet;
    for (int i = 0; i < onglets.size(); ++i)
        onglets[i]->setToggleState (i == onglet, juce::dontSendNotification);

    const auto couleur = palette::onglet (onglet);
    style.setAccent (couleur);
    titre.setColour (juce::Label::textColourId, couleur);
    lectureCc.setColour (juce::Label::textColourId, couleur);
    boutonGenerer.setColour (juce::TextButton::buttonColourId, couleur.withAlpha (0.3f));
    vue.setActif (onglet);
    majCalques();

    const auto id = [this] (const char* base) { return PluginProcessor::id (onglet, base); };
    attacheLongueur.reset();
    attacheLongueur = std::make_unique<juce::ParameterAttachment> (*processor.param (onglet, "longueur"), [this] (float) { majLongueurs(); }, nullptr);
    attacheTernaire.reset();
    attacheTernaire = std::make_unique<AttacheBouton> (processor.apvts, id ("ternaire"), ternaire);
    attacheGrille.reset();
    attacheGrille = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, id ("grille"), grille);
    attacheGrilleTernaire.reset();
    attacheGrilleTernaire = std::make_unique<AttacheBouton> (processor.apvts, id ("grille_ternaire"), grilleTernaire);
    attacheDepart.reset();
    attacheDepart = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, id ("depart"), depart);
    attacheCadenas.reset();
    attacheCadenas = std::make_unique<AttacheBouton> (processor.apvts, id ("verrou"), cadenas);
    attacher (fondu, "fondu");
    logo.setCouleur (couleur);
    attacheEvolution.reset();
    attacheEvolution = std::make_unique<AttacheBouton> (processor.apvts, id ("evolution"), evolution);
    attacheLie.reset();
    attacheLie = std::make_unique<AttacheBouton> (processor.apvts, id ("lie"), lie);
    attacheCc.reset();
    attacheCc = std::make_unique<AttacheSlider> (processor.apvts, id ("cc"), cc);
    attacheCanal.reset();
    attacheCanal = std::make_unique<AttacheSlider> (processor.apvts, id ("canal"), canal);
    attacher (swing, "swing");
    attacher (decalage, "phase");
    attacher (minimum, "min");
    attacher (maximum, "max");
    attacher (lissage, "lissage");
    attacher (coherence, "coherence");

    versionConnue = -1;
    versionLectureConnue = -1;
    reprendreCourbe();
    majLongueurs();
    majEvolution (true);
    for (auto* c : getChildren())
        c->repaint();
    repaint();
}

//== Courbe ======================================================================
PluginEditor::Etat PluginEditor::etatActuel() const
{
    Etat e;
    e.courbe = vue.getCourbe (onglet);
    e.famille = famille;
    e.membre = membre;
    e.reglage = reglage;
    e.modifiee = modifiee;
    return e;
}

PluginEditor::Etat PluginEditor::etatOnglet (int t) const
{
    if (t == onglet)
        return etatActuel();
    Etat e;
    e.courbe = processor.getCourbe (t);
    e.famille = processor.getFamille (t);
    e.membre = processor.getMembre (t);
    e.reglage = processor.getReglage (t);
    return e;
}

PluginEditor::Instantane PluginEditor::instantane() const
{
    Instantane i;
    for (int t = 0; t < nombreOnglets; ++t)
        i[static_cast<size_t> (t)] = etatOnglet (t);
    return i;
}

void PluginEditor::memoriser()
{
    historique.push_back (instantane());
    if (historique.size() > maxHistorique)
        historique.erase (historique.begin());
    futur.clear();
}

void PluginEditor::restaurer (const Instantane& i)
{
    const auto pareilles = [] (const courbes::Courbe& a, const courbes::Courbe& b)
    {
        if (a.taille() != b.taille())
            return false;
        for (int k = 0; k < a.taille(); ++k)
        {
            const auto &p = a.point (k), &q = b.point (k);
            if (std::abs (p.x - q.x) > 1.0e-7f || std::abs (p.y - q.y) > 1.0e-7f || std::abs (p.tension - q.tension) > 1.0e-7f || p.type != q.type)
                return false;
        }
        return true;
    };
    for (int t = 0; t < nombreOnglets; ++t)
    {
        const auto& e = i[static_cast<size_t> (t)];
        if (t == onglet)
        {
            appliquer (e, false);
            continue;
        }
        if (! pareilles (e.courbe, processor.getCourbe (t)) || e.famille != processor.getFamille (t))
        {
            processor.setCourbe (t, e.courbe, e.famille, e.membre, e.reglage);
            vue.setCourbe (t, e.courbe);
            versionsCalques[static_cast<size_t> (t)] = processor.getVersionCourbe (t);
        }
    }
}

void PluginEditor::appliquer (const Etat& e, bool memoriserAvant)
{
    if (memoriserAvant)
        memoriser();
    famille = e.famille;
    membre = e.membre;
    reglage = e.reglage;
    modifiee = e.modifiee;
    vue.setCourbe (onglet, e.courbe);
    familles.setSelectedId (famille + 1, juce::dontSendNotification);
    afficherReglages();
    envoyerCourbe();
    majTitreMembre();
}

void PluginEditor::annuler()
{
    if (historique.empty())
        return;
    futur.push_back (instantane());
    const auto i = historique.back();
    historique.pop_back();
    restaurer (i);
    majTitreMembre();
}

void PluginEditor::retablir()
{
    if (futur.empty())
        return;
    historique.push_back (instantane());
    const auto i = futur.back();
    futur.pop_back();
    restaurer (i);
    majTitreMembre();
}

void PluginEditor::majCalques()
{
    for (int t = 0; t < nombreOnglets; ++t)
    {
        const bool visible = t == onglet || (multi && processor.visible[static_cast<size_t> (t)].load());
        if (visible && t != onglet && processor.getVersionCourbe (t) != versionsCalques[static_cast<size_t> (t)])
        {
            vue.setCourbe (t, processor.getCourbe (t));
            versionsCalques[static_cast<size_t> (t)] = processor.getVersionCourbe (t);
        }
        vue.setCalque (t, visible, true);
    }
}

bool PluginEditor::keyPressed (const juce::KeyPress& k)
{
    const auto mods = k.getModifiers();
    if (mods.isCommandDown() && (k.getKeyCode() == 'z' || k.getKeyCode() == 'Z'))
    {
        if (mods.isShiftDown())
            retablir();
        else
            annuler();
        return true;
    }
    if (mods.isCommandDown() && (k.getKeyCode() == 'y' || k.getKeyCode() == 'Y'))
    {
        retablir();
        return true;
    }
    return false;
}

void PluginEditor::choisirMembre (int f, int m)
{
    Etat e;
    e.famille = juce::jlimit (0, courbes::nombreFamilles - 1, f);
    e.membre = juce::jlimit (0, courbes::membresParFamille - 1, m);
    e.reglage = courbes::membre (e.famille, e.membre);
    courbes::generer (e.famille, e.reglage, e.courbe);
    appliquer (e, true);
}

void PluginEditor::generer (bool toutAuHasard)
{
    auto& hasard = juce::Random::getSystemRandom();
    Etat e = etatActuel();
    e.reglage.graine = static_cast<uint32_t> (hasard.nextInt (1 << 30)) + 1u;
    if (toutAuHasard)
        for (auto& v : e.reglage.v)
            v = hasard.nextFloat();
    courbes::generer (e.famille, e.reglage, e.courbe);
    e.membre = -1;
    e.modifiee = false;
    appliquer (e, true);
}

void PluginEditor::regenerer()
{
    if (majEnCours)
        return;
    for (int i = 0; i < courbes::nombreReglages; ++i)
        reglage.v[static_cast<size_t> (i)] = static_cast<float> (reglagesFamille[static_cast<size_t> (i)].slider.getValue());
    courbes::Courbe c;
    courbes::generer (famille, reglage, c);
    membre = -1;
    modifiee = false;
    vue.setCourbe (onglet, c);
    envoyerCourbe();
    majTitreMembre();
}

void PluginEditor::transformer (std::function<bool (courbes::Courbe&)> f)
{
    Etat e = etatActuel();
    if (! f (e.courbe))
        return;
    e.modifiee = true;
    appliquer (e, true);
}

void PluginEditor::afficherReglages()
{
    const juce::ScopedValueSetter<bool> svs (majEnCours, true);
    const auto& info = courbes::infoFamille (famille);
    for (int i = 0; i < courbes::nombreReglages; ++i)
    {
        auto& r = reglagesFamille[static_cast<size_t> (i)];
        r.nom.setText (T (info.reglages[i]), juce::dontSendNotification);
        r.slider.setValue (reglage.v[static_cast<size_t> (i)], juce::dontSendNotification);
        r.slider.setTooltip (T (info.aide) + T ("\nCe réglage refait la forme en direct."));
    }
    familles.setTooltip (T (info.aide));
}

void PluginEditor::envoyerCourbe()
{
    processor.setCourbe (onglet, vue.getCourbe (onglet), famille, membre, reglage);
    versionConnue = processor.getVersionCourbe (onglet);
    // Evolution eteinte : la seed suit la courbe. Allumee : la seed ne change qu'avec "Garder comme seed".
    if (! evolutionAllumee (onglet))
        processor.setSeed (onglet, seedActuelle());
}

bool PluginEditor::evolutionAllumee (int t) const
{
    return processor.valeur (t, "evolution") > 0.5f;
}

PluginProcessor::Seed PluginEditor::seedActuelle() const
{
    PluginProcessor::Seed sd;
    sd.courbe = vue.getCourbe (onglet);
    sd.famille = famille;
    sd.reglage = reglage;
    return sd;
}

void PluginEditor::menuDesSeeds()
{
    const auto stockees = processor.getSeedsStockees (onglet);
    juce::PopupMenu m, supprimer;
    m.addItem (1, T ("Stocker la seed actuelle"));
    for (int i = 0; i < static_cast<int> (stockees.size()); ++i)
    {
        if (i == 0)
            m.addSeparator();
        m.addItem (100 + i, stockees[static_cast<size_t> (i)].nom);
        supprimer.addItem (1000 + i, stockees[static_cast<size_t> (i)].nom);
    }
    if (! stockees.empty())
    {
        m.addSeparator();
        m.addSubMenu (T ("Supprimer"), supprimer);
    }
    juce::Component::SafePointer<PluginEditor> moi (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuSeeds), [moi, stockees] (int choix)
    {
        if (moi == nullptr || choix == 0)
            return;
        const int t = moi->onglet;
        if (choix == 1)
        {
            auto sd = moi->processor.getSeed (t);
            sd.nom = T ("Seed ") + juce::String (stockees.size() + 1) + " (" + T (courbes::nomFamille (sd.famille)) + ")";
            moi->processor.stockerSeed (t, sd);
        }
        else if (choix >= 1000)
            moi->processor.supprimerSeed (t, choix - 1000);
        else if (choix - 100 < static_cast<int> (stockees.size()))
        {
            // Une seed stockee devient la seed et la courbe en cours.
            const auto& sd = stockees[static_cast<size_t> (choix - 100)];
            moi->processor.setSeed (t, sd);
            Etat e;
            e.courbe = sd.courbe;
            e.famille = sd.famille;
            e.membre = -1;
            e.reglage = sd.reglage;
            moi->appliquer (e, true);
        }
        moi->majEvolution (true);
    });
}

void PluginEditor::reprendreCourbe()
{
    versionConnue = processor.getVersionCourbe (onglet);
    famille = processor.getFamille (onglet);
    membre = processor.getMembre (onglet);
    reglage = processor.getReglage (onglet);
    modifiee = false;
    familles.setSelectedId (famille + 1, juce::dontSendNotification);
    vue.setCourbe (onglet, processor.getCourbe (onglet));
    afficherReglages();
    majTitreMembre();
}

void PluginEditor::majTitreMembre()
{
    juce::String t;
    if (membre >= 0)
        t << T ("Forme ") << (membre + 1) << " / " << courbes::membresParFamille;
    else
        t << T ("Forme réglée");
    if (modifiee)
        t << T ("  ·  modifiée");
    nomMembre.setText (t, juce::dontSendNotification);
    boutonAnnuler.setEnabled (! historique.empty());
    boutonRetablir.setEnabled (! futur.empty());
    boutonColler.setEnabled (pressePapier != nullptr);
}

void PluginEditor::majLongueurs()
{
    const bool avecTernaire = processor.valeur (onglet, "ternaire") > 0.5f;
    ternaireConnu = avecTernaire;
    longueursAutorisees.clear();
    const auto& l = PluginProcessor::longueurs();
    for (int i = 0; i < static_cast<int> (l.size()); ++i)
        if (avecTernaire || ! l[static_cast<size_t> (i)].ternaire)
            longueursAutorisees.push_back (i);

    const int indexJoue = PluginProcessor::longueurJouee (juce::roundToInt (processor.valeur (onglet, "longueur")), avecTernaire);
    int position = 0;
    for (int i = 0; i < static_cast<int> (longueursAutorisees.size()); ++i)
        if (longueursAutorisees[static_cast<size_t> (i)] == indexJoue)
            position = i;

    const juce::ScopedValueSetter<bool> svs (majEnCours, true);
    longueur.setRange (0.0, static_cast<double> (longueursAutorisees.size() - 1), 1.0);
    longueur.setValue (position, juce::dontSendNotification);
    longueur.updateText();
    majGrille();
}

void PluginEditor::majGrille()
{
    const double npm = processor.noiresParMesure.load();
    const double cycle = processor.cycleNoires (onglet, npm);
    const double caseN = juce::jmax (1.0e-3, processor.caseNoires (onglet));
    vue.setGrille (cycle / caseN, 1.0 / caseN, npm / caseN);
}

//== Mes courbes =================================================================
juce::File PluginEditor::fichierMesCourbes() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Droz").getChildFile ("Droz Courbes").getChildFile ("mes-courbes.xml");
}

void PluginEditor::menuMesCourbes()
{
    auto racine = juce::parseXML (fichierMesCourbes());
    juce::PopupMenu m, supprimer;
    m.addItem (1, T ("Enregistrer la courbe actuelle…"));
    int n = 0;
    juce::StringArray noms;
    if (racine != nullptr)
        for (auto* e : racine->getChildIterator())
            noms.add (e->getStringAttribute ("nom"));
    if (! noms.isEmpty())
    {
        m.addSeparator();
        for (const auto& nom : noms)
        {
            m.addItem (100 + n, nom);
            supprimer.addItem (1000 + n, nom);
            ++n;
        }
        m.addSeparator();
        m.addSubMenu (T ("Supprimer"), supprimer);
    }

    juce::Component::SafePointer<PluginEditor> moi (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&mesCourbes), [moi, noms] (int choix)
    {
        if (moi == nullptr || choix == 0)
            return;
        const auto fichier = moi->fichierMesCourbes();
        auto doc = juce::parseXML (fichier);
        if (doc == nullptr)
            doc = std::make_unique<juce::XmlElement> ("MES_COURBES");

        if (choix == 1)
        {
            auto* fenetre = new juce::AlertWindow (T ("Enregistrer la courbe"), T ("Nom de la courbe :"), juce::MessageBoxIconType::NoIcon, moi);
            fenetre->setLookAndFeel (&moi->style);
            fenetre->addTextEditor ("nom", T (courbes::nomFamille (moi->famille)) + " " + juce::String (noms.size() + 1));
            fenetre->addButton (T ("Enregistrer"), 1, juce::KeyPress (juce::KeyPress::returnKey));
            fenetre->addButton (T ("Annuler"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
            fenetre->enterModalState (true, juce::ModalCallbackFunction::create ([moi, fenetre] (int r)
            {
                if (moi == nullptr || r != 1)
                    return;
                const auto nom = fenetre->getTextEditorContents ("nom").trim();
                if (nom.isEmpty())
                    return;
                const auto fichierCible = moi->fichierMesCourbes();
                auto docCible = juce::parseXML (fichierCible);
                if (docCible == nullptr)
                    docCible = std::make_unique<juce::XmlElement> ("MES_COURBES");
                auto* e = docCible->createNewChildElement ("COURBE");
                const auto& c = moi->vue.getCourbe (moi->onglet);
                juce::StringArray points;
                for (int i = 0; i < c.taille(); ++i)
                    points.add (juce::String (c.point (i).x, 5) + " " + juce::String (c.point (i).y, 5) + " "
                                + juce::String (c.point (i).tension, 4) + " " + juce::String (static_cast<int> (c.point (i).type)));
                e->setAttribute ("nom", nom);
                e->setAttribute ("famille", moi->famille);
                e->setAttribute ("points", points.joinIntoString (";"));
                fichierCible.getParentDirectory().createDirectory();
                docCible->writeTo (fichierCible);
            }), true);
            return;
        }

        if (choix >= 1000)
        {
            if (auto* e = doc->getChildElement (choix - 1000))
                doc->removeChildElement (e, true);
            fichier.getParentDirectory().createDirectory();
            doc->writeTo (fichier);
            return;
        }

        if (auto* e = doc->getChildElement (choix - 100))
        {
            Etat etat = moi->etatActuel();
            etat.courbe.vider();
            for (const auto& morceau : juce::StringArray::fromTokens (e->getStringAttribute ("points"), ";", ""))
            {
                const auto v = juce::StringArray::fromTokens (morceau, " ", "");
                if (v.size() >= 4)
                    etat.courbe.ajouter (v[0].getFloatValue(), v[1].getFloatValue(), v[2].getFloatValue(),
                                         static_cast<courbes::Segment> (juce::jlimit (0, 2, v[3].getIntValue())));
            }
            etat.courbe.trier();
            if (etat.courbe.taille() < 2)
                return;
            etat.famille = juce::jlimit (0, courbes::nombreFamilles - 1, e->getIntAttribute ("famille", moi->famille));
            etat.membre = -1;
            etat.modifiee = true;
            moi->appliquer (etat, true);
        }
    });
}

//== Evolution ===================================================================
bool PluginEditor::evoCommune() const
{
    return processor.valeur (onglet, "lie") > 0.5f;
}

void PluginEditor::appliquerStyle (int i)
{
    const auto& s = courbes::styleEvolution (i);
    const bool c = evoCommune();
    processor.setEvo (onglet, c, evo::garder, s.garder);
    processor.setEvo (onglet, c, evo::revenir, s.revenir);
    processor.setEvo (onglet, c, evo::varier, s.varier);
    processor.setEvo (onglet, c, evo::changer, s.changer);
    processor.setEvo (onglet, c, evo::intensite, s.intensite);
    majEvolution (true);
}

void PluginEditor::reglerFamilles (std::function<float (int)> valeurDe)
{
    const bool c = evoCommune();
    std::array<float, courbes::nombreFamilles> v {};
    for (int f = 0; f < courbes::nombreFamilles; ++f)
        v[static_cast<size_t> (f)] = valeurDe (f);
    for (int f = 0; f < courbes::nombreFamilles; ++f)
        processor.setEvo (onglet, c, evo::poids0 + f, v[static_cast<size_t> (f)]);
    majEvolution (true);
}

void PluginEditor::majEvolution (bool forcer)
{
    const bool c = evoCommune();
    const bool allumee = processor.valeur (onglet, "evolution") > 0.5f;
    titreReglages.setText (c ? T ("Réglages communs (tous les onglets liés)") : T ("Réglages de cet onglet seulement"), juce::dontSendNotification);
    coherence.slider.setEnabled (c);
    nouvelleSuite.setButtonText (T ("Suite ") + juce::String (processor.getGraine() % 10000u));
    suitePrecedente.setEnabled (! suitesPrecedentes.empty());

    // Valeurs des sliders (sauf celui qu'on tient), et vraies chances en %.
    const juce::ScopedValueSetter<bool> svs (majEnCours, true);
    float total = 0.0f;
    for (int i = 0; i < 4; ++i)
        total += processor.getEvo (onglet, c, i);
    for (int i = 0; i < 5; ++i)
    {
        auto& l = issues[static_cast<size_t> (i)];
        const float v = processor.getEvo (onglet, c, i);
        if (forcer || ! l.slider.isMouseButtonDown())
            l.slider.setValue (v, juce::dontSendNotification);
        const float affiche = i == evo::intensite ? v : (total > 0.0f ? 100.0f * v / total : 0.0f);
        l.pourcent.setText (juce::String (juce::roundToInt (affiche)) + " %", juce::dontSendNotification);
    }
    const bool parMesures = processor.apvts.getRawParameterValue ("evol_rythme")->load() > 0.5f;
    nomCycles.setText (parMesures ? T ("mesures") : T ("itérations"), juce::dontSendNotification);
    if (forcer || ! cycles.isMouseButtonDown())
        cycles.setValue (parMesures ? processor.apvts.getRawParameterValue ("evol_mesures")->load()
                                    : processor.getEvo (onglet, c, evo::cycles),
                         juce::dontSendNotification);

    mesuresEvo.setVisible (parMesures && processor.panneauEvolution.load());
    cycles.setVisible (! parMesures && processor.panneauEvolution.load());
    nomCycles.setVisible (! parMesures && processor.panneauEvolution.load());

    // Familles : en multi-solo, seules les familles en solo comptent, a parts egales.
    int solos = 0;
    for (int f = 0; f < courbes::nombreFamilles; ++f)
        solos += processor.getSolo (onglet, c, f) ? 1 : 0;
    float totalFamilles = 0.0f;
    for (int f = 0; f < courbes::nombreFamilles; ++f)
        totalFamilles += processor.getEvo (onglet, c, evo::poids0 + f);
    for (int f = 0; f < courbes::nombreFamilles; ++f)
    {
        auto& l = poids[static_cast<size_t> (f)];
        const float v = processor.getEvo (onglet, c, evo::poids0 + f);
        const bool solo = processor.getSolo (onglet, c, f);
        if (forcer || ! l.slider.isMouseButtonDown())
            l.slider.setValue (v, juce::dontSendNotification);
        l.slider.setEnabled (solos == 0);
        l.seule.setToggleState (solo, juce::dontSendNotification);
        const float part = solos > 0 ? (solo ? 100.0f / static_cast<float> (solos) : 0.0f)
                                     : (totalFamilles > 0.0f ? 100.0f * v / totalFamilles : 0.0f);
        l.pourcent.setText (juce::String (juce::roundToInt (part)) + " %", juce::dontSendNotification);
    }
    aucunSolo.setEnabled (solos > 0);
    for (auto& b : groupeTout)
        b.setEnabled (solos == 0);
    for (auto& b : groupeRien)
        b.setEnabled (solos == 0);

    for (auto* comp : std::initializer_list<juce::Component*> { &vueProchaine, &nouvelleSuite, &suitePrecedente, &nomSuite })
        comp->setAlpha (allumee ? 1.0f : 0.4f);
    const auto sd = processor.getSeed (onglet);
    vueJouee.setCourbe (&sd.courbe, palette::onglet (onglet), T ("Seed : ") + T (courbes::nomFamille (sd.famille)));
}

//== Minuteur ====================================================================
void PluginEditor::timerCallback()
{
    // Courbe changee ailleurs (projet recharge, preset du DAW) : on la reprend.
    if (processor.getVersionCourbe (onglet) != versionConnue && ! vue.enTrainDeModifier())
        reprendreCourbe();

    // Ce que joue l'evolution.
    const bool allumee = processor.valeur (onglet, "evolution") > 0.5f;
    const auto couleur = palette::onglet (onglet);
    if (processor.getVersionLecture (onglet) != versionLectureConnue)
    {
        versionLectureConnue = processor.getVersionLecture (onglet);
        PluginProcessor::Lecture l;
        processor.getLecture (onglet, l);
        derniereLecture = l;
        for (int i = 0; i < PluginProcessor::Lecture::tailleHistorique; ++i)
            historiqueVues[static_cast<size_t> (i)].setCourbe (i < l.nbHistorique ? &l.historique[static_cast<size_t> (i)] : nullptr,
                                                               i == 0 ? couleur : couleur.withAlpha (0.6f), {});
        // L'evolution a change la courbe : elle remplace la courbe affichee (et devient celle qu'on modifie).
        // Seulement si l'audio a deja pris nos dernieres modifications, et pas pendant qu'on modifie.
        if (allumee && ! vue.enTrainDeModifier() && l.versionCourbe >= processor.getVersionCourbe (onglet))
        {
            const auto& affichee = vue.getCourbe (onglet);
            bool pareille = affichee.taille() == l.jouee.taille();
            for (int i = 0; pareille && i < affichee.taille(); ++i)
                pareille = std::abs (affichee.point (i).x - l.jouee.point (i).x) < 1.0e-6f && std::abs (affichee.point (i).y - l.jouee.point (i).y) < 1.0e-6f
                           && std::abs (affichee.point (i).tension - l.jouee.point (i).tension) < 1.0e-6f;
            if (! pareille)
            {
                vue.setCourbe (onglet, l.jouee);
                famille = l.familleJouee;
                reglage = l.reglageJouee;
                membre = -1;
                modifiee = false;
                familles.setSelectedId (famille + 1, juce::dontSendNotification);
                afficherReglages();
                majTitreMembre();
                nomMembre.setText (T ("Variante de l'évolution"), juce::dontSendNotification);
            }
        }
        if (allumee && l.prochaineValide)
            vueProchaine.setCourbe (&l.prochaine, couleur.withAlpha (0.8f),
                                    T ("Prochaine : ") + T (courbes::nomFamille (l.familleProchaine))
                                        + (l.cyclesAvant > 1 ? T (" (dans ") + juce::String (l.cyclesAvant) + T (" cycles)") : juce::String()));
        else
            vueProchaine.setCourbe (nullptr, couleur, T ("Prochaine"));
    }
    vue.setJouee (nullptr);
    majEvolution (false);

    // Autres courbes affichees (multicourbe) : elles suivent aussi leur evolution.
    for (int t = 0; t < nombreOnglets; ++t)
    {
        if (t == onglet || ! multi || ! processor.visible[static_cast<size_t> (t)].load() || ! evolutionAllumee (t) || vue.enTrainDeModifier())
            continue;
        PluginProcessor::Lecture l;
        processor.getLecture (t, l);
        const auto& affichee = vue.getCourbe (t);
        bool pareille = affichee.taille() == l.jouee.taille();
        for (int i = 0; pareille && i < affichee.taille(); ++i)
            pareille = std::abs (affichee.point (i).x - l.jouee.point (i).x) < 1.0e-6f && std::abs (affichee.point (i).y - l.jouee.point (i).y) < 1.0e-6f;
        if (! pareille && l.versionCourbe >= processor.getVersionCourbe (t))
            vue.setCourbe (t, l.jouee);
    }
    relancer.setEnabled (courbes::groupeFamille (famille) >= 2);

    if ((processor.valeur (onglet, "ternaire") > 0.5f) != ternaireConnu)
        majLongueurs();
    else
        majGrille();

    for (int t = 0; t < nombreOnglets; ++t)
    {
        const int dernier = processor.ccAffiche[static_cast<size_t> (t)].load();
        onglets[t]->setInfos (processor.envoie (t), juce::roundToInt (processor.valeur (t, "cc")),
                              processor.valeur (t, "evolution") > 0.5f, processor.visible[static_cast<size_t> (t)].load(),
                              dernier < 0 ? -1.0f : static_cast<float> (dernier) / 127.0f);
    }

    majCalques();
    for (int t = 0; t < nombreOnglets; ++t)
        vue.setLecture (t, processor.phaseAffichee[static_cast<size_t> (t)].load());
    const int v = processor.ccAffiche[static_cast<size_t> (onglet)].load();
    lectureCc.setText ("CC " + juce::String (juce::roundToInt (processor.valeur (onglet, "cc"))) + T ("  →  ")
                           + (v < 0 ? juce::String ("-") : juce::String (v)),
                       juce::dontSendNotification);
}

//== Dessin et placement =========================================================
void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (palette::fond);
    if (zoneEvolution.isEmpty())
        return;
    g.setColour (palette::panneau);
    g.fillRoundedRectangle (zoneEvolution.toFloat(), 8.0f);
    g.setColour (palette::onglet (onglet).withAlpha (0.5f));
    g.fillRect (zoneEvolution.getX() + 12, zoneEvolution.getY(), zoneEvolution.getWidth() - 24, 2);
}

void PluginEditor::placer (Reglette& r, juce::Rectangle<int> zone, bool nomAGauche, int largeurNom)
{
    if (nomAGauche)
        r.nom.setBounds (zone.removeFromLeft (largeurNom));
    else
        r.nom.setBounds (zone.removeFromTop (16));
    r.slider.setBounds (zone);
}

void PluginEditor::resized()
{
    if (ready)
    {
        processor.editorWidth = getWidth();
        processor.editorHeight = getHeight();
    }

    auto area = getLocalBounds().reduced (14);

    // Barre d'onglets : logo, reglages globaux, 8 onglets avec Visible / Solo / Mute.
    {
        auto barre = area.removeFromTop (58);
        auto gauche = barre.removeFromLeft (330);
        logo.setBounds (gauche.removeFromTop (32));
        gauche.removeFromTop (2);
        version.setBounds (gauche.removeFromLeft (44));
        boutonInsectes.setBounds (gauche.removeFromLeft (72).reduced (2, 0));
        boutonPanneau.setBounds (gauche.removeFromLeft (78).reduced (2, 0));
        toutDemute.setBounds (gauche.removeFromLeft (68).reduced (2, 0));
        toutDesolo.setBounds (gauche.reduced (2, 0));
        barre.removeFromLeft (6);
        const int w = barre.getWidth() / nombreOnglets;
        for (int t = 0; t < nombreOnglets; ++t)
        {
            auto carte = barre.removeFromLeft (w).reduced (3, 0);
            onglets[t]->setBounds (carte);
            auto colonne = carte.removeFromRight (22).reduced (2, 3);
            const int h = colonne.getHeight() / 3;
            boutonsVisible[static_cast<size_t> (t)].setBounds (colonne.removeFromTop (h));
            boutonsSolo[static_cast<size_t> (t)].setBounds (colonne.removeFromTop (h));
            boutonsMute[static_cast<size_t> (t)].setBounds (colonne);
        }
    }
    area.removeFromTop (10);

    // Panneau Evolution a droite (masquable).
    const bool panneau = processor.panneauEvolution.load();
    for (auto* c : std::initializer_list<juce::Component*> { &evolution, &nomSuite, &nouvelleSuite, &suitePrecedente, &vueJouee, &vueProchaine,
                                                             &revenirSeed, &garderSeed, &menuSeeds, &lie, &coherence.slider, &coherence.nom,
                                                             &titreReglages, &styles[0], &styles[1], &styles[2], &rythme, &cycles, &nomCycles,
                                                             &mesuresEvo, &titreIssues, &titreFamilles, &aucunSolo, &fullRandom,
                                                             &cadenas, &seedDepart, &fondu.slider, &fondu.nom, &titreHistorique })
        c->setVisible (panneau);
    for (auto& l : issues)
        for (auto* c : std::initializer_list<juce::Component*> { &l.nom, &l.pourcent, &l.slider })
            c->setVisible (panneau);
    for (auto& h : historiqueVues)
        h.setVisible (panneau);
    for (auto& l : poids)
        for (auto* c : std::initializer_list<juce::Component*> { &l.nom, &l.pourcent, &l.slider, &l.seule })
            c->setVisible (panneau);
    for (int g = 0; g < courbes::nombreGroupes; ++g)
        for (auto* c : std::initializer_list<juce::Component*> { &nomsGroupes[static_cast<size_t> (g)], &groupeTout[static_cast<size_t> (g)],
                                                                 &groupeRien[static_cast<size_t> (g)] })
            c->setVisible (panneau);

    zoneEvolution = {};
    if (panneau)
    {
        zoneEvolution = area.removeFromRight (340);
        area.removeFromRight (14);
        auto e = zoneEvolution.reduced (12, 10);
        auto ligne = e.removeFromTop (28);
        evolution.setBounds (ligne.removeFromLeft (96));
        ligne.removeFromLeft (4);
        cadenas.setBounds (ligne.removeFromLeft (76));
        ligne.removeFromLeft (6);
        suitePrecedente.setBounds (ligne.removeFromRight (28));
        ligne.removeFromRight (4);
        nouvelleSuite.setBounds (ligne);
        nomSuite.setVisible (false);
        e.removeFromTop (6);
        ligne = e.removeFromTop (60);
        vueJouee.setBounds (ligne.removeFromLeft (ligne.getWidth() / 2 - 3));
        ligne.removeFromLeft (6);
        vueProchaine.setBounds (ligne);
        e.removeFromTop (4);
        ligne = e.removeFromTop (24);
        revenirSeed.setBounds (ligne.removeFromLeft (128));
        ligne.removeFromLeft (4);
        garderSeed.setBounds (ligne.removeFromLeft (124));
        ligne.removeFromLeft (4);
        menuSeeds.setBounds (ligne);
        e.removeFromTop (4);
        ligne = e.removeFromTop (24);
        seedDepart.setBounds (ligne.removeFromLeft (150));
        ligne.removeFromLeft (8);
        placer (fondu, ligne, true, 48);
        e.removeFromTop (4);
        titreHistorique.setBounds (e.removeFromTop (16));
        ligne = e.removeFromTop (28);
        {
            const int w = ligne.getWidth() / PluginProcessor::Lecture::tailleHistorique;
            for (auto& h : historiqueVues)
                h.setBounds (ligne.removeFromLeft (w).reduced (1, 0));
        }
        e.removeFromTop (6);
        ligne = e.removeFromTop (26);
        lie.setBounds (ligne.removeFromLeft (110));
        ligne.removeFromLeft (8);
        placer (coherence, ligne, true, 72);
        e.removeFromTop (6);
        titreReglages.setBounds (e.removeFromTop (18));
        ligne = e.removeFromTop (26);
        {
            const int w = ligne.getWidth() / 3;
            for (auto& st : styles)
                st.setBounds (ligne.removeFromLeft (w).reduced (2, 0));
        }
        e.removeFromTop (6);
        ligne = e.removeFromTop (24);
        rythme.setBounds (ligne.removeFromLeft (150));
        ligne.removeFromLeft (6);
        mesuresEvo.setBounds (ligne.withWidth (120));
        cycles.setBounds (ligne.removeFromLeft (84));
        ligne.removeFromLeft (6);
        nomCycles.setBounds (ligne);
        e.removeFromTop (4);
        titreIssues.setBounds (e.removeFromTop (18));
        for (auto& l : issues)
        {
            ligne = e.removeFromTop (20);
            l.nom.setBounds (ligne.removeFromLeft (72));
            l.pourcent.setBounds (ligne.removeFromRight (46));
            l.slider.setBounds (ligne);
        }
        e.removeFromTop (6);
        ligne = e.removeFromTop (20);
        aucunSolo.setBounds (ligne.removeFromRight (78).reduced (0, 1));
        ligne.removeFromRight (4);
        fullRandom.setBounds (ligne.removeFromRight (84).reduced (0, 1));
        titreFamilles.setBounds (ligne);
        const int h = juce::jlimit (14, 21, e.getHeight() / (courbes::nombreFamilles + courbes::nombreGroupes));
        for (int g = 0; g < courbes::nombreGroupes; ++g)
        {
            ligne = e.removeFromTop (h);
            groupeRien[static_cast<size_t> (g)].setBounds (ligne.removeFromRight (42).reduced (0, 1));
            ligne.removeFromRight (4);
            groupeTout[static_cast<size_t> (g)].setBounds (ligne.removeFromRight (42).reduced (0, 1));
            nomsGroupes[static_cast<size_t> (g)].setBounds (ligne);
            for (int f = 0; f < courbes::nombreFamilles; ++f)
            {
                if (courbes::groupeFamille (f) != g)
                    continue;
                auto& l = poids[static_cast<size_t> (f)];
                ligne = e.removeFromTop (h);
                ligne.removeFromLeft (10);
                l.nom.setBounds (ligne.removeFromLeft (104));
                l.seule.setBounds (ligne.removeFromRight (26).reduced (0, 1));
                ligne.removeFromRight (4);
                l.pourcent.setBounds (ligne.removeFromRight (40));
                l.slider.setBounds (ligne);
            }
        }
    }

    // En-tete : familles et formes.
    auto haut = area.removeFromTop (32);
    familles.setBounds (haut.removeFromLeft (180).reduced (0, 2));
    haut.removeFromLeft (8);
    precedent.setBounds (haut.removeFromLeft (30).reduced (0, 2));
    haut.removeFromLeft (4);
    suivant.setBounds (haut.removeFromLeft (30).reduced (0, 2));
    boutonGenerer.setBounds (haut.removeFromRight (92).reduced (0, 2));
    haut.removeFromRight (6);
    relancer.setBounds (haut.removeFromRight (84).reduced (0, 2));
    haut.removeFromRight (6);
    mesCourbes.setBounds (haut.removeFromRight (104).reduced (0, 2));
    haut.removeFromLeft (10);
    nomMembre.setBounds (haut);

    area.removeFromTop (6);
    {
        auto ligne = area.removeFromTop (38);
        const int w = ligne.getWidth() / courbes::nombreReglages;
        for (auto& r : reglagesFamille)
            placer (r, ligne.removeFromLeft (w).withTrimmedRight (16), false, 0);
    }
    area.removeFromTop (6);
    {
        auto ligne = area.removeFromTop (28);
        boutonAnnuler.setBounds (ligne.removeFromLeft (38));
        ligne.removeFromLeft (4);
        boutonRetablir.setBounds (ligne.removeFromLeft (38));
        ligne.removeFromLeft (12);
        boutonCrayon.setBounds (ligne.removeFromLeft (74));
        ligne.removeFromLeft (12);
        for (auto* b : { &boutonInverser, &boutonRetourner })
        {
            b->setBounds (ligne.removeFromLeft (84));
            ligne.removeFromLeft (4);
        }
        for (auto* b : { &boutonDoubler, &boutonMoitie })
        {
            b->setBounds (ligne.removeFromLeft (38));
            ligne.removeFromLeft (4);
        }
        ligne.removeFromLeft (8);
        for (auto* b : { &boutonCopier, &boutonColler })
        {
            b->setBounds (ligne.removeFromLeft (66));
            ligne.removeFromLeft (4);
        }
        boutonMulti.setBounds (ligne.removeFromRight (130));
    }

    // Bas : temps, sortie.
    auto bas = area.removeFromBottom (150);
    {
        auto ligne = bas.removeFromTop (40);
        auto gauche = ligne.removeFromLeft (ligne.getWidth() * 42 / 100);
        nomLongueur.setBounds (gauche.removeFromTop (16));
        ternaire.setBounds (gauche.removeFromRight (78).reduced (0, 1));
        gauche.removeFromRight (6);
        longueur.setBounds (gauche);
        ligne.removeFromLeft (16);
        auto g = ligne.removeFromLeft (290);
        nomGrille.setBounds (g.removeFromTop (16));
        grille.setBounds (g.removeFromLeft (76).reduced (0, 1));
        g.removeFromLeft (6);
        grilleTernaire.setBounds (g.removeFromLeft (78).reduced (0, 1));
        g.removeFromLeft (6);
        boutonAimant.setBounds (g.removeFromLeft (72).reduced (0, 1));
        ligne.removeFromLeft (16);
        placer (swing, ligne, false, 0);
    }
    bas.removeFromTop (10);
    {
        auto ligne = bas.removeFromTop (40);
        const int w = ligne.getWidth() / 4;
        for (auto* r : { &decalage, &minimum, &maximum, &lissage })
            placer (*r, ligne.removeFromLeft (w).withTrimmedRight (16), false, 0);
    }
    bas.removeFromTop (14);
    {
        auto ligne = bas.removeFromTop (28);
        nomDepart.setBounds (ligne.removeFromLeft (50));
        depart.setBounds (ligne.removeFromLeft (86));
        ligne.removeFromLeft (16);
        ecoute.setBounds (ligne.removeFromLeft (90));
        ligne.removeFromLeft (20);
        nomCc.setBounds (ligne.removeFromLeft (28));
        cc.setBounds (ligne.removeFromLeft (110));
        ligne.removeFromLeft (14);
        nomCanal.setBounds (ligne.removeFromLeft (48));
        canal.setBounds (ligne.removeFromLeft (100));
        lectureCc.setBounds (ligne);
    }

    area.removeFromBottom (6);
    aide.setBounds (area.removeFromBottom (16));
    area.removeFromBottom (4);
    area.removeFromTop (6);
    vue.setBounds (area);
}
