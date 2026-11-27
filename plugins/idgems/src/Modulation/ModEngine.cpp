#include "ModEngine.h"

namespace idg
{
juce::String sourceName (int s)
{
    if (s >= SrcLfo0 && s < SrcRnd0) return "LFO " + juce::String (s - SrcLfo0 + 1);
    if (s >= SrcRnd0 && s < SrcEnv) return "RND " + juce::String (s - SrcRnd0 + 1);
    if (s == SrcEnv) return "ENV";
    if (s >= SrcMacro0 && s < NumSources) return "M" + juce::String (s - SrcMacro0 + 1);
    return "?";
}

juce::Colour sourceColour (int s)
{
    if (s < SrcRnd0) return juce::Colour (0xff36c8ff);
    if (s < SrcEnv) return juce::Colour (0xffb06bff);
    if (s == SrcEnv) return juce::Colour (0xff50ff9a);
    return juce::Colour (0xffff8c1a);
}

int ModShared::addConnection (int src, int dst, float depth, bool bipolar)
{
    int existing = findConnection (src, dst);
    if (existing >= 0) { slots[existing].depth = depth; return existing; }
    for (int i = 0; i < MaxModSlots; ++i)
    {
        if (slots[i].src.load() < 0)
        {
            slots[i].depth = depth;
            slots[i].bipolar = bipolar;
            slots[i].dst = dst;
            slots[i].src = src; // écrit en dernier : le slot devient valide
            return i;
        }
    }
    return -1;
}

int ModShared::findConnection (int src, int dst) const
{
    for (int i = 0; i < MaxModSlots; ++i)
        if (slots[i].src.load() == src && slots[i].dst.load() == dst) return i;
    return -1;
}

juce::ValueTree ModShared::toTree() const
{
    juce::ValueTree t ("MODS");
    const auto& reg = registry();
    for (const auto& s : slots)
    {
        int src = s.src.load(), dst = s.dst.load();
        if (src < 0 || dst < 0 || dst >= (int) reg.all.size()) continue;
        juce::ValueTree c ("CONN");
        c.setProperty ("src", src, nullptr);
        c.setProperty ("dst", reg.all[(size_t) dst].id, nullptr);
        c.setProperty ("depth", s.depth.load(), nullptr);
        c.setProperty ("bipolar", s.bipolar.load(), nullptr);
        t.appendChild (c, nullptr);
    }
    for (int i = 0; i < NumLfos; ++i)
    {
        juce::ValueTree l ("LFOSHAPE");
        l.setProperty ("index", i, nullptr);
        l.setProperty ("pts", shapes[i].toString(), nullptr);
        t.appendChild (l, nullptr);
    }
    return t;
}

void ModShared::fromTree (const juce::ValueTree& t)
{
    if (! t.isValid()) return;
    clearAll();
    const auto& reg = registry();
    for (auto c : t)
    {
        if (c.hasType ("CONN"))
        {
            int src = c.getProperty ("src", -1);
            int dst = reg.find (c.getProperty ("dst").toString());
            if (src >= 0 && src < NumSources && dst >= 0)
                addConnection (src, dst, (float) c.getProperty ("depth", 0.5f), (bool) c.getProperty ("bipolar", false));
        }
        else if (c.hasType ("LFOSHAPE"))
        {
            int i = c.getProperty ("index", -1);
            if (i >= 0 && i < NumLfos) setShape (i, LfoShape::fromString (c.getProperty ("pts").toString()));
        }
    }
}

// ---------------------------------------------------------------------------
void ModEngine::init (ParamBank& b, ModShared& s)
{
    bank = &b;
    shared = &s;
    for (int i = 0; i < NumLfos; ++i)
        lp[i] = { b.byId (lfoId (i, "rate")), b.byId (lfoId (i, "sync")), b.byId (lfoId (i, "div")), b.byId (lfoId (i, "mode")),
                  b.byId (lfoId (i, "smooth")), b.byId (lfoId (i, "phase")), b.byId (lfoId (i, "delay")), b.byId (lfoId (i, "stereo")) };
    for (int i = 0; i < NumRandMods; ++i)
        rp[i] = { b.byId (rndId (i, "rate")), b.byId (rndId (i, "sync")), b.byId (rndId (i, "div")), b.byId (rndId (i, "style")),
                  b.byId (rndId (i, "stereo")) };
    for (int i = 0; i < NumMacros; ++i) macros[i] = b.byId (G::macro (i));
    envAtt = b.byId (G::envAtt);
    envRel = b.byId (G::envRel);
    envGain = b.byId (G::envGain);
}

void ModEngine::prepare (double newSr) { sr = newSr; reset(); }

void ModEngine::reset()
{
    for (auto& l : lfo) l = LfoRt();
    for (int i = 0; i < NumRandMods; ++i) { rnd[i] = RndRt(); rnd[i].lx = 0.1 + i * 0.01; }
    env = 0.0f;
    rng.seed (0xA11CEu);
}

void ModEngine::process (int n, double ppq, double spb, bool /*playing*/, float envInputPeak, bool trigger, float chaos)
{
    const double dt = (double) n / sr;
    const double beats = (double) n / spb;

    // ---- LFO
    for (int i = 0; i < NumLfos; ++i)
    {
        auto& p = lp[i];
        auto& r = lfo[i];
        const int mode = p.mode->geti();
        const bool sync = p.sync->on();
        const double cycleBeats = divisionBeats (p.div->geti());
        const double inc = sync ? beats / cycleBeats : p.rate->get() * dt;

        if (trigger && mode >= 2) { r.phase = 0.0; r.sinceTrig = 0.0; r.envDone = false; }
        else r.sinceTrig += dt;

        double phase;
        if (mode == 1 && sync) phase = ppq / cycleBeats;     // calé sur la position de l'hôte
        else
        {
            if (r.sinceTrig >= p.delay->get() || mode < 2)
            {
                r.phase += inc;
                if (mode == 3 && r.phase >= 1.0) { r.phase = 1.0; r.envDone = true; }
            }
            phase = r.phase;
        }
        phase += p.phase->get();
        const int tIdx = shared->tableIndex[i].load (std::memory_order_acquire);
        const float* table = shared->tables[i][tIdx];
        const float stereo = p.stereo->get();
        const float smooth = p.smooth->get();
        const float a = smooth <= 0.0001f ? 0.0f : (float) std::exp (-dt / (smooth * 0.25));
        for (int ch = 0; ch < 2; ++ch)
        {
            double ph = phase + (ch == 1 ? stereo : 0.0);
            if (mode == 3) ph = juce::jlimit (0.0, 0.9999, ph);
            else ph -= std::floor (ph);
            float v = table[juce::jlimit (0, LfoShape::tableSize - 1, (int) (ph * LfoShape::tableSize))];
            r.sm[ch] = v + (r.sm[ch] - v) * a;
            values[SrcLfo0 + i][ch] = r.sm[ch];
        }
        if (r.phase > 1.0e6) r.phase -= std::floor (r.phase);
        shared->lfoPhase[i].store ((float) (phase - std::floor (phase)), std::memory_order_relaxed);
    }

    // ---- Random (Perlin, S&H, Sinus, Lorenz)
    for (int i = 0; i < NumRandMods; ++i)
    {
        auto& p = rp[i];
        auto& r = rnd[i];
        const int style = p.style->geti();
        const bool stereo = p.stereo->on();
        double pos;
        if (p.sync->on()) pos = ppq / divisionBeats (p.div->geti());
        else { r.t += p.rate->get() * dt; pos = r.t; }
        const auto idx = (int64_t) std::floor (pos);
        const float frac = (float) (pos - (double) idx);
        if (idx != r.lastIdx)
        {
            r.lastIdx = idx;
            for (int ch = 0; ch < 2; ++ch) { r.from[ch] = r.to[ch]; r.to[ch] = rng.uni(); }
            if (! stereo) r.to[1] = r.to[0];
        }
        for (int ch = 0; ch < 2; ++ch)
        {
            int c = stereo ? ch : 0;
            float v;
            switch (style)
            {
                case 0: v = dsp::smoothNoise (pos, (uint32_t) (i * 2 + c + 1)); break;
                case 1: v = r.to[c]; break;
                case 2: { float u = 0.5f - 0.5f * std::cos (dsp::pi * frac); v = r.from[c] + (r.to[c] - r.from[c]) * u; break; }
                default:
                {
                    if (ch == 0)
                    {
                        double h = juce::jlimit (0.0, 0.02, p.rate->get() * dt * 0.5);
                        for (int k = 0; k < 4; ++k)
                        {
                            double dx = 10.0 * (r.ly - r.lx), dy = r.lx * (28.0 - r.lz) - r.ly, dz = r.lx * r.ly - 8.0 / 3.0 * r.lz;
                            r.lx += dx * h * 0.25; r.ly += dy * h * 0.25; r.lz += dz * h * 0.25;
                        }
                        if (! std::isfinite (r.lx)) { r.lx = 0.1; r.ly = 0; r.lz = 0; }
                    }
                    v = (float) juce::jlimit (0.0, 1.0, 0.5 + (c == 0 ? r.lx : r.ly) / 45.0);
                    break;
                }
            }
            values[SrcRnd0 + i][ch] = v;
        }
    }

    // ---- Envelope follower
    {
        float level = envInputPeak * dsp::dbToGain (envGain->get());
        float db = dsp::gainToDb (level);
        float target = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
        float ms = target > env ? envAtt->get() : envRel->get();
        float a = (float) std::exp (-dt / (ms * 0.001));
        env = target + (env - target) * a;
        values[SrcEnv][0] = values[SrcEnv][1] = env;
    }

    // ---- Macros
    for (int i = 0; i < NumMacros; ++i)
        values[SrcMacro0 + i][0] = values[SrcMacro0 + i][1] = juce::jlimit (0.0f, 1.0f, macros[i]->raw->load (std::memory_order_relaxed));

    for (int s = 0; s < NumSources; ++s) shared->value[s].store (values[s][0], std::memory_order_relaxed);

    // ---- Application aux paramètres
    bank->clearMod();
    const float depthMul = 0.5f + chaos;
    const int numParams = bank->size();
    for (const auto& slot : shared->slots)
    {
        const int src = slot.src.load (std::memory_order_acquire);
        if (src < 0 || src >= NumSources) continue;
        const int dst = slot.dst.load (std::memory_order_relaxed);
        if (dst < 0 || dst >= numParams) continue;
        const float depth = slot.depth.load (std::memory_order_relaxed) * depthMul;
        const bool bip = slot.bipolar.load (std::memory_order_relaxed);
        auto& p = (*bank)[dst];
        for (int ch = 0; ch < 2; ++ch)
        {
            float v = values[src][ch];
            if (bip) v = v * 2.0f - 1.0f;
            p.modAcc[ch] += depth * v;
        }
        p.modActive = true;
    }
}
} // namespace idg
