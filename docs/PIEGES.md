# Pieges et lecons

Une entree par lecon : date, prenom, le piege, la parade. Ajout libre par PR, sans relecture obligatoire.
"Verifie" = constate dans ce depot. "Rapporte" = vecu ailleurs par un membre, pas encore reproduit ici.

## Compilation

- 2026-11-27, Fx, verifie. JUCE 9 sous Linux demande aussi `libxi-dev`, `libxrender-dev` et `libxcomposite-dev`, absents des listes qu'on trouve partout. Sans eux : `X11/extensions/XInput2.h: No such file`. La liste complete est dans `compilation.yml`.
- 2026-11-27, Fx, verifie. JUCE 9.0.6 ne produit pas de CLAP : aucune trace dans son CMake. Pour Bitwig en CLAP il faudra clap-juce-extensions, compatibilite JUCE 9 a verifier.
- 2026-11-27, Fx, non verifie. Les jobs Windows et Mac de `compilation.yml` n'ont encore jamais tourne. Au premier echec : lire le journal du job, corriger, noter ici.
- 2026-11-27, YoY, rapporte. Chez lui la compilation Mac n'avait jamais tourne non plus, seule la Windows etait eprouvee.

- 2026-11-27, Fx. Ecarts voulus avec le setup de YoY : JUCE 9.0.6 au lieu de 8.0.9, VST3 seul (pas d'AU ni de Standalone, pour compiler plus vite : `-DDROZ_FORMATS="VST3;AU"` si besoin), pluginval fige en v1.0.4, Linux compile aussi, travail par PR.

## Son

- 2026-11-27, Fx, verifie. Un lissage un pole calcule en `float` se fige a environ 3e-5 de sa cible pres de 1.0 : le pas devient plus petit que la precision. Le gain n'arrive jamais a l'unite. `droz::Smoother` garde son etat en `double`, et son test le verifie.
- 2026-11-27, Fx, a retenir. Faire dialoguer plusieurs instances par une variable partagee ne marche que si le DAW les charge dans le meme processus. Bitwig en mode d'hebergement "Individually" (ou plugin coche dans les exceptions) et Reaper en processus separes les isolent. Dans Bitwig, "Together" (le defaut), "By manufacturer" ou "By plug-in" conviennent. Parade durable : memoire partagee nommee ou canal entre processus.
- 2026-11-27, Fx, a retenir. Freeze, Flatten ou export d'une seule piste : les autres pistes ne tournent pas. Un plugin qui depend d'une autre piste doit l'afficher clairement.

## Interface

- 2026-11-27, YoY rapporte, Fx verifie. La taille de fenetre doit etre gardee dans l'etat du plugin, sinon le DAW la perd a la reouverture. Piege dans le piege : `setResizeLimits` appele avant `setSize` ramene la fenetre au minimum et `resized()` ecrase la taille gardee. La premiere version du modele avait ce defaut, le test "interface" l'a attrape.
- 2026-11-27, Fx, verifie. JUCE 9 : `createEditor()` est prive vu de l'exterieur. Un hote passe par `createEditorAndMakeActive()` et appelle `editorBeingDeleted()` avant de detruire l'editeur.
- 2026-11-27, YoY, rapporte. Les accents cassent si la chaine passe brute dans `juce::String`. Parade : `juce::String::fromUTF8`, et `/utf-8` sous MSVC (deja dans le CMake racine).

- 2026-11-27, YoY, rapporte. Ne donner une taille a un composant qu'apres avoir cree tous ses enfants : sinon `resized()` touche des pointeurs nuls et plante.
- 2026-11-27, YoY, rapporte. Interface redimensionnable a ratio fixe : un composant contenu dessine a une taille de base, mis a l'echelle par `setTransform (AffineTransform::scale (s))`.
- 2026-11-27, YoY, rapporte. Ajouter `using AudioProcessor::processBlock;` dans le processeur, sinon avertissement "hidden". Ne pas redefinir `JUCE_STANDALONE_APPLICATION`.

## Distribution

- 2026-11-27, Fx, verifie. Un tiret long ou un caractere special dans un nom de fichier zippe casse la decompression sur certains systemes (`mapname: conversion failed` sous Linux). Noms de fichiers en ASCII, tirets simples.
- 2026-11-27, Fx, verifie. Les Artifacts d'Actions demandent un compte GitHub et expirent (30 jours ici). Pour donner un plugin a quelqu'un : une Release (etiquette `<dossier>-v<version>`).
- 2026-11-27, Fx, a retenir. Deux plugins avec le meme couple (code fabricant, code) sont confondus par le DAW. `tools/droz.py verifier` bloque les doublons. Un plugin deja sorti ailleurs garde ses codes d'origine via `fabricant` et `code_fabricant` dans son `plugin.json`.

## MIDI

- 2026-11-27, Zomban, verifie. JUCE decide a la compilation si un VST3 envoie du MIDI (`JucePlugin_ProducesMidiOutput`) : `producesMidi()` a `true` ne suffit pas. Mettre `"midi_sortie": true` dans le `plugin.json`. Les CC partent vers le DAW en `LegacyMIDICCOutEvent`.
- 2026-11-27, Zomban, a retenir. Ableton ne route la sortie MIDI que des instruments, pas des effets audio : un plugin qui envoie du MIDI doit etre de type `instrument`.
- 2026-11-27, Zomban, verifie. `AudioParameterBool` garde telle quelle une valeur intermediaire envoyee par l'hote (0.37) mais la sauve en 0 ou 1 : le test "etat" de l'outil commun echoue. Parade : un `AudioParameterChoice` { "Non", "Oui" }, qui fonctionne aussi avec `ButtonAttachment`.
