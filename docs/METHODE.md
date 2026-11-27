# La methode Droz

Comment on fabrique un plugin ici, de l'idee a la version publiee. Elle vaut pour tout le monde et pour tout assistant de code.

Le but : qu'une demande d'une phrase donne quand meme un plugin reflechi, et que nos plugins tiennent a cote de ceux qu'on utilise tous les jours. Ce qui peut se verifier tout seul est verifie par le controle **Verdict**. Le reste est une liste courte, a cocher a la main avant une version 1.0.0.

Etat de l'art releve le 27/11/2026. Chaque choix technique dit ce qu'on a retenu, ce qu'on a ecarte et pourquoi, avec sa source (partie 5).

## 1. Le chemin d'un plugin

| Etape | Ce qu'on produit | Qui bloque si c'est rate |
|---|---|---|
| 1. Idee | une phrase : le son voulu, pour quoi faire | personne |
| 2. Fiche | `plugins/<dossier>/FICHE.md` remplie | Verdict |
| 3. Squelette | `python tools/droz.py nouveau ...`, PR ouverte, Verdict vert sur le modele | Verdict |
| 4. Son | le traitement, sans interface soignee. Une brique utile a d'autres va dans `libs/droz_dsp` avec son test | Verdict |
| 5. Ecoute | rendus WAV sec et traite sur les boucles du plugin, ecoutes par le responsable | le responsable |
| 6. Interface | habillage Droz (`docs/HABILLAGE.md`), un visuel anime qui montre ce que fait le son | Verdict (charte), le responsable (gout) |
| 7. Sortie | `version` montee, fusion : la publication part seule | Verdict |

Le son passe avant l'interface : un plugin qui sonne avec trois potards bruts vaut mieux qu'une belle fenetre sur un traitement pas fini. Une fonction a la fois, finie.

## 2. La fiche

La fiche tient en une page. Elle s'ecrit avant le code et fait foi : quand le plugin change, la fiche change dans la meme PR. Modele : `plugins/_modele/FICHE.md`.

| Section | A quoi elle sert |
|---|---|
| Intention | savoir quand s'arreter |
| References d'ecoute | donner une cible a l'oreille, et dire ce qu'on ne copie pas |
| Signal | l'ordre des traitements, la latence, le lien au tempo : decides, pas decouverts en route |
| Reglages en facade | 6 au plus, avec ce qu'on entend en tournant chacun. Les identifiants sont ceux du code |
| Cas limites | silence, niveau maximal, transport arrete, boucle, saut de tempo, automation rapide, 192 kHz, mono |
| Qualite attendue | repliement, charge, latence, transparence en position neutre |
| Comment on verifie | quoi ecouter, sur quelle boucle, quoi mesurer |
| Hors perimetre | ce qu'on ne commence pas |
| Questions ouvertes | les suppositions de l'assistant, a faire valider |

### Quand la demande tient en une phrase

L'assistant ne demande pas a la personne de remplir la fiche. Il la remplit lui-meme :

1. Il reformule l'idee en une phrase et la fait confirmer.
2. Il ecrit la fiche avec ce qui a ete dit. Ce qu'il ne sait pas, il le suppose, et l'ecrit `SUPPOSE : ...` dans "Questions ouvertes".
3. Il pose au plus trois questions a choix, seulement sur ce qui change le son ou les reglages en facade. Sans reponse, il garde ses suppositions.
4. Il resume la fiche en cinq lignes, puis il code.

Exemple. Demande : "fais-moi un gate rythmique qui respire". L'assistant suppose : effet stereo cale au tempo, motif de 16 pas, attaque et relachement reglables pour eviter les clics, pas de sidechain, latence nulle, reference ShaperBox Volume pour la forme et un tremolo doux pour le caractere. Il demande : motif fixe ou dessine a la main ? swing ou non ? un mode qui suit l'enveloppe de l'entree ? Puis il avance.

### Ce que le Verdict refuse

- pas de `FICHE.md`, une section absente, ou une ligne `A REMPLIR` encore la ;
- un tableau de reglages vide, ou plus de 6 reglages en facade ;
- un reglage de la fiche qui n'existe pas dans le plugin compile.

## 3. Deux paliers

