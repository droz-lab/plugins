# Stretch (plugin)

## But
Gel de frame sonore a un rate, avec grain et texture. Migre depuis la branche `claude/focused-cori-y3tqih` du depot IDGems-Plugin.

## Etat (session du 27/11/2026)
- Encore en cours cote IDGems : retour de YoY sur les versions precedentes : « pas ouf ». Cahier des charges : vitesse toujours 0, mix toujours 100 %, filtre coupe-bas/coupe-haut.
- Sources copiees dans `src/` (processeur, editeur, StretchEngine.h). Outil de rendu dans `tools/StretchRender.cpp`.
- Pas encore compile dans ce depot. A adapter a JUCE 9 et a `cmake/DrozPlugin.cmake`.

## A faire
- Compiler et corriger.
- Appliquer le cahier des charges ci-dessus.
