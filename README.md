# DROZ LAB

Plugins audio VST3 du collectif [Droz](https://drozrecordz.com).

**[Ouvrir le centre Droz Lab](https://github.com/droz-lab/plugins/releases/tag/dernier)** : installer les plugins, suivre les chantiers, savoir par ou commencer. La page se met a jour toute seule.

<!-- vitrine:debut -->
| Plugin | Quoi | Par | Windows |
|---|---|---|---|
| [Droz Test v0](plugins/test-v0/NOTES.md) | Plugin de test de l'atelier | equipe | [Installer](https://github.com/droz-lab/plugins/releases/download/dernier/Droz-Test-v0-Setup.exe) |
| [Droz Courbes](https://github.com/droz-lab/plugins/tree/zomban/courbes-famille/plugins/courbes) | Generateur de courbes synchronisees au tempo | Zomban | en chantier, [essais](https://github.com/droz-lab/plugins/actions/workflows/compilation.yml?query=branch%3Azomban%2Fcourbes-famille+is%3Asuccess) |
| [Droz Master](https://github.com/droz-lab/plugins/tree/claude/eloquent-carson-svfmmm/plugins/master) | Chaine de mastering a modules reordonnables | YoY | en chantier, [essais](https://github.com/droz-lab/plugins/actions/workflows/compilation.yml?query=branch%3Aclaude%2Feloquent-carson-svfmmm+is%3Asuccess) |
| [IDGems](https://github.com/droz-lab/plugins/tree/yoy/idgems-plugins/plugins/idgems) | Machine a glitch randomisable : rack d'effets IDGems | YoY | en chantier, [essais](https://github.com/droz-lab/plugins/actions/workflows/compilation.yml?query=branch%3Ayoy%2Fidgems-plugins+is%3Asuccess) |
| [IDGems Stretch](https://github.com/droz-lab/plugins/tree/yoy/idgems-plugins/plugins/stretch) | Gel de frame sonore a un rate, grain et texture | YoY | en chantier, [essais](https://github.com/droz-lab/plugins/actions/workflows/compilation.yml?query=branch%3Ayoy%2Fidgems-plugins+is%3Asuccess) |
| [Markov](https://github.com/droz-lab/plugins/tree/yoy/markov-generateur/plugins/markov) | Générateur MIDI markovien sur 8 pistes | YoY | en chantier, [essais](https://github.com/droz-lab/plugins/actions/workflows/compilation.yml?query=branch%3Ayoy%2Fmarkov-generateur+is%3Asuccess) |
<!-- vitrine:fin -->

**Installer** : clique sur "Installer", double-clique sur le fichier, choisis ton dossier VST3. Windows signale un editeur inconnu : "Informations complementaires" puis "Executer quand meme".

<details>
<summary>Mac, Linux, zips</summary>

Zips de chaque version : [Releases](https://github.com/droz-lab/plugins/releases).

- Mac : `curl -fsSL https://raw.githubusercontent.com/droz-lab/plugins/main/tools/installer.sh | bash -s -- <dossier>`
- Mac a la main : copie le `.vst3` dans `~/Library/Audio/Plug-Ins/VST3/`, puis `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"<Nom>.vst3"`.
- Windows a la main : copie le dossier `.vst3` du zip dans ton dossier VST3.

</details>

[Commencer ici](docs/ATELIER.md#commencer-ici--un-seul-message-a-coller) · [Participer](docs/ATELIER.md) · [Methode](docs/METHODE.md) · [Habillage](docs/HABILLAGE.md) · [Issues](https://github.com/droz-lab/plugins/issues)
