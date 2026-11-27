<h1 align="center">DROZ LAB</h1>

<p align="center">Les plugins audio du collectif Droz. VST3, Windows et Mac, faits a plusieurs.</p>

<p align="center">
  <a href="https://github.com/droz-lab/plugins/releases/tag/dernier"><b>Telecharger</b></a> ·
  <a href="#les-plugins">Les plugins</a> ·
  <a href="#demarrer-un-plugin">Participer</a> ·
  <a href="docs/HABILLAGE.md">Habillage</a>
</p>

<p align="center">
  <a href="https://github.com/droz-lab/plugins/actions/workflows/compilation.yml"><img alt="Compilation" src="https://github.com/droz-lab/plugins/actions/workflows/compilation.yml/badge.svg?branch=main"></a>
  <a href="https://github.com/droz-lab/plugins/releases"><img alt="Derniere version" src="https://img.shields.io/github/v/release/droz-lab/plugins?label=version&color=F0431F"></a>
</p>

<p align="center"><img alt="L'habillage commun des plugins Droz" src="docs/habillage.png" width="640"></p>

Atelier commun du collectif Droz : nos plugins sont compiles et verifies automatiquement, avec les memes regles pour tous nos Claude.

## Les plugins

<!-- vitrine:debut -->
| Plugin | Ce qu'il fait | Version | Responsable |
|---|---|---|---|
| [Droz Premier Jet](plugins/premier-jet/NOTES.md) | Plugin de test de l'atelier : un gain, et le nom de chaque membre affiche dans l'interface. | 0.1.1 | equipe |
<!-- vitrine:fin -->

Pour refaire ce tableau apres un changement de `plugin.json` : `python tools/droz.py vitrine`.

## Telecharger les plugins

**[Derniere version de chaque plugin](https://github.com/droz-lab/plugins/releases/tag/dernier)**

- Windows : telecharge `<Nom>-Setup.exe` et double-clique. Il pose le plugin dans le dossier VST3, rien d'autre a faire. Windows previent que l'editeur est inconnu (nos plugins ne sont pas signes) : "Informations complementaires", puis "Executer quand meme". Ferme ton DAW avant de remplacer une version.
- Mac : dans le Terminal, `curl -fsSL https://raw.githubusercontent.com/droz-lab/plugins/main/tools/installer.sh | bash -s -- <dossier>`, ou le `-Mac.zip` a la main (voir plus bas).

## Demarrer un plugin

1. Ouvre Claude Code sur ce depot.
2. Colle ceci en remplissant les `<...>` :

```
Lis CLAUDE.md, docs/PIEGES.md et docs/HABILLAGE.md.
Je veux creer un plugin : <nom>. C'est un <effet | instrument>.
Le son voulu : <deux ou trois phrases, des references si tu en as>.
Je suis <prenom>, niveau en code : <aucun | un peu | a l'aise>. DAW : <Ableton | Bitwig>, sur <Windows | Mac>.
Cree-le avec tools/droz.py, ouvre une PR, et dis-moi quoi ecouter.
```

3. Ton Claude cree `plugins/<dossier>/`, pousse une branche et ouvre une PR. GitHub compile pour Windows (et Linux pour les tests). La version Mac se compile a la demande (Actions > Compilation > Run workflow, case "mac") et a chaque publication.
4. Recupere le plugin : onglet **Actions** > le dernier run > **Artifacts** > `plugins-Windows` ou `plugins-Mac`. L'artifact `apercu` contient la capture de l'interface et les rendus WAV.

## Premiere contribution (test de l'atelier)

1. Accepte l'invitation a l'organisation, ouvre Claude Code sur ce depot.
2. Dis a ton Claude : `Lis CLAUDE.md. Je suis <prenom>. Ajoute plugins/premier-jet/equipe/<prenom>.txt avec la ligne "<ton texte>", ouvre une PR et envoie-moi la capture de l'interface.`
3. Quand le controle Verdict est vert, fusionne ta PR (bouton Merge).
4. Installe le plugin avec une des commandes ci-dessous et ouvre "Droz Premier Jet" dans ton DAW.

## Installer un plugin

En une commande, derniere version publiee (par defaut `premier-jet`) :

- Windows, dans PowerShell : `irm https://raw.githubusercontent.com/droz-lab/plugins/main/tools/installer.ps1 | iex`
- Mac, dans le Terminal : `curl -fsSL https://raw.githubusercontent.com/droz-lab/plugins/main/tools/installer.sh | bash`

A la main :


- Windows : dezippe, copie le dossier `.vst3` dans `C:\Program Files\Common Files\VST3\`. Ferme le DAW avant de remplacer une version.
- Mac : copie le `.vst3` dans `~/Library/Audio/Plug-Ins/VST3/`, puis une fois par version dans le Terminal : `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"<Nom>.vst3"` (nos plugins ne sont pas signes par Apple).
- Bitwig : si un plugin fait communiquer ses instances entre elles, son hebergement ne doit pas etre sur "Individually" ("Together" ou "By plug-in" conviennent).

## Ce qu'il y a dans le depot

| Dossier | Role | Qui modifie |
|---|---|---|
| `plugins/<dossier>/` | Un plugin : `plugin.json` (identite), `src/`, `NOTES.md` (memoire du plugin), `boucles/` (WAV courts pour les rendus) | Son responsable |
| `plugins/_modele/` | Le point de depart, compile a chaque changement commun | PR relue |
| `libs/droz_dsp/` | Briques DSP partagees, C++ pur, testees | PR relue |
| `libs/droz_ui/` | Habillage commun des interfaces, regle dans `docs/HABILLAGE.md` | PR relue |
| `carnet/<prenom>/` | Reflexions et idees de chacun | Chacun chez soi |
| `tools/droz.py` | Creer un plugin, verifier le registre | PR relue |
| `tools/outil/` | Tests communs, rendu WAV et capture d'interface sans DAW | PR relue |
| `.github/workflows/` | Compilation et sorties | PR relue |
| `CLAUDE.md`, `docs/PIEGES.md` | Regles et lecons communes a tous nos Claude | PR relue (PIEGES : ajout libre) |

## Regles du jeu

1. Personne ne pousse sur `main`. Une branche par sujet (`<prenom>/<dossier>-<sujet>`), une PR, le controle **Verdict** au vert.
2. Chacun est chez lui dans son dossier de plugin. Tout ce qui est commun passe par une PR relue par un autre membre.
3. L'identite d'un plugin sorti ne change plus : `code`, `cible`, identifiants des parametres. Sinon les projets des autres cassent.
4. Une lecon qui servira aux autres : une ligne dans `docs/PIEGES.md`. Une idee ou un souci : onglet **Issues**.
5. Tous les plugins portent l'habillage Droz (`docs/HABILLAGE.md`), a la creation comme a chaque mise a jour. Une envie differente se propose pour tout le monde, pas pour un seul plugin.
6. Pas de secret, pas de sample sous droits, pas de binaire lourd dans le depot.

## Publier une version

1. Monter `version` dans `plugins/<dossier>/plugin.json`, fusionner la PR.
2. Onglet **Actions** > **Sortie** > **Run workflow**, taper le dossier du plugin (ex. `premier-jet`).
3. Les zips Windows, Mac et Linux arrivent dans l'onglet **Releases**, sans limite de duree.

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
