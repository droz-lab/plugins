#include "PluginEditor.h"

namespace
{
// Texte accentue : toujours passer par fromUTF8, jamais une chaine brute dans juce::String.
juce::String T (const char* utf8) { return juce::String::fromUTF8 (utf8); }

constexpr float rayonPoint = 5.0f;
constexpr float rayonPrise = 9.0f;
} // namespace

//==============================================================================
Style::Style()
{
    setColour (juce::Slider::textBoxTextColourId, palette::texte);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::trackColourId, palette::accent);
    setColour (juce::Slider::backgroundColourId, palette::grilleForte);
    setColour (juce::Slider::thumbColourId, palette::texte);
    setColour (juce::Label::textColourId, palette::texte);
    setColour (juce::ComboBox::backgroundColourId, palette::panneauClair);
    setColour (juce::ComboBox::outlineColourId, palette::grilleForte);
    setColour (juce::ComboBox::textColourId, palette::texte);
    setColour (juce::ComboBox::arrowColourId, palette::accent);
    setColour (juce::TextButton::buttonColourId, palette::panneauClair);
    setColour (juce::TextButton::buttonOnColourId, palette::accent);
    setColour (juce::TextButton::textColourOffId, palette::texte);
    setColour (juce::TextButton::textColourOnId, palette::fond);
    setColour (juce::PopupMenu::backgroundColourId, palette::panneauClair);
    setColour (juce::PopupMenu::textColourId, palette::texte);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, palette::accent.withAlpha (0.3f));
    setColour (juce::TooltipWindow::backgroundColourId, palette::panneauClair.brighter (0.1f));
    setColour (juce::TooltipWindow::textColourId, palette::texte);
    setColour (juce::TooltipWindow::outlineColourId, palette::grilleForte);
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
    g.setColour (s.findColour (juce::Slider::thumbColourId));
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
    g.setColour (c);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (allume ? c : palette::grilleForte);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

//==============================================================================
VueCourbe::VueCourbe()
{
    setTooltip (T ("Glisser un point : le déplacer. Glisser un segment verticalement : le courber.\n"
                   "Double-clic : ajouter ou supprimer un point. Clic droit sur un point : forme du segment.\n"
                   "Maj enfoncée : inverse l'aimant de la grille."));
}

void VueCourbe::setCourbe (const courbes::Courbe& c)
{
    courbe = c;
    survole = choisi = -1;
    repaint();
}

void VueCourbe::setJouee (const courbes::Courbe* c)
{
    afficheJouee = c != nullptr;
    if (c != nullptr)
        jouee = *c;
    repaint();
}

void VueCourbe::setLecture (float phase, float v)
{
    lecture = phase;
    valeur = v;
    repaint();
}

void VueCourbe::setGrille (double c, double parTemps, double parMesure)
{
    cases = juce::jlimit (1.0, 512.0, c);
    casesParTemps = juce::jmax (1.0e-3, parTemps);
    casesParMesure = juce::jmax (1.0e-3, parMesure);
    repaint();
}

