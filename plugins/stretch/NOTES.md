# Stretch (plugin)

## But
Gel de frame sonore a un rate, avec grain et texture. Migre depuis la branche `claude/focused-cori-y3tqih` du depot IDGems-Plugin.

## Etat (session du 27/11/2026)
- Encore en cours cote IDGems : retour de YoY sur les versions precedentes : « pas ouf ». Cahier des charges : vitesse toujours 0, mix toujours 100 %, filtre coupe-bas/coupe-haut.
- Sources copiees dans `src/` (processeur, editeur, StretchEngine.h). Outil de rendu dans `tools/StretchRender.cpp`.
- Pas encore compile dans ce depot. A adapter a JUCE 9 et a `cmake/DrozPlugin.cmake`.

## Correctif du 2026-11-27 (Fx, a la demande de Fx, propose a YoY par PR)

Pour que les tests communs passent et que le plugin soit installable a l'essai.

- Grain : sa longueur ne bouge plus de plus de 2 echantillons par echantillon (`Effects/StretchEngine.h`). Un grand saut (1 Hz vers 500 Hz) glisse comme une bande au lieu de claquer ; il met jusqu'a une demi-seconde. Les petits mouvements gardent le lissage de 10 ms. A ECOUTER par YoY : c'est le seul changement audible, a ajuster ou a refuser si le saut net etait voulu.
- Coupe-bas et coupe-haut : entree et sortie en fondu de 5 ms au lieu d'une coupure nette a 20 Hz et 20 kHz (clic en bout de course). En position neutre le son reste intact.
- Taille de la fenetre gardee au pixel pres dans l'etat (`uiW`, `uiH`).

## A faire
- Compiler et corriger.
- Appliquer le cahier des charges ci-dessus.
