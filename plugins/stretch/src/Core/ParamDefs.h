#pragma once
#include "JuceIncludes.h"

namespace idg
{
enum Family { FamGlitch, FamDestruction, FamPitch, FamFilters, FamSpace, FamMovement, NumFamilies };

enum FxId
{
    FxStutter, FxBeatRepeat, FxReverse, FxTapeStop, FxShuffle, FxFreeze, FxScratch,
    FxBitcrush, FxDistortion, FxRingMod, FxFreqShift, FxCorruption,
    FxPitchShift, FxFormant,
    FxFilter, FxComb, FxVocoder, FxPixelSort,
    FxDelay, FxReverb, FxChorus, FxFlanger, FxPhaser,
    FxGate, FxTremolo, FxAutoPan, FxStereo,
    NumFx
};

enum class PType { Float, Bool, Choice };
enum class PitchKind { None, Semis, Hz, Midi };

struct ParamDef
{
    juce::String id, name, unit, help;
    PType type = PType::Float;
    float min = 0.0f, max = 1.0f, def = 0.0f, skew = 1.0f;
    bool symSkew = false;
    juce::StringArray choices;
    int effect = -1;          // -1 = global
    int family = -1;
    int localIndex = -1;      // index du paramètre dans son effet (hors communs)
    bool randomizable = false;
    float rmin = 0.0f, rmax = 1.0f; // plage de random par défaut (normalisée)
    PitchKind pitch = PitchKind::None;
    bool common = false;      // on / mix / mode / par

    juce::NormalisableRange<float> range() const;
    juce::String toText (float value) const;
};

struct EffectDef
{
    juce::String key, name, shortName, help;
    Family family;
    std::vector<ParamDef> params; // paramètres spécifiques
    float defMix = 1.0f;
    bool triggerable = false;     // réagit aux déclenchements (séquenceur, MIDI, transitoires)
    bool stft = false;            // utilise une FFT (latence en mode normal)
    bool noZeroLatency = false;   // indisponible en mode sans latence
};

const std::vector<EffectDef>& effectDefs();
const juce::StringArray& familyNames();
juce::Colour familyColour (int family);

// Divisions rythmiques
const juce::StringArray& divisionNames();
double divisionBeats (int index);
int divisionIndex (const juce::String& name);

const juce::StringArray& seqRateNames();
double seqRateBeats (int index);

// Identifiants des paramètres globaux
namespace G
{
    inline const juce::String macro (int i) { return "macro" + juce::String (i + 1); }
    inline const juce::String famMix (int f) { return "fam" + juce::String (f) + "_mix"; }
    static const char* const chaos      = "chaos";
    static const char* const globalMix  = "global_mix";
    static const char* const inGain     = "in_gain";
    static const char* const outGain    = "out_gain";
    static const char* const morphX     = "morph_x";
    static const char* const morphY     = "morph_y";
    static const char* const fbAmount   = "fb_amount";
    static const char* const fbTime     = "fb_time";
    static const char* const ceiling    = "lim_ceiling";
    static const char* const limRelease = "lim_release";
    static const char* const hpFreq     = "hp_freq";
    static const char* const monoOn     = "mono_on";
    static const char* const monoFreq   = "mono_freq";
    static const char* const ottDepth   = "ott_depth";
    static const char* const ottTime    = "ott_time";
    static const char* const ottUp      = "ott_up";
    static const char* const ottDown    = "ott_down";
    static const char* const ottGain    = "ott_gain";
    static const char* const clipDrive  = "clip_drive";
    static const char* const zeroLat    = "zero_latency";
    static const char* const destOS     = "dest_os";
    static const char* const transOn    = "trans_on";
    static const char* const transSens  = "trans_sens";
    static const char* const transSrc   = "trans_src";
    static const char* const capture    = "capture";
    static const char* const captureLen = "capture_len";
    static const char* const scaleOn    = "scale_on";
    static const char* const scaleKey   = "scale_key";
    static const char* const scaleType  = "scale_type";
    static const char* const pitchSnap  = "pitch_snap";
    static const char* const envAtt     = "env_att";
    static const char* const envRel     = "env_rel";
    static const char* const envGain    = "env_gain";
    static const char* const envSrc     = "env_src";
    static const char* const lfoTrig    = "lfo_trig";
}

constexpr int NumLfos = 10;
constexpr int NumRandMods = 5;
constexpr int NumMacros = 8;

inline juce::String fxParamId (int fx, const juce::String& key) { return "fx" + juce::String (fx) + "_" + key; }
inline juce::String lfoId (int i, const juce::String& key) { return "lfo" + juce::String (i + 1) + "_" + key; }
inline juce::String rndId (int i, const juce::String& key) { return "rnd" + juce::String (i + 1) + "_" + key; }

/** Registre de tous les paramètres (ordre = ordre d'exposition à l'hôte). */
struct Registry
{
    std::vector<ParamDef> all;
    std::map<juce::String, int> index;
    int fxOn[NumFx], fxMix[NumFx];
    int fxFirst[NumFx], fxCount[NumFx];

    int find (const juce::String& id) const
    {
        auto it = index.find (id);
        return it == index.end() ? -1 : it->second;
    }
};

const Registry& registry();
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

const juce::StringArray& modeNames();
} // namespace idg