juce::Rectangle<float> VueCourbe::zone() const
{
    return getLocalBounds().toFloat().reduced (12.0f, 14.0f);
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

juce::Point<float> VueCourbe::aimanter (juce::Point<float> c, const juce::ModifierKeys& mods) const
{
    if (aimant != mods.isShiftDown())
    {
        const float n = static_cast<float> (cases);
        c.x = std::round (c.x * n) / n;
        c.y = std::round (c.y * 16.0f) / 16.0f;
    }
    return c;
}

int VueCourbe::pointSous (juce::Point<float> e) const
{
    int meilleur = -1;
    float distance = rayonPrise;
    for (int i = 0; i < courbe.taille(); ++i)
    {
        const auto& p = courbe.point (i);
        const float d = versEcran (p.x, p.y).getDistanceFrom (e);
        if (d <= distance)
        {
            distance = d;
            meilleur = i;
        }
    }
    return meilleur;
}

int VueCourbe::segmentSous (float x) const
{
    int i = courbe.taille() - 1;
    for (int k = 0; k < courbe.taille(); ++k)
        if (courbe.point (k).x > x)
        {
            i = k - 1;
            break;
        }
    return i < 0 ? courbe.taille() - 1 : i;
}

void VueCourbe::modifie()
{
    repaint();
    if (onModif)
        onModif();
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
    g.setColour (palette::panneau);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);

    // Grille : cases fines, temps plus marques, mesures encore plus.
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

    // Courbe de base, remplie.
    const auto trait = chemin (courbe);
    juce::Path remplissage (trait);
    remplissage.lineTo (z.getRight(), z.getBottom());
    remplissage.lineTo (z.getX(), z.getBottom());
    remplissage.closeSubPath();
    g.setGradientFill (juce::ColourGradient (palette::accent.withAlpha (afficheJouee ? 0.15f : 0.35f), z.getX(), z.getY(),
                                             palette::accent.withAlpha (0.02f), z.getX(), z.getBottom(), false));
    g.fillPath (remplissage);
    g.setColour (palette::accent.withAlpha (afficheJouee ? 0.55f : 1.0f));
    g.strokePath (trait, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Courbe jouee par l'evolution, par-dessus.
    if (afficheJouee)
    {
        g.setColour (palette::accent2);
        g.strokePath (chemin (jouee), juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Tete de lecture.
    const auto& lue = afficheJouee ? jouee : courbe;
    const auto tete = versEcran (lecture, lue.valeur (lecture));
    g.setColour (palette::accent2.withAlpha (0.45f));
    g.drawVerticalLine (juce::roundToInt (tete.x), z.getY(), z.getBottom());
    g.setColour (palette::accent2);
    g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (tete));

    // Points de la courbe de base.
    for (int i = 0; i < courbe.taille(); ++i)
    {
        const auto& p = courbe.point (i);
        const auto e = versEcran (p.x, p.y);
        const bool actif = i == survole || i == choisi;
        const float r = actif ? rayonPoint + 2.0f : rayonPoint;
        g.setColour (palette::fond);
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (e));
        g.setColour (actif ? palette::texte : palette::accent);
        if (p.type == courbes::Segment::marche)
            g.drawRect (juce::Rectangle<float> (r * 1.8f, r * 1.8f).withCentre (e), 2.0f);
        else
            g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (e), 2.0f);
    }
}

void VueCourbe::mouseMove (const juce::MouseEvent& e)
{
    const int s = pointSous (e.position);
    if (s != survole)
    {
        survole = s;
        repaint();
    }
    setMouseCursor (s >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::UpDownResizeCursor);
}

void VueCourbe::mouseDown (const juce::MouseEvent& e)
{
    choisi = pointSous (e.position);
    if (e.mods.isPopupMenu())
    {
        if (choisi >= 0)
            menuPoint (choisi);
        return;
    }
    if (choisi >= 0)
        glisse = Glisse::point;
    else if (courbe.taille() >= 2)
    {
        choisi = segmentSous (juce::jlimit (0.0f, 0.9999f, versCourbe (e.position).x));
        tensionDepart = courbe.point (choisi).tension;
        glisse = Glisse::tension;
    }
    repaint();
}

void VueCourbe::mouseDrag (const juce::MouseEvent& e)
{
    if (choisi < 0 || choisi >= courbe.taille())
        return;

    if (glisse == Glisse::point)
    {
        const auto c = aimanter (versCourbe (e.position), e.mods);
        // Le point reste entre ses voisins : l'ordre des points ne change jamais en glissant.
        const float gauche = choisi > 0 ? courbe.point (choisi - 1).x : 0.0f;
        const float droite = choisi < courbe.taille() - 1 ? courbe.point (choisi + 1).x : 0.9999f;
        auto& p = courbe.point (choisi);
        p.x = juce::jlimit (gauche, droite, c.x);
        p.y = juce::jlimit (0.0f, 1.0f, c.y);
        modifie();
    }
    else if (glisse == Glisse::tension)
    {
        auto& p = courbe.point (choisi);
        // Le sens suit la pente : tirer vers le haut bombe toujours la courbe vers le haut.
        const auto& suivant = courbe.point ((choisi + 1) % courbe.taille());
        const float sens = suivant.y >= p.y ? 1.0f : -1.0f;
        const float dy = static_cast<float> (e.getDistanceFromDragStartY()) / 150.0f;
        p.tension = juce::jlimit (-1.0f, 1.0f, tensionDepart + dy * sens);
        modifie();
    }
}

void VueCourbe::mouseUp (const juce::MouseEvent&)
{
    glisse = Glisse::rien;
    choisi = -1;
    repaint();
}

