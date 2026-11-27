// La fenetre du centre : onglet Plugins (installer, mettre a jour, desinstaller) et onglet Atelier.
// Habillage : droz::ui uniquement (docs/HABILLAGE.md). Tout travail long passe par un fil a part, un a la fois.
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
/** Une ligne de l'onglet Plugins. */
class Ligne final : public juce::Component
{
public:
    static constexpr int hauteur = 64, hauteurTitre = 30;

    std::function<void (const juce::String& nom, bool installer)> surAction;

    Ligne()
    {
        principal.setTooltip (fr ("Pose le plugin dans le dossier VST3 choisi. Windows demande les droits à chaque fois."));
        retirer.setTooltip (fr ("Retire le plugin du dossier VST3."));
        retirer.setButtonText (fr ("Désinstaller"));
        principal.onClick = [this] { cliquer (true); };
        retirer.onClick = [this] { cliquer (false); };
        addAndMakeVisible (principal);
        addAndMakeVisible (retirer);
    }

    void regler (const Plugin& p, const juce::String& titreSection, bool occupe, bool bloque, const Resultat& dernier, bool catalogueLu)
    {
        plugin = p;
        avecCatalogue = catalogueLu;
        titre = titreSection;
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

    int hauteurVoulue() const { return hauteur + (titre.isNotEmpty() ? hauteurTitre : 0); }

    void paint (juce::Graphics& g) override
    {
        auto zone = getLocalBounds();
        if (titre.isNotEmpty())
        {
            g.setColour (ui::colours::dim);
            g.setFont (ui::caps (9.0f));
            g.drawText (ui::upper (titre), zone.removeFromTop (hauteurTitre).withTrimmedTop (10), juce::Justification::centredLeft);
        }
        g.setColour (ui::colours::panel);
        g.fillRoundedRectangle (zone.toFloat(), ui::metrics::corner);

        auto texte = zone.reduced (ui::metrics::gap, 0).withTrimmedRight (largeurDroite);
        auto haut = texte.withHeight (20).withY (zone.getY() + 12);
        auto nom = plugin.nom.startsWithIgnoreCase ("droz ") ? plugin.nom.substring (5) : plugin.nom;
        const auto police = ui::caps (12.0f);
        g.setFont (police);
        g.setColour (ui::colours::text);
        const int largeurNom = juce::jmin (haut.getWidth() - 60, juce::roundToInt (juce::GlyphArrangement::getStringWidth (police, ui::upper (nom))) + 4);
        g.drawText (ui::upper (nom), haut.removeFromLeft (largeurNom), juce::Justification::centredLeft, true);
        if (plugin.essai)
        {
            // Plugin en chantier : une etiquette discrete, pas une couleur.
            const auto etiquette = haut.removeFromLeft (54).withTrimmedLeft (8).withSizeKeepingCentre (46, 16).toFloat();
            g.setColour (ui::colours::dim);
            g.drawRoundedRectangle (etiquette.reduced (0.5f), ui::metrics::corner, 1.0f);
            g.setFont (ui::caps (8.0f));
            g.drawText ("ESSAI", etiquette, juce::Justification::centred);
        }

        auto bas = texte.withHeight (18).withY (zone.getY() + 34);
        g.setFont (ui::body (12.0f));
        if (resultat.message.isNotEmpty() && ! resultat.ok)
        {
            g.setColour (ui::colours::text);
            g.drawText (resultat.message, bas, juce::Justification::centredLeft, true);
        }
        else
        {
            g.setColour (ui::colours::dim);
            const auto qui = plugin.auteur.isNotEmpty() ? "par " + plugin.auteur : juce::String();
            const auto quoi = plugin.dansCatalogue ? plugin.description : avecCatalogue ? fr ("absent du catalogue") : juce::String();
            g.drawText (qui + (qui.isNotEmpty() && quoi.isNotEmpty() ? "  -  " : "") + quoi, bas, juce::Justification::centredLeft, true);
        }

        // Etat et versions, a gauche des boutons.
        auto etatZone = zone.withTrimmedRight (largeurBoutons + ui::metrics::gap).removeFromRight (largeurEtat);
        const auto etat = plugin.etat();
        juce::String un, deux;
        auto couleur = ui::colours::dim;
        if (enCours)
        {
            un = "en cours...";
            couleur = ui::colours::accent;
        }
        else if (etat == Etat::miseAJour)
        {
            un = fr ("mise à jour");
            deux = plugin.version != plugin.versionInstallee ? plugin.versionInstallee + " > " + plugin.version
                                                             : plugin.version + ", build " + plugin.build;
            couleur = ui::colours::accent; // l'orange dit "il y a quelque chose a faire ici"
        }
        else if (etat == Etat::installe)
        {
            un = fr ("installé");
            deux = "version " + plugin.versionInstallee;
            couleur = ui::colours::text;
        }
        else
        {
            un = fr ("non installé");
            deux = "version " + plugin.version;
        }
        g.setColour (couleur);
        g.setFont (ui::caps (9.0f));
        g.drawText (ui::upper (un), etatZone.withHeight (16).withY (zone.getY() + 15), juce::Justification::centredLeft, true);
        g.setColour (ui::colours::dim);
        g.setFont (ui::body (12.0f));
        g.drawText (deux, etatZone.withHeight (16).withY (zone.getY() + 34), juce::Justification::centredLeft, true);
    }

    void resized() override
    {
        auto zone = getLocalBounds();
        if (titre.isNotEmpty())
            zone.removeFromTop (hauteurTitre);
        auto boutons = zone.removeFromRight (largeurBoutons).withTrimmedRight (ui::metrics::gap).withSizeKeepingCentre (largeurBoutons - ui::metrics::gap, ui::metrics::row);
        retirer.setBounds (boutons.removeFromRight (112));
        boutons.removeFromRight (8);
        principal.setBounds (boutons);
    }

    Plugin plugin;

private:
    // L'action refait la liste, donc detruit cette ligne : on la lance apres le clic, avec une copie du nom.
    void cliquer (bool installer)
    {
        juce::MessageManager::callAsync ([action = surAction, nom = plugin.nom, installer]
        {
            if (action)
                action (nom, installer);
        });
    }

    static constexpr int largeurBoutons = 264, largeurEtat = 140, largeurDroite = largeurBoutons + largeurEtat + 24;
    juce::String titre;
    bool enCours = false, avecCatalogue = true;
    Resultat resultat;
    juce::TextButton principal, retirer;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Ligne)
};

