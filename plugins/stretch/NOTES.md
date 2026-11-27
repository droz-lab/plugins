# Stretch (plugin)

## But
Gel de frame sonore a un rate, avec grain et texture. Migre depuis la branche `claude/focused-cori-y3tqih` du depot IDGems-Plugin.

## Etat (session du 27/11/2026)
- Encore en cours cote IDGems : retour de YoY sur les versions precedentes : « pas ouf ». Cahier des charges : vitesse toujours 0, mix toujours 100 %, filtre coupe-bas/coupe-haut.
- Sources copiees dans `src/` (processeur, editeur, StretchEngine.h). Outil de rendu dans `tools/StretchRender.cpp`.

## Session du 27/11/2026 (suite) : premiere compilation verte
- Compile sous JUCE 9 (Linux local), tests communs OK, pluginval niveau 5 OK.
- Grain bouge pendant une boucle : avant, la fenetre gardait sa position en proportion, et un grand saut de grain faisait balayer la tete de lecture a toute vitesse (claquement). Maintenant : fondu enchaine de 15 ms entre l'ancienne et la nouvelle longueur, a la meme position de lecture quand c'est possible (invisible pour une petite automation).
- Filtres coupe-bas / coupe-haut / reso : lissage ~25 ms (avant ~2 ms, un saut 20 Hz -> 20 kHz claquait), coefficients recalcules tous les 4 echantillons.
- Taille de fenetre : largeur et hauteur exactes gardees dans l'etat (`uiW`/`uiH`).
- Boucle d'essai `boucles/batterie-120.wav` : synthese maison, 4 s, 120 BPM.

## A faire
- Ecouter une automation du Grain : le fondu de 15 ms convient-il ?
- Appliquer le cahier des charges ci-dessus. Compilation Mac avant la sortie.
