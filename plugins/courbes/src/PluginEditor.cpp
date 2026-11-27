#include "PluginEditor.h"

namespace
{
// Texte accentue : toujours passer par fromUTF8, jamais une chaine brute dans juce::String.
juce::String T (const char* utf8) { return juce::String::fromUTF8 (utf8); }

constexpr float rayonPoint = 5.0f;
constexpr float rayonPrise = 9.0f;
} // namespace

//==============================================================================
VueCourbe::VueCourbe()
{
    setRepaintsOnMouseActivity (false);
    setTooltip (T ("Glisser un point : le déplacer. Glisser un segment verticalement : le courber.\n"
                   "Double-clic : ajouter ou supprimer un point. Clic droit sur un point : forme du segment.\n"
                   "Maj enfoncée : aimanter à la grille."));
}

void VueCourbe::setCourbe (const courbes::Courbe& c)
{
    courbe = c;
    survole = choisi = -1;
    repaint();
}

void VueCourbe::setLecture (float phase, float v)
{
    lecture = phase;
    valeur = v;
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
    // Segment qui contient x : il part du dernier point dont x <= x (ou du dernier point, s'il boucle).
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

void VueCourbe::paint (juce::Graphics& g)
{
    const auto z = zone();
    g.setColour (palette::panneau);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    // Grille : 16 divisions, plus marquees tous les quarts.
    for (int i = 0; i <= 16; ++i)
    {
        const float x = z.getX() + z.getWidth() * static_cast<float> (i) / 16.0f;
        g.setColour (i % 4 == 0 ? palette::grilleForte : palette::grille);
        g.drawVerticalLine (juce::roundToInt (x), z.getY(), z.getBottom());
    }
    for (int i = 0; i <= 4; ++i)
    {
        const float y = z.getY() + z.getHeight() * static_cast<float> (i) / 4.0f;
        g.setColour (i % 2 == 0 ? palette::grilleForte : palette::grille);
        g.drawHorizontalLine (juce::roundToInt (y), z.getX(), z.getRight());
    }

    // La courbe, echantillonnee au pixel.
    juce::Path trait;
    const int largeur = juce::jmax (2, juce::roundToInt (z.getWidth()));
    for (int px = 0; px <= largeur; ++px)
    {
        const float x = static_cast<float> (px) / static_cast<float> (largeur);
        const auto e = versEcran (x, courbe.valeur (juce::jmin (x, 0.99999f)));
        if (px == 0)
            trait.startNewSubPath (e);
        else
            trait.lineTo (e);
    }
    juce::Path remplissage (trait);
    remplissage.lineTo (z.getRight(), z.getBottom());
    remplissage.lineTo (z.getX(), z.getBottom());
    remplissage.closeSubPath();
    g.setGradientFill (juce::ColourGradient (palette::accent.withAlpha (0.35f), z.getX(), z.getY(),
                                             palette::accent.withAlpha (0.02f), z.getX(), z.getBottom(), false));
    g.fillPath (remplissage);
    g.setColour (palette::accent);
    g.strokePath (trait, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Tete de lecture.
    const auto tete = versEcran (lecture, courbe.valeur (lecture));
    g.setColour (palette::accent2.withAlpha (0.5f));
    g.drawVerticalLine (juce::roundToInt (tete.x), z.getY(), z.getBottom());
    g.setColour (palette::accent2);
    g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (tete));

    // Points.
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
        auto c = versCourbe (e.position);
        if (e.mods.isShiftDown())
        {
            c.x = std::round (c.x * 16.0f) / 16.0f;
            c.y = std::round (c.y * 8.0f) / 8.0f;
        }
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
    auto c = versCourbe (e.position);
    if (e.mods.isShiftDown())
    {
        c.x = std::round (c.x * 16.0f) / 16.0f;
        c.y = std::round (c.y * 8.0f) / 8.0f;
    }
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
    style.setColour (juce::Slider::rotarySliderFillColourId, palette::accent);
    style.setColour (juce::Slider::rotarySliderOutlineColourId, palette::grilleForte);
    style.setColour (juce::Slider::thumbColourId, palette::texte);
    style.setColour (juce::Slider::textBoxTextColourId, palette::texte);
    style.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    style.setColour (juce::Label::textColourId, palette::texte);
    style.setColour (juce::ComboBox::backgroundColourId, palette::panneau);
    style.setColour (juce::ComboBox::outlineColourId, palette::grilleForte);
    style.setColour (juce::ComboBox::textColourId, palette::texte);
    style.setColour (juce::ComboBox::arrowColourId, palette::accent);
    style.setColour (juce::TextButton::buttonColourId, palette::panneau);
    style.setColour (juce::TextButton::textColourOffId, palette::texte);
    style.setColour (juce::PopupMenu::backgroundColourId, palette::panneau);
    style.setColour (juce::PopupMenu::highlightedBackgroundColourId, palette::accent.withAlpha (0.3f));
    style.setColour (juce::TooltipWindow::backgroundColourId, palette::panneau.brighter (0.15f));
    style.setColour (juce::TooltipWindow::textColourId, palette::texte);
    setLookAndFeel (&style);

    titre.setText (T ("Courbes"), juce::dontSendNotification);
    titre.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    titre.setColour (juce::Label::textColourId, palette::accent);
    addAndMakeVisible (titre);

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

    boutonGenerer.setButtonText (T ("Générer"));
    boutonGenerer.setTooltip (T ("Invente une nouvelle forme dans cette famille (remplace la courbe actuelle)."));
    boutonGenerer.setColour (juce::TextButton::buttonColourId, palette::accent.withAlpha (0.25f));
    boutonGenerer.onClick = [this] { generer(); };
    addAndMakeVisible (boutonGenerer);

    nomMembre.setJustificationType (juce::Justification::centredLeft);
    nomMembre.setColour (juce::Label::textColourId, palette::texteDoux);
    addAndMakeVisible (nomMembre);

    vue.onModif = [this]
    {
        modifiee = true;
        envoyerCourbe();
        majTitreMembre();
    };
    addAndMakeVisible (vue);

    aide.setText (T ("Glisser : déplacer  ·  Glisser un segment : courber  ·  Double-clic : ajouter / supprimer  ·  Clic droit : forme  ·  Maj : grille"),
                  juce::dontSendNotification);
    aide.setFont (juce::FontOptions (12.0f));
    aide.setColour (juce::Label::textColourId, palette::texteDoux);
    addAndMakeVisible (aide);

    vitesse.addItemList (PluginProcessor::nomsVitesses(), 1);
    vitesse.setTooltip (T ("Durée d'un cycle de la courbe, calée sur le tempo du morceau."));
    attacheVitesse = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, "vitesse", vitesse);
    addAndMakeVisible (vitesse);
    nomVitesse.setText (T ("Vitesse"), juce::dontSendNotification);
    nomVitesse.setJustificationType (juce::Justification::centred);
    nomVitesse.setColour (juce::Label::textColourId, palette::texteDoux);
    addAndMakeVisible (nomVitesse);

    preparerPotard (decalage, "phase", T ("Décalage"), T ("Décale la courbe dans le temps (100 % = un cycle entier)."));
    preparerPotard (minimum, "min", T ("Minimum"), T ("Valeur envoyée quand la courbe est en bas."));
    preparerPotard (maximum, "max", T ("Maximum"), T ("Valeur envoyée quand la courbe est en haut. Plus petit que le minimum : la courbe s'inverse."));
    preparerPotard (lissage, "lissage", T ("Lissage"), T ("Adoucit les sauts de la courbe (marches, portes)."));
    preparerPotard (ecoute, "ecoute", T ("Écoute"), T ("Son test dont le volume suit la courbe, pour entendre la forme. Laisser à 0 en usage normal."));
    preparerPotard (cc, "cc", T ("CC"), T ("Numéro du contrôleur MIDI envoyé (0 à 127)."));
    preparerPotard (canal, "canal", T ("Canal"), T ("Canal MIDI de l'envoi (1 à 16)."));

    lectureCc.setJustificationType (juce::Justification::centredRight);
    lectureCc.setFont (juce::FontOptions (15.0f));
    lectureCc.setColour (juce::Label::textColourId, palette::accent2);
    lectureCc.setTooltip (T ("Dernière valeur envoyée (0 à 127)."));
    addAndMakeVisible (lectureCc);

    famille = processor.getFamille();
    membre = processor.getMembre();
    graine = processor.getGraine();
    familles.setSelectedId (famille + 1, juce::dontSendNotification);
    vue.setCourbe (processor.getCourbe());
    versionConnue = processor.getVersionCourbe();
    majTitreMembre();

    // Lire la taille gardee AVANT setResizeLimits : il redimensionne la fenetre (encore vide) au minimum,
    // et resized() ecraserait la taille gardee. Piege verifie par le test "interface".
    const int width = processor.editorWidth.load(), height = processor.editorHeight.load();
    setResizable (true, true);
    setResizeLimits (480, 320, 2000, 1400);
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

void PluginEditor::preparerPotard (Potard& p, const juce::String& id, const juce::String& nom, const juce::String& aideTexte)
{
    p.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    p.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    p.slider.setColour (juce::Slider::textBoxBackgroundColourId, palette::panneau);
    p.slider.setTooltip (aideTexte + T ("\nDouble-clic : valeur par défaut."));
    p.attache = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, id, p.slider);
    if (auto* param = processor.apvts.getParameter (id))
        p.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    addAndMakeVisible (p.slider);
    p.nom.setText (nom, juce::dontSendNotification);
    p.nom.setJustificationType (juce::Justification::centred);
    p.nom.setColour (juce::Label::textColourId, palette::texteDoux);
    addAndMakeVisible (p.nom);
}

