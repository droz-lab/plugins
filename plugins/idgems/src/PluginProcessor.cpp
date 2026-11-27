#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace idg;

namespace
{
constexpr int chunkSize = 32;
}

IDGemsProcessor::IDGemsProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    const auto& reg = registry();
    for (const auto& d : reg.all)
    {
        auto* p = apvts.getParameter (d.id);
        jassert (p != nullptr);
        params.push_back (p);
    }
    bank.init (apvts);
    rack.init (bank, rackShared);
    modEngine.init (bank, modShared);

    idxChaos = reg.find (G::chaos);
    idxGlobalMix = reg.find (G::globalMix);
    idxInGain = reg.find (G::inGain);
    idxOutGain = reg.find (G::outGain);
    idxFbAmount = reg.find (G::fbAmount);
    idxFbTime = reg.find (G::fbTime);
    idxMorphX = reg.find (G::morphX);
    idxMorphY = reg.find (G::morphY);
#define IDG_GI(n) gi.n = reg.find (G::n); jassert (gi.n >= 0);
    IDG_GI (capture) IDG_GI (captureLen) IDG_GI (transOn) IDG_GI (transSens) IDG_GI (transSrc) IDG_GI (envSrc) IDG_GI (lfoTrig)
    IDG_GI (scaleOn) IDG_GI (scaleKey) IDG_GI (scaleType) IDG_GI (pitchSnap) IDG_GI (destOS) IDG_GI (hpFreq) IDG_GI (monoOn)
    IDG_GI (monoFreq) IDG_GI (ottDepth) IDG_GI (ottTime) IDG_GI (ottUp) IDG_GI (ottDown) IDG_GI (ottGain) IDG_GI (clipDrive)
    IDG_GI (ceiling) IDG_GI (limRelease) IDG_GI (zeroLat)
#undef IDG_GI

    for (auto& c : ccToParam) c = -1;
    for (auto& m : midiHeld) m = false;
    for (int k = 0; k < 2; ++k) { meterIn[k] = 0.0f; meterOut[k] = 0.0f; }

    juce::PropertiesFile::Options opts;
    opts.applicationName = "IDGems";
    opts.filenameSuffix = "settings";
    opts.folderName = "IDGems";
    opts.osxLibrarySubFolder = "Application Support";
    appProps.setStorageParameters (opts);

    for (int i = 0; i < NumSnapshots; ++i)
    {
        float a = juce::MathConstants<float>::twoPi * (float) i / NumSnapshots;
        snapshots[i].x = 0.5f + 0.38f * std::cos (a);
        snapshots[i].y = 0.5f + 0.38f * std::sin (a);
    }

    presets = std::make_unique<PresetManager> (*this);
#if ! IDGEMS_TESTS
    startTimerHz (30);
#endif
}

IDGemsProcessor::~IDGemsProcessor() { stopTimer(); }

bool IDGemsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;
    const auto in = layouts.getMainInputChannelSet();
    if (in != juce::AudioChannelSet::stereo() && in != juce::AudioChannelSet::mono()) return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

void IDGemsProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = samplesPerBlock;
    rack.prepare (sr, chunkSize);
    rack.reset();
    modEngine.prepare (sr);
    output.prepare (sr);
    recorder.prepare (sr);
    for (auto& c : capBuf) c.assign ((size_t) (sr * 13.0), 0.0f);
    captureState_ = 0; capMix = 0.0f;
    fbBuf.allocate ((int) (sr * 2.2));
    fbHp.reset();
    dryDelay.prepare (Rack::rackLatency (false) + 16);
    dryDelay.reset();
    dryL.assign (chunkSize, 0.0f);
    dryR.assign (chunkSize, 0.0f);
    inGainSm.reset (sr, 0.02);
    globalMixSm.reset (sr, 0.03);
    inGainSm.setCurrentAndTargetValue (dsp::dbToGain (getValue (G::inGain)));
    globalMixSm.setCurrentAndTargetValue (getValue (G::globalMix));
    for (auto& s : seqRt) s.reset();
    trFast = trSlow = 0; trHold = 0;
    const bool zl = zeroLatency();
    lastZeroLat = zl;
    lastLatency = Rack::rackLatency (zl) + output.latency (zl);
    setLatencySamples (lastLatency);
}