void VueCourbe::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int s = pointSous (e.position);
    if (s >= 0)
    {
        if (courbe.taille() > 2)
        {
            courbe.retirer (s);
            survole = -1;
            modifie();
        }
        return;
    }
    const auto c = aimanter (versCourbe (e.position), e.mods);
    if (c.x < 0.0f || c.x > 1.0f)
        return;
    // Le nouveau point prend la forme du segment ou il tombe.
    const auto type = courbe.taille() > 0 ? courbe.point (segmentSous (c.x)).type : courbes::Segment::courbe;
    if (courbe.ajouter (c.x, c.y, 0.0f, type))
    {
        courbe.trier();
        modifie();
    }
}

void VueCourbe::menuPoint (int index)
{
    const auto type = courbe.point (index).type;
    juce::PopupMenu m;
    m.addSectionHeader (T ("Segment qui part de ce point"));
    m.addItem (1, T ("Courbe"), true, type == courbes::Segment::courbe);
    m.addItem (2, T ("En S"), true, type == courbes::Segment::s);
    m.addItem (3, T ("Marche (tient puis saute)"), true, type == courbes::Segment::marche);
    m.addItem (4, T ("Remettre droit"), type != courbes::Segment::marche);
    m.addSeparator();
    m.addItem (5, T ("Supprimer le point"), courbe.taille() > 2);

    juce::Component::SafePointer<VueCourbe> moi (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [moi, index] (int choix)
                     {
                         if (moi == nullptr || choix == 0 || index >= moi->courbe.taille())
                             return;
                         auto& p = moi->courbe.point (index);
                         if (choix == 1) p.type = courbes::Segment::courbe;
                         if (choix == 2) p.type = courbes::Segment::s;
                         if (choix == 3) p.type = courbes::Segment::marche;
                         if (choix == 4) p.tension = 0.0f;
                         if (choix == 5) moi->courbe.retirer (index);
                         moi->choisi = moi->survole = -1;
                         moi->modifie();
                     });
}

