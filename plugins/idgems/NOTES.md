# IDGems (plugin)

## But
Rack d'effets IDGems, migre depuis le depot IDGems-Plugin (branche main) par YoY.

## Etat (session du 27/11/2026)
- Sources copiees depuis IDGems-Plugin/Source dans `src/`, ressources dans `src/Resources/`.
- Pas encore compile : les sources sont ecrites pour JUCE 8 et CMake propre. A adapter a JUCE 9 et a `cmake/DrozPlugin.cmake`.
- Tests de stabilite deplaces dans `tests/` (pas dans `src/`, sinon CMake les compile dans le plugin).
- Gate : `Effects/Gate.h`. Ses briques pures sont dans `libs/droz_dsp` (porte.h, filtres.h, tampon.h, rng.h).

## A faire
- Compiler et corriger les erreurs JUCE 9.
- Verifier les tests communs (ctest) et pluginval.
