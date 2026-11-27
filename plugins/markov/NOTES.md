# Markov

Responsable : yoy

## But

Générateur MIDI markovien sur 8 pistes. Pas d'entrée audio. Chaque piste choisit ses notes avec une chaîne de Markov sur les degrés de la gamme, et son rythme avec une grille de pas (chance de jouer + vélocité tirée dans un écart). Chaque piste sort sur son canal MIDI (piste 1 = canal 1, ..., piste 8 = canal 8) ; l'utilisateur met un synthé par piste dans son DAW.

## Etat

- v0.1.0 (2026-11-27, branche `yoy/markov-generateur`) :
  - moteur markovien ordre 1 et ordre 2, température, 10 gammes (dont chromatique et personnalisée), tessiture par piste ;
  - grille de pas (1 à 32) : chance 0-100 %, vélocité centrale + écart ; mode « Chaîne » rythmique (la chance dépend du pas précédent) ;
  - synchro sur le tempo et la position du DAW, notes à l'échantillon près, note-off garanti (arrêt, boucle, saut, piste éteinte, contournement) ;
  - apprentissage depuis le MIDI entrant, bouton Réinitialiser ;
  - interface redimensionnable (ratio fixe), écoute intégrée (petit son par piste) pour entendre sans synthé.
- Tests : `markov_moteur` (30 vérifications, C++ pur), `markov_plugin` (20, plugin entier), tests communs, pluginval niveau 5 : tout vert sous Linux le 2026-11-27.
- Pas encore vérifié dans un vrai DAW (pas de DAW dans l'environnement de Claude). A faire par yoy : Ableton et Reaper, voir plus bas.

## Architecture MIDI : option B (un seul port, piste n = canal n)

- Option A (8 bus de sortie MIDI VST3) : **impossible avec JUCE 9.0.6**, vérifié dans le code : `juce_audio_plugin_client_VST3.cpp`, `getBusCount` renvoie 1 au plus pour les bus d'événements en sortie (`JucePlugin_ProducesMidiOutput`). Il faudrait patcher le wrapper JUCE (code commun, figé) : abandonné.
- Option B retenue, et par défaut : une sortie MIDI, 16 canaux, piste n sur le canal n.
- Ableton Live (rapporté, à vérifier par yoy) : Live sait router la sortie MIDI d'un plugin vers une autre piste MIDI (« MIDI From » = piste de Markov, puis choisir « Markov » dans la 2e liste, puis le canal). Live ne gère pas plusieurs ports MIDI d'un plugin : l'option B est la seule qui y marche de toute façon.
- Reaper (rapporté, à vérifier) : sortie MIDI des VST3 prise en charge ; un envoi (send) par piste de synthé depuis la piste Markov, audio coupé, MIDI « canal n -> tous ».
- Le modèle commun déclare `NEEDS_MIDI_OUTPUT FALSE` pour tous ; `plugin.cmake` le passe à TRUE pour Markov seulement (propriété `JUCE_NEEDS_MIDI_OUTPUT`, lue par JUCE à la génération). Pas besoin de toucher au commun.

## Décisions

- **Hasard à compteur** : chaque tirage = `random01 (graine, piste, numéro de pas, usage)`. Le même pas donne toujours le même tirage, quelle que soit la taille de bloc. Conséquence voulue : une boucle du DAW rejoue exactement la même suite, et l'export sonne comme l'écoute. Pour changer : bouton Régénérer (nouvelle graine).
- **Saut / boucle / relecture** : la chaîne est rejouée en silence depuis le pas 0 jusqu'à la position (au plus 65 536 pas en arrière, pour borner le coût). Les pas avant 0 (pré-roll) ne jouent pas.
- **Changement de matrice, gamme ou température pendant la lecture** : la chaîne continue depuis la note en cours (pas de recalcul), pour que l'oreille suive le geste. Seuls graine, division et activation d'une piste relancent son calcul.
- **États = degrés**, la hauteur réelle est celle du degré la plus proche de la note précédente dans la tessiture. Première note de chaque piste = tonique.
- **Ordre 2** : pas éditable à la souris (une matrice 12x12x12 n'est pas lisible). Rempli par l'apprentissage ; là où il est vide, l'ordre 1 prend le relais.
- **Température** : p proportionnel à w^(1/T), T de 0,1 à 4 (1 au milieu du potard). Une ligne vide devient uniforme : jamais de blocage.
- **Matrice par défaut** : poids selon la distance en degrés (unisson 0,35, seconde 1, tierce 0,75, quarte 0,35, puis 0,25 / 0,18 / 0,12). Aucun zéro. Tant qu'elle n'est pas modifiée, elle s'adapte à la taille de la gamme.
- **Interrupteurs en listes « Non / Oui »** plutôt qu'en `AudioParameterBool` : un Bool garde la valeur brute envoyée par l'hôte (0,3 par ex.) qui ne revient pas après sauvegarde. Le test commun « etat » l'a attrapé. Voir proposition de correction du test commun ci-dessous.
- **Données partagées** : matrices et pas dans un `Pattern` de taille fixe (environ 62 ko), copie sous `juce::SpinLock`, le fil audio ne fait que `ScopedTryLockType`. Apprentissage : le fil audio compte, publie une copie sous try-lock ; un `juce::Timer` du processeur (fil des messages, 10 Hz) l'applique à la matrice de la piste choisie. Ce timer ne cadence jamais les notes.
- **Positions comparées en échantillons** et pas en temps musical : sinon une erreur d'arrondi à la frontière de deux blocs avance une note d'un échantillon (trouvé par le test « à l'échantillon près »).
- **Écoute intégrée** : une voix par piste (sinus + harmonique 2, attaque 2 ms, déclin 350 ms, relâche 40 ms), volume lissé, limiteur `tanh`, sortie toujours finie. Coupée par défaut.
- **Suppression du plugin** : quand l'hôte arrête d'appeler le plugin, il ne peut plus rien envoyer. Le note-off est garanti tant que l'hôte appelle `processBlock` ou `processBlockBypassed` (arrêt, contournement). C'est à l'hôte de couper les notes d'une source supprimée (Ableton et Reaper le font, rapporté).
- Une piste = une note à la fois (durée max = 100 % du pas) : une seule note à suivre par piste, note-off simple et sûr.

## A faire / idées

- Vérifier dans Ableton et Reaper (routage, canal, boucles, export) et noter le résultat ici.
- Proposer une PR `commun:` : le test « etat » de `tools/outil/outil.cpp` devrait comparer les paramètres Bool après arrondi (ou leur passer 0/1). Pas fait ici (hors périmètre).
- Export MIDI (glisser un clip vers le DAW), « évolution » (la boucle varie d'un passage à l'autre tout en restant reproductible).

