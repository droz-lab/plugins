<!-- Page ecrite par `python tools/droz.py accueil` et rangee toute seule (centre.yml). Ne pas modifier a la main. -->
# DROZ LAB

Plugins audio VST3 du collectif [Droz](https://drozrecordz.com).

## Droz Software Center

**[Telecharger le Droz Software Center](https://github.com/droz-lab/plugins/releases/download/dernier/Droz-Software-Center-Setup.exe)** (Windows)

Une seule appli pour installer, mettre a jour et desinstaller tous les plugins ci-dessous, chantiers compris, d'un clic. Double-clique sur le fichier telecharge. Windows signale un editeur inconnu : "Informations complementaires" puis "Executer quand meme".

## Plugins

Aucun plugin publie pour l'instant : les premiers sont en chantier, juste en dessous.

## En chantier

Les plugins en cours de fabrication. Chaque mise a jour d'une branche qui passe les controles devient installable "a l'essai", ici et dans le Droz Software Center.

### Droz Courbes

`0.4.0 essai` - par Zomban

Generateur de courbes synchronisees au tempo, rangees par familles (rampes, pompes, marches, rebonds...), envoyees en MIDI CC pour moduler n'importe quel parametre.

[Installer l'essai (Windows)](https://github.com/droz-lab/plugins/releases/download/dernier/Droz-Courbes-Setup.exe) · [suivre le chantier](https://github.com/droz-lab/plugins/tree/zomban/courbes-famille/plugins/courbes)

### Droz Master

`0.1.0 essai` - par YoY

Chaine de mastering a modules reordonnables : EQ, EQ dynamique, compresseur a styles, multibande, suppresseur de resonances, exciter multibande, clipper, limiteur. Chaque module en stereo, mid-side, gauche-droite ou transitoire-tonal.

[Installer l'essai (Windows)](https://github.com/droz-lab/plugins/releases/download/dernier/Droz-Master-Setup.exe) · [suivre le chantier](https://github.com/droz-lab/plugins/tree/claude/eloquent-carson-svfmmm/plugins/master)

### IDGems

`0.1.0 en chantier` - par YoY

Machine a glitch randomisable : rack d'effets IDGems

pas encore installable · [suivre le chantier](https://github.com/droz-lab/plugins/tree/yoy/idgems-plugins/plugins/idgems)

### IDGems Stretch

`0.1.0 en chantier` - par YoY

Gel de frame sonore a un rate, grain et texture

pas encore installable · [suivre le chantier](https://github.com/droz-lab/plugins/tree/yoy/idgems-plugins/plugins/stretch)

### Markov

`0.1.0 essai` - par YoY

Générateur MIDI markovien sur 8 pistes

[Installer l'essai (Windows)](https://github.com/droz-lab/plugins/releases/download/dernier/Markov-Setup.exe) · [suivre le chantier](https://github.com/droz-lab/plugins/tree/yoy/markov-generateur/plugins/markov)

## Participer

- Membres du collectif : [par ou commencer](docs/ATELIER.md). Ou en est chaque chantier : [la page de l'atelier](https://github.com/droz-lab/plugins/releases/tag/dernier).
- Une idee de plugin : [formulaire](https://github.com/droz-lab/plugins/issues/new?template=idee-de-plugin.yml). Un souci : [formulaire](https://github.com/droz-lab/plugins/issues/new?template=souci.yml).
- Comment on travaille : [la methode](docs/METHODE.md), [l'habillage](docs/HABILLAGE.md).

<details>
<summary>Mac, Linux, zips</summary>

Zips de chaque version : [Releases](https://github.com/droz-lab/plugins/releases).

- Mac : `curl -fsSL https://raw.githubusercontent.com/droz-lab/plugins/main/tools/installer.sh | bash -s -- <dossier>`
- Mac a la main : copie le `.vst3` dans `~/Library/Audio/Plug-Ins/VST3/`, puis `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"<Nom>.vst3"`.
- Windows a la main : copie le dossier `.vst3` du zip dans ton dossier VST3.

</details>