//==============================================================================
PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p)
{
    setLookAndFeel (&style);

    titre.setText ("COURBES", juce::dontSendNotification);
    titre.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    titre.setColour (juce::Label::textColourId, palette::accent);
    addAndMakeVisible (titre);

    // En-tete : familles, formes, generation.
    for (int f = 0; f < courbes::nombreFamilles; ++f)
        familles.addItem (T (courbes::nomFamille (f)), f + 1);
    familles.setTooltip (T ("Famille de formes. Choisir une famille charge sa première forme."));
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

    relancer.setButtonText (T ("Relancer"));
    relancer.setTooltip (T ("Même réglages, nouveau tirage au sort (utile pour les familles de hasard et les gates)."));
    relancer.onClick = [this] { generer (false); };
    addAndMakeVisible (relancer);

    boutonGenerer.setButtonText (T ("Générer"));
    boutonGenerer.setTooltip (T ("Invente une nouvelle forme dans cette famille : tous les réglages au hasard."));
    boutonGenerer.setColour (juce::TextButton::buttonColourId, palette::accent.withAlpha (0.3f));
    boutonGenerer.onClick = [this] { generer (true); };
    addAndMakeVisible (boutonGenerer);

    nomMembre.setColour (juce::Label::textColourId, palette::texteDoux);
    addAndMakeVisible (nomMembre);

    for (auto& r : reglagesFamille)
    {
        r.slider.setRange (0.0, 1.0);
        r.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        r.slider.setDoubleClickReturnValue (true, 0.5);
        r.slider.onValueChange = [this] { regenerer(); };
        addAndMakeVisible (r.slider);
        preparerNom (r.nom, {});
    }

    vue.onModif = [this]
    {
        modifiee = true;
        membre = -1;
        envoyerCourbe();
        majTitreMembre();
    };
    addAndMakeVisible (vue);

    aide.setText (T ("Glisser : déplacer  ·  Glisser un segment : courber  ·  Double-clic : ajouter / supprimer  ·  Clic droit : forme  ·  Maj : sans aimant"),
                  juce::dontSendNotification);
    aide.setFont (juce::FontOptions (12.0f));
    aide.setColour (juce::Label::textColourId, palette::texteDoux);
    addAndMakeVisible (aide);

    // Temps : longueur du cycle et grille.
    preparerNom (nomLongueur, T ("Longueur"));
    longueur.setTooltip (T ("Durée d'un cycle de la courbe, calée sur le tempo."));
    longueur.setTextBoxStyle (juce::Slider::TextBoxRight, false, 96, 20);
    longueur.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    longueur.setSliderSnapsToMousePosition (false);
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
    longueur.setDoubleClickReturnValue (false, 0.0);
    longueur.onDragEnd = [] {};
    addAndMakeVisible (longueur);
    attacheLongueur = std::make_unique<juce::ParameterAttachment> (
        *processor.apvts.getParameter ("longueur"), [this] (float) { majLongueurs(); }, nullptr);

    ternaire.setButtonText (T ("Ternaire"));
    preparerBascule (ternaire, attacheTernaire, "ternaire", T ("Inclure les longueurs ternaires (T) dans le slider Longueur."));
    ternaire.onClick = [this] { majLongueurs(); };

    preparerNom (nomGrille, T ("Grille"));
    for (int i = 0; i < static_cast<int> (PluginProcessor::grilles().size()); ++i)
        grille.addItem (PluginProcessor::grilles()[static_cast<size_t> (i)].nom, i + 1);
    grille.setTooltip (T ("Taille d'une case de la grille (aimant et repères)."));
    attacheGrille = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, "grille", grille);
    grille.onChange = [this] { majGrille(); };
    addAndMakeVisible (grille);
    grilleTernaire.setButtonText (T ("Ternaire"));
    preparerBascule (grilleTernaire, attacheGrilleTernaire, "grille_ternaire", T ("Grille en ternaire : chaque case dure 2/3 de la valeur choisie."));
    grilleTernaire.onClick = [this] { majGrille(); };

    boutonAimant.setButtonText (T ("Aimant"));
    boutonAimant.setClickingTogglesState (true);
    boutonAimant.setToggleState (processor.aimant.load(), juce::dontSendNotification);
    boutonAimant.setTooltip (T ("Les points se collent à la grille. Maj enfoncée : l'inverse."));
    boutonAimant.onClick = [this]
    {
        processor.aimant = boutonAimant.getToggleState();
        vue.setAimant (processor.aimant.load());
    };
    addAndMakeVisible (boutonAimant);
    vue.setAimant (processor.aimant.load());

    // Sortie.
    preparer (decalage, "phase", T ("Décalage"), T ("Décale la courbe dans le temps (100 % = un cycle entier)."), 56);
    preparer (minimum, "min", T ("Minimum"), T ("Valeur envoyée quand la courbe est en bas."), 56);
    preparer (maximum, "max", T ("Maximum"), T ("Valeur envoyée quand la courbe est en haut. Plus petit que le minimum : la courbe s'inverse."), 56);
    preparer (lissage, "lissage", T ("Lissage"), T ("Adoucit les sauts de la courbe (marches, gates)."), 64);

    ecoute.setButtonText (T ("Écoute"));
    preparerBascule (ecoute, attacheEcoute, "ecoute_on", T ("Son test dont le volume suit la courbe, pour entendre la forme. Éteint = plugin muet."));
    ecoute.setColour (juce::TextButton::buttonOnColourId, palette::accent2);

    for (auto* s : { &cc, &canal })
    {
        s->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 40, 22);
        s->setColour (juce::Slider::textBoxOutlineColourId, palette::grilleForte);
        addAndMakeVisible (*s);
    }
    cc.setTooltip (T ("Numéro du contrôleur MIDI envoyé (0 à 127)."));
    canal.setTooltip (T ("Canal MIDI de l'envoi (1 à 16)."));
    attacheCc = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, "cc", cc);
    attacheCanal = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, "canal", canal);
    preparerNom (nomCc, "CC");
    preparerNom (nomCanal, T ("Canal"));

    lectureCc.setJustificationType (juce::Justification::centredRight);
    lectureCc.setFont (juce::FontOptions (16.0f));
    lectureCc.setColour (juce::Label::textColourId, palette::accent2);
    lectureCc.setTooltip (T ("Dernière valeur envoyée (0 à 127)."));
    addAndMakeVisible (lectureCc);

    // Evolution.
    evolution.setButtonText (T ("Évolution"));
    preparerBascule (evolution, attacheEvolution, "evolution",
                     T ("Allumé : à chaque cycle, la courbe peut rester, revenir à ta courbe, varier ou changer de famille, selon les chances réglées dessous."));
    evolution.setColour (juce::TextButton::buttonOnColourId, palette::accent2);
    evolCycles.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 36, 22);
    evolCycles.setColour (juce::Slider::textBoxOutlineColourId, palette::grilleForte);
    evolCycles.setTooltip (T ("Nombre de cycles entre deux tirages au sort."));
    attacheEvolCycles = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, "evol_cycles", evolCycles);
    addAndMakeVisible (evolCycles);
    preparerNom (nomEvolCycles, T ("Tirage tous les (cycles)"));
    preparerNom (titreIssues, T ("À chaque tirage, chances de…"));
    titreIssues.setColour (juce::Label::textColourId, palette::texte);
    preparer (garder, "p_garder", T ("Garder"), T ("Chance de garder la courbe en cours."), 48);
    preparer (revenir, "p_revenir", T ("Revenir"), T ("Chance de revenir à ta courbe (celle de l'éditeur)."), 48);
    preparer (varier, "p_varier", T ("Varier"), T ("Chance d'inventer une variante dans la même famille."), 48);
    preparer (changer, "p_changer", T ("Changer"), T ("Chance de passer à une autre famille (choisie avec les chances par famille, dessous)."), 48);
    preparer (intensite, "intensite", T ("Intensité"), T ("À quel point les variantes s'éloignent : 0 = même réglages, nouveau tirage ; 100 = tout au hasard."), 48);
    preparerNom (titrePoids, T ("Chances par famille (pour « Changer »)"));
    titrePoids.setColour (juce::Label::textColourId, palette::texte);
    for (int f = 0; f < courbes::nombreFamilles; ++f)
        preparer (poids[static_cast<size_t> (f)], "poids_" + juce::String (f), T (courbes::nomFamille (f)),
                  T ("Chance de choisir cette famille quand l'évolution change de famille. 0 = jamais."), 44);
    joue.setColour (juce::Label::textColourId, palette::accent2);
    joue.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (joue);

    // Etat de depart.
    famille = processor.getFamille();
    membre = processor.getMembre();
    reglage = processor.getReglage();
    familles.setSelectedId (famille + 1, juce::dontSendNotification);
    vue.setCourbe (processor.getCourbe());
    versionConnue = processor.getVersionCourbe();
    afficherReglages();
    majTitreMembre();
    majLongueurs();

    // Lire la taille gardee AVANT setResizeLimits : il redimensionne la fenetre (encore vide) au minimum,
    // et resized() ecraserait la taille gardee. Piege verifie par le test "interface".
    const int width = processor.editorWidth.load(), height = processor.editorHeight.load();
    setResizable (true, true);
    setResizeLimits (900, 620, 2200, 1500);
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

