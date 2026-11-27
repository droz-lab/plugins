// La fenetre du centre : une seule page, une grille de cartes centree. Par plugin : sa fiche en clair, sa
// version, un bouton. Pas de bouton Actualiser : le catalogue se relit quand la fenetre revient devant (ou F5). Habillage : droz::ui uniquement (docs/HABILLAGE.md). Tout travail long passe par un fil a part,
// un a la fois.
#pragma once

#include "Moteur.h"
#include "Systeme.h"

#include <droz/ui.h>
#include <juce_data_structures/juce_data_structures.h>

#include <functional>
#include <map>

namespace centre
{
namespace ui = droz::ui;

//==============================================================================
/** Une carte par plugin, tout centre : auteur, nom, fiche, etat, bouton. */
class Carte final : public juce::Component
{
public:
    static constexpr int hauteur = 208, largeurMin = 288;

    std::function<void (const juce::String& nom, bool installer)> surAction;

    Carte()
    {
        principal.setTooltip (fr ("Pose le plugin dans le dossier VST3 choisi. Windows demande les droits à chaque fois."));
        retirer.setTooltip (fr ("Retire le plugin du dossier VST3."));
        retirer.setButtonText (fr ("Désinstaller"));
        principal.onClick = [this] { cliquer (true); };
        retirer.onClick = [this] { cliquer (false); };
        addAndMakeVisible (principal);
        addAndMakeVisible (retirer);
    }

    void regler (const Plugin& p, bool occupe, bool bloque, const Resultat& dernier, bool catalogueLu)
    {
        plugin = p;
        avecCatalogue = catalogueLu;
        enCours = occupe;
        resultat = dernier;
        const auto etat = p.etat();
        principal.setVisible (p.dansCatalogue && etat != Etat::installe);
        principal.setButtonText (resultat.verrouille ? fr ("Réessayer") : etat == Etat::miseAJour ? fr ("Mettre à jour") : fr ("Installer"));
        retirer.setVisible (p.installe);
        principal.setEnabled (! bloque);
        retirer.setEnabled (! bloque);
        resized();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto zone = getLocalBounds();
        g.setColour (ui::colours::panel);
        g.fillRoundedRectangle (zone.toFloat(), ui::metrics::corner);
        const auto dedans = zone.reduced (ui::metrics::margin);
        int y = dedans.getY();
        const auto ligne = [&] (int h) { const auto r = dedans.withY (y).withHeight (h); y += h; return r; };

        // Qui, et l'etiquette des chantiers.
        juce::StringArray qui;
        if (plugin.auteur.isNotEmpty())
            qui.add ("par " + plugin.auteur);
        if (plugin.essai)
            qui.add ("essai");
        g.setColour (ui::colours::dim);
        g.setFont (ui::caps (9.0f));
        g.drawText (ui::upper (qui.joinIntoString ("  -  ")), ligne (12), juce::Justification::centred, true);
        y += 8;

        // Nom, sans le prefixe "Droz" : il est deja dans l'en-tete.
        const auto nom = plugin.nom.startsWithIgnoreCase ("droz ") ? plugin.nom.substring (5) : plugin.nom;
        g.setColour (ui::colours::text);
        g.setFont (ui::caps (15.0f));
        g.drawText (ui::upper (nom), ligne (20), juce::Justification::centred, true);
        y += 12;

        // La fiche en clair, ou ce qui vient de se passer si ca a rate.
        const bool echec = resultat.message.isNotEmpty() && ! resultat.ok;
        const auto fiche = echec ? resultat.message
                         : plugin.fiche.isNotEmpty() ? plugin.fiche
                         : plugin.dansCatalogue ? plugin.description
                         : avecCatalogue ? fr ("Absent du catalogue.") : juce::String();
        g.setColour (echec ? ui::colours::text : ui::colours::dim);
        g.setFont (ui::body (13.0f));
        g.drawFittedText (fiche, ligne (52), juce::Justification::centredTop, 3, 1.0f);
        y += 8;

        if (! plugin.reglages.isEmpty() && ! echec)
        {
            g.setColour (ui::colours::dim);
            g.setFont (ui::body (11.0f));
            g.drawText (plugin.reglages.joinIntoString ("  -  "), dedans.withY (y).withHeight (14), juce::Justification::centred, true);
        }

        // Etat, juste au-dessus du bouton.
        const auto etat = plugin.etat();
        juce::String un;
        auto couleur = ui::colours::dim;
        if (enCours)
        {
            un = "en cours...";
            couleur = ui::colours::accent;
        }
        else if (etat == Etat::miseAJour)
        {
            un = fr ("mise à jour  ") + (plugin.version != plugin.versionInstallee ? plugin.versionInstallee + " > " + plugin.version : juce::String ("nouvel essai"));
            couleur = ui::colours::accent; // l'orange dit "il y a quelque chose a faire ici"
        }
        else if (etat == Etat::installe)
        {
            un = fr ("installé  ") + plugin.versionInstallee;
            couleur = ui::colours::text;
        }
        else
        {
            un = "version " + plugin.version;
        }
        g.setColour (couleur);
        g.setFont (ui::caps (9.0f));
        g.drawText (ui::upper (un), dedans.withTop (dedans.getBottom() - ui::metrics::row - ui::metrics::gap - 12).withHeight (12),
                    juce::Justification::centred, true);
    }

