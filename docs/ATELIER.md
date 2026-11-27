# L'atelier : travailler dans le depot

Pour les membres du collectif. La page publique est le [README](../README.md).

## A chaque session

Tu n'as rien a retenir. Ouvre le depot avec ton assistant et dis-lui ce que tu veux. Il commence par un bilan (`python tools/droz.py bilan`) : il met ta branche a jour, verifie tes fiches, et te demande en une fois ce qui lui manque (ton contexte la premiere fois, un acces, une boucle, une decision). S'il ne le fait pas, dis-lui : "Lis CLAUDE.md et fais le bilan."

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
4. Recupere le plugin : onglet **Actions** > le dernier run > **Artifacts** > `plugins-Windows` ou `plugins-Mac`. L'artifact `apercu` contient la capture de l'interface et les rendus WAV.

## Premiere contribution (test de l'atelier)

1. Accepte l'invitation a l'organisation, ouvre le depot avec ton assistant de code.
2. Demande : `Lis les regles du depot. Je suis <prenom>. Ajoute plugins/test-v0/equipe/<prenom>.txt avec la ligne "<ton texte>", ouvre une PR et envoie-moi la capture de l'interface.`
3. Quand le controle Verdict est vert, fusionne ta PR (bouton Merge).
4. Installe le plugin avec le lien du README et ouvre "Droz Test v0" dans ton DAW.

## Ce qu'il y a dans le depot

| Dossier | Role | Qui modifie |
|---|---|---|
| `plugins/<dossier>/` | Un plugin : `plugin.json` (identite), `FICHE.md` (ce qu'il doit faire, ecrite avant le code), `src/`, `NOTES.md` (memoire du plugin), `boucles/` (WAV courts pour les rendus) | Son responsable |
| `plugins/_modele/` | Le point de depart, compile a chaque changement commun | PR relue |
| `libs/droz_dsp/` | Briques DSP partagees, C++ pur, testees | PR relue |
| `libs/droz_ui/` | Habillage commun des interfaces, regle dans `docs/HABILLAGE.md` | PR relue |
| `carnet/<prenom>/` | Reflexions et idees de chacun, et `PROFIL.md` : ton contexte (niveau en code, DAW, systeme, ecoute, references), pour que l'assistant ne te le redemande pas | Chacun chez soi |
| `tools/droz.py` | Creer un plugin, verifier le registre | PR relue |
| `tools/outil/` | Tests communs, rendu WAV et capture d'interface sans DAW | PR relue |
| `.github/workflows/` | Compilation et sorties | PR relue |
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

1. Monter `version` dans `plugins/<dossier>/plugin.json` et lancer `python tools/droz.py vitrine` (tableau du README).
2. Fusionner la PR. La version part toute seule : installeur Windows, zips Mac et Linux.
3. Un seul fichier est mis en avant : l'installeur Windows (`<Nom>-Setup.exe`), sur la page fixe `dernier` que pointe le README. Il demande le dossier VST3 et le retient. Les zips restent sur la Release de la version.

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
