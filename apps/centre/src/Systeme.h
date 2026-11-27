// Ce qui depend du systeme : lancer un installeur (avec demande de droits sous Windows), savoir si un
// fichier est tenu par un autre programme, dossier VST3 par defaut. Tout le reste de l'appli est portable.
#pragma once

#include <juce_core/juce_core.h>

namespace centre::systeme
{
/** Dossier VST3 propose au premier lancement : celui retenu par les installeurs Droz, sinon celui du systeme. */
juce::File dossierVst3ParDefaut();

/** Lance un programme et attend sa fin. Sous Windows : passe par l'interpreteur de commandes du systeme,
    qui affiche la demande de droits si le programme en a besoin. Renvoie son code de sortie, -1 s'il n'a pas
    pu etre lance (raison dans `erreur`). */
int lancerEtAttendre (const juce::File& programme, const juce::StringArray& arguments, bool enAdmin, juce::String& erreur);

/** Programmes qui tiennent ce fichier (un DAW qui a charge le plugin). Vide : le fichier est libre. */
juce::StringArray quiUtilise (const juce::File& fichier);

/** Pour l'essai automatique : charge le fichier comme le ferait un DAW, jusqu'a relacher(). */
void* tenir (const juce::File& fichier);
void relacher (void* prise);
} // namespace centre::systeme
