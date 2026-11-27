# Fiche : Droz Test v0

<!--
La fiche se remplit AVANT le code et fait foi : si le plugin change, la fiche change dans la meme PR.
Regles et exemples : docs/METHODE.md. Fiche ecrite apres coup pour ce plugin, qui existait avant la methode.
-->

## Intention

Plugin de test de l'atelier, pas un outil de production. Il sert a verifier que chaque membre arrive au bout
de la chaine : branche, PR, Verdict, installeur, ouverture dans le DAW. Le son n'est pas le sujet : un gain.

## References d'ecoute

- Utility d'Ableton Live : on prend le gain seul. On ne prend ni la largeur, ni la phase, ni le mono des basses.

## Signal

- Entree stereo ou mono -> gain lisse (20 ms) -> sortie, meme nombre de canaux.
- Pas de MIDI, pas de sidechain, pas de lien au tempo. Latence : 0 echantillon.

## Reglages en facade

| id | Nom | Plage | Defaut | Ce qu'on entend en le tournant |
|---|---|---|---|---|
| gain | Gain | -60 a +12 dB | 0 dB | le niveau monte ou descend, sans clic |

## Cas limites

- Silence en entree, puis reprise du son : sortie a zero exact, aucune queue.
- Niveau d'entree a 0 dBFS, reglages au maximum : +12 dB en sortie, pas de limiteur (un gain ne cree pas de niveau tout seul).
- Transport arrete, boucle, saut de position, changement de tempo : sans effet, le plugin ne lit pas le transport.
- Automation rapide d'un reglage : lissee sur 20 ms, pas de clic (test "clics").
- 44,1 a 192 kHz, blocs de 16 a 2048 echantillons, mono : meme comportement partout (tests "robustesse" et "mono").

## Qualite attendue

- Repliement : aucun, le traitement est lineaire.
- Charge : negligeable, bien sous 1 % d'un coeur.
- Latence : 0.
- Position neutre (0 dB) : le signal ressort identique.

## Comment on verifie

- Tests communs et pluginval au vert.
- Capture de l'interface : la ligne de chaque membre (`equipe/<prenom>.txt`) y apparait.
- Dans le DAW : le plugin s'ouvre, le gain agit, la taille de fenetre revient a la reouverture du projet.

## Hors perimetre

Tout traitement du son autre que le gain. Ce plugin ne passera pas au palier pro : il reste en 0.x.

## Questions ouvertes

Aucune. A faire a sa prochaine mise a jour : le passer a l'habillage Droz (`docs/HABILLAGE.md`).