Le numero de version dit ou en est le plugin. On ne monte pas en 1.0.0 par habitude.

| | Chantier (0.x) | Pro (1.0.0 et plus) |
|---|---|---|
| Esprit | on cherche, on s'amuse, on publie vite pour essayer a trois | on le pose sur un master ou dans un live sans y penser |
| Fiche | obligatoire | obligatoire |
| Tests communs | les echecs bloquent, les alertes s'affichent | echecs et alertes bloquent |
| pluginval | niveau 5 bloquant, niveau 10 lance pour information | niveau 10 bloquant |
| Liste a la main (3.2) | non | oui, cochee dans la PR qui monte en 1.0.0 |

### 3.1 Ce que le Verdict verifie

| Controle | Ce qu'il attrape | Depuis |
|---|---|---|
| registre | deux plugins avec le meme code : le DAW les confond | depart |
| fiche | plugin sans fiche, fiche incomplete, reglage absent du plugin | cette methode |
| robustesse | NaN ou explosion a 44,1, 48 et 96 kHz, blocs de 16 a 1024, blocs irreguliers, reglages au hasard | depart |
| silence | bruit de fond ou oscillation apres la fin du son | depart |
| etat | reglages perdus au rechargement du projet, plantage sur un etat corrompu | depart |
| determinisme, taille de bloc | export different de l'ecoute (alerte) | depart |
| clics | parametre continu non lisse | depart |
| interface | plantage a la reouverture, taille de fenetre perdue | depart |
| temps reel | allocation ou liberation memoire dans `processBlock`, tous reglages en mouvement (alerte en chantier) | cette methode |
| transport | plantage ou NaN quand l'hote ne donne aucune position, est a l'arret, boucle, ou change de tempo (20 a 999) | cette methode |
| reprise | meme instance repreparee a 96 et 192 kHz puis a 44,1 | cette methode |
| charge | plus de 25 % d'un coeur (alerte) | depart |
| pluginval | ce qu'un vrai hote fait subir au plugin : automation, etats, bus, fils d'execution | depart |
| installeur Windows | installeur qui ne pose pas le plugin au bon endroit | depart |

Le controle "temps reel" compte les `malloc` et `free` pendant `processBlock`. Il tourne sous Linux. Il ne voit ni les verrous ni les appels systeme : voir RealtimeSanitizer en partie 6.

### 3.2 Avant une 1.0.0 : ce qu'aucune machine ne verifie

A cocher dans la PR, par le responsable et un autre membre :

- [ ] Ecoute sur enceintes et au casque, sur trois sources : batterie, basse, voix ou nappe. Rien de genant aux extremes de chaque reglage.
- [ ] Compare a la reference de la fiche, a niveau egal.
- [ ] Un projet complet joue 10 minutes dans Ableton Live et dans Bitwig : pas de decrochage, pas de derive par rapport au tempo, export identique a l'ecoute.
- [ ] Boucle, saut de position et changement de tempo pendant la lecture : le plugin reste cale.
- [ ] Automation de chaque reglage dessinee dans le DAW : pas de clic, pas d'escalier audible.
- [ ] Un projet enregistre avec la version precedente se rouvre avec le meme son.
- [ ] Fenetre a 100, 125 et 150 % d'echelle Windows, et redimensionnee : lisible, fluide.
- [ ] Essaye sur une deuxieme machine que celle du responsable.
- [ ] Presets d'usine : 8 au moins, nommes, qui montrent l'etendue du plugin.
- [ ] La fiche n'a plus aucun `SUPPOSE`.

## 4. Regles de code

Elles sont dans `CLAUDE.md` (section "Regles de code") et ne sont pas recopiees ici. Les trois qui comptent le plus :

1. Dans `processBlock`, rien qui puisse attendre : pas d'allocation, pas de verrou, pas de fichier, pas de reseau. Tout se reserve dans `prepareToPlay`.
2. Ce qu'un projet enregistre ne casse jamais : identifiants de parametres, `code` et `cible` sont figes a la premiere sortie, l'etat est versionne et les anciens formats sont convertis.
3. L'export sonne comme l'ecoute : rendu deterministe, independant de la taille de bloc, latence annoncee.

## 5. Choix techniques et sources

