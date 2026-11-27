#include "Moteur.h"

#include "Systeme.h"

namespace centre
{
int comparerVersions (const juce::String& a, const juce::String& b)
{
    const auto pa = juce::StringArray::fromTokens (a, ".", {});
    const auto pb = juce::StringArray::fromTokens (b, ".", {});
    for (int i = 0; i < juce::jmax (pa.size(), pb.size()); ++i)
    {
        const int va = pa[i].getIntValue(), vb = pb[i].getIntValue();
        if (va != vb)
            return va < vb ? -1 : 1;
    }
    return 0;
}

Etat Plugin::etat() const
{
    if (! installe)
        return Etat::absent;
    if (! dansCatalogue)
        return Etat::installe;
    const int ordre = comparerVersions (version, versionInstallee);
    // Un essai garde souvent le meme numero d'une compilation a l'autre : c'est le build qui les distingue.
    const bool autreBuild = ordre == 0 && build.isNotEmpty() && buildInstalle.isNotEmpty() && build != buildInstalle;
    return (ordre > 0 || autreBuild) ? Etat::miseAJour : Etat::installe;
}

juce::File Moteur::paquet (const juce::File& dossier, const juce::String& nom)
{
    return dossier.getChildFile (nom + ".vst3");
}

juce::File Moteur::binaire (const juce::File& dossier, const juce::String& nom)
{
   #if JUCE_WINDOWS
    return paquet (dossier, nom).getChildFile ("Contents").getChildFile ("x86_64-win").getChildFile (nom + ".vst3");
   #else
    return paquet (dossier, nom).getChildFile ("Contents").getChildFile ("x86_64-linux").getChildFile (nom + ".so");
   #endif
}

Plugin* Moteur::trouver (const juce::String& nom)
{
    for (auto& p : plugins)
        if (p.nom == nom)
            return &p;
    return nullptr;
}

juce::File Moteur::fichierCache() const
{
    return dossierDonnees == juce::File() ? juce::File() : dossierDonnees.getChildFile ("catalogue.json");
}

//==============================================================================
static bool estUneAdresse (const juce::String& s)
{
    return s.startsWithIgnoreCase ("https://") || s.startsWithIgnoreCase ("http://");
}

static std::unique_ptr<juce::InputStream> ouvrir (const juce::String& adresse, juce::String& erreur)
{
    int statut = 0;
    auto flux = juce::URL (adresse).createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                           .withConnectionTimeoutMs (10000)
                                                           .withNumRedirectsToFollow (8)
                                                           .withExtraHeaders ("Cache-Control: no-cache")
                                                           .withStatusCode (&statut));
    if (flux == nullptr)
    {
        erreur = fr ("pas de réponse (réseau coupé ?)");
        return nullptr;
    }
    if (statut != 0 && statut != 200)
    {
        erreur = fr ("réponse ") + juce::String (statut);
        return nullptr;
    }
    return flux;
}

bool Moteur::lireCatalogue (juce::String& texte)
{
    if (estUneAdresse (source))
    {
        auto flux = ouvrir (source, erreurCatalogue);
        if (flux == nullptr)
            return false;
        texte = flux->readEntireStreamAsString();
    }
    else
    {
        const juce::File fichier (juce::File::getCurrentWorkingDirectory().getChildFile (source));
        if (! fichier.existsAsFile())
        {
            erreurCatalogue = "fichier introuvable : " + fichier.getFullPathName();
            return false;
        }
        texte = fichier.loadFileAsString();
    }
    if (texte.trim().isEmpty())
    {
        erreurCatalogue = "catalogue vide";
        return false;
    }
    return true;
}

bool Moteur::analyser (const juce::String& texte)
{
    const auto racine = juce::JSON::parse (texte);
    const auto* liste = racine["plugins"].getArray();
    if (! racine.isObject() || liste == nullptr)
    {
        erreurCatalogue = "catalogue illisible";
        return false;
    }

    plugins.clear();
    for (const auto& v : *liste)
    {
        Plugin p;
        p.dossier = v["dossier"].toString();
        p.nom = v["nom"].toString();
        p.auteur = v["auteur"].toString();
        p.description = v["description"].toString();
        p.fiche = v["fiche"].toString();
        if (const auto* reglages = v["reglages"].getArray())
            for (const auto& r : *reglages)
                p.reglages.add (r.toString());
        p.version = v["version"].toString();
        p.build = v["build"].toString();
        p.installeur = v["installeur"].toString();
        p.page = v["page"].toString();
        p.essai = v["etat"].toString() == "essai";
        p.dansCatalogue = true;
        if (p.nom.isNotEmpty() && trouver (p.nom) == nullptr)
            plugins.push_back (p);
    }

    genere = racine["genere"].toString();
    versionCentre = racine["centre"]["version"].toString();
    installeurCentre = racine["centre"]["installeur"].toString();

    atelier = {};
    const auto a = racine["atelier"];
    if (const auto* demandes = a["pr"].getArray())
        for (const auto& v : *demandes)
        {
            DemandeDeFusion d;
            d.numero = (int) v["numero"];
            d.titre = v["titre"].toString();
            d.auteur = v["auteur"].toString();
            d.verdict = v["verdict"].toString();
            d.adresse = v["url"].toString();
            d.brouillon = (bool) v["brouillon"];
            atelier.demandes.push_back (d);
        }
    if (const auto* fiches = a["fiches_a_ecrire"].getArray())
        for (const auto& v : *fiches)
            atelier.fichesAEcrire.add (v.toString());
    atelier.message = a["message"].toString();
    if (const auto* liens = a["liens"].getArray())
        for (const auto& v : *liens)
            atelier.liens.push_back ({ v["nom"].toString(), v["url"].toString() });
    return true;
}

