# Regles pour tout Claude qui travaille dans droz-lab

Depot commun a plusieurs personnes et plusieurs Claude. Ces regles gardent le depot coherent. Reponds en francais, court, sans jargon inutile : certains membres ne codent pas.

## Debut de session

1. `python tools/droz.py bilan` : ce qui est pret, ce qui manque. Regle toi-meme ce qu'il liste sous "A REGLER" (branche en retard sur `main`, fiche absente), dans cet ordre, avant tout autre travail. Si la commande n'existe pas, ta branche est ancienne : `git fetch origin main && git merge origin/main`, puis relis ce fichier.
2. Lis `docs/METHODE.md`, `docs/PIEGES.md`, `docs/HABILLAGE.md`, `carnet/<prenom>/PROFIL.md`, puis le `FICHE.md` et le `NOTES.md` du plugin concerne.
3. Ce que le bilan liste sous "A DEMANDER" : pose-le a la personne en un seul message, a choix quand c'est possible, et range les reponses (profil dans `carnet/<prenom>/PROFIL.md`, le reste dans la fiche ou `NOTES.md`) pour ne jamais les redemander.
4. Il te manque autre chose pour bien faire (une reference d'ecoute, une decision, un acces, un fichier, un outil) : dis-le et demande-le tout de suite. Ne devine pas en silence, ne contourne pas. Ne touche pas au plugin d'un autre sans qu'il l'ait demande.

## Avant de coder : la fiche

Meme si la demande tient en une phrase, aucun code avant que `plugins/<dossier>/FICHE.md` soit remplie. Protocole complet : `docs/METHODE.md`. En bref :

1. Reformule l'idee en une phrase, fais-la confirmer.
2. Remplis la fiche toi-meme avec ce qui a ete dit. Ce que tu ne sais pas, tu le supposes et tu l'ecris `SUPPOSE : ...` dans "Questions ouvertes".
3. Pose au plus trois questions a choix, seulement sur ce qui change le son ou les reglages en facade. Sans reponse, garde tes suppositions et avance.
4. Resume la fiche en cinq lignes a la personne, puis code. Le son d'abord, l'interface ensuite.
5. Le plugin change : la fiche change dans la meme PR. Le Verdict refuse une fiche absente, incomplete, ou dont les reglages n'existent pas dans le plugin.

Plugin existant : relis sa fiche avant de toucher au code. Pas de fiche : ecris-la d'abord.

## Creer un plugin

`python tools/droz.py nouveau <dossier> --nom "<Nom>" --type effet|instrument --auteur <prenom> --description "<le son voulu>"`

Jamais de copie a la main : le script attribue un code unique et verifie le registre. Le plugin part du modele (un gain) : remplace le traitement dans `src/`, garde la structure.

Instrument : le modele est un effet. Premiere chose a faire dans `PluginProcessor` : retirer l'entree audio des bus, `acceptsMidi()` a `true`, et produire le son a partir des notes.

## Perimetre

- Tu modifies `plugins/<dossier>/` du plugin en cours, rien d'autre.
- Besoin de changer du commun (`libs/`, `cmake/`, `tools/`, `.github/`, `CMakeLists.txt`, ce fichier) : PR separee, titre commencant par `commun:`, et dis-le clairement a la personne. Elle doit etre relue par un autre membre.
- Une brique DSP utile a plusieurs plugins va dans `libs/droz_dsp/` avec son test, pas en copie dans chaque plugin.

## Branches et PR

- Jamais de push sur `main`. Branche `<prenom>/<dossier>-<sujet>`, PR vers `main`.
- La PR est prete quand le controle **Verdict** est vert. S'il est rouge : lis le journal du job en echec et corrige toi-meme, y compris sur Mac ou Windows. Ne demande de l'aide qu'apres deux essais.
- Ne desactive jamais un test pour faire passer une PR. Si un test commun est faux pour ton plugin, propose sa correction dans une PR `commun:`.

## Avant de dire "c'est fait"

1. Les tests communs passent (`ctest`, ou le run Actions) : sortie finie, retour au silence, etat sauvegarde et recharge, pas de clic, interface qui s'ouvre et se rouvre, aucune allocation dans `processBlock`, transport (arret, boucle, sauts de tempo), reprise a une autre frequence.
2. pluginval passe : niveau 5 en chantier (version 0.x), niveau 10 au palier pro (1.0.0 et plus), ou toute alerte des tests communs bloque aussi. Passer en 1.0.0 se decide avec la liste de `docs/METHODE.md`.
3. Tu as produit une capture de l'interface (regarde-la toi-meme, compare-la a `docs/habillage.png`, puis envoie-la : la personne est peut-etre sur son telephone) et au moins un rendu WAV sec + traite, et tu dis quoi ecouter et ou le telecharger.
4. `plugins/<dossier>/FICHE.md` decrit le plugin tel qu'il est, `NOTES.md` est a jour. Publier = monter `version` dans `plugin.json`, relancer `python tools/droz.py vitrine` (tableau du README) et fusionner : la sortie part toute seule, rien d'autre a faire ni a proposer.

## Regles de code

- `processBlock` : aucune allocation, aucun verrou, aucun acces fichier ou reseau, aucun `juce::String` construit. `juce::ScopedNoDenormals` en tete.
- Tout parametre continu est lisse (`droz::Smoother` ou equivalent). Le test "clics" le verifie.
- Identifiants de parametres : stables a vie apres une sortie. Nouveau parametre = nouvel identifiant, jamais de reutilisation.
- Etat : champ `version_etat`, lecture tolerante (un etat illisible ne fait pas planter), anciens formats convertis.
- Latence : annoncee avec `setLatencySamples`, a jour quand elle change.
- Taille de fenetre gardee dans l'etat. Texte accentue via `juce::String::fromUTF8`.
- Rendu deterministe : meme entree, meme sortie (aleatoire seme), pour que l'export sonne comme l'ecoute.
- `plugin.json` : `code` et `cible` ne changent plus apres la premiere sortie.
- Donnees partagees interface vers audio (courbes, tables) : copie de taille fixe sous `juce::SpinLock`, le fil audio fait seulement `ScopedTryLockType` et garde l'ancienne version si le verrou est pris. Valeurs simples : `std::atomic`.
- Si le plugin peut monter en niveau (distorsion, feedback, resonance) : limiteur de securite en sortie, et aucune valeur non finie ne sort.
- Interface : info-bulle sur chaque controle, double-clic = valeur par defaut, polices et images libres de droits (SIL OFL) via `juce_add_binary_data`.
- Une fonction a la fois, finie, plutot que plusieurs a moitie.
- Habillage Droz obligatoire, pour tout le monde : tout plugin, nouveau ou mis a jour, suit `docs/HABILLAGE.md` et se monte avec `droz::ui` (`libs/droz_ui`). Aucune couleur, police ou controle dessine hors de `droz::ui`. Si la personne demande autre chose, propose-le comme evolution de l'habillage commun (PR `commun:`), pas comme exception dans son plugin. Quand tu modifies un plugin pas encore a la charte, tu le mets a la charte dans la meme PR.
- Mac : pas compile sur les PR. Si le plugin touche a du code propre a un systeme, lancer Actions > Compilation > Run workflow avec la case "mac" avant de fusionner.

## Memoire

- `plugins/<dossier>/FICHE.md` : ce que le plugin doit faire (intention, signal, reglages, cas limites). Ecrite avant le code, elle fait foi.
- `plugins/<dossier>/NOTES.md` : memoire du plugin (etat, decisions, ce qui a ete essaye et abandonne). A mettre a jour a chaque session.
- `docs/METHODE.md` : le chemin d'un plugin, les deux paliers, les choix techniques et leurs sources.
- `docs/PIEGES.md` : lecons utiles a tous. Ajout libre, une entree courte, datee, avec le prenom.
- `carnet/<prenom>/` : reflexions et idees de chacun, un fichier par sujet. Chacun n'ecrit que dans son dossier. `PROFIL.md` y garde le contexte de la personne (niveau en code, DAW, systeme, ecoute, references). Le depot est public : rien de personnel.
- Ce fichier : regles stables. Il ne grossit pas. Modification par PR `commun:` uniquement.

## Securite

- Aucun secret dans le depot ni dans une conversation (jeton, mot de passe, cle). Si quelqu'un en colle un, dis-le.
- Code tiers : fige a une version ou un commit precis, licence notee dans la PR.
- Pas de sample sous droits. Dans `boucles/` : sons faits par nous, courts (moins de 10 s), mono ou stereo.

## Fin de session

Trois lignes pour la personne : ce qui a change, le lien du run ou de la PR, ce qu'il faut ecouter ou decider.
