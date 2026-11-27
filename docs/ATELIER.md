# L'atelier : travailler dans le depot

Pour les membres du collectif. La page publique est le [README](../README.md).

Tout l'atelier dans une appli : le [Droz Software Center](https://github.com/droz-lab/plugins/releases/download/dernier/Droz-Software-Center-Setup.exe) (Windows). Onglet Plugins : installer, mettre a jour, desinstaller, chantiers compris. Onglet Atelier : les PR en attente, les fiches a ecrire, et un bouton qui copie le message ci-dessous. Sans l'appli : [la page de l'atelier](https://github.com/droz-lab/plugins/releases/tag/dernier), a jour toute seule.

## Commencer ici : un seul message a coller

Le depot sait maintenant guider ton assistant tout seul. Tu donnes une idee, meme en une phrase : il la transforme en fiche de plugin, te pose ses questions en une fois, code, et le depot verifie derriere lui ce qu'on oublie tous (clics, plantages quand le DAW boucle, reglages perdus a la reouverture). Tu passes ton temps sur le son, pas sur la procedure.

Rien ne te bloque en version 0.x : la methode s'affiche en alertes, elle ne fait pas rougir ton travail. Elle devient stricte le jour ou tu decides de sortir une 1.0.0.

Colle ce message tel quel dans ton assistant, ou que tu sois (appli, site, terminal). Remplace seulement ton prenom :

```
Tu travailles sur le depot GitHub droz-lab/plugins : les plugins audio du collectif Droz.
Je suis <fx | yoy | zomban>. Fais tout ce qui suit dans l'ordre, sans me demander la permission a chaque etape.

1. Ouvre ou clone https://github.com/droz-lab/plugins. Si tu n'y as pas acces, ou si tu ne peux pas
   pousser une branche ni ouvrir une PR, dis-le-moi tout de suite et explique-moi pas a pas quoi faire.
   Ne me demande jamais de coller un mot de passe ou un jeton ici.
2. Retrouve ma branche de travail (elle commence par mon prenom). Mets-la a jour avec main :
   git fetch origin main, puis git merge origin/main. S'il y a un conflit, garde mon travail dans
   mon plugin. Dans un fichier commun (workflows, cmake, tools, docs), garde les deux changements
   s'ils vont ensemble, sinon la version de main. Dis-moi ce que tu as fait.
3. Lis CLAUDE.md en entier, puis docs/METHODE.md.
4. Lance : python tools/droz.py bilan
   Regle toi-meme tout ce qu'il liste sous "A REGLER".
5. Pose-moi en UN seul message, avec des choix quand c'est possible, tout ce qu'il liste sous
   "A DEMANDER" et tout ce qui te manque d'autre pour bien travailler. Range mes reponses la ou
   CLAUDE.md le dit, pour ne plus jamais me les redemander.
6. Pour chacun de mes plugins sans FICHE.md : ecris-la a partir de ce que le plugin fait deja,
   note ce que tu supposes, et resume-la-moi en cinq lignes.
7. Pousse, ouvre ou mets a jour la PR, attends le controle Verdict. S'il est rouge, lis le journal
   et corrige. S'il y a des alertes, explique-les-moi simplement et propose la correction.
8. Termine par trois lignes : ce qui a change, le lien de la PR, ce que je dois ecouter ou decider.
```

Les fois suivantes, dis simplement ce que tu veux ("ajoute un swing a mon gate", "fais-moi un delai qui degrade"). L'assistant refait son bilan tout seul. S'il ne le fait pas : "Lis CLAUDE.md et fais le bilan."

Une regle te gene, un controle te bloque a tort : ouvre une Issue. La methode est la pour nous servir, elle se corrige.

## Demarrer un plugin

1. Ouvre le depot avec ton assistant de code.
2. Colle ceci en remplissant les `<...>` :

```
Lis les regles du depot, docs/METHODE.md, docs/PIEGES.md et docs/HABILLAGE.md.
Je veux creer un plugin : <nom>. C'est un <effet | instrument>.
Le son voulu : <deux ou trois phrases, des references si tu en as>.
Je suis <prenom>, niveau en code : <aucun | un peu | a l'aise>. DAW : <Ableton | Bitwig>, sur <Windows | Mac>.
Cree-le avec tools/droz.py, ouvre une PR, et dis-moi quoi ecouter.
```

Une phrase suffit aussi ("fais-moi un gate rythmique qui respire") : l'assistant doit alors remplir la fiche du plugin (`FICHE.md`) lui-meme, te poser trois questions au plus, et te la resumer avant de coder. C'est la [methode](METHODE.md), et le controle Verdict refuse un plugin sans fiche.

3. L'assistant cree `plugins/<dossier>/`, pousse une branche et ouvre une PR. GitHub compile pour Windows (et Linux pour les tests). La version Mac se compile a la demande (Actions > Compilation > Run workflow, case "mac") et a chaque publication.
4. Recupere le plugin : des que le controle Verdict de ta PR est vert, il apparait "a l'essai" dans le Droz Software Center, chez toi comme chez les autres. Rien a faire de ton cote. Mac : onglet **Actions** > le dernier run > **Artifacts** > `plugins-Mac`. L'artifact `apercu` contient la capture de l'interface et les rendus WAV.

## Essayer le plugin d'un autre

Ouvre le Droz Software Center, clique sur Actualiser : les chantiers dont le Verdict est vert sont dans "En chantier, a l'essai". Installer pose le plugin dans ton dossier VST3. A chaque nouveau Verdict vert de la branche, la ligne passe a "mise a jour".
Ferme le DAW ou retire le plugin des pistes avant de mettre a jour : Windows verrouille un plugin charge, et l'appli te le dit sans rien casser.

## Ce qu'il y a dans le depot

| Dossier | Role | Qui modifie |
|---|---|---|
| `plugins/<dossier>/` | Un plugin : `plugin.json` (identite), `FICHE.md` (ce qu'il doit faire, ecrite avant le code), `src/`, `NOTES.md` (memoire du plugin), `boucles/` (WAV courts pour les rendus) | Son responsable |
| `plugins/_modele/` | Le point de depart, compile a chaque changement commun | PR relue |
| `libs/droz_dsp/` | Briques DSP partagees, C++ pur, testees | PR relue |
| `libs/droz_ui/` | Habillage commun des interfaces, regle dans `docs/HABILLAGE.md` | PR relue |
| `carnet/<prenom>/` | Reflexions et idees de chacun, et `PROFIL.md` : ton contexte (niveau en code, DAW, systeme, ecoute, references), pour que l'assistant ne te le redemande pas | Chacun chez soi |
| `apps/centre/` | Le Droz Software Center : l'appli Windows qui installe les plugins. Memoire dans `apps/centre/NOTES.md` | PR relue |
| `tools/droz.py` | Creer un plugin, verifier le registre, ecrire le catalogue lu par l'appli | PR relue |
| `tools/outil/` | Tests communs, rendu WAV et capture d'interface sans DAW | PR relue |
| `.github/workflows/` | Compilation, sorties, essais des chantiers, catalogue | PR relue |
| `CLAUDE.md`, `docs/METHODE.md`, `docs/PIEGES.md` | Regles, methode et lecons communes | PR relue (PIEGES : ajout libre) |

## Regles du jeu

1. Personne ne pousse sur `main`. Une branche par sujet (`<prenom>/<dossier>-<sujet>`), une PR, le controle **Verdict** au vert.
2. Chacun est chez lui dans son dossier de plugin. Tout ce qui est commun passe par une PR relue par un autre membre.
3. L'identite d'un plugin sorti ne change plus : `code`, `cible`, identifiants des parametres. Sinon les projets des autres cassent.
4. Une lecon qui servira aux autres : une ligne dans `docs/PIEGES.md`. Une idee ou un souci : onglet **Issues**.
5. Tous les plugins portent l'habillage Droz (`docs/HABILLAGE.md`), a la creation comme a chaque mise a jour. Une envie differente se propose pour tout le monde, pas pour un seul plugin.
6. Pas de secret, pas de sample sous droits, pas de binaire lourd dans le depot.
7. Un plugin a sa fiche (`FICHE.md`) avant son code. Version 0.x : on cherche, on publie vite. Version 1.0.0 : palier pro, tous les controles bloquent ([methode](METHODE.md)).

## Publier une version

Une seule facon de faire, pour tous les plugins et toutes les versions :

1. Monter `version` dans `plugins/<dossier>/plugin.json`.
2. Fusionner la PR. La version part toute seule : installeur Windows, zips Mac et Linux.
3. Le Droz Software Center la propose a tout le monde : "mise a jour" chez ceux qui avaient l'ancienne. Un seul fichier par plugin sur la page fixe `dernier` : l'installeur Windows (`<Nom>-Setup.exe`). Les zips restent sur la Release de la version.

Le Droz Software Center se publie pareil : monter `version` dans `apps/centre/app.json` et fusionner. L'appli previent ses utilisateurs qu'une nouvelle version existe.

Un nouveau plugin fusionne dans `main` est publie de la meme facon, a sa premiere fusion.
Une seule version publiee par plugin : a chaque publication, les versions depassees sont retirees toutes seules (Release et etiquette).
Bitwig : si un plugin fait communiquer ses instances entre elles, son hebergement ne doit pas etre sur "Individually".

## Compiler chez soi (facultatif)

Il faut CMake 3.25+, un compilateur C++20 (Visual Studio 2022, Xcode ou g++) et Python 3.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDROZ_PLUGINS=<dossier> -DDROZ_INSTALLER=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

`DROZ_INSTALLER=ON` copie le plugin dans le dossier VST3 du systeme. L'outil `<Cible>_outil` (dans `build`) sait faire `test`, `params`, `rendu entree.wav sortie.wav --param gain=0.8` et `capture ui.png`.

## A decider ensemble

- Licence. JUCE est gratuit sous AGPLv3 (code ouvert) ou payant sous licence commerciale : a trancher avant de distribuer hors du collectif.
- Format CLAP pour Bitwig : pas dans JUCE 9.0.6, possible via clap-juce-extensions (a verifier avec JUCE 9).