//==============================================================================
/** Liste deroulante des lignes, avec un message quand elle est vide. */
class Liste final : public juce::Component
{
public:
    void paint (juce::Graphics& g) override
    {
        if (lignes.isEmpty())
        {
            g.setColour (ui::colours::dim);
            g.setFont (ui::body (13.0f));
            g.drawFittedText (vide, getLocalBounds().reduced (ui::metrics::gap), juce::Justification::centred, 3);
        }
    }

    void resized() override
    {
        int y = 0;
        for (auto* l : lignes)
        {
            l->setBounds (0, y, getWidth(), l->hauteurVoulue());
            y += l->hauteurVoulue() + 8;
        }
    }

    int hauteurVoulue() const
    {
        int h = 0;
        for (auto* l : lignes)
            h += l->hauteurVoulue() + 8;
        return juce::jmax (h, 120);
    }

    juce::OwnedArray<Ligne> lignes;
    juce::String vide;
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

        // Onglets
        for (auto* b : { &ongletPlugins, &ongletAtelier })
        {
            b->setClickingTogglesState (true);
            b->setRadioGroupId (1);
            addAndMakeVisible (*b);
        }
        ongletPlugins.setTooltip (fr ("Installer, mettre à jour et désinstaller les plugins Droz."));
        ongletAtelier.setTooltip (fr ("Ce qui attend dans l'atelier, et le message à coller dans ton assistant."));
        ongletPlugins.setToggleState (true, juce::dontSendNotification);
        ongletPlugins.onClick = [this] { montrer (false); };
        ongletAtelier.onClick = [this] { montrer (true); };