### 5.1 Framework : JUCE 9, on reste

- Retenu : JUCE 9 (9.0.6 epingle), C++20, CMake. C'est la voie la plus eprouvee, celle qui sort tous les formats, et celle pour laquelle un assistant de code a le plus d'exemples publics. Contenu de la version 9 : https://forum.juce.com/t/juce-9-is-available-now/69175
- Ecarte, iPlug2 : licence plus souple et CLAP natif, mais petite equipe, pas de versions publiees, et son auteur a lance un successeur. https://github.com/iPlug2/iPlug2
- Ecarte, Rust (nih-plug) : annonce en maintenance, ni AU ni AAX, beaucoup moins d'exemples. https://github.com/robbert-vdh/nih-plug
- Ecarte, CLAP d'abord avec clap-wrapper : propre, mais interface, parametres et etat sont a assembler soi-meme. https://github.com/free-audio/clap-wrapper
- Ecarte pour l'instant, iPlug3, YUP, RAVET : pas encore de version stable. A regarder de nouveau dans un an.
- En appoint, Faust et gen~ / RNBO (Max 9) : bons pour prototyper un traitement, qu'on recrit ou qu'on integre ensuite dans le plugin JUCE. Pas une chaine complete. https://faustdoc.grame.fr/manual/faq

### 5.2 Formats : VST3 seul, CLAP ensuite

- Retenu : VST3, le seul format commun a Ableton Live et Bitwig sur Windows et Mac.
- Le SDK VST3 est sous licence MIT depuis sa version 3.8 : ni contrat ni frais, il ne pese plus sur notre choix de licence. https://steinbergmedia.github.io/vst3_dev_portal/pages/VST+3+Licensing/VST3+License
- CLAP : toujours pas dans JUCE, repousse a une refonte ulterieure. La voie pratique est clap-juce-extensions (MIT), qui annonce JUCE 6 a 8 ; Pamplejuce l'emploie avec JUCE 9. A essayer sur le modele avant de le promettre. https://github.com/free-audio/clap-juce-extensions

### 5.3 Validation : pluginval, niveau 5 puis 10

- Retenu : pluginval 1.0.4. Son auteur donne le niveau 5 comme minimum de compatibilite avec les hotes ; le gabarit de reference Pamplejuce le lance au niveau 10. D'ou nos deux paliers. https://github.com/Tracktion/pluginval
- Constate ici le 27/11/2026 : le modele et Test v0 passent deja le niveau 10.
- A suivre : pluginval 2.0.0 est en beta, avec des rapports lisibles par une machine. On y passera quand il sera stable. https://github.com/Tracktion/pluginval/releases

### 5.4 Temps reel

- La regle vient de l'article de Ross Bencina : le fil audio ne doit rien faire dont la duree n'est pas bornee, donc ni allocation, ni verrou, ni entree-sortie. http://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing
- Retenu aujourd'hui : notre sonde d'allocation dans les tests communs. Elle ne demande aucun outil en plus et tourne a chaque PR.
- Retenu pour la suite : RealtimeSanitizer de clang. Une fonction marquee non bloquante est surveillee a l'execution, et toute allocation, tout verrou ou appel systeme y est signale. Il demande un clang recent, a installer sur la machine de compilation. https://clang.llvm.org/docs/RealtimeSanitizer.html

### 5.5 Interface

- Retenu : interface JUCE native avec `droz::ui`. Meme rendu partout, testable sans DAW, capture automatique.
- Ecarte pour l'instant, interface web (WebView de JUCE) : rechargement a chaud et beaucoup d'exemples, mais un affichage casse a ete signale dans Ableton Live 12.4.1 a 125 % d'echelle, et le rendu depend du navigateur du systeme. https://forum.juce.com/t/juce-webviewplugindemo-not-showing-properly-in-ableton-live/68963
- A suivre : Visage (auteur de Vital, MIT), rendu par la carte graphique avec flou et halo en temps reel. Pas d'integration JUCE documentee. https://github.com/VitalAudio/visage
- A mesurer : le rendu Direct2D de JUCE sous Windows a connu des lenteurs. Tout visuel anime se chronometre sur une machine modeste avant une 1.0.0. https://forum.juce.com/t/direct2d-performance-update/65008
- Utile : melatonin_blur (MIT) pour ombres et flous avec cache, melatonin_inspector (MIT) pour inspecter une interface en Debug. https://github.com/sudara/melatonin_blur