void IDGemsProcessor::updateTransport (int)
{
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 999.0, *b);
            if (auto ts = pos->getTimeSignature())
                beatsPerBar = juce::jmax (1, juce::roundToInt ((double) ts->numerator * 4.0 / (double) juce::jmax (1, ts->denominator)));
            playing = pos->getIsPlaying();
            if (playing)
                if (auto p = pos->getPpqPosition()) ppq = *p;
        }
    }
    uiBpm = bpm;
}

void IDGemsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int mainIns = getBus (true, 0) != nullptr ? getBus (true, 0)->getNumberOfChannels() : 2;
    if (buffer.getNumChannels() < 2 || numSamples == 0) return;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    if (mainIns == 1) std::memcpy (R, L, sizeof (float) * (size_t) numSamples);

    const float* scL = nullptr;
    const float* scR = nullptr;
    bool hasSc = false;
    if (auto* scBus = getBus (true, 1))
    {
        if (scBus->isEnabled() && scBus->getNumberOfChannels() > 0)
        {
            auto sc = getBusBuffer (buffer, true, 1);
            if (sc.getNumChannels() > 0)
            {
                scL = sc.getReadPointer (0);
                scR = sc.getNumChannels() > 1 ? sc.getReadPointer (1) : scL;
                hasSc = true;
            }
        }
    }

    // ---- MIDI : déclenchements, LFO, CC
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            midiLfoTrigger = true;
            const int fx = msg.getNoteNumber() - 36;
            if (fx >= 0 && fx < NumFx)
            {
                midiHeld[fx] = true;
                rackShared.momentary[fx] = true;
                pendingTrig[fx] = true;
            }
        }
        else if (msg.isNoteOff())
        {
            const int fx = msg.getNoteNumber() - 36;
            if (fx >= 0 && fx < NumFx && midiHeld[fx].load())
            {
                midiHeld[fx] = false;
                rackShared.momentary[fx] = false;
            }
        }
        else if (msg.isController())
        {
            int s1, n1, s2, n2;
            ccFifo.prepareToWrite (1, s1, n1, s2, n2);
            if (n1 > 0) ccEvents[(size_t) s1] = { msg.getControllerNumber(), msg.getControllerValue() };
            ccFifo.finishedWrite (n1 + n2);
        }
    }

    updateTransport (numSamples);

    if (panicFlag.exchange (false))
    {
        rack.reset();
        fbBuf.clear();
        fbHp.reset();
        dryDelay.reset();
        output.reset();
        panicMute = (int) (sr * 0.05);
    }

    for (int k = 0; k < 2; ++k) { meterIn[k].store (0.0f); }
    float outPeak[2] { 0, 0 };
    float grMin = 1.0f;
    for (int off = 0; off < numSamples; off += chunkSize)
    {
        const int n = juce::jmin (chunkSize, numSamples - off);
        processChunk (L + off, R + off, hasSc ? scL + off : nullptr, hasSc ? scR + off : nullptr, n, hasSc);
        for (int i = 0; i < n; ++i)
        {
            outPeak[0] = juce::jmax (outPeak[0], std::abs (L[off + i]));
            outPeak[1] = juce::jmax (outPeak[1], std::abs (R[off + i]));
        }
        grMin = juce::jmin (grMin, output.takeGainReduction());
    }
    meterOut[0] = outPeak[0];
    meterOut[1] = outPeak[1];
    meterGR = grMin;
    outLevel = juce::jmax (outPeak[0], outPeak[1]);
    uiPpq = ppq;

    for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, numSamples);
}

