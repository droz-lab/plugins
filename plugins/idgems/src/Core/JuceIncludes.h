#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <atomic>
#include <array>
#include <vector>
#include <cmath>

/** Littéral UTF-8 (textes en français avec accents). */
#define IDG_T(x) juce::String (juce::CharPointer_UTF8 (x))
