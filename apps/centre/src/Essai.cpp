// Essai automatique, sans fenetre : DrozSoftwareCenter --essai --catalogue a.json --suite b.json --dossier <vst3> --donnees <tmp>
// Lit le catalogue, installe le premier plugin, verifie le fichier pose, coupe le reseau, met a jour (plugin tenu
// ouvert puis relache), desinstalle. Code de sortie non nul au moindre ecart. Lance par le controle Verdict.

#include "Essai.h"

#include "Moteur.h"
#include "Systeme.h"

#include <cstdio>

namespace centre
{
namespace
{
struct Journal
{
    juce::File fichier;
    int ecarts = 0;

    void ligne (const juce::String& texte)
    {
        std::printf ("%s\n", texte.toRawUTF8());
        std::fflush (stdout);
        if (fichier != juce::File())
            fichier.appendText (texte + "\n", false, false, "\n");
    }

    bool verifier (bool condition, const juce::String& quoi)
    {
        ligne (juce::String (condition ? "  ok     " : "  ECART  ") + quoi);
        if (! condition)
            ++ecarts;
        return condition;
    }
};

juce::String valeur (const juce::StringArray& arguments, const juce::String& option)
{
    const int i = arguments.indexOf (option);
    return i >= 0 && i + 1 < arguments.size() ? arguments[i + 1].unquoted() : juce::String();
}
} // namespace

int lancerEssai (const juce::StringArray& arguments)
{
    const auto ici = juce::File::getCurrentWorkingDirectory();
    Journal j;
    if (const auto f = valeur (arguments, "--journal"); f.isNotEmpty())
    {
        j.fichier = ici.getChildFile (f);
        j.fichier.deleteFile();
    }

    Moteur m;
    m.source = valeur (arguments, "--catalogue");
    const auto suite = valeur (arguments, "--suite");
    const auto dossier = valeur (arguments, "--dossier");
    const auto donnees = valeur (arguments, "--donnees");
    if (m.source.isEmpty() || dossier.isEmpty() || donnees.isEmpty())
    {
        j.ligne ("usage : --essai --catalogue <fichier ou adresse> [--suite <catalogue plus recent>] --dossier <dossier VST3 d'essai> --donnees <dossier temporaire> [--journal <fichier>]");
        return 2;
    }
    m.dossierVst3 = ici.getChildFile (dossier);
    m.dossierDonnees = ici.getChildFile (donnees);
    m.dossierVst3.createDirectory();

    j.ligne ("Droz Software Center " DROZ_CENTRE_VERSION " : essai");
    j.ligne ("1. Catalogue : " + m.source);
    m.charger();
    j.verifier (! m.horsLigne, "catalogue lu" + (m.horsLigne ? " (" + m.erreurCatalogue + ")" : juce::String()));
    for (const auto& p : m.plugins)
        j.ligne ("         " + p.nom + " " + p.version + (p.essai ? " (essai)" : "") + ", par " + p.auteur);
    if (! j.verifier (! m.plugins.empty() && m.plugins.front().dansCatalogue, "au moins un plugin dans le catalogue"))
        return 1;
    const auto nom = m.plugins.front().nom;
    const auto premiere = m.plugins.front().version;

    if (m.trouver (nom)->installe) // reste d'un essai precedent
        m.desinstaller (nom);

    j.ligne ("2. Installer " + nom + " dans " + m.dossierVst3.getFullPathName());
    j.verifier (m.trouver (nom)->etat() == Etat::absent, "absent avant installation");
    auto r = m.installer (nom);
    j.verifier (r.ok, "installation : " + r.message);
    auto* p = m.trouver (nom);
    j.verifier (p != nullptr && p->etat() == Etat::installe, fr ("la ligne passe à « installé »"));
    j.verifier (p != nullptr && p->versionInstallee == premiere, "version lue sur le disque : " + (p != nullptr ? p->versionInstallee : juce::String ("?")));
   #if JUCE_WINDOWS
    j.verifier (Moteur::binaire (m.dossierVst3, nom).existsAsFile(), "fichier pose : " + Moteur::binaire (m.dossierVst3, nom).getFullPathName());
   #else
    j.verifier (Moteur::paquet (m.dossierVst3, nom).isDirectory(), "paquet pose : " + Moteur::paquet (m.dossierVst3, nom).getFullPathName());
   #endif

    j.ligne (fr ("3. Sans réseau"));
    {
        Moteur coupe;
        coupe.source = m.dossierDonnees.getChildFile ("absent").getChildFile ("catalogue.json").getFullPathName();
        coupe.dossierVst3 = m.dossierVst3;
        coupe.dossierDonnees = m.dossierDonnees.getChildFile ("vide");
        coupe.charger();
        j.verifier (coupe.horsLigne && coupe.erreurCatalogue.isNotEmpty(), "l'appli dit qu'elle n'a pas lu le catalogue : " + coupe.erreurCatalogue);
        const auto* vu = coupe.trouver (nom);
        j.verifier (vu != nullptr && vu->installe && vu->versionInstallee == premiere, fr ("ce qui est installé reste listé"));
    }

    if (suite.isNotEmpty())
    {
        j.ligne (fr ("4. Mise à jour : ") + suite);
        m.source = suite;
        m.charger();
        p = m.trouver (nom);
        if (j.verifier (p != nullptr && p->etat() == Etat::miseAJour, fr ("la ligne affiche « mise à jour »")))
        {
            const auto nouvelle = p->version;
           #if JUCE_WINDOWS
            // Plugin charge dans un DAW : rien ne doit etre touche, et l'appli doit dire lequel est verrouille.
            void* prise = systeme::tenir (Moteur::binaire (m.dossierVst3, nom));
            if (j.verifier (prise != nullptr, fr ("plugin chargé comme dans un DAW")))
            {
                r = m.installer (nom);
                j.verifier (! r.ok && r.verrouille, fr ("mise à jour refusée, plugin verrouillé : ") + r.message);
                j.verifier (r.message.contains (nom), "le message nomme le plugin");
                j.verifier (m.trouver (nom)->versionInstallee == premiere, fr ("rien n'a été modifié"));
                r = m.desinstaller (nom);
                j.verifier (! r.ok && r.verrouille && Moteur::binaire (m.dossierVst3, nom).existsAsFile(), fr ("désinstallation refusée aussi"));
                systeme::relacher (prise);
            }
           #endif
            // « Tout mettre a jour » : chaque ligne en etat mise a jour, l'une apres l'autre.
            int faits = 0;
            juce::StringArray aFaire;
            for (const auto& q : m.plugins)
                if (q.etat() == Etat::miseAJour)
                    aFaire.add (q.nom);
            for (const auto& n : aFaire)
            {
                r = m.installer (n);
                j.verifier (r.ok, fr ("plugin relâché, mise à jour : ") + r.message);
                faits += r.ok ? 1 : 0;
            }
            p = m.trouver (nom);
            j.verifier (faits >= 1 && p != nullptr && p->etat() == Etat::installe && p->versionInstallee == nouvelle,
                        fr ("tout est à niveau : ") + (p != nullptr ? p->versionInstallee : juce::String ("?")));
        }
    }

    j.ligne (fr ("5. Désinstaller"));
    r = m.desinstaller (nom);
    j.verifier (r.ok, fr ("désinstallation : ") + r.message);
    j.verifier (! Moteur::paquet (m.dossierVst3, nom).exists(), fr ("le plugin n'est plus dans le dossier VST3"));
    p = m.trouver (nom);
    j.verifier (p != nullptr && p->etat() == Etat::absent, fr ("la ligne repasse à « non installé »"));

    j.ligne (j.ecarts == 0 ? fr ("ESSAI RÉUSSI") : fr ("ESSAI RATÉ : ") + juce::String (j.ecarts) + fr (" écart(s)"));
    return j.ecarts == 0 ? 0 : 1;
}
} // namespace centre