void IDGemsProcessor::processChunk (float* L, float* R, const float* scL, const float* scR, int n, bool hasSc)
{
    const double spb = sr * 60.0 / bpm;
    const bool zl = rv (gi.zeroLat) > 0.5f;

    // ---- gain d'entrée + vu-mètre
    inGainSm.setTargetValue (dsp::dbToGain (bank[idxInGain].get()));
    float inPk[2] { meterIn[0].load(), meterIn[1].load() };
    for (int i = 0; i < n; ++i)
    {
        const float g = inGainSm.getNextValue();
        L[i] *= g; R[i] *= g;
        if (! dsp::finite (L[i])) L[i] = 0.0f;
        if (! dsp::finite (R[i])) R[i] = 0.0f;
        inPk[0] = juce::jmax (inPk[0], std::abs (L[i]));
        inPk[1] = juce::jmax (inPk[1], std::abs (R[i]));
    }
    meterIn[0] = inPk[0]; meterIn[1] = inPk[1];

    // ---- capture d'entrée (attrape une mesure et la boucle)
    {
        const bool want = rv (gi.capture) > 0.5f;
        int st = captureState_.load();
        static const double lens[] { 1.0, 2.0, 0.0, 0.0 };
        const int li = juce::jlimit (0, 3, (int) rv (gi.captureLen));
        const double beats = li < 2 ? lens[li] : (li == 2 ? 1.0 : 2.0) * beatsPerBar;
        if (! want && (st == 1 || st == 2)) st = 0;
        if (want && st == 0) { st = 1; capArmBeat = (int64_t) std::floor (ppq); }
        if (st == 1 && (int64_t) std::floor (ppq) > capArmBeat)
        {
            st = 2; capPos = 0;
            capTarget = juce::jlimit (256, (int) capBuf[0].size() - 1, (int) (beats * spb));
        }
        const float step = 1.0f / (float) (0.01 * sr);
        for (int i = 0; i < n; ++i)
        {
            if (st == 2)
            {
                capBuf[0][(size_t) capPos] = L[i];
                capBuf[1][(size_t) capPos] = R[i];
                if (++capPos >= capTarget) { capLen = capTarget; capPos = 0; st = 3; }
            }
            else if (st == 3 || capMix > 0.0f)
            {
                capMix = juce::jlimit (0.0f, 1.0f, capMix + (st == 3 && want ? step : -step));
                if (capLen > 0)
                {
                    const int fade = juce::jmin (256, capLen / 4);
                    float w = juce::jmin (1.0f, (float) (capPos + 1) / (float) fade, (float) (capLen - capPos) / (float) fade);
                    float l = capBuf[0][(size_t) capPos] * w, r = capBuf[1][(size_t) capPos] * w;
                    L[i] = L[i] * (1.0f - capMix) + l * capMix;
                    R[i] = R[i] * (1.0f - capMix) + r * capMix;
                    if (++capPos >= capLen) capPos = 0;
                }
                if (st == 3 && ! want && capMix <= 0.0f) st = 0;
            }
        }
        if (st == 3 && ! want && capMix <= 0.0f) st = 0;
        captureState_ = st;
    }

    // ---- boucle de feedback globale (sortie -> entrée)
    const float fbAmt = bank[idxFbAmount].get();
    if (fbAmt > 0.0001f)
    {
        const double d = juce::jlimit (1.0, (double) fbBuf.size - 4.0, bank[idxFbTime].get() * 0.001 * sr);
        fbHp.set (sr, 60.0f, 0.707f);
        for (int i = 0; i < n; ++i)
        {
            L[i] += dsp::fastTanh (fbHp.highpass (0, fbBuf.delay (0, d))) * fbAmt;
            R[i] += dsp::fastTanh (fbHp.highpass (1, fbBuf.delay (1, d))) * fbAmt;
        }
        fbHp.sanitize();
    }

    // ---- détection de transitoires + suiveur d'enveloppe
    bool transient = false;
    float envPeak = 0.0f;
    {
        const bool trOn = rv (gi.transOn) > 0.5f;
        const float sens = rv (gi.transSens);
        const bool useSc = hasSc && rv (gi.transSrc) > 0.5f;
        const bool envSc = hasSc && rv (gi.envSrc) > 0.5f;
        const float aF = std::exp (-1.0f / (0.003f * (float) sr));
        const float aS = std::exp (-1.0f / (0.12f * (float) sr));
        const float ratio = 1.4f + (1.0f - sens) * 3.0f;
        const float floorG = dsp::dbToGain (-55.0f + (1.0f - sens) * 25.0f);
        for (int i = 0; i < n; ++i)
        {
            float x = useSc ? juce::jmax (std::abs (scL[i]), std::abs (scR[i])) : juce::jmax (std::abs (L[i]), std::abs (R[i]));
            trFast = x > trFast ? x : x + (trFast - x) * aF;
            trSlow = x + (trSlow - x) * aS;
            if (trOn && trHold <= 0 && trFast > trSlow * ratio && trFast > floorG) { transient = true; trHold = (int) (0.07 * sr); }
            float e = envSc ? juce::jmax (std::abs (scL[i]), std::abs (scR[i])) : juce::jmax (std::abs (L[i]), std::abs (R[i]));
            envPeak = juce::jmax (envPeak, e);
        }
        trHold -= n;
    }

    // ---- modulation
    const float chaos = bank[idxChaos].get();
    const int lfoTrigMode = (int) rv (gi.lfoTrig);
    const bool lfoTrigger = ((lfoTrigMode == 0 || lfoTrigMode == 2) && midiLfoTrigger) || ((lfoTrigMode == 1 || lfoTrigMode == 2) && transient);
    midiLfoTrigger = false;
    modEngine.process (n, ppq, spb, playing, envPeak, lfoTrigger, chaos);

    scale.enabled = rv (gi.scaleOn) > 0.5f;
    scale.key = (int) rv (gi.scaleKey);
    scale.type = (int) rv (gi.scaleType);
    scale.snap = rv (gi.pitchSnap) > 0.5f;
    {
        const auto& rs = random.settings;
        const float driftAmt = rs.driftOn ? rs.driftAmount : 0.0f;
        driftTime += (double) n / sr * (0.05 + rs.driftSpeed * 2.0);
        bank.update (scale, driftAmt, driftTime, rack.activeFlags());
    }

    // ---- séquenceurs, pads, transitoires
    RackControl rc;
    rc.chaos = chaos;
    static const int osf[] { 1, 2, 4 };
    rc.osFactor = osf[juce::jlimit (0, 2, (int) rv (gi.destOS))];
    const auto& reg = registry();
    const int throwDelay = reg.fxFirst[FxDelay] + 8, throwReverb = reg.fxFirst[FxReverb] + 6;
    for (int fx = 0; fx < NumFx; ++fx)
    {
        const bool mom = rackShared.momentary[fx].load (std::memory_order_relaxed);
        const bool rise = mom && ! lastMomentary[fx];
        lastMomentary[fx] = mom;
        bool gate = true, trig = false;
        const bool seqOn = seqMaster.load (std::memory_order_relaxed) && seqData[fx].enabled.load (std::memory_order_relaxed);
        if (seqOn) seqRt[fx].tick (seqData[fx], ppq, chaos, audioRng, gate, trig);
        else seqData[fx].current.store (-1, std::memory_order_relaxed);
        const bool isThrow = (fx == FxDelay && bank[throwDelay].on()) || (fx == FxReverb && bank[throwReverb].on());
        if (isThrow && ! seqOn) gate = false;
        if (mom) gate = true;
        trig = trig || rise || pendingTrig[fx];
        pendingTrig[fx] = false;
        if (transient && rackShared.transTrig[fx].load (std::memory_order_relaxed) && effectDefs()[(size_t) fx].triggerable) trig = true;
        rc.gate[fx] = gate;
        rc.trig[fx] = trig;
    }

    // ---- rack
    ProcCtx ctx;
    ctx.sr = sr; ctx.baseSr = sr; ctx.os = 1; ctx.bpm = bpm; ctx.ppq = ppq; ctx.spb = spb;
    ctx.playing = playing; ctx.beatsPerBar = beatsPerBar; ctx.scale = &scale; ctx.zeroLatency = zl;

    std::memcpy (dryL.data(), L, sizeof (float) * (size_t) n);
    std::memcpy (dryR.data(), R, sizeof (float) * (size_t) n);
    dryDelay.delay = Rack::rackLatency (zl);
    for (int i = 0; i < n; ++i) dryDelay.process (dryL[(size_t) i], dryR[(size_t) i]);

    rack.process (L, R, n, ctx, rc);

    // ---- dry/wet global
    globalMixSm.setTargetValue (bank[idxGlobalMix].get());
    for (int i = 0; i < n; ++i)
    {
        const float m = globalMixSm.getNextValue();
        if (m >= 0.99999f) continue;
        L[i] = dryL[(size_t) i] + (L[i] - dryL[(size_t) i]) * m;
        R[i] = dryR[(size_t) i] + (R[i] - dryR[(size_t) i]) * m;
    }

    // ---- chaîne de sortie
    OutputChain::Settings os;
    os.hpFreq = rv (gi.hpFreq);
    os.monoOn = rv (gi.monoOn) > 0.5f;
    os.monoFreq = rv (gi.monoFreq);
    os.ottDepth = bank[gi.ottDepth].get();
    os.ottTime = rv (gi.ottTime);
    os.ottUp = rv (gi.ottUp);
    os.ottDown = rv (gi.ottDown);
    os.ottGainDb = rv (gi.ottGain);
    os.outGainDb = bank[idxOutGain].get();
    os.clipDriveDb = rv (gi.clipDrive);
    os.ceilingDb = rv (gi.ceiling);
    os.releaseMs = rv (gi.limRelease);
    os.zeroLatency = zl;
    output.process (L, R, n, os);

    if (panicMute > 0)
    {
        const int total = (int) (sr * 0.05);
        for (int i = 0; i < n; ++i)
        {
            const float g = 1.0f - (float) juce::jmax (0, panicMute) / (float) total;
            L[i] *= g; R[i] *= g;
            --panicMute;
        }
    }

    // ---- enregistreur, feedback, oscilloscope
    recorder.push (L, R, n);
    for (int i = 0; i < n; ++i) fbBuf.push (L[i], R[i]);
    {
        int s1, n1, s2, n2;
        scopeFifo.prepareToWrite (n, s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) scopeData[(size_t) (s1 + i)] = 0.5f * (L[i] + R[i]);
        for (int i = 0; i < n2; ++i) scopeData[(size_t) (s2 + i)] = 0.5f * (L[n1 + i] + R[n1 + i]);
        scopeFifo.finishedWrite (n1 + n2);
    }

    // ---- avance du transport
    const double before = ppq;
    ppq += (double) n / spb;
    if ((int64_t) std::floor (ppq) != (int64_t) std::floor (before)) ++beatCounter;
    if ((int64_t) std::floor (ppq / beatsPerBar) != (int64_t) std::floor (before / beatsPerBar)) ++barCounter;
}