        // Onglet Plugins
        changer.setTooltip (fr ("Choisir le dossier où ton DAW lit ses VST3. Il est retenu pour la suite."));
        actualiser.setTooltip (fr ("Relire le catalogue et le dossier VST3."));
        toutMettre.setTooltip (fr ("Met à niveau tous les plugins marqués « mise à jour », l'un après l'autre."));
        nouvelleVersion.setTooltip (fr ("Ouvre le téléchargement de la nouvelle version du centre."));
        changer.onClick = [this] { choisirDossier(); };
        actualiser.onClick = [this] { recharger(); };
        toutMettre.onClick = [this] { toutMettreAJour(); };
        nouvelleVersion.onClick = [this] { juce::URL (etat.installeurCentre).launchInDefaultBrowser(); };
        for (auto* b : { &changer, &actualiser, &toutMettre })
            addAndMakeVisible (*b);
        addChildComponent (nouvelleVersion);
        vue.setViewedComponent (&liste, false);
        vue.setScrollBarsShown (true, false);
        vue.setScrollBarThickness (8);
        addAndMakeVisible (vue);

        // Onglet Atelier
        prenom.addItemList ({ "fx", "yoy", "zomban" }, 1);
        prenom.setSelectedId (juce::jmax (1, reglages != nullptr ? reglages->getIntValue ("prenom", 1) : 1), juce::dontSendNotification);
        prenom.setTooltip (fr ("Ton prénom : il est glissé dans le message."));
        prenom.onChange = [this] { if (reglages != nullptr) reglages->setValue ("prenom", prenom.getSelectedId()); };
        copier.setTooltip (fr ("Copie le message à coller tel quel dans ton assistant. Il met ta branche à jour et s'occupe du reste."));
        copier.onClick = [this] { copierMessage(); };
        toutesLesDemandes.setTooltip (fr ("Ouvre la liste des PR sur GitHub."));
        toutesLesDemandes.onClick = [] { juce::URL (juce::String (adresseDepot) + "/pulls").launchInDefaultBrowser(); };
        for (auto* c : std::initializer_list<juce::Component*> { &prenom, &copier, &toutesLesDemandes })
            addChildComponent (*c);

