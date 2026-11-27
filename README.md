# DROZ LAB

Plugins audio VST3 du collectif [Droz](https://drozrecordz.com).

## Installer les plugins

**[Telecharger le Droz Software Center](https://github.com/droz-lab/plugins/releases/download/dernier/Droz-Software-Center-Setup.exe)** (Windows)

Une seule appli pour installer, mettre a jour et desinstaller tous les plugins Droz, ceux en chantier compris. Double-clique sur le fichier telecharge. Windows signale un editeur inconnu : "Informations complementaires" puis "Executer quand meme".

Sans l'appli, ou pour voir ou en est chaque plugin : [la page de l'atelier](https://github.com/droz-lab/plugins/releases/tag/dernier), refaite toute seule.

<details>
<summary>Mac, Linux, zips</summary>

Zips de chaque version : [Releases](https://github.com/droz-lab/plugins/releases).

- Mac : `curl -fsSL https://raw.githubusercontent.com/droz-lab/plugins/main/tools/installer.sh | bash -s -- <dossier>`
- Mac a la main : copie le `.vst3` dans `~/Library/Audio/Plug-Ins/VST3/`, puis `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"<Nom>.vst3"`.
- Windows a la main : copie le dossier `.vst3` du zip dans ton dossier VST3.

</details>

## Participer

[Commencer ici](docs/ATELIER.md#commencer-ici--un-seul-message-a-coller) · [L'atelier](docs/ATELIER.md) · [Methode](docs/METHODE.md) · [Habillage](docs/HABILLAGE.md) · [Issues](https://github.com/droz-lab/plugins/issues)