// ---------------------------------------------------------------------------
// Thread interface
// ---------------------------------------------------------------------------
void IDGemsProcessor::timerCallback()
{
    // latence déclarée (change avec le mode sans latence)
    const bool zl = zeroLatency();
    const int lat = Rack::rackLatency (zl) + output.latency (zl);
    if (lat != lastLatency)
    {
        lastLatency = lat;
        setLatencySamples (lat);
    }

    random.timerTick();

    // MIDI CC : learn + contrôle
    {
        int s1, n1, s2, n2;
        ccFifo.prepareToRead (ccFifo.getNumReady(), s1, n1, s2, n2);
        auto handle = [this] (std::pair<int, int> ev)
        {
            const int cc = juce::jlimit (0, 127, ev.first);
            if (learnTarget >= 0)
            {
                for (auto& c : ccToParam) if (c.load() == learnTarget) c = -1;
                ccToParam[cc] = learnTarget;
                learnTarget = -1;
            }
            const int p = ccToParam[cc].load();
            if (p >= 0 && p < (int) params.size()) setNorm (p, (float) ev.second / 127.0f);
        };
        for (int i = 0; i < n1; ++i) handle (ccEvents[(size_t) (s1 + i)]);
        for (int i = 0; i < n2; ++i) handle (ccEvents[(size_t) (s2 + i)]);
        ccFifo.finishedRead (n1 + n2);
    }

    // arrangement de snapshots
    if (arrangePlaying.load())
    {
        int totalBars = 0;
        for (auto& a : arrange) if (a.snapshot >= 0) totalBars += juce::jmax (1, a.bars);
        if (totalBars > 0)
        {
            const int64_t bar = (barCounter.load() - arrangeStartBar) % totalBars;
            int acc = 0, cur = -1;
            for (int i = 0; i < NumArrangeSlots; ++i)
            {
                if (arrange[i].snapshot < 0) continue;
                acc += juce::jmax (1, arrange[i].bars);
                if (bar < acc) { cur = i; break; }
            }
            if (cur != arrangeCurrent.load() && cur >= 0)
            {
                arrangeCurrent = cur;
                const auto& s = snapshots[arrange[cur].snapshot];
                if (s.valid) { morphEnabled = true; setNorm (idxMorphX, s.x); setNorm (idxMorphY, s.y); }
            }
        }
    }
    else
    {
        arrangeStartBar = barCounter.load() + 1;
        arrangeCurrent = -1;
    }

    // morphing XY
    if (morphEnabled.load())
    {
        const float x = getNorm (idxMorphX), y = getNorm (idxMorphY);
        if (std::abs (x - lastMorphX) > 1.0e-4f || std::abs (y - lastMorphY) > 1.0e-4f)
        {
            lastMorphX = x; lastMorphY = y;
            applyMorph (x, y);
        }
    }
}

