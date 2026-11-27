#pragma once
#include "Theme.h"

namespace idg::ui
{
/** Logo vectoriel de chaque effet, dans un carré 0..1 (à tracer en contour). */
juce::Path effectIcon (int fx);

/** Dessine le logo d'un effet avec un léger décalage chromatique. */
void drawEffectIcon (juce::Graphics& g, int fx, juce::Rectangle<float> area, juce::Colour c, float thickness = 1.6f, bool glitch = true);
}
