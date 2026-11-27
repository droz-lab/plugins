# IDGems (plugin)

## But
Rack d'effets IDGems, migre depuis le depot IDGems-Plugin (branche main) par YoY.

## Etat (session du 27/11/2026)
- Sources copiees depuis IDGems-Plugin/Source dans `src/`, ressources dans `src/Resources/`.
- Adapte a JUCE 9 et a `cmake/DrozPlugin.cmake` (voir plus bas).
- Tests de stabilite deplaces dans `tests/` (pas dans `src/`, sinon CMake les compile dans le plugin).
- Gate : `Effects/Gate.h`. Ses briques pures sont dans `libs/droz_dsp` (porte.h, filtres.h, tampon.h, rng.h).

## Session du 27/11/2026 (suite) : premiere compilation verte
- Compile sous JUCE 9 (Linux local), tests communs OK, pluginval niveau 5 OK.
- Interrupteurs : classe `SnappedBool` (ParamDefs.cpp) qui arrondit a 0/1. `AudioParameterBool` de JUCE gardait 0.37 tel quel mais rechargeait 0 : le test "etat" echouait sur `fx0_on`. Les identifiants ne changent pas.
- Taille de fenetre : largeur et hauteur exactes gardees dans l'etat (`uiw`/`uih`), l'ancien `uiscale` sert encore si elles manquent.
- Chaine de sortie : gain, drive, plafond, gain et dosage OTT lisses (~10 ms, etat en double).
- Feedback global : bug corrige. Il relisait le meme echantillon pendant tout un bloc (son en escalier). Lecture a (retard - i) ; un retard plus court qu'un bloc est porte a la taille du bloc. Quantite et temps lisses (~20 ms).
- Boucle d'essai `boucles/batterie-120.wav` : synthese maison, 4 s, 120 BPM.

## A faire
- Ecouter le feedback global apres la correction (le son a change, il devrait etre plus propre).
- Tester dans un DAW (Bitwig, Reaper), compilation Mac a lancer avant la sortie.