void IDGemsProcessor::setNorm (int index, float v)
{
    if (index < 0 || index >= (int) params.size()) return;
    auto* p = params[(size_t) index];
    v = juce::jlimit (0.0f, 1.0f, v);
    if (registry().all[(size_t) index].type != PType::Float)
        v = p->convertTo0to1 (std::round (p->convertFrom0to1 (v)));
    if (std::abs (p->getValue() - v) > 1.0e-6f) p->setValueNotifyingHost (v);
}

void IDGemsProcessor::setNormGesture (int index, float v)
{
    if (index < 0 || index >= (int) params.size()) return;
    auto* p = params[(size_t) index];
    p->beginChangeGesture();
    setNorm (index, v);
    p->endChangeGesture();
}

void IDGemsProcessor::setValue (const juce::String& id, float realValue)
{
    const int i = indexOf (id);
    if (i < 0) return;
    setNorm (i, params[(size_t) i]->convertTo0to1 (realValue));
}

void IDGemsProcessor::panic()
{
    panicFlag = true;
    setNorm (idxFbAmount, 0.0f);
    for (auto& m : rackShared.momentary) m = false;
}

void IDGemsProcessor::setMomentary (int fx, bool down)
{
    if (fx < 0 || fx >= NumFx) return;
    rackShared.momentary[fx] = down;
}