void Moteur::charger()
{
    plugins.clear();
    atelier = {};
    versionCentre.clear();
    installeurCentre.clear();
    genere.clear();
    horsLigne = false;
    catalogueAncien = false;
    erreurCatalogue.clear();

    juce::String texte;
    if (lireCatalogue (texte) && analyser (texte))
    {
        if (fichierCache() != juce::File())
        {
            dossierDonnees.createDirectory();
            fichierCache().replaceWithText (texte);
        }
    }
    else
    {
        horsLigne = true;
        plugins.clear();
        const auto raison = erreurCatalogue;
        // Sans reseau : le dernier catalogue lu, pour garder noms, auteurs et descriptions.
        if (fichierCache().existsAsFile() && analyser (fichierCache().loadFileAsString()))
            catalogueAncien = true;
        else
            plugins.clear();
        erreurCatalogue = raison;
    }
    relireInstalles();
}

//==============================================================================
// Valeur d'un champ texte dans moduleinfo.json (ecrit par JUCE, pas toujours du JSON strict).
static juce::String champ (const juce::String& texte, const juce::String& cle)
{
    const int debut = texte.indexOf ("\"" + cle + "\"");
    if (debut < 0)
        return {};
    const auto suite = texte.substring (debut + cle.length() + 2).fromFirstOccurrenceOf (":", false, false);
    return suite.fromFirstOccurrenceOf ("\"", false, false).upToFirstOccurrenceOf ("\"", false, false);
}

// Ce que l'installeur a pose : version et build dans droz.ini (tools/installeur.iss), sinon moduleinfo.json.
static bool lireInstalle (const juce::File& dossier, const juce::String& nom, juce::String& version, juce::String& build, bool& droz)
{
    const auto paquet = Moteur::paquet (dossier, nom);
    version.clear();
    build.clear();
    droz = false;
    if (! paquet.isDirectory())
        return false;
    const auto ressources = paquet.getChildFile ("Contents").getChildFile ("Resources");
    const auto marque = ressources.getChildFile ("droz.ini");
    if (marque.existsAsFile())
    {
        droz = true;
        juce::StringArray lignes;
        lignes.addLines (marque.loadFileAsString());
        for (const auto& ligne : lignes)
        {
            const auto cle = ligne.upToFirstOccurrenceOf ("=", false, false).trim().toLowerCase();
            const auto valeur = ligne.fromFirstOccurrenceOf ("=", false, false).trim();
            if (cle == "version")
                version = valeur;
            else if (cle == "build")
                build = valeur;
        }
    }
    const auto info = ressources.getChildFile ("moduleinfo.json");
    if (info.existsAsFile())
    {
        const auto texte = info.loadFileAsString();
        if (version.isEmpty())
            version = champ (texte, "Version");
        if (champ (texte, "Vendor") == "Droz")
            droz = true;
    }
    return true;
}

void Moteur::relireInstalles()
{
    plugins.erase (std::remove_if (plugins.begin(), plugins.end(), [] (const Plugin& p) { return ! p.dansCatalogue; }), plugins.end());
    for (auto& p : plugins)
    {
        bool droz = false;
        p.installe = lireInstalle (dossierVst3, p.nom, p.versionInstallee, p.buildInstalle, droz);
    }

    // Plugins Droz presents dans le dossier mais absents du catalogue (retires, ou catalogue pas lu).
    for (const auto& entree : juce::RangedDirectoryIterator (dossierVst3, false, "*.vst3", juce::File::findDirectories))
    {
        const auto nom = entree.getFile().getFileNameWithoutExtension();
        if (trouver (nom) != nullptr)
            continue;
        Plugin p;
        p.nom = nom;
        bool droz = false;
        p.installe = lireInstalle (dossierVst3, nom, p.versionInstallee, p.buildInstalle, droz);
        if (p.installe && droz)
            plugins.push_back (p);
    }
}