        setLookAndFeel (&look);
        setSize (860, 600);
        montrer (false);
        recharger();
    }

    ~Fenetre() override
    {
        fil.removeAllJobs (true, 15000);
        setLookAndFeel (nullptr);
    }

    /** Vrai si une installation tourne : la fenetre ne doit pas se fermer au milieu. */
    bool occupee() const { return ! occupes.isEmpty(); }

    void montrer (bool atelier)
    {
        surAtelier = atelier;
        (atelier ? ongletAtelier : ongletPlugins).setToggleState (true, juce::dontSendNotification);
        for (auto* c : std::initializer_list<juce::Component*> { &changer, &actualiser, &toutMettre, &vue })
            c->setVisible (! atelier);
        nouvelleVersion.setVisible (! atelier && nouvelleVersionDispo());
        for (auto* c : std::initializer_list<juce::Component*> { &prenom, &copier, &toutesLesDemandes })
            c->setVisible (atelier);
        for (auto* l : liensDemandes)
            l->setVisible (atelier);
        for (auto* l : liens)
            l->setVisible (atelier);
        resized();
        repaint();
    }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        g.fillAll (ui::colours::background);
        auto zone = ui::drawHeader (g, getLocalBounds(), "Software Center", DROZ_CENTRE_VERSION);
        zone = ui::drawFooter (g, zone, pied());
        zone.reduce (ui::metrics::margin, ui::metrics::margin);
        zone.removeFromTop (ui::metrics::row + ui::metrics::gap);

        if (! surAtelier)
        {
            auto barre = zone.removeFromTop (ui::metrics::row);
            barre.removeFromRight (largeurBarre);
            g.setColour (ui::colours::dim);
            g.setFont (ui::caps (9.0f));
            g.drawText ("DOSSIER VST3", barre.removeFromLeft (104), juce::Justification::centredLeft);
            g.setColour (ui::colours::text);
            g.setFont (ui::body (13.0f));
            g.drawText (dossierAffiche.getFullPathName(), barre, juce::Justification::centredLeft, true);
            if (avis().isNotEmpty())
            {
                zone.removeFromTop (ui::metrics::gap);
                auto cadre = zone.removeFromTop (36);
                g.setColour (ui::colours::panel);
                g.fillRoundedRectangle (cadre.toFloat(), ui::metrics::corner);
                g.setColour (ui::colours::text);
                g.setFont (ui::body (13.0f));
                g.drawText (avis(), cadre.reduced (ui::metrics::gap, 0).withTrimmedRight (nouvelleVersion.isVisible() ? 190 : 0),
                            juce::Justification::centredLeft, true);
            }
            return;
        }

        // Atelier : trois cadres.
        const auto zones = zonesAtelier (zone);
        auto dedans = ui::drawPanel (g, zones.demandes, "PR en attente");
        if (etat.atelier.demandes.empty())
        {
            g.setColour (ui::colours::dim);
            g.setFont (ui::body (13.0f));
            g.drawText (etat.horsLigne ? fr ("Catalogue non lu : pas d'état de l'atelier.") : fr ("Aucune PR en attente."), dedans.removeFromTop (22),
                        juce::Justification::centredLeft, true);
        }
        dedans = ui::drawPanel (g, zones.fiches, fr ("Fiches à écrire"));
        g.setColour (etat.atelier.fichesAEcrire.isEmpty() ? ui::colours::dim : ui::colours::text);
        g.setFont (ui::body (13.0f));
        g.drawFittedText (etat.atelier.fichesAEcrire.isEmpty() ? fr ("Aucune : chaque plugin en chantier a sa fiche.")
                                                             : etat.atelier.fichesAEcrire.joinIntoString (", ") + fr (". Le message ci-dessous s'en charge."),
                          dedans, juce::Justification::topLeft, 2);
        dedans = ui::drawPanel (g, zones.message, fr ("Reprendre ou créer un plugin"));
        g.setColour (ui::colours::dim);
        g.setFont (ui::body (13.0f));
        g.drawFittedText (fr ("Un seul message à coller dans ton assistant, où que tu sois. Il lit les règles du dépôt, te pose ses questions en une fois et s'occupe du reste."),
                          dedans.removeFromTop (36), juce::Justification::topLeft, 2);
        if (messageCopie)
        {
            g.setColour (ui::colours::text);
            g.drawText (fr ("Copié. Colle-le dans ton assistant."), dedans.withTrimmedTop (ui::metrics::gap).removeFromTop (ui::metrics::row).withTrimmedLeft (110 + 190 + 2 * ui::metrics::gap),
                        juce::Justification::centredLeft, true);
        }
    }

    void resized() override
    {
        auto zone = getLocalBounds();
        zone.removeFromTop (ui::metrics::header);
        zone.removeFromBottom (ui::metrics::footer);
        zone.reduce (ui::metrics::margin, ui::metrics::margin);
        auto onglets = zone.removeFromTop (ui::metrics::row);
        ongletPlugins.setBounds (onglets.removeFromLeft (120));
        onglets.removeFromLeft (4);
        ongletAtelier.setBounds (onglets.removeFromLeft (120));
        zone.removeFromTop (ui::metrics::gap);

        if (! surAtelier)
        {
            auto barre = zone.removeFromTop (ui::metrics::row);
            toutMettre.setBounds (barre.removeFromRight (176));
            barre.removeFromRight (8);
            actualiser.setBounds (barre.removeFromRight (104));
            barre.removeFromRight (8);
            changer.setBounds (barre.removeFromRight (92));
            if (avis().isNotEmpty())
            {
                zone.removeFromTop (ui::metrics::gap);
                auto cadre = zone.removeFromTop (36);
                nouvelleVersion.setBounds (cadre.removeFromRight (190).withTrimmedRight (6).withSizeKeepingCentre (178, ui::metrics::row));
            }
            zone.removeFromTop (ui::metrics::gap);
            vue.setBounds (zone);
            liste.setSize (juce::jmax (100, vue.getWidth() - 12), liste.hauteurVoulue());
            return;
        }

        const auto zones = zonesAtelier (zone);
        auto dedans = zones.demandes.reduced (ui::metrics::gap).withTrimmedTop (18);
        for (auto* l : liensDemandes)
            l->setBounds (dedans.removeFromTop (22));
        toutesLesDemandes.setBounds (zones.demandes.reduced (ui::metrics::gap).removeFromTop (16).removeFromRight (120).withHeight (20).translated (0, -2));
        dedans = zones.message.reduced (ui::metrics::gap).withTrimmedTop (18 + 36 + ui::metrics::gap);
        auto ligne = dedans.removeFromTop (ui::metrics::row);
        prenom.setBounds (ligne.removeFromLeft (110));
        ligne.removeFromLeft (ui::metrics::gap);
        copier.setBounds (ligne.removeFromLeft (190));
        dedans.removeFromTop (ui::metrics::gap);
        ligne = dedans.removeFromTop (ui::metrics::row);
        for (auto* l : liens)
        {
            l->setBounds (ligne.removeFromLeft (juce::jmin (150, (zones.message.getWidth() - 2 * ui::metrics::gap) / juce::jmax (1, liens.size()) - 8)));
            ligne.removeFromLeft (8);
        }
    }

    /** Captures : rend la fenetre telle qu'elle est. */
    juce::Image capture (float echelle) { return createComponentSnapshot (getLocalBounds(), true, echelle); }

