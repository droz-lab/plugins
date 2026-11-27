// Description de tous les parametres : sert a creer les parametres ET a construire l'interface.
// Les identifiants ("eq3_gain"...) ne changent JAMAIS apres une sortie.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

namespace master
{
inline juce::String T (const char* utf8) { return juce::String::fromUTF8 (utf8); }

/** Les 8 modules, dans l'ordre de leurs numeros (stables : ils sont gardes dans l'etat). */
enum TypeModule { mEq, mEqDyn, mComp, mMulti, mReso, mExciter, mClipper, mLimiteur, nbModules };

enum class Genre { continu, liste, interrupteur };

struct DescParam
{
    juce::String id, nom, court;
    Genre genre = Genre::continu;
    float min = 0, max = 1, defaut = 0, centre = 0; // centre > 0 : echelle tordue (frequences, temps)
    juce::String unite;
    juce::StringArray choix;
    juce::String aide;
    float pas = 0.01f;
};

struct DescGroupe
{
    juce::String titre;
    std::vector<DescParam> params;
};

struct DescModule
{
    juce::String id, nom, court, aide;
    std::vector<DescGroupe> groupes;
};

const std::vector<DescModule>& modules();
const std::vector<DescParam>& globaux();

/** Parametres communs a chaque module (suffixes stables). */
inline juce::String idActif (int m) { return modules()[static_cast<size_t> (m)].id + "_actif"; }
inline juce::String idMode (int m) { return modules()[static_cast<size_t> (m)].id + "_mode"; }
inline juce::String idDoseA (int m) { return modules()[static_cast<size_t> (m)].id + "_dose_a"; }
inline juce::String idDoseB (int m) { return modules()[static_cast<size_t> (m)].id + "_dose_b"; }

/** Noms des deux parties selon le mode (stereo : une seule). */
juce::String nomPartie (int mode, bool b);

juce::AudioProcessorValueTreeState::ParameterLayout creeLayout();
} // namespace master
