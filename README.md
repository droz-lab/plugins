<h1 align="center">DROZ LAB</h1>

<p align="center">Les plugins audio du collectif Droz. VST3, Windows et Mac.</p>

<p align="center">
  <a href="#les-plugins">Les plugins</a> ·
  <a href="#installer">Installer</a> ·
  <a href="docs/ATELIER.md">Participer</a>
</p>

## Les plugins

<!-- vitrine:debut -->
| Plugin | Ce qu'il fait | Par | Etat | Telecharger |
|---|---|---|---|---|
| [Droz Test v0](plugins/test-v0/NOTES.md) | Plugin de test de l'atelier : un gain, et le nom de chaque membre affiche dans l'interface. | equipe | publie | [Installer (Windows)](https://github.com/droz-lab/plugins/releases/download/dernier/Droz-Test-v0-Setup.exe) |
| [Droz Courbes](https://github.com/droz-lab/plugins/tree/zomban/courbes-famille/plugins/courbes) | Generateur de courbes synchronisees au tempo, rangees par familles (rampes, pompes, marches, rebonds...), envoyees en MIDI CC pour moduler n'importe quel parametre. | Zomban | en chantier | - |
| [Droz Master](https://github.com/droz-lab/plugins/tree/claude/eloquent-carson-svfmmm/plugins/master) | Chaine de mastering a modules reordonnables : EQ, EQ dynamique, compresseur a styles, multibande, suppresseur de resonances, exciter multibande, clipper, limiteur. Chaque module en stereo, mid-side, gauche-droite ou transitoire-tonal. | YoY | en chantier | - |
| [IDGems](https://github.com/droz-lab/plugins/tree/yoy/idgems-plugins/plugins/idgems) | Machine a glitch randomisable : rack d'effets IDGems | YoY | en chantier | - |
| [IDGems Stretch](https://github.com/droz-lab/plugins/tree/yoy/idgems-plugins/plugins/stretch) | Gel de frame sonore a un rate, grain et texture | YoY | en chantier | - |
| [Markov](https://github.com/droz-lab/plugins/tree/yoy/markov-generateur/plugins/markov) | Générateur MIDI markovien sur 8 pistes | YoY | en chantier | - |
<!-- vitrine:fin -->

## Installer

Clique sur "Installer (Windows)" dans le tableau, puis double-clique sur le fichier telecharge.

1. Windows previent que l'editeur est inconnu (nos plugins ne sont pas signes) : "Informations complementaires", puis "Executer quand meme".
2. L'installeur demande ton dossier VST3. Garde celui qui est propose, ou choisis le tien : il sera retenu pour les prochains plugins.
3. Ferme ton DAW avant de remplacer une version, puis relance-le.

C'est pareil pour tous les plugins et toutes les versions.

<details>
<summary>Mac, Linux, ou installation a la main</summary>

Les zips Mac, Linux et Windows de chaque version sont sur la page [Releases](https://github.com/droz-lab/plugins/releases), dans la version du plugin.

- Mac, en une commande dans le Terminal : `curl -fsSL https://raw.githubusercontent.com/droz-lab/plugins/main/tools/installer.sh | bash -s -- <dossier>`
- Mac a la main : copie le `.vst3` dans `~/Library/Audio/Plug-Ins/VST3/`, puis une fois par version : `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"<Nom>.vst3"` (nos plugins ne sont pas signes par Apple).
- Windows a la main : dezippe et copie le dossier `.vst3` dans ton dossier VST3.
- Bitwig : si un plugin fait communiquer ses instances entre elles, son hebergement ne doit pas etre sur "Individually" ("Together" ou "By plug-in" conviennent).

</details>

## Participer

Pour les membres du collectif : [l'atelier](docs/ATELIER.md), [l'habillage commun](docs/HABILLAGE.md), [les lecons](docs/PIEGES.md).
Une idee ou un souci : onglet [Issues](https://github.com/droz-lab/plugins/issues).