    void resized() override
    {
        auto bas = getLocalBounds().reduced (ui::metrics::margin).removeFromBottom (ui::metrics::row);
        // Un bouton : toute la largeur. Deux : moitie-moitie.
        if (principal.isVisible() && retirer.isVisible())
        {
            principal.setBounds (bas.removeFromLeft ((bas.getWidth() - ui::metrics::gap) / 2));
            bas.removeFromLeft (ui::metrics::gap);
            retirer.setBounds (bas);
        }
        else
        {
            principal.setBounds (bas);
            retirer.setBounds (bas);
        }
    }

    Plugin plugin;

private:
    // L'action refait la grille, donc detruit cette carte : on la lance apres le clic, avec une copie du nom.
    void cliquer (bool installer)
    {
        juce::MessageManager::callAsync ([action = surAction, nom = plugin.nom, installer]
        {
            if (action)
                action (nom, installer);
        });
    }

    bool enCours = false, avecCatalogue = true;
    Resultat resultat;
    juce::TextButton principal, retirer;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Carte)
};

//==============================================================================
/** La grille des cartes, par sections. Les colonnes se partagent la largeur : tout reste centre. */
class Grille final : public juce::Component
{
public:
    struct Section
    {
        juce::String titre;
        juce::OwnedArray<Carte> cartes;
        juce::Rectangle<int> zoneTitre;
    };

    void vider()
    {
        sections.clear();
    }

    Carte* ajouter (const juce::String& titre)
    {
        if (sections.isEmpty() || sections.getLast()->titre != titre)
            sections.add (new Section())->titre = titre;
        auto* carte = sections.getLast()->cartes.add (new Carte());
        addAndMakeVisible (carte);
        return carte;
    }

    bool vide() const { return sections.isEmpty(); }

    void paint (juce::Graphics& g) override
    {
        if (sections.isEmpty())
        {
            g.setColour (ui::colours::dim);
            g.setFont (ui::body (13.0f));
            g.drawFittedText (messageVide, getLocalBounds().reduced (ui::metrics::margin), juce::Justification::centred, 3);
            return;
        }
        // Titre de section : au centre, un filet de chaque cote.
        for (auto* s : sections)
        {
            const auto police = ui::caps (9.0f);
            const auto texte = ui::upper (s->titre);
            const int largeur = juce::roundToInt (juce::GlyphArrangement::getStringWidth (police, texte)) + 2 * ui::metrics::gap;
            const auto milieu = s->zoneTitre.withSizeKeepingCentre (largeur, s->zoneTitre.getHeight());
            g.setColour (ui::colours::dim);
            g.setFont (police);
            g.drawText (texte, milieu, juce::Justification::centred);
            g.setColour (ui::colours::line);
            const int y = s->zoneTitre.getCentreY();
            g.fillRect (s->zoneTitre.getX(), y, milieu.getX() - s->zoneTitre.getX(), 1);
            g.fillRect (milieu.getRight(), y, s->zoneTitre.getRight() - milieu.getRight(), 1);
        }
    }