void PluginEditor::preparerNom (juce::Label& l, const juce::String& texte)
{
    l.setText (texte, juce::dontSendNotification);
    l.setFont (juce::FontOptions (13.0f));
    l.setColour (juce::Label::textColourId, palette::texteDoux);
    l.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (l);
}

void PluginEditor::preparer (Reglette& r, const juce::String& id, const juce::String& nom, const juce::String& aideTexte, int largeurTexte)
{
    r.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, largeurTexte, 20);
    r.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    r.slider.setTooltip (aideTexte + T ("\nDouble-clic : valeur par défaut."));
    r.attache = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, id, r.slider);
    if (auto* param = processor.apvts.getParameter (id))
        r.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    addAndMakeVisible (r.slider);
    preparerNom (r.nom, nom);
}

void PluginEditor::preparerBascule (juce::TextButton& b, std::unique_ptr<AttacheBouton>& a, const juce::String& id, const juce::String& aideTexte)
{
    b.setClickingTogglesState (true);
    b.setTooltip (aideTexte);
    a = std::make_unique<AttacheBouton> (processor.apvts, id, b);
    addAndMakeVisible (b);
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

void PluginEditor::choisirMembre (int f, int m)
{
    famille = juce::jlimit (0, courbes::nombreFamilles - 1, f);
    membre = juce::jlimit (0, courbes::membresParFamille - 1, m);
    reglage = courbes::membre (famille, membre);
    courbes::Courbe c;
    courbes::generer (famille, reglage, c);
    vue.setCourbe (c);
    modifiee = false;
    familles.setSelectedId (famille + 1, juce::dontSendNotification);
    afficherReglages();
    envoyerCourbe();
    majTitreMembre();
}

void PluginEditor::generer (bool toutAuHasard)
{
    auto& hasard = juce::Random::getSystemRandom();
    reglage.graine = static_cast<uint32_t> (hasard.nextInt (1 << 30)) + 1u;
    if (toutAuHasard)
        for (auto& v : reglage.v)
            v = hasard.nextFloat();
    courbes::Courbe c;
    courbes::generer (famille, reglage, c);
    membre = -1;
    vue.setCourbe (c);
    modifiee = false;
    afficherReglages();
    envoyerCourbe();
    majTitreMembre();
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
    vue.setCourbe (c);
    envoyerCourbe();
    majTitreMembre();
}

void PluginEditor::envoyerCourbe()
{
    processor.setCourbe (vue.getCourbe(), famille, membre, reglage);
    versionConnue = processor.getVersionCourbe();
}

void PluginEditor::majTitreMembre()
{
    juce::String t;
    if (membre >= 0)
        t << T ("Forme ") << (membre + 1) << " / " << courbes::membresParFamille;
    else
        t << T ("Forme réglée");
    if (modifiee)
        t << T ("  ·  modifiée à la main");
    nomMembre.setText (t, juce::dontSendNotification);
}

void PluginEditor::majLongueurs()
{
    const bool avecTernaire = processor.apvts.getRawParameterValue ("ternaire")->load() > 0.5f;
    ternaireConnu = avecTernaire;
    longueursAutorisees.clear();
    const auto& l = PluginProcessor::longueurs();
    for (int i = 0; i < static_cast<int> (l.size()); ++i)
        if (avecTernaire || ! l[static_cast<size_t> (i)].ternaire)
            longueursAutorisees.push_back (i);

    const int indexJoue = PluginProcessor::longueurJouee (juce::roundToInt (processor.apvts.getRawParameterValue ("longueur")->load()), avecTernaire);
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
    const bool avecTernaire = processor.apvts.getRawParameterValue ("ternaire")->load() > 0.5f;
    const int index = PluginProcessor::longueurJouee (juce::roundToInt (processor.apvts.getRawParameterValue ("longueur")->load()), avecTernaire);
    const double cycle = PluginProcessor::enNoires (PluginProcessor::longueurs()[static_cast<size_t> (index)], npm);
    const auto& gl = PluginProcessor::grilles();
    const int g = juce::jlimit (0, static_cast<int> (gl.size()) - 1, juce::roundToInt (processor.apvts.getRawParameterValue ("grille")->load()));
    double caseNoires = gl[static_cast<size_t> (g)].noires;
    if (processor.apvts.getRawParameterValue ("grille_ternaire")->load() > 0.5f)
        caseNoires *= 2.0 / 3.0;
    vue.setGrille (cycle / caseNoires, 1.0 / caseNoires, npm / caseNoires);
}

void PluginEditor::timerCallback()
{
    // Courbe changee ailleurs (projet recharge, preset du DAW) : on la reprend.
    if (processor.getVersionCourbe() != versionConnue && ! vue.enTrainDeModifier())
    {
        versionConnue = processor.getVersionCourbe();
        famille = processor.getFamille();
        membre = processor.getMembre();
        reglage = processor.getReglage();
        modifiee = false;
        familles.setSelectedId (famille + 1, juce::dontSendNotification);
        vue.setCourbe (processor.getCourbe());
        afficherReglages();
        majTitreMembre();
    }

    // Courbe jouee par l'evolution.
    const bool evolutionActive = processor.apvts.getRawParameterValue ("evolution")->load() > 0.5f;
    if (processor.getVersionJouee() != versionJoueeConnue)
    {
        versionJoueeConnue = processor.getVersionJouee();
        courbes::Courbe c;
        int f = 0;
        processor.getCourbeJouee (c, f);
        bool pareille = c.taille() == vue.getCourbe().taille();
        for (int i = 0; pareille && i < c.taille(); ++i)
            pareille = std::abs (c.point (i).x - vue.getCourbe().point (i).x) < 1.0e-5f
                       && std::abs (c.point (i).y - vue.getCourbe().point (i).y) < 1.0e-5f;
        vue.setJouee (pareille ? nullptr : &c);
        joue.setText (pareille ? T ("Joue : ta courbe") : T ("Joue : ") + T (courbes::nomFamille (f)) + T (" (variante)"),
                      juce::dontSendNotification);
    }
    joue.setVisible (evolutionActive);

    const bool avecTernaire = processor.apvts.getRawParameterValue ("ternaire")->load() > 0.5f;
    if (avecTernaire != ternaireConnu)
        majLongueurs();
    else
        majGrille();

    vue.setLecture (processor.phaseAffichee.load(), processor.valeurAffichee.load());
    const int v = processor.ccAffiche.load();
    lectureCc.setText ("CC " + juce::String (juce::roundToInt (processor.apvts.getRawParameterValue ("cc")->load()))
                           + T ("  →  ") + (v < 0 ? juce::String ("-") : juce::String (v)),
                       juce::dontSendNotification);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (palette::fond);
    g.setColour (palette::panneau);
    g.fillRoundedRectangle (zoneEvolution.toFloat(), 8.0f);
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

    auto area = getLocalBounds().reduced (16);

    // Panneau Evolution a droite.
    zoneEvolution = area.removeFromRight (290);
    area.removeFromRight (14);
    {
        auto e = zoneEvolution.reduced (12);
        auto ligne = e.removeFromTop (30);
        evolution.setBounds (ligne.removeFromLeft (110));
        ligne.removeFromLeft (10);
        joue.setBounds (ligne);
        e.removeFromTop (8);
        ligne = e.removeFromTop (24);
        nomEvolCycles.setBounds (ligne.removeFromLeft (160));
        evolCycles.setBounds (ligne.removeFromLeft (100));
        e.removeFromTop (8);
        titreIssues.setBounds (e.removeFromTop (18));
        for (auto* r : { &garder, &revenir, &varier, &changer, &intensite })
            placer (*r, e.removeFromTop (24), true, 72);
        e.removeFromTop (10);
        titrePoids.setBounds (e.removeFromTop (18));
        const int h = juce::jlimit (16, 24, e.getHeight() / courbes::nombreFamilles);
        for (auto& r : poids)
            placer (r, e.removeFromTop (h), true, 112);
    }

    // En-tete.
    auto haut = area.removeFromTop (34);
    titre.setBounds (haut.removeFromLeft (130));
    familles.setBounds (haut.removeFromLeft (180).reduced (0, 2));
    haut.removeFromLeft (8);
    precedent.setBounds (haut.removeFromLeft (32).reduced (0, 2));
    haut.removeFromLeft (4);
    suivant.setBounds (haut.removeFromLeft (32).reduced (0, 2));
    boutonGenerer.setBounds (haut.removeFromRight (100).reduced (0, 2));
    haut.removeFromRight (8);
    relancer.setBounds (haut.removeFromRight (90).reduced (0, 2));
    haut.removeFromLeft (12);
    nomMembre.setBounds (haut);

    area.removeFromTop (8);
    {
        auto ligne = area.removeFromTop (40);
        const int w = ligne.getWidth() / courbes::nombreReglages;
        for (auto& r : reglagesFamille)
            placer (r, ligne.removeFromLeft (w).withTrimmedRight (16), false, 0);
    }

    // Bas : temps, sortie.
    auto bas = area.removeFromBottom (140);
    {
        auto ligne = bas.removeFromTop (40);
        auto gauche = ligne.removeFromLeft (ligne.getWidth() * 55 / 100);
        nomLongueur.setBounds (gauche.removeFromTop (16));
        ternaire.setBounds (gauche.removeFromRight (84).reduced (0, 1));
        gauche.removeFromRight (8);
        longueur.setBounds (gauche);
        ligne.removeFromLeft (20);
        nomGrille.setBounds (ligne.removeFromTop (16));
        grille.setBounds (ligne.removeFromLeft (80).reduced (0, 1));
        ligne.removeFromLeft (8);
        grilleTernaire.setBounds (ligne.removeFromLeft (84).reduced (0, 1));
        ligne.removeFromLeft (8);
        boutonAimant.setBounds (ligne.removeFromLeft (76).reduced (0, 1));
    }
    bas.removeFromTop (10);
    {
        auto ligne = bas.removeFromTop (40);
        const int w = ligne.getWidth() / 4;
        for (auto* r : { &decalage, &minimum, &maximum, &lissage })
            placer (*r, ligne.removeFromLeft (w).withTrimmedRight (16), false, 0);
    }
    bas.removeFromTop (12);
    {
        auto ligne = bas.removeFromTop (28);
        ecoute.setBounds (ligne.removeFromLeft (100));
        ligne.removeFromLeft (24);
        nomCc.setBounds (ligne.removeFromLeft (28));
        cc.setBounds (ligne.removeFromLeft (110));
        ligne.removeFromLeft (18);
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
