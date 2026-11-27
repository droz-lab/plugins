#include "Rack.h"
#include "EffectFactory.h"
#include "Stft.h"

namespace idg
{
struct Rack::Runtime
{
    std::unique_ptr<EffectModule> mod;
    float active = 0.0f, target = 0.0f;
    float gate = 1.0f;
    float mixSm = -1.0f;
    dsp::LatencyDelay dryDelay;
    std::unique_ptr<juce::dsp::Oversampling<float>> os2, os4;
    int tail = 0;            // échantillons restant pour laisser sonner une queue (throw)
    bool destruction = false, stft = false, noZeroLat = false;
    Family family = FamGlitch;
};

Rack::Rack() = default;
Rack::~Rack() = default;

void Rack::init (ParamBank& b, RackShared& s)
{
    bank = &b;
    shared = &s;
    for (int f = 0; f < NumFamilies; ++f) famMixIdx[f] = registry().find (G::famMix (f));
    rt.clear();
    rt.resize (NumFx);
    const auto& defs = effectDefs();
    for (int i = 0; i < NumFx; ++i)
    {
        auto& r = rt[(size_t) i];
        r.mod = createEffect (i);
        r.mod->bind (b.fxParams (i));
        r.family = defs[(size_t) i].family;
        r.destruction = r.family == FamDestruction;
        r.stft = defs[(size_t) i].stft;
        r.noZeroLat = defs[(size_t) i].noZeroLatency;
    }
}

int Rack::rackLatency (bool zeroLatency)
{
    if (zeroLatency) return 0;
    int l = 0;
    for (const auto& d : effectDefs()) if (d.stft) l += StftFrame::latency;
    return l;
}

void Rack::prepare (double sr, int maxBlock)
{
    sampleRate = sr;
    for (auto* v : { &dryL, &dryR, &ddL, &ddR, &xL, &xR, &gL, &gR, &aL, &aR, &tL, &tR })
        v->assign ((size_t) maxBlock, 0.0f);
    for (auto& r : rt)
    {
        r.mod->prepare (r.destruction ? sr * 4.0 : sr, maxBlock);
        r.dryDelay.prepare (StftFrame::latency + 8);
        r.dryDelay.delay = r.stft ? StftFrame::latency : 0;
        if (r.destruction)
        {
            using OS = juce::dsp::Oversampling<float>;
            r.os2 = std::make_unique<OS> (2, 1, OS::filterHalfBandPolyphaseIIR, false, false);
            r.os4 = std::make_unique<OS> (2, 2, OS::filterHalfBandPolyphaseIIR, false, false);
            r.os2->initProcessing ((size_t) maxBlock);
            r.os4->initProcessing ((size_t) maxBlock);
        }
        r.active = r.target = 0.0f;
        r.gate = 1.0f;
        r.mixSm = -1.0f;
    }
}

void Rack::reset()
{
    for (auto& r : rt)
    {
        r.mod->reset();
        r.dryDelay.reset();
        if (r.os2) r.os2->reset();
        if (r.os4) r.os4->reset();
        r.tail = 0;
    }
}

void Rack::process (float* L, float* R, int n, ProcCtx& ctx, const RackControl& rc)
{
    const auto& reg = registry();
    auto& b = *bank;
    const auto order = shared->getOrder();

    bool anySolo = false;
    for (int i = 0; i < NumFx; ++i) anySolo |= shared->solo[i].load (std::memory_order_relaxed);
    for (int f = 0; f < NumFamilies; ++f) anySolo |= shared->famSolo[f].load (std::memory_order_relaxed);

    // Cible d'activité de chaque effet
    for (int fx = 0; fx < NumFx; ++fx)
    {
        auto& r = rt[(size_t) fx];
        bool on = b[reg.fxOn[fx]].on() || shared->momentary[fx].load (std::memory_order_relaxed);
        if (r.mod->throwMode() && r.tail > 0) on = true;
        if (ctx.zeroLatency && r.noZeroLat) on = false;
        if (anySolo && ! (shared->solo[fx].load (std::memory_order_relaxed)
                          || shared->famSolo[(int) r.family].load (std::memory_order_relaxed)))
            on = false;
        r.target = on ? 1.0f : 0.0f;
        activeArr[fx] = r.target > 0.0f || r.active > 0.0f;
    }

    // chaîne strictement en série, dans l'ordre choisi
    for (int sIdx = 0; sIdx < NumFx; ++sIdx)
        processSlot (order[(size_t) sIdx], L, R, n, ctx, rc);
}

void Rack::processSlot (int fx, float* L, float* R, int n, ProcCtx& ctx, const RackControl& rc)
{
    const auto& reg = registry();
    auto& b = *bank;
    auto& r = rt[(size_t) fx];
    const size_t bytes = sizeof (float) * (size_t) n;
    const bool lat = ! ctx.zeroLatency && r.stft;

    // Signal sec (retardé si l'effet a de la latence, pour rester aligné)
    std::memcpy (dryL.data(), L, bytes); std::memcpy (dryR.data(), R, bytes);
    std::memcpy (ddL.data(), L, bytes); std::memcpy (ddR.data(), R, bytes);
    if (lat)
        for (int i = 0; i < n; ++i) r.dryDelay.process (ddL[(size_t) i], ddR[(size_t) i]);

    if (r.active <= 0.0f && r.target <= 0.0f)
    {
        shared->processing[fx].store (false, std::memory_order_relaxed);
        shared->activity[fx].store (0.0f, std::memory_order_relaxed);
        if (lat) { std::memcpy (L, ddL.data(), bytes); std::memcpy (R, ddR.data(), bytes); }
        return; // effet éteint : aucun calcul
    }
    shared->processing[fx].store (true, std::memory_order_relaxed);

    if (r.active <= 0.0f && r.target > 0.0f)
    {
        r.mod->reset(); // repart d'un état propre
        r.tail = 0;
        if (r.os2) r.os2->reset();
        if (r.os4) r.os4->reset();
    }

    const bool throwing = r.mod->throwMode();
    ctx.gate = rc.gate[fx];
    if (rc.trig[fx]) r.mod->trigger (ctx);

    std::memcpy (xL.data(), dryL.data(), bytes);
    std::memcpy (xR.data(), dryR.data(), bytes);

    // Traitement (suréchantillonné pour la famille destruction)
    juce::dsp::Oversampling<float>* os = nullptr;
    if (r.destruction && rc.osFactor == 2) os = r.os2.get();
    if (r.destruction && rc.osFactor == 4) os = r.os4.get();
    if (os != nullptr)
    {
        float* chans[2] { xL.data(), xR.data() };
        juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
        auto up = os->processSamplesUp (block);
        ProcCtx c2 = ctx;
        c2.os = rc.osFactor;
        c2.sr = ctx.baseSr * rc.osFactor;
        c2.spb = ctx.spb * rc.osFactor;
        r.mod->process (up.getChannelPointer (0), up.getChannelPointer (1), (int) up.getNumSamples(), c2);
        os->processSamplesDown (block);
    }
    else
        r.mod->process (xL.data(), xR.data(), n, ctx);

    // Dry/wet : propre × famille × chaos, avec rampes anti-clic
    float mix = b[reg.fxMix[fx]].get();
    const float famMix = b[famMixIdx[(int) r.family]].get();
    // Chaos ne dose que les effets d'espace : le glitch reste affirmé
    const float c = rc.chaos;
    if (r.family == FamSpace)
    {
        if (c < 0.5f) mix *= c * 2.0f;
        else mix += (1.0f - mix) * (c - 0.5f) * 1.6f;
    }
    mix = juce::jlimit (0.0f, 1.0f, mix * famMix);
    if (r.mixSm < 0.0f) r.mixSm = mix;

    const float gateTarget = (throwing || rc.gate[fx]) ? 1.0f : 0.0f;
    const float rampStep = 1.0f / (float) (0.004 * ctx.baseSr); // rampes de ~4 ms
    float peak = 0.0f;
    bool bad = false;
    for (int i = 0; i < n; ++i)
    {
        r.active += juce::jlimit (-rampStep, rampStep, r.target - r.active);
        r.gate += juce::jlimit (-rampStep, rampStep, gateTarget - r.gate);
        r.mixSm += (mix - r.mixSm) * 0.002f;
        const float g = r.mixSm * r.active * r.gate;
        const float dl = ddL[(size_t) i], dr = ddR[(size_t) i];
        const float ol = dl + (xL[(size_t) i] - dl) * g;
        const float orr = dr + (xR[(size_t) i] - dr) * g;
        if (! dsp::finite (ol) || ! dsp::finite (orr) || std::abs (ol) > 1000.0f || std::abs (orr) > 1000.0f) { bad = true; break; }
        L[i] = ol; R[i] = orr;
        peak = juce::jmax (peak, std::abs (xL[(size_t) i] - dl), std::abs (xR[(size_t) i] - dr));
    }
    if (bad)
    {
        // protection : un effet qui produit un NaN ou explose est réinitialisé, on rend le signal sec
        r.mod->reset();
        std::memcpy (L, ddL.data(), bytes); std::memcpy (R, ddR.data(), bytes);
        peak = 0.0f;
    }
    r.active = juce::jlimit (0.0f, 1.0f, r.active);

    if (throwing)
    {
        if (rc.gate[fx]) r.tail = (int) (ctx.baseSr * 0.5);
        else if (peak > 1.0e-4f) r.tail = juce::jmax (r.tail, (int) (ctx.baseSr * 0.05));
        else r.tail = juce::jmax (0, r.tail - n);
    }
    else r.tail = 0;

    shared->activity[fx].store (peak * r.active * r.gate, std::memory_order_relaxed);
}
} // namespace idg