void PluginEditor::choisirMembre (int f, int m)
{
    famille = juce::jlimit (0, courbes::nombreFamilles - 1, f);
    membre = juce::jlimit (0, courbes::membresParFamille - 1, m);
    const auto r = courbes::membre (famille, membre);
    graine = r.graine;
    courbes::Courbe c;
    courbes::generer (famille, r, c);
    vue.setCourbe (c);
    modifiee = false;
    familles.setSelectedId (famille + 1, juce::dontSendNotification);
    envoyerCourbe();
    majTitreMembre();
}

void PluginEditor::generer()
{
    auto& hasard = juce::Random::getSystemRandom();
    graine = static_cast<uint32_t> (hasard.nextInt (1 << 30)) + 1u;
    courbes::Hasard h (graine);
    const courbes::Reglage r { h.suivant(), h.suivant(), h.suivant(), graine };
    courbes::Courbe c;
    courbes::generer (famille, r, c);
    membre = -1;
    vue.setCourbe (c);
    modifiee = false;
    envoyerCourbe();
    majTitreMembre();
}

void PluginEditor::envoyerCourbe()
{
    processor.setCourbe (vue.getCourbe(), famille, membre, graine);
    versionConnue = processor.getVersionCourbe();
}

void PluginEditor::majTitreMembre()
{
    juce::String t = T (courbes::nomFamille (famille)) + "  ";
    if (membre >= 0)
        t << (membre + 1) << " / " << courbes::membresParFamille;
    else
        t << T ("générée n° ") << static_cast<juce::int64> (graine % 10000u);
    if (modifiee)
        t << T ("  (modifiée)");
    nomMembre.setText (t, juce::dontSendNotification);
}