    /** Range les cartes pour cette largeur et renvoie la hauteur totale. */
    int ranger (int largeur)
    {
        const int gap = ui::metrics::gap;
        const int colonnes = juce::jlimit (1, 4, (largeur + gap) / (Carte::largeurMin + gap));
        const int largeurCarte = (largeur - (colonnes - 1) * gap) / colonnes;
        int y = 0;
        for (auto* s : sections)
        {
            s->zoneTitre = { 0, y, largeur, hauteurTitre };
            y += hauteurTitre;
            // Une derniere rangee incomplete reste centree.
            const int n = s->cartes.size();
            for (int i = 0; i < n; ++i)
            {
                const int rangee = i / colonnes, dansRangee = juce::jmin (colonnes, n - rangee * colonnes);
                const int largeurRangee = dansRangee * largeurCarte + (dansRangee - 1) * gap;
                const int x = (largeur - largeurRangee) / 2 + (i % colonnes) * (largeurCarte + gap);
                s->cartes[i]->setBounds (x, y + rangee * (Carte::hauteur + gap), largeurCarte, Carte::hauteur);
            }
            y += ((n + colonnes - 1) / colonnes) * (Carte::hauteur + gap) + 4;
        }
        return juce::jmax (y, 160);
    }

    juce::OwnedArray<Section> sections;
    juce::String messageVide;

private:
    static constexpr int hauteurTitre = 36;
};

//==============================================================================
struct Options
{
    juce::String catalogue;  // vide : l'adresse fixe
    juce::File dossierVst3;  // vide : celui retenu, sinon celui du systeme
    juce::File dossierDonnees;
    bool synchrone = false;  // captures : tout se fait sur place, aucun reglage n'est lu ni ecrit
};

class Fenetre final : public juce::Component
{
public:
    explicit Fenetre (const Options& o) : options (o)
    {
        if (! options.synchrone)
        {
            juce::PropertiesFile::Options p;
            p.applicationName = "DrozSoftwareCenter";
            p.filenameSuffix = ".settings";
            p.folderName = "Droz";
            p.osxLibrarySubFolder = "Application Support";
            reglages = std::make_unique<juce::PropertiesFile> (p);
        }
        moteur.source = options.catalogue.isNotEmpty() ? options.catalogue : juce::String (adresseCatalogue);
        moteur.dossierVst3 = options.dossierVst3 != juce::File() ? options.dossierVst3
                           : reglages != nullptr && reglages->getValue ("dossierVst3").isNotEmpty() ? juce::File (reglages->getValue ("dossierVst3"))
                           : systeme::dossierVst3ParDefaut();
        moteur.dossierDonnees = options.dossierDonnees != juce::File() ? options.dossierDonnees
                              : reglages != nullptr ? reglages->getFile().getSiblingFile ("SoftwareCenter") : juce::File();
        dossierAffiche = moteur.dossierVst3;

        changer.setTooltip (fr ("Choisir le dossier où ton DAW lit ses VST3. Il est retenu pour la suite."));
        toutMettre.setTooltip (fr ("Met à niveau tous les plugins marqués « mise à jour », l'un après l'autre."));
        nouvelleVersion.setTooltip (fr ("Ouvre le téléchargement de la nouvelle version du centre."));
        changer.onClick = [this] { choisirDossier(); };
        toutMettre.onClick = [this] { toutMettreAJour(); };
        nouvelleVersion.onClick = [this] { juce::URL (etat.installeurCentre).launchInDefaultBrowser(); };
        addAndMakeVisible (changer);
        addChildComponent (toutMettre);
        addChildComponent (nouvelleVersion);
        vue.setViewedComponent (&grille, false);
        vue.setScrollBarsShown (true, false);
        vue.setScrollBarThickness (barre);
        addAndMakeVisible (vue);

        setLookAndFeel (&look);
        setWantsKeyboardFocus (true);
        setSize (1000, 720);
        recharger();
    }