void IDGemsProcessor::clearMidiLearn (int paramIndex)
{
    for (auto& c : ccToParam) if (c.load() == paramIndex) c = -1;
}

int IDGemsProcessor::ccForParam (int paramIndex) const
{
    for (int i = 0; i < 128; ++i) if (ccToParam[i].load() == paramIndex) return i;
    return -1;
}

void IDGemsProcessor::initEffect (int fx)
{
    const auto& reg = registry();
    for (int k = 0; k < reg.fxCount[fx]; ++k)
    {
        const int i = reg.fxFirst[fx] + k;
        setNorm (i, params[(size_t) i]->getDefaultValue());
    }
    setNorm (reg.fxMix[fx], params[(size_t) reg.fxMix[fx]]->getDefaultValue());
}

juce::ValueTree IDGemsProcessor::copyEffect (int fx) const
{
    juce::ValueTree t ("FXCOPY");
    const auto& reg = registry();
    t.setProperty ("fx", fx, nullptr);
    auto put = [&] (int i)
    {
        auto key = reg.all[(size_t) i].id.fromFirstOccurrenceOf ("_", false, false);
        t.setProperty (key, getNorm (i), nullptr);
    };
    put (reg.fxMix[fx]);
    for (int k = 0; k < reg.fxCount[fx]; ++k) put (reg.fxFirst[fx] + k);
    t.appendChild (seqData[fx].toTree(), nullptr);
    return t;
}

void IDGemsProcessor::pasteEffect (int fx, const juce::ValueTree& t)
{
    if (! t.isValid()) return;
    const auto& reg = registry();
    const int srcFx = t.getProperty ("fx", -1);
    auto get = [&] (int i)
    {
        auto key = reg.all[(size_t) i].id.fromFirstOccurrenceOf ("_", false, false);
        if (t.hasProperty (key)) setNorm (i, (float) t.getProperty (key));
    };
    get (reg.fxMix[fx]);
    if (srcFx == fx) for (int k = 0; k < reg.fxCount[fx]; ++k) get (reg.fxFirst[fx] + k);
    seqData[fx].fromTree (t.getChildWithName ("SEQ"));
}

