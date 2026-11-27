// Le moteur du centre, sans fenetre : lire le catalogue, savoir ce qui est installe, installer, mettre a jour,
// desinstaller. L'interface et l'essai automatique (--essai) passent tous les deux par lui.
#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace centre
{
inline juce::String fr (const char* texte) { return juce::String::fromUTF8 (texte); }

/** Adresse fixe du catalogue, refait par .github/workflows/centre.yml. */
inline const char* const adresseCatalogue = "https://github.com/droz-lab/plugins/releases/download/dernier/catalogue.json";
inline const char* const adresseDepot = "https://github.com/droz-lab/plugins";

/** -1, 0 ou 1. "0.10.0" est plus recent que "0.9.2". */
int comparerVersions (const juce::String& a, const juce::String& b);

enum class Etat { absent, installe, miseAJour };

struct Plugin
{
    // Catalogue
    juce::String dossier, nom, auteur, description, version, build, installeur, page;
    bool essai = false;          // plugin en chantier, publie automatiquement a chaque Verdict vert
    bool dansCatalogue = false;  // faux : trouve dans le dossier VST3 mais absent du catalogue
    // Sur cette machine
    bool installe = false;
    juce::String versionInstallee, buildInstalle;

    Etat etat() const;
};

struct DemandeDeFusion
{
    int numero = 0;
    juce::String titre, auteur, verdict, adresse;
    bool brouillon = false;
};

struct Lien
{
    juce::String nom, adresse;
};

struct Atelier
{
    std::vector<DemandeDeFusion> demandes;
    juce::StringArray fichesAEcrire;
    juce::String message; // le message a coller dans son assistant, avec <prenom> a remplacer
    std::vector<Lien> liens;
};

struct Resultat
{
    bool ok = false;
    bool verrouille = false; // le plugin est charge dans un DAW : rien n'a ete touche
    juce::String message;
};

class Moteur
{
public:
    juce::String source = adresseCatalogue; // adresse https ou chemin d'un fichier
    juce::File dossierVst3;
    juce::File dossierDonnees;              // dernier catalogue lu, pour l'afficher sans reseau

    /** Lit le catalogue puis le dossier VST3. Sans reseau : horsLigne, et ce qui est installe reste liste. */
    void charger();
    /** Relit seulement le dossier VST3. */
    void relireInstalles();

    Resultat installer (const juce::String& nom);
    Resultat desinstaller (const juce::String& nom);

    Plugin* trouver (const juce::String& nom);
    static juce::File paquet (const juce::File& dossierVst3, const juce::String& nom);
    /** Le fichier que le DAW charge, donc celui que Windows verrouille. */
    static juce::File binaire (const juce::File& dossierVst3, const juce::String& nom);

    std::vector<Plugin> plugins;
    Atelier atelier;
    juce::String versionCentre, installeurCentre; // derniere version publiee de l'appli
    juce::String genere;                         // date du catalogue
    bool horsLigne = false;
    bool catalogueAncien = false; // horsLigne, mais on affiche le dernier catalogue lu
    juce::String erreurCatalogue;

private:
    bool lireCatalogue (juce::String& texte);
    bool analyser (const juce::String& texte);
    bool obtenirInstalleur (const Plugin& p, juce::File& fichier, juce::String& erreur);
    juce::File fichierCache() const;
};

/** Retire un paquet .vst3 (appele aussi par l'appli relancee avec les droits, option --retirer). */
bool retirerPaquet (const juce::File& paquet);
} // namespace centre