    ~Fenetre() override
    {
        fil.removeAllJobs (true, 15000);
        setLookAndFeel (nullptr);
    }

    /** Vrai si une installation tourne : la fenetre ne doit pas se fermer au milieu. */
    bool occupee() const { return ! occupes.isEmpty(); }

    /** La fenetre revient au premier plan : on relit le catalogue, sans bouton, au plus une fois par demi-minute. */
    void revenir()
    {
        const auto maintenant = juce::Time::getMillisecondCounter();
        if (! lecture && occupes.isEmpty() && maintenant - derniereLecture > 30000)
            recharger();
    }

    bool keyPressed (const juce::KeyPress& touche) override
    {
        if (touche == juce::KeyPress::F5Key && ! lecture && occupes.isEmpty())
        {
            recharger();
            return true;
        }
        return false;
    }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        g.fillAll (ui::colours::background);
        auto zone = ui::drawHeader (g, getLocalBounds(), "Software Center", DROZ_CENTRE_VERSION);
        zone = ui::drawFooter (g, zone, pied());

        // Le dossier VST3, au centre : libelle, chemin, puis le bouton Changer (place dans resized).
        g.setColour (ui::colours::dim);
        g.setFont (ui::caps (9.0f));
        g.drawText ("DOSSIER VST3", zoneLibelle, juce::Justification::centredLeft);
        g.setColour (ui::colours::text);
        g.setFont (ui::body (13.0f));
        g.drawText (dossierAffiche.getFullPathName(), zoneChemin, juce::Justification::centredLeft, true);

        if (avis().isNotEmpty())
        {
            g.setColour (ui::colours::dim);
            g.setFont (ui::body (13.0f));
            g.drawText (avis(), zoneAvis, juce::Justification::centred, true);
        }
    }

    void resized() override
    {
        auto zone = getLocalBounds();
        zone.removeFromTop (ui::metrics::header);
        zone.removeFromBottom (ui::metrics::footer);
        // Meme marge a gauche et a droite : la barre de defilement a sa place reservee des deux cotes.
        zone.reduce (ui::metrics::margin + barre, ui::metrics::margin);

        // Dossier VST3 : un groupe centre.
        const auto ligne = zone.removeFromTop (ui::metrics::row);
        const int largeurLibelle = juce::roundToInt (juce::GlyphArrangement::getStringWidth (ui::caps (9.0f), "DOSSIER VST3")) + ui::metrics::gap;
        const int largeurBouton = 96;
        const int largeurChemin = juce::jmin (ligne.getWidth() - largeurLibelle - largeurBouton - ui::metrics::gap,
                                              juce::roundToInt (juce::GlyphArrangement::getStringWidth (ui::body (13.0f), dossierAffiche.getFullPathName())) + 4);
        auto groupe = ligne.withSizeKeepingCentre (largeurLibelle + largeurChemin + ui::metrics::gap + largeurBouton, ligne.getHeight());
        zoneLibelle = groupe.removeFromLeft (largeurLibelle);
        zoneChemin = groupe.removeFromLeft (largeurChemin);
        groupe.removeFromLeft (ui::metrics::gap);
        changer.setBounds (groupe);

        // Une ligne de plus seulement quand il y a quelque chose a dire ou a faire.
        const bool aJour = nombreAMettreAJour() == 0;
        toutMettre.setVisible (! aJour);
        nouvelleVersion.setVisible (nouvelleVersionDispo());
        zoneAvis = {};
        if (avis().isNotEmpty())
        {
            zone.removeFromTop (ui::metrics::gap);
            zoneAvis = zone.removeFromTop (ui::metrics::row);
            if (nouvelleVersion.isVisible())
            {
                const int largeurTexte = juce::roundToInt (juce::GlyphArrangement::getStringWidth (ui::body (13.0f), avis())) + 4;
                auto ensemble = zoneAvis.withSizeKeepingCentre (juce::jmin (zoneAvis.getWidth(), largeurTexte + ui::metrics::gap + 132), zoneAvis.getHeight());
                nouvelleVersion.setBounds (ensemble.removeFromRight (132));
                ensemble.removeFromRight (ui::metrics::gap);
                zoneAvis = ensemble;
            }
        }
        if (! aJour)
        {
            zone.removeFromTop (ui::metrics::gap);
            toutMettre.setBounds (zone.removeFromTop (ui::metrics::row).withSizeKeepingCentre (240, ui::metrics::row));
        }
        zone.removeFromTop (4);
        // La vue deborde a droite de la largeur de sa barre : la grille reste au centre de la fenetre.
        vue.setBounds (zone.withTrimmedRight (-barre));
        grille.setSize (zone.getWidth(), grille.ranger (zone.getWidth()));
    }

    /** Captures : rend la fenetre telle qu'elle est. */
    juce::Image capture (float echelle) { return createComponentSnapshot (getLocalBounds(), true, echelle); }

