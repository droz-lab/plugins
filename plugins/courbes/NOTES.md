# Courbes

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

## A faire / idees

- Tester dans Bitwig et Ableton le routage du CC (voir "Utilisation" ci-dessous) et noter ce qui marche.
- Plusieurs sorties (2 a 4 courbes, un CC chacune) : une "famille" jouee en meme temps.
- Declenchement par note MIDI (la courbe repart a chaque note) en plus du tempo.
- CC 14 bits pour les parametres qui demandent de la finesse.
- Sauvegarder ses propres formes dans une famille "Mes courbes".

## Utilisation (a confirmer en vrai)

- Bitwig : sur une piste instrument, ou en Note FX avant l'instrument vise. Le CC sort dans la chaine ;
  un modulateur "MIDI" (CC 20) sur l'appareil suivant le mappe sur n'importe quel parametre.
- Ableton : sur une piste MIDI. Une autre piste choisit "MIDI From : <piste> / Courbes" ;
  le plugin vise doit savoir apprendre un CC (MIDI learn). Mapper un parametre Ableton natif
  demande Max for Live.

## Decisions et lecons

- 2026-11-27 : nom "Courbes" (plus "Droz Courbes"). Fichiers a telecharger au nom du plugin
  (Courbes-Windows), pas de zip dans un zip, Windows seulement sauf demande de Zomban.

- Type "instrument" (pas effet) : Ableton ne laisse router la sortie MIDI que des instruments.
- Sortie MIDI : JUCE la decide a la compilation (JucePlugin_ProducesMidiOutput). Ajout commun
  `"midi_sortie": true` dans plugin.json (PR `commun:` separee, branche zomban/commun-midi-sortie).
- Courbe partagee interface -> audio : copie fixe sous SpinLock, le fil audio fait un TryLock et garde
  l'ancienne si le verrou est pris (regle commune).
- Courbe gardee dans l'etat (enfant "COURBE", points en texte) ; un etat sans courbe garde la courbe courante.