void PluginEditor::timerCallback()
{
    // Courbe changee ailleurs (projet recharge, preset du DAW) : on la reprend.
    if (processor.getVersionCourbe() != versionConnue && ! vue.enTrainDeModifier())
    {
        versionConnue = processor.getVersionCourbe();
        famille = processor.getFamille();
        membre = processor.getMembre();
        graine = processor.getGraine();
        modifiee = false;
        familles.setSelectedId (famille + 1, juce::dontSendNotification);
        vue.setCourbe (processor.getCourbe());
        majTitreMembre();
    }
    vue.setLecture (processor.phaseAffichee.load(), processor.valeurAffichee.load());
    const int v = processor.ccAffiche.load();
    lectureCc.setText ("CC " + juce::String (juce::roundToInt (processor.apvts.getRawParameterValue ("cc")->load()))
                           + T ("  →  ") + (v < 0 ? juce::String ("-") : juce::String (v)),
                       juce::dontSendNotification);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (palette::fond);
}

void PluginEditor::resized()
{
    if (ready)
    {
        processor.editorWidth = getWidth();
        processor.editorHeight = getHeight();
    }

    auto area = getLocalBounds().reduced (14);

    auto haut = area.removeFromTop (32);
    titre.setBounds (haut.removeFromLeft (150));
    familles.setBounds (haut.removeFromLeft (140).reduced (0, 2));
    haut.removeFromLeft (8);
    precedent.setBounds (haut.removeFromLeft (30).reduced (0, 2));
    haut.removeFromLeft (4);
    suivant.setBounds (haut.removeFromLeft (30).reduced (0, 2));
    boutonGenerer.setBounds (haut.removeFromRight (100).reduced (0, 2));
    haut.removeFromLeft (10);
    nomMembre.setBounds (haut);

    area.removeFromTop (8);
    auto bas = area.removeFromBottom (juce::jlimit (90, 130, area.getHeight() / 4));
    area.removeFromBottom (4);
    aide.setBounds (area.removeFromBottom (18));
    area.removeFromBottom (4);
    vue.setBounds (area);

    // Rangee de reglages : vitesse, puis les potards, puis la lecture du CC.
    lectureCc.setBounds (bas.removeFromRight (110).withSizeKeepingCentre (110, 24));
    const int largeurVitesse = 120;
    const int w = (bas.getWidth() - largeurVitesse) / 7;
    auto colonne = [&bas, w] { return bas.removeFromLeft (w); };

    auto v = bas.removeFromLeft (largeurVitesse);
    nomVitesse.setBounds (v.removeFromTop (18));
    vitesse.setBounds (v.withSizeKeepingCentre (v.getWidth() - 8, 26));

    for (auto* p : { &decalage, &minimum, &maximum, &lissage, &ecoute, &cc, &canal })
    {
        auto c = colonne();
        p->nom.setBounds (c.removeFromTop (18));
        p->slider.setBounds (c);
    }
}
