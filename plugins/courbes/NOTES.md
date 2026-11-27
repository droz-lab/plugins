# Droz Courbes

Responsable : Zomban (Bitwig, doit aussi marcher dans Ableton ; Windows seulement sauf demande).

## But

Generateur de courbes synchronisees au tempo, rangees par familles (comme les familles de formes
de ShaperBox, mais avec notre propre look), envoyees en MIDI CC pour moduler n'importe quel parametre
de Bitwig ou d'un autre plugin. Un bouton "Generer" invente de nouvelles formes dans une famille.

## Etat

- v0.1.0 (2026-11-27) : premiere version.
  - Courbe editable : points, segments "courbe" (tension), "en S" et "marche" ; jusqu'a 64 points.
  - 8 familles x 8 formes : Rampes, Ondes, Pompes, Marches, Rebonds, Portes, Hasard, Enveloppes.
    "Generer" tire 3 variations + une graine au hasard dans la famille.
  - Sortie : un CC (numero 0-127, canal 1-16, defaut CC 20 canal 1), plage min/max, lissage.
    Reevalue tous les 32 echantillons, envoye seulement quand la valeur change.
  - Synchro : position du DAW quand il joue ; horloge interne au meme tempo quand il est arrete.
  - "Ecoute" : son test 220 Hz dont le volume suit la courbe (0 par defaut = muet).
  - Verifie : tests communs 9/9, pluginval niveau 5 (Linux), verification CC a part
    (calage tempo, plage, canal, transport arrete, etat).
  - Pas encore ecoute / teste dans un DAW.

- 2026-11-27 (suite) : retours de Zomban.
  - Slider "Longueur" (1/32 a 16 mesures) + bouton "Ternaire" pour inclure ou non les T.
  - Grille 1/4 a 1/32 + bouton "Ternaire" + "Aimant" (Maj = inverse).
  - Decalage, Minimum, Maximum, Lissage en sliders. "Ecoute" en On/Off.
  - 16 familles (dont Gates, Gates euclidiens, Gates nuances, Hasard lisse / brutal / nerveux,
    Derive, Harmoniques, Pulsations, Arpeges), 4 reglages par famille qui refont la forme en direct,
    "Relancer" (nouveau tirage) et "Generer" (tout au hasard).
  - Evolution : a chaque cycle (ou tous les N), tirage Garder / Revenir / Varier / Changer, avec
    Intensite et chances par famille. Tirage seme par la position : meme endroit = meme suite.
    Lecture relancee ailleurs : repart de la courbe de base.
  - Verifie : tests communs 9/9, pluginval 5, test a part (ternaire, evolution, etat).
  - Identite visuelle "Zomban" : rien trouve d'exploitable en ligne (SoundCloud djzomban inaccessible
    d'ici). En attente d'images / couleurs de Zomban.

- v0.2.0 (2026-11-27) : retours de Zomban.
  - 8 onglets (couleur + insecte dessine), chacun sa courbe, son CC (20 a 27 par defaut), Actif on/off.
    Parametres "o1_..." a "o8_..." (pas de reprise des reglages v0.1 : Zomban s'en fiche).
  - Affichage "Toutes les courbes" : onglets allumes superposes, points selectionnables sur plusieurs courbes.
  - Selection multiple facon Ableton : cadre, deplacer, poignees pour etirer, Suppr, Ctrl+A.
  - Annuler / Retablir (tous les onglets), Inverser, Retourner, x2, /2, Copier / Coller, Mes courbes
    (fichier mes-courbes.xml dans le dossier de donnees utilisateur), Swing, Depart Tempo / Note.
  - Evolution : prochaine courbe tiree a l'avance et affichee ; rythme "chaque iteration" ou
    "toutes les N mesures" (toutes les courbes ensemble) ; styles Stable / Evolutif / Chaotique ;
    vraies chances en % ; familles par groupes (Tout / Rien / seule) ; onglets lies (reglages communs,
    tirage commun par mesure + Coherence) ou propres ; "Nouvelle suite" / suite precedente.
  - Verifie : tests communs 9/9, pluginval 5, test a part (8 CC, ternaire, N mesures synchro,
    prochaine = jouee, depart Note, etat, transformations).

## A faire / idees

- Tester dans Bitwig et Ableton le routage du CC (voir "Utilisation" ci-dessous) et noter ce qui marche.
- CC 14 bits pour les parametres qui demandent de la finesse.
- Affichage multicourbe : les courbes de longueurs differentes sont dessinees chacune sur sa propre longueur (a revoir si genant).

## Utilisation (a confirmer en vrai)

- Bitwig : sur une piste instrument, ou en Note FX avant l'instrument vise. Le CC sort dans la chaine ;
  un modulateur "MIDI" (CC 20) sur l'appareil suivant le mappe sur n'importe quel parametre.
- Ableton : sur une piste MIDI. Une autre piste choisit "MIDI From : <piste> / Courbes" ;
  le plugin vise doit savoir apprendre un CC (MIDI learn). Mapper un parametre Ableton natif
  demande Max for Live.

## Decisions et lecons

- Parametres abandonnes (ne jamais reutiliser) : "vitesse", "ecoute", et les identifiants v0.1 sans prefixe d'onglet.
- Interrupteurs en AudioParameterChoice Non/Oui, pas AudioParameterBool (voir PIEGES).
- Evolution calculee dans le fil audio : generer() et trier() sont sans allocation (tri par insertion).

- 2026-11-27 : nom "Courbes" (plus "Droz Courbes"). Fichiers a telecharger au nom du plugin
  (Courbes-Windows), pas de zip dans un zip, Windows seulement sauf demande de Zomban.

- Type "instrument" (pas effet) : Ableton ne laisse router la sortie MIDI que des instruments.
- Sortie MIDI : JUCE la decide a la compilation (JucePlugin_ProducesMidiOutput). Ajout commun
  `"midi_sortie": true` dans plugin.json (PR `commun:` separee, branche zomban/commun-midi-sortie).
- Courbe partagee interface -> audio : copie fixe sous SpinLock, le fil audio fait un TryLock et garde
  l'ancienne si le verrou est pris (regle commune).
- Courbe gardee dans l'etat (enfant "COURBE", points en texte) ; un etat sans courbe garde la courbe courante.
