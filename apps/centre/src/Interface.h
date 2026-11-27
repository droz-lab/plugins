// La fenetre du centre : une seule page, la liste des plugins. Pour chacun : sa fiche en clair, sa version,
// un bouton. Habillage : droz::ui uniquement (docs/HABILLAGE.md). Tout travail long passe par un fil a part,
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
/** Un plugin : nom, auteur, version, fiche, etat, bouton. */
class Ligne final : public juce::Component
{
public:
    static constexpr int hauteurTitre = 34;

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

    int hauteurVoulue() const
    {
        return 112 + (plugin.reglages.isEmpty() ? 0 : 20) + (titre.isNotEmpty() ? hauteurTitre : 0);
    }

    void paint (juce::Graphics& g) override
    {
        auto zone = getLocalBounds();
        if (titre.isNotEmpty())
        {
            g.setColour (ui::colours::dim);
            g.setFont (ui::caps (10.0f));
            g.drawText (ui::upper (titre), zone.removeFromTop (hauteurTitre).withTrimmedTop (12), juce::Justification::centredLeft);
        }
        g.setColour (ui::colours::panel);
        g.fillRoundedRectangle (zone.toFloat(), ui::metrics::corner);

        auto texte = zone.reduced (marge, 0).withTrimmedRight (largeurDroite + marge);
        int y = zone.getY() + 14;

        // Nom, et l'etiquette des chantiers.
        auto haut = texte.withY (y).withHeight (24);
        const auto nom = plugin.nom.startsWithIgnoreCase ("droz ") ? plugin.nom.substring (5) : plugin.nom;
        const auto police = ui::caps (15.0f);
        g.setFont (police);
        g.setColour (ui::colours::text);
        const int largeurNom = juce::jmin (haut.getWidth() - 70, juce::roundToInt (juce::GlyphArrangement::getStringWidth (police, ui::upper (nom))) + 6);
        g.drawText (ui::upper (nom), haut.removeFromLeft (largeurNom), juce::Justification::centredLeft, true);
        if (plugin.essai)
        {
            const auto etiquette = haut.removeFromLeft (64).withTrimmedLeft (10).withSizeKeepingCentre (52, 18).toFloat();
            g.setColour (ui::colours::dim);
            g.drawRoundedRectangle (etiquette.reduced (0.5f), ui::metrics::corner, 1.0f);
            g.setFont (ui::caps (9.0f));
            g.drawText ("ESSAI", etiquette, juce::Justification::centred);
        }
        y += 26;

        // Auteur et version.
        juce::StringArray qui;
        if (plugin.auteur.isNotEmpty())
            qui.add ("par " + plugin.auteur);
        if (plugin.dansCatalogue)
            qui.add ("version " + plugin.version);
        g.setColour (ui::colours::dim);
        g.setFont (ui::body (13.0f));
        g.drawText (qui.joinIntoString ("  -  "), texte.withY (y).withHeight (18), juce::Justification::centredLeft, true);
        y += 24;

        // La fiche en clair, ou ce qui vient de se passer si ca a rate.
        const bool echec = resultat.message.isNotEmpty() && ! resultat.ok;
        const auto fiche = echec ? resultat.message
                         : plugin.fiche.isNotEmpty() ? plugin.fiche
                         : plugin.dansCatalogue ? plugin.description
                         : avecCatalogue ? fr ("Absent du catalogue.") : juce::String();
        g.setColour (ui::colours::text);
        g.setFont (ui::body (14.0f));
        g.drawFittedText (fiche, texte.withY (y).withHeight (38), juce::Justification::topLeft, 2, 1.0f);
        y += 42;

        if (! plugin.reglages.isEmpty())
        {
            g.setColour (ui::colours::dim);
            g.setFont (ui::body (13.0f));
            g.drawText (fr ("Réglages : ") + plugin.reglages.joinIntoString (", "), texte.withY (y).withHeight (18), juce::Justification::centredLeft, true);
        }

        // Etat, au-dessus du bouton.
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
            un = fr ("non installé");
        }
        g.setColour (couleur);
        g.setFont (ui::caps (10.0f));
        g.drawText (ui::upper (un), zone.withTrimmedRight (marge).removeFromRight (largeurDroite).withY (zone.getY() + 16).withHeight (18),
                    juce::Justification::centredLeft, true);
    }

    void resized() override
    {
        auto zone = getLocalBounds();
        if (titre.isNotEmpty())
            zone.removeFromTop (hauteurTitre);
        auto droite = zone.withTrimmedRight (marge).removeFromRight (largeurDroite).withTrimmedTop (42);
        auto premier = droite.removeFromTop (hauteurBouton);
        droite.removeFromTop (6);
        // Un seul bouton quand c'est possible : il prend la premiere place.
        if (principal.isVisible())
        {
            principal.setBounds (premier);
            retirer.setBounds (droite.removeFromTop (ui::metrics::row));
        }
        else
        {
            retirer.setBounds (premier);
        }
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

    static constexpr int marge = 18, largeurDroite = 210, hauteurBouton = 30;
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
            g.setFont (ui::body (14.0f));
            g.drawFittedText (vide, getLocalBounds().reduced (ui::metrics::gap), juce::Justification::centred, 3);
        }
    }

    void resized() override
    {
        int y = 0;
        for (auto* l : lignes)
        {
            l->setBounds (0, y, getWidth(), l->hauteurVoulue());
            y += l->hauteurVoulue() + 10;
        }
    }

    int hauteurVoulue() const
    {
        int h = 0;
        for (auto* l : lignes)
            h += l->hauteurVoulue() + 10;
        return juce::jmax (h, 160);
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

        setLookAndFeel (&look);
        setSize (1040, 760);
        recharger();
    }

    ~Fenetre() override
    {
        fil.removeAllJobs (true, 15000);
        setLookAndFeel (nullptr);
    }

    /** Vrai si une installation tourne : la fenetre ne doit pas se fermer au milieu. */
    bool occupee() const { return ! occupes.isEmpty(); }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        g.fillAll (ui::colours::background);
        auto zone = ui::drawHeader (g, getLocalBounds(), "Software Center", DROZ_CENTRE_VERSION);
        zone = ui::drawFooter (g, zone, pied());
        zone.reduce (marge, ui::metrics::margin);

        auto barre = zone.removeFromTop (hauteurBarre);
        barre.removeFromRight (largeurBarre);
        g.setColour (ui::colours::dim);
        g.setFont (ui::caps (10.0f));
        g.drawText ("DOSSIER VST3", barre.removeFromLeft (116), juce::Justification::centredLeft);
        g.setColour (ui::colours::text);
        g.setFont (ui::body (14.0f));
        g.drawText (dossierAffiche.getFullPathName(), barre, juce::Justification::centredLeft, true);
        if (avis().isNotEmpty())
        {
            zone.removeFromTop (ui::metrics::gap);
            auto cadre = zone.removeFromTop (40);
            g.setColour (ui::colours::panel);
            g.fillRoundedRectangle (cadre.toFloat(), ui::metrics::corner);
            g.setColour (ui::colours::text);
            g.setFont (ui::body (14.0f));
            g.drawText (avis(), cadre.reduced (18, 0).withTrimmedRight (nouvelleVersion.isVisible() ? 200 : 0), juce::Justification::centredLeft, true);
        }
    }

    void resized() override
    {
        auto zone = getLocalBounds();
        zone.removeFromTop (ui::metrics::header);
        zone.removeFromBottom (ui::metrics::footer);
        zone.reduce (marge, ui::metrics::margin);

        auto barre = zone.removeFromTop (hauteurBarre);
        toutMettre.setBounds (barre.removeFromRight (196));
        barre.removeFromRight (8);
        actualiser.setBounds (barre.removeFromRight (116));
        barre.removeFromRight (8);
        changer.setBounds (barre.removeFromRight (104));
        nouvelleVersion.setVisible (nouvelleVersionDispo());
        if (avis().isNotEmpty())
        {
            zone.removeFromTop (ui::metrics::gap);
            auto cadre = zone.removeFromTop (40);
            nouvelleVersion.setBounds (cadre.removeFromRight (200).withTrimmedRight (8).withSizeKeepingCentre (184, ui::metrics::row));
        }
        zone.removeFromTop (ui::metrics::gap);
        vue.setBounds (zone);
        liste.setSize (juce::jmax (100, vue.getWidth() - 12), liste.hauteurVoulue());
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

    /** Refait les lignes a partir du dernier etat connu. Sections : publies, essais, hors catalogue. */
    void reconstruire()
    {
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

    ui::LookAndFeel look;
    juce::TooltipWindow bulles { this, 600 };
    juce::TextButton changer { "Changer" }, actualiser { "Actualiser" }, toutMettre { fr ("Tout mettre à jour") },
        nouvelleVersion { fr ("Télécharger") };
    juce::Viewport vue;
    Liste liste;
    std::unique_ptr<juce::FileChooser> selecteur;

    static constexpr int marge = 20, hauteurBarre = 30, largeurBarre = 196 + 116 + 104 + 16 + 16;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Fenetre)
};
} // namespace centre
