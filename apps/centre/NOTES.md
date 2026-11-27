# Droz Software Center

Responsable : Fx

## But

Une appli Windows qui liste tous les plugins Droz, les installe, les met a jour et les desinstalle depuis une seule fenetre. Plugins publies et plugins en chantier ("essai"). Pour chacun : sa fiche en clair, sa version, un bouton.

Telecharger : `Droz-Software-Center-Setup.exe` sur la [page de l'atelier](https://github.com/droz-lab/plugins/releases/tag/dernier).

## Etat

- v0.3.0 : interface reprise a la charte (tailles de texte, marges et grille de `docs/HABILLAGE.md`). Une grille de cartes centree, tout est symetrique. Plus de bouton Actualiser : le catalogue se relit quand la fenetre revient devant, ou avec F5. "Tout mettre a jour" n'apparait que s'il y a quelque chose a mettre a jour.
- v0.2.0 : une seule fenetre, plus grande. Chaque plugin montre l'intention et les reglages de sa `FICHE.md`. Icone Droz. L'onglet Atelier, le message a copier et les liens sont retires.
- v0.1.0 : premiere version. Onglets Plugins et Atelier, dossier VST3 au choix, "Tout mettre a jour", plugin verrouille par un DAW detecte avant de toucher a quoi que ce soit, affichage sans reseau.

## Comment ca marche

| Morceau | Ou | Role |
|---|---|---|
| Catalogue | `catalogue.json` sur la page `dernier`, ecrit par `python tools/droz.py catalogue` (`centre.yml`) | plugins publies, plugins a l'essai, PR en attente, fiches a ecrire, message a coller, derniere version de l'appli |
| Essais | page `dernier`, comme tout le reste, remplie par `essais.yml` apres chaque Verdict vert d'une branche (un push suffit, pas besoin de PR) | un installeur par plugin en chantier, toujours le dernier, et `essais.json` qui dit de quel commit il vient |
| Installer | l'appli telecharge l'installeur du plugin (`tools/installeur.iss`) et le lance en silencieux avec `/DIR=<dossier VST3>` | Windows demande les droits a chaque installation (pas de signature de code) |
| Ce qui est installe | `<Nom>.vst3\Contents\Resources\droz.ini`, pose par l'installeur : `version` et `build` | la verite est sur le disque, pas dans un registre de l'appli. Sans ce fichier : `moduleinfo.json` |
| Desinstaller | l'appli retire le paquet `.vst3`. Dossier protege : elle se relance avec les droits (`--retirer`) juste pour ce retrait | |
| Plugin charge dans un DAW | gestionnaire de redemarrage de Windows, puis essai d'ouverture en ecriture | l'appli nomme le plugin et le programme, ne touche a rien, propose Reessayer |
| Reglages | `%APPDATA%\Droz\DrozSoftwareCenter.settings` : dossier VST3, prenom. A cote : dernier catalogue lu | sans reseau, l'appli montre ce qui est installe et dit que le catalogue n'a pas ete lu |

Sources : `src/Moteur` (toute la logique, sans fenetre), `src/Systeme` (ce qui depend de Windows), `src/Interface.h` (la fenetre, `droz::ui` seulement), `src/Essai.cpp` (essai automatique), `src/Main.cpp` (modes de lancement).

## Verifier

Le controle Verdict (job "Droz Software Center", Windows) : compile l'appli, fabrique son installeur, l'installe en silencieux, puis lance

```
DrozSoftwareCenter.exe --essai --catalogue a.json --suite b.json --dossier <vst3> --donnees <tmp>
```

Sans fenetre : lit le catalogue, installe le plugin modele, verifie le fichier pose, coupe le reseau, charge le plugin comme un DAW et verifie que la mise a jour est refusee sans rien casser, met a jour, desinstalle. Code de sortie non nul au moindre ecart. Le journal et les captures des onglets sont dans l'artifact `apercu-centre`.

Chez soi, sans Windows (faux installeurs, pas de reseau https) :

```
cmake -S . -B build -DDROZ_CENTRE=ON -DDROZ_PLUGINS=_modele
cmake --build build --target DrozSoftwareCenter
python apps/centre/essai/fabriquer.py /tmp/essai --faux
<appli> --essai --catalogue /tmp/essai/a.json --suite /tmp/essai/b.json --dossier /tmp/essai/vst3 --donnees /tmp/essai/donnees
<appli> --capture /tmp/essai/captures --catalogue /tmp/essai/exemple.json --dossier /tmp/essai/vst3-exemple
```

`<appli> --catalogue <fichier> --dossier <dossier>` ouvre la fenetre sur un catalogue local.

## Publier une version

Monter `version` dans `apps/centre/app.json`, fusionner. `sortie.yml` cree la Release `centre-v<version>` et pose l'installeur sur la page `dernier`. L'appli deja installee affiche "Une nouvelle version du centre existe" et ouvre le telechargement.

## Decisions et lecons

- 2026-11-27, Fx. Appli autonome, Windows seul, JUCE et `droz::ui`, dans le meme depot. Pas de version plugin, pas de Mac ni Linux.
- 2026-11-27, Fx. Tout est installable, chantiers compris. Une mise a jour ne part jamais sans clic.
- 2026-11-27, Fx. L'appli ne se met pas a jour elle-meme : elle previent et ouvre le telechargement.
- 2026-11-27, Fx. L'appli s'installe pour l'utilisateur, sans droits d'administrateur. Les droits ne sont demandes que pour ecrire dans le dossier VST3.
- 2026-11-27, Fx. Un essai garde souvent le meme numero de version d'un commit a l'autre : c'est le `build` (7 premiers caracteres du commit) qui dit s'il y a du nouveau.
- 2026-11-27, Fx. Les essais ne sont publies que pour les branches du depot, jamais pour une PR venue d'un autre depot : sinon n'importe qui pourrait faire distribuer un executable par l'appli.

- 2026-11-27, Fx. Retour a chaud sur la 0.1.0 : trop d'infos. Une seule fenetre, les fiches, les versions, un bouton. Ce qui sert aux membres (ou en est l'atelier) vit sur la page d'accueil du depot, pas dans l'appli.
- 2026-11-27, Fx. Icone : l'image du collectif (`icone.png`), la meme que l'avatar de l'organisation.

- 2026-11-27, Fx. Retour sur la 0.2.0 : symetrique, epure, a la charte. D'ou les cartes centrees, les tailles de texte de la charte (15, 13, 11, 9) et un seul bouton par carte quand c'est possible.

## Pas dans cette version

- Mise a jour a chaud d'un plugin charge dans un DAW : Windows verrouille le fichier, et le DAW garde l'ancienne version en memoire.
- Signature du code : Windows affiche un avertissement a l'installation de l'appli. Pistes dans `docs/METHODE.md`, partie 5.
- Suivi detaille des chantiers, journal des versions, boite a idees, comptes, licences.
