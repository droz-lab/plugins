#pragma once

#include <juce_core/juce_core.h>

namespace centre
{
/** Mode --essai : toute la chaine sans fenetre. Renvoie 0 si tout est conforme. */
int lancerEssai (const juce::StringArray& arguments);
} // namespace centre