void IDGemsProcessor::randomEffectOrder (juce::Random& r, float amount)
{
    auto order = rackShared.getOrder();
    const int swaps = juce::roundToInt (amount * 6.0f);
    for (int s = 0; s < swaps; ++s)
    {
        int a = r.nextInt (NumFx), b = r.nextInt (NumFx);
        if (rackShared.fxLocked[order[(size_t) a]].load() || rackShared.fxLocked[order[(size_t) b]].load()) continue;
        std::swap (order[(size_t) a], order[(size_t) b]);
    }
    rackShared.setOrder (order);
}

// ---- Undo / A-B ------------------------------------------------------------
void IDGemsProcessor::pushUndo()
{
    undoStack.push_back (captureState());
    if (undoStack.size() > 64) undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void IDGemsProcessor::undo()
{
    if (undoStack.empty()) return;
    redoStack.push_back (captureState());
    auto s = undoStack.back();
    undoStack.pop_back();
    restoreState (s);
}

void IDGemsProcessor::redo()
{
    if (redoStack.empty()) return;
    undoStack.push_back (captureState());
    auto s = redoStack.back();
    redoStack.pop_back();
    restoreState (s);
}

void IDGemsProcessor::switchAB (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == abSlot) return;
    abStates[abSlot] = captureState();
    if (abStates[slot].isValid()) restoreState (abStates[slot]);
    abSlot = slot;
}

void IDGemsProcessor::copyAtoB()
{
    abStates[0] = captureState();
    abStates[1] = abStates[0].createCopy();
}

// ---- Snapshots ---------------------------------------------------------------
void IDGemsProcessor::storeSnapshot (int i)
{
    auto& s = snapshots[i];
    const auto& reg = registry();
    s.values.assign (reg.all.size(), 0.0f);
    for (size_t k = 0; k < reg.all.size(); ++k) s.values[k] = getNorm ((int) k);
    s.valid = true;
}

void IDGemsProcessor::recallSnapshot (int i)
{
    auto& s = snapshots[i];
    if (! s.valid) return;
    pushUndo();
    setNorm (idxMorphX, s.x);
    setNorm (idxMorphY, s.y);
    lastMorphX = s.x; lastMorphY = s.y;
    const auto& reg = registry();
    for (size_t k = 0; k < reg.all.size() && k < s.values.size(); ++k)
        if (snapshotIncludes (reg.all[k])) setNorm ((int) k, s.values[k]);
}

void IDGemsProcessor::clearSnapshot (int i) { snapshots[i].valid = false; snapshots[i].values.clear(); }

void IDGemsProcessor::applyMorph (float x, float y)
{
    float w[NumSnapshots] {};
    float total = 0.0f;
    int best = -1;
    float bestW = -1.0f;
    for (int i = 0; i < NumSnapshots; ++i)
    {
        if (! snapshots[i].valid) continue;
        float dx = x - snapshots[i].x, dy = y - snapshots[i].y;
        w[i] = 1.0f / (dx * dx + dy * dy + 0.004f); // inverse du carré de la distance : transitions progressives
        total += w[i];
        if (w[i] > bestW) { bestW = w[i]; best = i; }
    }
    if (total <= 0.0f || best < 0) return;
    const auto& reg = registry();
    for (size_t k = 0; k < reg.all.size(); ++k)
    {
        const auto& d = reg.all[k];
        if (! snapshotIncludes (d)) continue;
        float v;
        if (d.type == PType::Float)
        {
            v = 0.0f;
            for (int i = 0; i < NumSnapshots; ++i)
                if (snapshots[i].valid && k < snapshots[i].values.size()) v += w[i] * snapshots[i].values[k];
            v /= total;
        }
        else
        {
            if (k >= snapshots[best].values.size()) continue;
            v = snapshots[best].values[k];
        }
        setNorm ((int) k, v);
    }
}

// ---- Réglages communs --------------------------------------------------------
juce::Colour IDGemsProcessor::getAccent()
{
    if (auto* f = appProps.getUserSettings())
        return juce::Colour::fromString (f->getValue ("accent", "ffff6a00"));
    return juce::Colour (0xffff6a00);
}

void IDGemsProcessor::setAccent (juce::Colour c)
{
    if (auto* f = appProps.getUserSettings()) { f->setValue ("accent", c.toString()); f->saveIfNeeded(); }
}

juce::AudioProcessorEditor* IDGemsProcessor::createEditor() { return new IDGemsEditor (*this); }

#if ! IDGEMS_TESTS
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new IDGemsProcessor(); }
#endif
