# IDGems (plugin)

## But
Rack d'effets IDGems, migre depuis le depot IDGems-Plugin (branche main) par YoY.

## Etat (session du 27/11/2026)
- Sources copiees depuis IDGems-Plugin/Source dans `src/`, ressources dans `src/Resources/`.
- Pas encore compile : les sources sont ecrites pour JUCE 8 et CMake propre. A adapter a JUCE 9 et a `cmake/DrozPlugin.cmake`.
- Tests de stabilite deplaces dans `tests/` (pas dans `src/`, sinon CMake les compile dans le plugin).
- Gate : `Effects/Gate.h`. Ses briques pures sont dans `libs/droz_dsp` (porte.h, filtres.h, tampon.h, rng.h).

## Correctif du 2026-11-27 (Fx, a la demande de Fx, propose a YoY par PR)

Pour que les tests communs passent et que le plugin soit installable a l'essai. Aucun reglage ajoute ni retire, aucun son change en position fixe.

- Gains de sortie lisses sur 20 ms dans `Engine/OutputChain.h` : `out_gain`, drive du clip, plafond, gain et profondeur de l'OTT. Avant : valeur lue une fois par bloc, donc un clic quand on bouge le reglage.
- Quantite de feedback global (`fb_amount`) lissee sur 20 ms dans `PluginProcessor.cpp`.
- Taille de la fenetre gardee au pixel pres dans l'etat du projet (`ui_w`, `ui_h`), en plus de l'echelle. Avant : seule l'echelle etait gardee, la hauteur revenait a 3 pixels pres.
- Rien a corriger pour l'etat des interrupteurs (`fx0_on`...) : c'etait le test commun qui se trompait, corrige dans `tools/outil/outil.cpp`.

## A faire
- Compiler et corriger les erreurs JUCE 9.
- Verifier les tests communs (ctest) et pluginval.