//==============================================================================
bool Moteur::obtenirInstalleur (const Plugin& p, juce::File& fichier, juce::String& erreur)
{
    if (! estUneAdresse (p.installeur))
    {
        // Chemin de fichier (essais) : absolu, ou relatif au catalogue.
        const auto base = estUneAdresse (source) ? juce::File::getCurrentWorkingDirectory()
                                                 : juce::File::getCurrentWorkingDirectory().getChildFile (source).getParentDirectory();
        fichier = base.getChildFile (p.installeur);
        if (! fichier.existsAsFile())
            erreur = "fichier introuvable : " + fichier.getFullPathName();
        return fichier.existsAsFile();
    }

    auto flux = ouvrir (p.installeur, erreur);
    if (flux == nullptr)
        return false;
    const auto dossier = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("DrozSoftwareCenter");
    dossier.createDirectory();
    fichier = dossier.getChildFile (p.nom.replaceCharacter (' ', '-') + "-Setup.exe");
    fichier.deleteFile();
    {
        juce::FileOutputStream sortie (fichier);
        if (! sortie.openedOk())
        {
            erreur = fr ("écriture impossible dans ") + dossier.getFullPathName();
            return false;
        }
        sortie.writeFromInputStream (*flux, -1);
    }
    if (fichier.getSize() < 1024) // une page d'erreur a la place du fichier
    {
        erreur = fr ("fichier reçu incomplet");
        fichier.deleteFile();
        return false;
    }
    return true;
}

static juce::String messageVerrou (const juce::String& nom, const juce::StringArray& par)
{
    return nom + fr (" est ouvert dans ") + par.joinIntoString (", ")
         + fr (". Retire-le des pistes ou ferme le DAW, puis réessaie. Rien n'a été modifié.");
}

Resultat Moteur::installer (const juce::String& nom)
{
    auto* p = trouver (nom);
    if (p == nullptr || ! p->dansCatalogue || p->installeur.isEmpty())
        return { false, false, fr ("Pas d'installeur pour ce plugin dans le catalogue.") };
    const Plugin voulu = *p;

    // Plugin charge dans un DAW : Windows verrouille le fichier. On ne lance rien.
    if (voulu.installe)
    {
        const auto par = systeme::quiUtilise (binaire (dossierVst3, nom));
        if (! par.isEmpty())
            return { false, true, messageVerrou (nom, par) };
    }

    juce::File setup;
    juce::String erreur;
    if (! obtenirInstalleur (voulu, setup, erreur))
        return { false, false, fr ("Téléchargement impossible : ") + erreur };

    // Installeur Inno Setup commun (tools/installeur.iss) : silencieux, dans le dossier choisi ici.
    const int code = systeme::lancerEtAttendre (setup, { "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/DIR=" + dossierVst3.getFullPathName() },
                                                false, erreur);
    if (estUneAdresse (voulu.installeur))
        setup.deleteFile();
    relireInstalles();

    if (code != 0)
        return { false, false, code < 0 ? fr ("Installation annulée : ") + erreur
                                        : fr ("L'installeur a échoué (code ") + juce::String (code) + fr ("). Rien d'autre n'a été touché.") };
    p = trouver (nom);
    if (p == nullptr || ! p->installe)
        return { false, false, fr ("L'installeur a tourné mais le plugin n'est pas dans ") + dossierVst3.getFullPathName() };
    if (p->versionInstallee != voulu.version || (voulu.build.isNotEmpty() && p->buildInstalle.isNotEmpty() && p->buildInstalle != voulu.build))
        return { false, false, fr ("Version posée : ") + p->versionInstallee + fr (", attendue : ") + voulu.version };
    return { true, false, nom + " " + voulu.version + fr (voulu.installe ? " mis à jour." : " installé.") };
}

bool retirerPaquet (const juce::File& paquet)
{
    // Garde-fou : on ne retire qu'un paquet VST3, jamais un dossier quelconque.
    if (! paquet.isDirectory() || ! paquet.hasFileExtension ("vst3") || ! paquet.getChildFile ("Contents").isDirectory())
        return false;
    return paquet.deleteRecursively() && ! paquet.exists();
}

Resultat Moteur::desinstaller (const juce::String& nom)
{
    const auto cible = paquet (dossierVst3, nom);
    if (cible.isDirectory())
    {
        const auto par = systeme::quiUtilise (binaire (dossierVst3, nom));
        if (! par.isEmpty())
            return { false, true, messageVerrou (nom, par) };

        if (! retirerPaquet (cible))
        {
            // Dossier protege (Program Files) : l'appli se relance avec les droits, juste pour ce retrait.
            juce::String erreur;
            const int code = systeme::lancerEtAttendre (juce::File::getSpecialLocation (juce::File::currentExecutableFile),
                                                        { "--retirer", cible.getFullPathName() }, true, erreur);
            if (code != 0 || cible.exists())
            {
                relireInstalles();
                return { false, false, fr ("Désinstallation impossible") + (erreur.isNotEmpty() ? " : " + erreur : juce::String()) + "." };
            }
        }
    }
    relireInstalles();
    return { true, false, nom + fr (" désinstallé.") };
}
} // namespace centre