private:
    //==========================================================================
    struct Instantane
    {
        std::vector<Plugin> plugins;
        bool horsLigne = false, catalogueAncien = false;
        juce::String erreur, versionCentre, installeurCentre, genere;
        juce::File dossier;
    };

    Instantane prendre() const
    {
        return { moteur.plugins, moteur.horsLigne, moteur.catalogueAncien, moteur.erreurCatalogue,
                 moteur.versionCentre, moteur.installeurCentre, moteur.genere, moteur.dossierVst3 };
    }

    /** Fait `travail` hors du fil de l'interface (un seul a la fois), puis `fin` sur le fil de l'interface. */
    void lancer (std::function<void()> travail, std::function<void()> fin = {})
    {
        if (options.synchrone)
        {
            travail();
            appliquer (prendre());
            if (fin)
                fin();
            return;
        }
        juce::Component::SafePointer<Fenetre> moi (this);
        fil.addJob ([this, moi, travail = std::move (travail), fin = std::move (fin)]
        {
            travail(); // `this` vit jusqu'a la fin : le destructeur attend le fil
            const auto photo = prendre();
            juce::MessageManager::callAsync ([moi, photo, fin]
            {
                if (moi != nullptr)
                {
                    moi->appliquer (photo);
                    if (fin)
                        fin();
                }
            });
        });
    }

    void recharger()
    {
        lecture = true;
        derniereLecture = juce::Time::getMillisecondCounter();
        mettreAJourBoutons();
        repaint();
        lancer ([this] { moteur.charger(); }, [this]
        {
            lecture = false;
            derniers.clear();
            reconstruire();
        });
    }

    void agir (juce::String nom, bool installer) // par valeur : reconstruire() detruit les cartes
    {
        if (occupes.contains (nom))
            return;
        occupes.add (nom);
        derniers.erase (nom);
        reconstruire();
        auto resultat = std::make_shared<Resultat>();
        lancer ([this, nom, installer, resultat] { *resultat = installer ? moteur.installer (nom) : moteur.desinstaller (nom); },
                [this, nom, resultat]
                {
                    occupes.removeString (nom);
                    derniers[nom] = *resultat;
                    dernierMessage = resultat->message;
                    reconstruire();
                });
    }

    void toutMettreAJour()
    {
        for (const auto& p : etat.plugins)
            if (p.etat() == Etat::miseAJour)
                agir (p.nom, true);
    }

    void choisirDossier()
    {
        selecteur = std::make_unique<juce::FileChooser> (fr ("Dossier où ton DAW lit ses VST3"), dossierAffiche);
        selecteur->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& s)
        {
            const auto choix = s.getResult();
            if (choix == juce::File())
                return;
            if (reglages != nullptr)
                reglages->setValue ("dossierVst3", choix.getFullPathName());
            dossierAffiche = choix;
            derniers.clear();
            lancer ([this, choix]
            {
                moteur.dossierVst3 = choix;
                moteur.relireInstalles();
            }, [this] { reconstruire(); });
        });
    }

    void appliquer (const Instantane& photo)
    {
        etat = photo;
        dossierAffiche = photo.dossier;
    }

    bool nouvelleVersionDispo() const
    {
        return etat.versionCentre.isNotEmpty() && etat.installeurCentre.isNotEmpty() && comparerVersions (etat.versionCentre, DROZ_CENTRE_VERSION) > 0;
    }

    int nombreAMettreAJour() const
    {
        int n = 0;
        for (const auto& p : etat.plugins)
            n += p.etat() == Etat::miseAJour ? 1 : 0;
        return n;
    }

    juce::String avis() const
    {
        if (lecture && etat.plugins.empty())
            return {};
        if (etat.horsLigne)
            return fr ("Catalogue non lu (") + etat.erreur + "). "
                 + (etat.catalogueAncien ? fr ("Voici le dernier catalogue connu et ce qui est installé.") : fr ("Voici ce qui est installé."));
        if (nouvelleVersionDispo())
            return fr ("Une nouvelle version du centre existe : ") + etat.versionCentre;
        return {};
    }

    juce::String pied() const
    {
        if (lecture)
            return "Lecture du catalogue...";
        if (! occupes.isEmpty())
            return fr ("En cours : ") + occupes.joinIntoString (", ") + fr (". Windows peut demander les droits.");
        if (dernierMessage.isNotEmpty())
            return dernierMessage;
        return fr ("Droz Software Center, par Fx") + (etat.genere.isNotEmpty() ? fr ("  -  catalogue du ") + etat.genere : juce::String());
    }

    void mettreAJourBoutons()
    {
        const int n = nombreAMettreAJour();
        toutMettre.setButtonText (fr ("Tout mettre à jour") + (n > 1 ? " (" + juce::String (n) + ")" : juce::String()));
        toutMettre.setEnabled (! lecture && occupes.isEmpty());
        changer.setEnabled (! lecture && occupes.isEmpty());
    }

    /** Refait les cartes a partir du dernier etat connu. Sections : publies, essais, hors catalogue. */
    void reconstruire()
    {
        grille.vider();
        const bool catalogueLu = ! etat.horsLigne || etat.catalogueAncien;
        const char* const titres[] = { "Plugins", "En chantier", catalogueLu ? "Installés, absents du catalogue" : "Installés" };
        for (int section = 0; section < 3; ++section)
            for (const auto& p : etat.plugins)
            {
                const int rang = ! p.dansCatalogue ? 2 : p.essai ? 1 : 0;
                if (rang != section)
                    continue;
                auto* carte = grille.ajouter (fr (titres[section]));
                const auto trouve = derniers.find (p.nom);
                carte->regler (p, occupes.contains (p.nom), lecture || occupes.contains (p.nom),
                               trouve != derniers.end() ? trouve->second : Resultat(), catalogueLu);
                carte->surAction = [moi = juce::Component::SafePointer<Fenetre> (this)] (const juce::String& nom, bool installer)
                {
                    if (moi != nullptr)
                        moi->agir (nom, installer);
                };
            }
        grille.messageVide = etat.horsLigne ? fr ("Aucun plugin Droz dans ce dossier VST3.")
                                            : fr ("Aucun plugin pour l'instant. Chaque plugin apparaît ici dès qu'il passe les contrôles.");
        mettreAJourBoutons();
        resized();
        repaint();
    }

    //==========================================================================
    Options options;
    std::unique_ptr<juce::PropertiesFile> reglages;
    Moteur moteur;            // touche seulement depuis `fil` (ou sur place en mode synchrone)
    juce::ThreadPool fil { 1 };
    Instantane etat;          // copie pour l'interface
    juce::File dossierAffiche;
    juce::StringArray occupes;
    std::map<juce::String, Resultat> derniers;
    juce::String dernierMessage;
    bool lecture = false;
    juce::uint32 derniereLecture = 0;
    juce::Rectangle<int> zoneLibelle, zoneChemin, zoneAvis;

    ui::LookAndFeel look;
    juce::TooltipWindow bulles { this, 600 };
    juce::TextButton changer { "Changer" }, toutMettre { fr ("Tout mettre à jour") }, nouvelleVersion { fr ("Télécharger") };
    juce::Viewport vue;
    Grille grille;
    std::unique_ptr<juce::FileChooser> selecteur;

    static constexpr int barre = 8;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Fenetre)
};
} // namespace centre
