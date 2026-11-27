#include "EffectFactory.h"
#include "Stutter.h"
#include "BeatRepeat.h"
#include "Reverse.h"
#include "TapeStop.h"
#include "Shuffle.h"
#include "Freeze.h"
#include "Scratch.h"
#include "Bitcrush.h"
#include "Distortion.h"
#include "RingMod.h"
#include "FreqShift.h"
#include "Corruption.h"
#include "PitchShift.h"
#include "Formant.h"
#include "Filter.h"
#include "Comb.h"
#include "Vocoder.h"
#include "PixelSort.h"
#include "Delay.h"
#include "Reverb.h"
#include "Chorus.h"
#include "Flanger.h"
#include "Phaser.h"
#include "Gate.h"
#include "Tremolo.h"
#include "AutoPan.h"
#include "StereoMS.h"

namespace idg
{
std::unique_ptr<EffectModule> createEffect (int id)
{
    switch (id)
    {
        case FxStutter:    return std::make_unique<Stutter>();
        case FxBeatRepeat: return std::make_unique<BeatRepeat>();
        case FxReverse:    return std::make_unique<Reverse>();
        case FxTapeStop:   return std::make_unique<TapeStop>();
        case FxShuffle:    return std::make_unique<Shuffle>();
        case FxFreeze:     return std::make_unique<Freeze>();
        case FxScratch:    return std::make_unique<Scratch>();
        case FxBitcrush:   return std::make_unique<Bitcrush>();
        case FxDistortion: return std::make_unique<Distortion>();
        case FxRingMod:    return std::make_unique<RingMod>();
        case FxFreqShift:  return std::make_unique<FreqShift>();
        case FxCorruption: return std::make_unique<Corruption>();
        case FxPitchShift: return std::make_unique<PitchShift>();
        case FxFormant:    return std::make_unique<Formant>();
        case FxFilter:     return std::make_unique<Filter>();
        case FxComb:       return std::make_unique<Comb>();
        case FxVocoder:    return std::make_unique<Vocoder>();
        case FxPixelSort:  return std::make_unique<PixelSort>();
        case FxDelay:      return std::make_unique<Delay>();
        case FxReverb:     return std::make_unique<Reverb>();
        case FxChorus:     return std::make_unique<Chorus>();
        case FxFlanger:    return std::make_unique<Flanger>();
        case FxPhaser:     return std::make_unique<Phaser>();
        case FxGate:       return std::make_unique<Gate>();
        case FxTremolo:    return std::make_unique<Tremolo>();
        case FxAutoPan:    return std::make_unique<AutoPan>();
        case FxStereo:     return std::make_unique<StereoMS>();
        default: break;
    }
    jassertfalse;
    return nullptr;
}
} // namespace idg