### 5.6 Chaine de reference

- Pamplejuce (MIT) est le gabarit le plus suivi : JUCE 9, tests Catch2 et mesures de performance, pluginval niveau 10, signature Windows et notarisation Mac, installeurs. On ne le copie pas, notre chaine fait deja l'essentiel ; on s'y compare a chaque evolution. https://github.com/sudara/pamplejuce
- Ce qu'il a et qu'on n'a pas : signature, notarisation, mesures de performance en integration continue, format CLAP.

### 5.7 Licence (a trancher ensemble)

- Le depot est public. JUCE est gratuit sous AGPLv3 (code ouvert), ou sous son palier Starter (code ferme, revenus annuels sous 20 000 $). Au-dela : 40 $ par mois et par poste. https://juce.com/get-juce/
- AGPLv3 est le choix coherent avec un depot public, et il autorise les modules GPLv3 (une partie de chowdsp_utils). Starter les interdit. https://github.com/Chowdhury-DSP/chowdsp_utils
- Tant que ce n'est pas tranche : aucun module GPLv3 dans le depot.

### 5.8 Signature (a chiffrer, pas commencee)

- Windows : Azure Artifact Signing, a partir de 9,99 $ par mois. En Europe il n'accepte que les organisations. Une association loi 1901 est une personne morale, mais le cas n'est documente nulle part : a tenter au nom de Droz RecordZ, avec SIREN, site et adresse mail sur son domaine. https://learn.microsoft.com/en-us/azure/artifact-signing/quickstart
- Windows, sinon : SignPath Foundation, gratuit pour un projet a licence libre (l'editeur affiche est SignPath Foundation), ou Certum Open Source, 49 EUR au nom d'une personne. https://signpath.org/terms et https://shop.certum.eu/open-source-code-signing-on-simplysign.html
- Mac : compte developpeur Apple, 99 USD par an, avec un numero D-U-N-S pour une organisation. Une exemption existe pour certains organismes sans but lucratif, sans garantie pour une association simplement declaree. https://developer.apple.com/help/account/membership/program-enrollment/

### 5.9 Travailler avec un assistant de code

- Ce que les projets de reference ecrivent dans leurs regles : lire avant de modifier, faire valider le plan avant un gros changement, changements minimaux, ne rien affirmer sur du code non lu. https://raw.githubusercontent.com/Tracktion/pluginval/develop/CLAUDE.md
- Pieges rapportes par ceux qui l'ont fait : architecture qui part en spaghetti, traitement du signal naif qui "marche" mais sonne mal, premier diagnostic de l'assistant faux. La parade commune : des tests ecrits, et la comparaison a une reference sur de vrais fichiers. https://forum.juce.com/t/how-can-i-best-use-ai-to-co-develop-my-plugin-with-juce/67558
- Chez nous : la fiche avant le code, le Verdict, et l'ecoute des rendus WAV par un humain.

## 6. Ce qui reste a brancher

Par ordre de rendement. Chaque point est une PR `commun:`.

1. RealtimeSanitizer dans le Verdict (verrous et appels systeme, que la sonde ne voit pas).
2. Rendus de reference : un WAV de sortie garde par plugin et par boucle, compare a chaque PR. Un son qui change sans qu'on l'ait voulu fait rougir la PR.
3. Mesure de repliement pour tout plugin non lineaire (balayage sinus, energie sous le fondamental).
4. Charge mesuree et comparee d'une version a l'autre, plutot qu'un seuil fixe.
5. Validateur VST3 de Steinberg en plus de pluginval.
6. Format CLAP sur le modele, valide par clap-validator.
7. Signature Windows, puis notarisation Mac.
8. Presets : format commun et presets d'usine embarques, dans `libs/`.

## 7. Faire evoluer la methode

- Une lecon : une ligne dans `docs/PIEGES.md`, tout de suite.
- Une regle ou un controle : PR `commun:`, relue par un autre membre.
- Un choix de la partie 5 se rouvre avec une source datee, pas avec une impression.
