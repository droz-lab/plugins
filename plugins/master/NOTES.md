# Droz Master

Responsable : yoy

## But

Chaîne de mastering à modules, dans l'esprit d'Ozone : 8 modules qu'on allume, éteint et range dans l'ordre qu'on veut.
Chaque module travaille au choix en stéréo, mid-side, gauche-droite ou transitoire-tonal, avec une dose par partie.
Direction artistique « cosmique » : ciel étoilé, nébuleuse lavande (la seule couleur du plugin), la chaîne en orbite.

## Etat

- v0.1.0 (2026-11-27) : première version complète.
  - Modules : EQ 5 bandes, EQ dynamique 3 bandes, compresseur à 5 caractères (Propre, Punch, Opto, Vintage, Bus),
    compresseur multibande 4 bandes, suppresseur de résonances, exciter multibande 3 bandes (Lampe, Bande,
    Transistor, Douce), clipper x4, limiteur à anticipation avec crêtes vraies.
  - Ordre libre : on glisse les planètes de l'orbite. Ordre par défaut : EQ, DYN, RÉSO, COMP, MULTI, EXCIT, CLIP, LIMIT.
  - Modes par module : Stéréo, Mid-Side, Gauche-Droite, Transitoire-Tonal. Dose A / Dose B (0 % = partie intacte).
  - Tests communs verts, y compris avec tous les modules allumés en stéréo, mid-side et transitoire-tonal.
    pluginval niveau 5 vert (Linux).
  - Charge mesurée : ~10 % d'un cœur (tout allumé, stéréo), ~21 % en transitoire-tonal partout.

## Fonctionnement (pour s'y retrouver dans le code)

- `src/dsp/` : C++ pur sans JUCE, testable seul. `Base.h` (filtres TPT, répartiteur Linkwitz-Riley compensé,
  suréchantillonnage demi-bande), un fichier par module, `Chaine.h` (découpe par mode, dose, fondu d'allumage).
- `src/Parametres.cpp` : la description de tous les paramètres. Sert à la fois à créer les paramètres et à
  construire l'interface. Ajouter un réglage = l'ajouter ici + le lire dans `PluginProcessor::litReglages`.
- `src/ui/` : `Habillage.h` (couleur, fond cosmique), `Orbite.h` (la chaîne), `Spectre.h`, `Panneau.h`, `Controles.h`.
- L'ordre de la chaîne n'est pas un paramètre (pas automatisable) : il est gardé dans l'état (`ordre="eq,deq,..."`).

## Latence

Fixe, la même que les modules soient allumés ou non (sinon elle changerait en cours de lecture) :
résonances 2048 (à 44,1/48 kHz, 4096 à 96 kHz), exciter 64, clipper 72, limiteur 1,5 ms + 36.
Total à 48 kHz : 2291 échantillons (47,7 ms), affichée sous les vumètres.

## Décisions et leçons

- Pro-C 3 et Soothe 3 : **pas** de décompilation (plugins commerciaux, licences contre, code inutilisable chez nous).
  Tout est conçu ici à partir de principes publics : calcul de gain à genou, lissage dans le domaine du gain,
  détection crête/RMS, topologie en retour pour le caractère Vintage ; pour les résonances, analyse court-terme,
  comparaison de chaque bande à une enveloppe d'une octave, réduction lissée en fréquence (netteté) et en temps.
  Les produits restent des **références d'écoute** pour comparer nos rendus.
- Un seul exemplaire de chaque module (pas d'instances multiples comme dans Ozone) : garde ~140 paramètres
  stables et lisibles dans un DAW. À revoir si le besoin se présente.
- Changer l'ordre, un mode, une forme d'EQ ou un caractère : fondu de 4 ms, application, puis (si le routage
  change) remise à zéro des modules et attente que la chaîne se remplisse avant de remonter. Pas de clic,
  mais un court silence (~50 ms) : voulu.
- Allumer / éteindre un module : fondu de 15 ms, le module tourne toujours (pas de « réchauffement » au rallumage).
- Interrupteurs déclarés en liste à deux choix, pas en `AudioParameterBool` : voir `docs/PIEGES.md`.
- Limiteur : la détection x4 voyait mal le haut du spectre avec un filtre court (crête vraie +1 dB sur bruit blanc).
  Filtre allongé (129 coefficients) + marge de 0,3 dB : crête vraie mesurée à -0,97 dBFS pour un plafond à -1.
- Transitoire-tonal : x = w·x + (1-w)·x exactement, w vient d'une enveloppe rapide comparée à une lente.
  Reconstruction parfaite quand le module ne fait rien.
- Limiteur de sécurité en sortie (0 dBFS, attaque instantanée) et silence + remise à zéro si une valeur non finie sort.

## A faire ensuite (idées)

- Points d'EQ déplaçables à la souris sur le spectre.
- Écoute solo d'une bande (multibande, exciter) et d'une partie (mid seul, side seul, transitoires seuls).
- Mode faible latence (résonances en fenêtre plus courte) pour jouer en direct.
- Préréglages de départ (streaming -14 LUFS, club, doux).
- Mesure de loudness (LUFS court terme) à côté des vumètres.

## Rendus

`boucles/mix-synthe.wav` : boucle de 8 s synthétisée par nous (batterie, basse, nappe avec une résonance
volontaire à 3,2 kHz). L'Actions la passe dans le plugin à chaque compilation (artefact « apercu »).