private:
    //==========================================================================
    struct Instantane
    {
        std::vector<Plugin> plugins;
        Atelier atelier;
        bool horsLigne = false, catalogueAncien = false;
        juce::String erreur, versionCentre, installeurCentre, genere;
        juce::File dossier;
    };

    struct ZonesAtelier
    {
        juce::Rectangle<int> demandes, fiches, message;
    };

    ZonesAtelier zonesAtelier (juce::Rectangle<int> zone) const
    {
        ZonesAtelier z;
        z.message = zone.removeFromBottom (18 + 36 + 2 * ui::metrics::row + 4 * ui::metrics::gap + 6);
        zone.removeFromBottom (ui::metrics::gap);
        z.fiches = zone.removeFromBottom (18 + 40 + 2 * ui::metrics::gap);
        zone.removeFromBottom (ui::metrics::gap);
        z.demandes = zone;
        return z;
    }

    Instantane prendre() const
    {
        return { moteur.plugins, moteur.atelier, moteur.horsLigne, moteur.catalogueAncien, moteur.erreurCatalogue,
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
        mettreAJourBoutons();
        repaint();
        lancer ([this] { moteur.charger(); }, [this]
        {
            lecture = false;
            derniers.clear();
            reconstruire();
        });
    }

    void agir (juce::String nom, bool installer) // par valeur : reconstruire() detruit les lignes
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

    void copierMessage()
    {
        auto message = etat.atelier.message;
        if (message.isEmpty()) // sans catalogue : l'essentiel tient en deux lignes
            message = fr ("Tu travailles sur le dépôt GitHub droz-lab/plugins. Je suis <fx | yoy | zomban>.\n"
                          "Ouvre docs/ATELIER.md, partie « Commencer ici », et fais tout ce que dit le message à coller.");
        juce::SystemClipboard::copyTextToClipboard (message.replace ("<fx | yoy | zomban>", prenom.getText()));
        messageCopie = true;
        repaint();
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

    juce::String avis() const
    {
        if (lecture && etat.plugins.empty())
            return {};
        if (etat.horsLigne)
            return fr ("Catalogue non lu (") + etat.erreur + "). "
                 + (etat.catalogueAncien ? fr ("Voici le dernier catalogue connu et ce qui est installé.") : fr ("Voici ce qui est installé."));
        if (nouvelleVersionDispo())
            return fr ("Une nouvelle version du centre existe : ") + etat.versionCentre + ".";
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
        bool aJour = true;
        for (const auto& p : etat.plugins)
            aJour = aJour && p.etat() != Etat::miseAJour;
        toutMettre.setEnabled (! aJour && ! lecture && occupes.isEmpty());
        actualiser.setEnabled (! lecture && occupes.isEmpty());
        changer.setEnabled (! lecture && occupes.isEmpty());
    }

    /** Refait les lignes et les liens a partir du dernier etat connu. */
    void reconstruire()
    {
        // Plugins, par section : publies, essais, hors catalogue.
        liste.lignes.clear();
        const bool catalogueLu = ! etat.horsLigne || etat.catalogueAncien;
        const char* const titres[] = { "Plugins", "En chantier, à l'essai", catalogueLu ? "Installés, absents du catalogue" : "Installés" };
        for (int section = 0; section < 3; ++section)
        {
            bool premier = true;
            for (const auto& p : etat.plugins)
            {
                const int rang = ! p.dansCatalogue ? 2 : p.essai ? 1 : 0;
                if (rang != section)
                    continue;
                auto* l = liste.lignes.add (new Ligne());
                const auto trouve = derniers.find (p.nom);
                l->regler (p, premier ? fr (titres[section]) : juce::String(), occupes.contains (p.nom), lecture || occupes.contains (p.nom),
                           trouve != derniers.end() ? trouve->second : Resultat(), catalogueLu);
                l->surAction = [moi = juce::Component::SafePointer<Fenetre> (this)] (const juce::String& nom, bool installer)
                {
                    if (moi != nullptr)
                        moi->agir (nom, installer);
                };
                liste.addAndMakeVisible (l);
                premier = false;
            }
        }
        liste.vide = etat.horsLigne ? fr ("Aucun plugin Droz dans ce dossier VST3.")
                                    : fr ("Aucun plugin publié pour l'instant. Les chantiers apparaissent ici dès que leur Verdict est vert.");

        // Atelier
        for (auto* l : liensDemandes)
            removeChildComponent (l);
        liensDemandes.clear();
        int n = 0;
        for (const auto& d : etat.atelier.demandes)
        {
            if (++n > 8)
                break;
            auto texte = "PR " + juce::String (d.numero) + "  -  " + d.titre + "  -  " + d.auteur + "  -  Verdict " + d.verdict + (d.brouillon ? " (brouillon)" : "");
            auto* l = liensDemandes.add (new juce::HyperlinkButton (texte, juce::URL (d.adresse)));
            l->setFont (ui::body (13.0f), false, juce::Justification::centredLeft);
            l->setColour (juce::HyperlinkButton::textColourId, ui::colours::text);
            l->setTooltip (fr ("Ouvre la PR sur GitHub."));
            addChildComponent (l);
        }
        for (auto* l : liens)
            removeChildComponent (l);
        liens.clear();
        auto adresses = etat.atelier.liens;
        if (adresses.empty())
            adresses = { { fr ("Méthode"), juce::String (adresseDepot) + "/blob/main/docs/METHODE.md" },
                         { "Habillage", juce::String (adresseDepot) + "/blob/main/docs/HABILLAGE.md" },
                         { fr ("Page de l'atelier"), juce::String (adresseDepot) + "/releases/tag/dernier" } };
        for (const auto& lien : adresses)
        {
            auto* b = liens.add (new juce::TextButton (lien.nom));
            const auto adresse = lien.adresse;
            b->setTooltip (adresse);
            b->onClick = [adresse] { juce::URL (adresse).launchInDefaultBrowser(); };
            addChildComponent (b);
        }

        mettreAJourBoutons();
        montrer (surAtelier);
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
    bool lecture = false, surAtelier = false, messageCopie = false;

    ui::LookAndFeel look;
    juce::TooltipWindow bulles { this, 600 };
    juce::TextButton ongletPlugins { "Plugins" }, ongletAtelier { "Atelier" };
    juce::TextButton changer { "Changer" }, actualiser { "Actualiser" }, toutMettre { fr ("Tout mettre à jour") },
        nouvelleVersion { fr ("Télécharger") };
    juce::Viewport vue;
    Liste liste;
    juce::ComboBox prenom;
    juce::TextButton copier { "Copier le message" }, toutesLesDemandes { "Tout voir" };
    juce::OwnedArray<juce::HyperlinkButton> liensDemandes;
    juce::OwnedArray<juce::TextButton> liens;
    std::unique_ptr<juce::FileChooser> selecteur;

    static constexpr int largeurBarre = 176 + 104 + 92 + 16 + 12;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Fenetre)
};
} // namespace centre
