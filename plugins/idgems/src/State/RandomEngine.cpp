#include "RandomEngine.h"
#include "../PluginProcessor.h"

namespace idg
{
juce::String RandomEngine::scopeLabel (Scope s, int target)
{
    switch (s)
    {
        case Scope::All: return IDG_T ("Général");
        case Scope::Family: return "Famille " + familyNames()[target];
        case Scope::Effect: return effectDefs()[(size_t) juce::jlimit (0, NumFx - 1, target)].shortName;
        case Scope::Sequencers: return IDG_T ("Séquenceurs");
    }
    return {};
}

void RandomEngine::request (Scope scope, int target, uint32_t seed)
{
    if (seed == 0) seed = (uint32_t) juce::Random::getSystemRandom().nextInt (999999) + 1;
    if (settings.sync == 0)
    {
        perform (scope, target, seed, scopeLabel (scope, target));
        return;
    }
    pending.active = true;
    pending.scope = scope;
    pending.target = target;
    pending.seed = seed;
    pending.counterAtRequest = settings.sync == 1 ? proc.getBeatCounter() : proc.getBarCounter();
    if (onChange) onChange();
}

float RandomEngine::pickTarget (int i, juce::Random& r)
{
    const auto& p = proc.bank[i];
    float lo = p.rmin.load(), hi = p.rmax.load();
    if (hi < lo) std::swap (lo, hi);
    if (settings.learn)
    {
        std::vector<const Entry*> favs;
        for (auto& e : history) if (e.favourite) favs.push_back (&e);
        if (! favs.empty() && r.nextFloat() < 0.65f)
        {
            const auto* e = favs[(size_t) r.nextInt ((int) favs.size())];
            auto params = e->state.getChildWithName (proc.apvts.state.getType());
            auto child = params.getChildWithProperty ("id", registry().all[(size_t) i].id);
            if (child.isValid())
            {
                auto* prm = proc.param (i);
                float v = prm->convertTo0to1 ((float) child.getProperty ("value"));
                return juce::jlimit (0.0f, 1.0f, v + (r.nextFloat() - 0.5f) * 0.15f * (hi - lo));
            }
        }
    }
    return lo + r.nextFloat() * (hi - lo);
}

void RandomEngine::perform (Scope scope, int target, uint32_t seed, const juce::String& label)
{
    perform (scope, target, seed, label, settings.amount);
}

void RandomEngine::perform (Scope scope, int target, uint32_t seed, const juce::String& label, float amount)
{
    proc.pushUndo();
    juce::Random r ((juce::int64) seed);
    seedShown = seed;
    const auto& reg = registry();
    const auto& defs = effectDefs();

    auto inScope = [&] (int fx)
    {
        switch (scope)
        {
            case Scope::All: case Scope::Sequencers: return true;
            case Scope::Family: return (int) defs[(size_t) fx].family == target;
            case Scope::Effect: return fx == target;
        }
        return false;
    };

    glide = {};
    std::vector<std::pair<int, float>> immediate;

    if (scope != Scope::Sequencers)
    {
        // 1. paramètres des effets
        for (int i = 0; i < (int) reg.all.size(); ++i)
        {
            const auto& d = reg.all[(size_t) i];
            if (d.effect < 0 || ! d.randomizable) continue;
            if (! inScope (d.effect)) continue;
            if (proc.bank[i].locked.load() || proc.rackShared.fxLocked[d.effect].load()) continue;
            const float cur = proc.getNorm (i);
            const float tgt = pickTarget (i, r);
            if (d.type == PType::Float)
            {
                float v = cur + (tgt - cur) * amount;
                if (settings.glideMs > 1.0f) glide.moves.push_back ({ i, { cur, v } });
                else immediate.push_back ({ i, v });
            }
            else
            {
                // choix / interrupteurs : probabilité de changer = dosage (le mode reste souvent en stéréo)
                float pChange = amount * (d.common ? 0.35f : 1.0f);
                if (r.nextFloat() < pChange) immediate.push_back ({ i, tgt });
            }
        }

        // 2. effets allumés / éteints
        auto canToggle = [&] (int fx) { return ! proc.rackShared.fxLocked[fx].load() && ! proc.bank[reg.fxOn[fx]].locked.load(); };
        if (scope == Scope::All)
        {
            const float pOn = (2.0f + 5.0f * amount) / (float) NumFx;
            int count = 0;
            for (int fx = 0; fx < NumFx; ++fx)
            {
                bool on = proc.getNorm (reg.fxOn[fx]) > 0.5f;
                if (canToggle (fx) && r.nextFloat() < amount * 0.8f) on = r.nextFloat() < pOn;
                if (on) ++count;
                immediate.push_back ({ reg.fxOn[fx], on ? 1.0f : 0.0f });
            }
            if (count == 0)
            {
                int fx = r.nextInt (NumFx);
                if (canToggle (fx)) immediate.push_back ({ reg.fxOn[fx], 1.0f });
            }
            if (settings.doOrder && amount > 0.35f) proc.randomEffectOrder (r, amount);
        }
        else if (scope == Scope::Family)
        {
            int count = 0;
            for (int fx = 0; fx < NumFx; ++fx)
            {
                if (! inScope (fx)) continue;
                bool on = proc.getNorm (reg.fxOn[fx]) > 0.5f;
                if (canToggle (fx) && r.nextFloat() < amount * 0.7f) on = r.nextFloat() < 0.5f;
                if (on) ++count;
                immediate.push_back ({ reg.fxOn[fx], on ? 1.0f : 0.0f });
            }
            if (count == 0)
                for (int fx = 0; fx < NumFx; ++fx)
                    if (inScope (fx) && canToggle (fx)) { immediate.push_back ({ reg.fxOn[fx], 1.0f }); break; }
        }
        else if (scope == Scope::Effect && canToggle (target))
            immediate.push_back ({ reg.fxOn[target], 1.0f });
    }

    // 3. séquenceurs
    const bool doSeq = scope == Scope::Sequencers || settings.doSeq;
    if (doSeq)
    {
        static const int lengths[] { 4, 5, 7, 8, 12, 16, 16, 16, 32 };
        static const int rates[] { 1, 3, 3, 3, 4, 5 };
        for (int fx = 0; fx < NumFx; ++fx)
        {
            if (! inScope (fx)) continue;
            auto& s = proc.seqData[fx];
            if (s.locked.load() || proc.rackShared.fxLocked[fx].load()) continue;
            const bool explicitSeq = scope == Scope::Sequencers;
            if (explicitSeq && ! s.enabled.load()) continue;
            if (! explicitSeq && r.nextFloat() > amount * 0.6f) continue;
            if (! explicitSeq) s.enabled = r.nextFloat() < 0.25f + 0.5f * amount;
            const float density = 0.25f + r.nextFloat() * 0.6f;
            s.length = lengths[r.nextInt (9)];
            s.rate = rates[r.nextInt (6)];
            s.swing = r.nextFloat() < 0.3f ? r.nextFloat() * 0.6f : 0.0f;
            for (int k = 0; k < MaxSteps; ++k)
            {
                s.on[k] = r.nextFloat() < density;
                s.prob[k] = r.nextFloat() < amount * 0.5f ? 0.4f + r.nextFloat() * 0.6f : 1.0f;
                s.ratchet[k] = r.nextFloat() < amount * 0.25f ? 2 + r.nextInt (3) : 1;
            }
        }
    }

    for (auto& m : immediate) proc.setNorm (m.first, m.second);

    if (! glide.moves.empty())
    {
        glide.active = true;
        glide.start = juce::Time::getMillisecondCounterHiRes();
        glide.duration = settings.glideMs;
        glideLabel = label;
        glideSeed = seed;
    }
    else addHistory (seed, label);

    if (onChange) onChange();
}

void RandomEngine::timerTick()
{
    if (pending.active)
    {
        const int64_t now = settings.sync == 1 ? proc.getBeatCounter() : proc.getBarCounter();
        if (now != pending.counterAtRequest || settings.sync == 0)
        {
            pending.active = false;
            perform (pending.scope, pending.target, pending.seed, scopeLabel (pending.scope, pending.target));
        }
    }

    if (glide.active)
    {
        const double t = juce::jlimit (0.0, 1.0, (juce::Time::getMillisecondCounterHiRes() - glide.start) / juce::jmax (1.0, glide.duration));
        const float e = (float) (t * t * (3.0 - 2.0 * t));
        for (auto& m : glide.moves) proc.setNorm (m.first, m.second.first + (m.second.second - m.second.first) * e);
        if (t >= 1.0)
        {
            glide.active = false;
            addHistory (glideSeed, glideLabel);
            if (onChange) onChange();
        }
    }

    if (settings.mutate)
    {
        static const int bars[] { 1, 2, 4, 8, 16 };
        const int64_t bar = proc.getBarCounter();
        if (lastMutateBar < 0 || bar < lastMutateBar) lastMutateBar = bar;
        if (bar - lastMutateBar >= bars[juce::jlimit (0, 4, settings.mutateBars)])
        {
            lastMutateBar = bar;
            const auto seed = (uint32_t) juce::Random::getSystemRandom().nextInt (999999) + 1;
            perform (Scope::All, 0, seed, "Mutate", settings.mutateAmount);
        }
    }
    else lastMutateBar = -1;
}

void RandomEngine::addHistory (uint32_t seed, const juce::String& label)
{
    Entry e;
    e.seed = seed;
    e.label = label;
    e.state = proc.captureState (true);
    history.push_back (std::move (e));
    while (history.size() > 100)
    {
        auto it = std::find_if (history.begin(), history.end(), [] (const Entry& x) { return ! x.favourite; });
        if (it == history.end()) break;
        history.erase (it);
    }
    cursor = (int) history.size() - 1;
}

void RandomEngine::recall (int index)
{
    if (index < 0 || index >= (int) history.size()) return;
    proc.pushUndo();
    cursor = index;
    seedShown = history[(size_t) index].seed;
    proc.restoreState (history[(size_t) index].state, true);
    if (onChange) onChange();
}

void RandomEngine::toggleFavourite()
{
    if (cursor < 0 || cursor >= (int) history.size()) return;
    history[(size_t) cursor].favourite = ! history[(size_t) cursor].favourite;
    if (onChange) onChange();
}

juce::ValueTree RandomEngine::toTree() const
{
    juce::ValueTree t ("RANDOM");
    t.setProperty ("amount", settings.amount, nullptr);
    t.setProperty ("sync", settings.sync, nullptr);
    t.setProperty ("glide", settings.glideMs, nullptr);
    t.setProperty ("learn", settings.learn, nullptr);
    t.setProperty ("mutate", settings.mutate, nullptr);
    t.setProperty ("mutateBars", settings.mutateBars, nullptr);
    t.setProperty ("mutateAmount", settings.mutateAmount, nullptr);
    t.setProperty ("drift", settings.driftOn, nullptr);
    t.setProperty ("driftAmount", settings.driftAmount, nullptr);
    t.setProperty ("driftSpeed", settings.driftSpeed, nullptr);
    t.setProperty ("order", settings.doOrder, nullptr);
    t.setProperty ("seq", settings.doSeq, nullptr);
    return t;
}

void RandomEngine::fromTree (const juce::ValueTree& t)
{
    if (! t.isValid()) return;
    settings.amount = t.getProperty ("amount", 0.5f);
    settings.sync = t.getProperty ("sync", 0);
    settings.glideMs = t.getProperty ("glide", 0.0f);
    settings.learn = t.getProperty ("learn", false);
    settings.mutate = t.getProperty ("mutate", false);
    settings.mutateBars = t.getProperty ("mutateBars", 1);
    settings.mutateAmount = t.getProperty ("mutateAmount", 0.2f);
    settings.driftOn = t.getProperty ("drift", false);
    settings.driftAmount = t.getProperty ("driftAmount", 0.0f);
    settings.driftSpeed = t.getProperty ("driftSpeed", 0.3f);
    settings.doOrder = t.getProperty ("order", true);
    settings.doSeq = t.getProperty ("seq", false);
}
} // namespace idg
