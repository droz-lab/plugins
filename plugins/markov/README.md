# Markov : mode d'emploi

Markov invente des mélodies pour 8 synthés à la fois. Il ne fait pas de son lui-même (sauf une petite « écoute » pour tester) : il envoie des notes MIDI. La piste 1 de Markov parle sur le canal MIDI 1, la piste 2 sur le canal 2, et ainsi de suite jusqu'à 8.

## 1. Récupérer le plugin

1. Sur GitHub, ouvre l'onglet **Actions**, puis le dernier run « Compilation » vert de la branche de Markov.
2. En bas, dans **Artifacts**, télécharge `plugins-Windows` (ou `plugins-Mac` si un Mac a été compilé).
3. Dézippe : tu obtiens un dossier `Markov.vst3`.

Ou, quand une version est publiée : onglet **Releases**, fichier `Markov-...-Windows.zip` ou `-Mac.zip`.

## 2. Compiler soi-même (facultatif)

1. Installe CMake 3.25 ou plus, Python 3, et Visual Studio 2022 (Windows) ou Xcode (Mac).
2. Dans un terminal, à la racine du dépôt :
   ```
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDROZ_PLUGINS=markov
   ```
3. Puis :
   ```
   cmake --build build --config Release
   ```
4. Le plugin est dans `build/Markov_artefacts/Release/VST3/Markov.vst3`.

## 3. Installer le VST3

1. Ferme ton DAW.
2. Windows : copie `Markov.vst3` dans `C:\Program Files\Common Files\VST3\`.
3. Mac : copie `Markov.vst3` dans `~/Library/Audio/Plug-Ins/VST3/`, puis dans le Terminal :
   ```
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Markov.vst3
   ```
4. Rouvre le DAW et relance la recherche de plugins si besoin.

## 4. Brancher dans Ableton Live

1. Crée une piste MIDI et mets-y **Markov** (catégorie Instruments, fabricant Droz).
2. Crée 8 autres pistes MIDI, chacune avec un synthé.
3. Sur la piste de synthé n° 1 : **MIDI From** = la piste de Markov, puis dans la liste du dessous choisis **Markov** (pas « Post FX »), puis le canal **Ch. 1**. Monitor sur **In**.
4. Recommence pour les pistes 2 à 8 avec les canaux **Ch. 2** à **Ch. 8**.

## 5. Brancher dans Reaper

1. Crée une piste et mets-y **Markov**.
2. Crée 8 pistes, chacune avec un synthé, armées et en monitoring (ou laisse-les simplement recevoir l'envoi).
3. Dans le bouton **Route** de la piste Markov, ajoute un envoi (send) vers chaque piste de synthé : audio **None**, MIDI **canal n → All** (n = 1 pour la 1re, etc.).

## 6. Première génération

1. Lance la lecture du DAW : Markov ne joue que quand le DAW joue, calé sur son tempo.
2. Pas de synthé branché ? Coche **Écoute** pour entendre un petit son intégré (décoche-la ensuite).
3. Les pistes 1 à 4 sont allumées au départ. Allume les autres avec « active ».
4. Choisis la **Gamme** et la **Tonique**. Règle la **Température** : à gauche, mélodies prévisibles ; à droite, plus aléatoires.
5. Clique **Régénérer** jusqu'à trouver une suite qui te plaît. Note la **Graine** : même graine + mêmes réglages = mêmes notes.
6. Clique une piste pour voir sa **Matrice** (chances de passer d'une note à l'autre : glisser sur une case pour la changer) et ses **Pas** (dessiner la chance de jouer, la vélocité, l'écart).
7. Pour apprendre d'un morceau : coche **Apprendre**, envoie des notes MIDI vers la piste de Markov (clavier ou clip) ; la matrice de la piste choisie se remplit. **Réinitialiser** pour recommencer.

Astuces : double-clic sur un réglage = valeur par défaut. Survoler un réglage affiche une explication.
