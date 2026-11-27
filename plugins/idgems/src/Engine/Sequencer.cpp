#include "Sequencer.h"

namespace idg
{
juce::ValueTree SeqData::toTree() const
{
    juce::ValueTree t ("SEQ");
    t.setProperty ("enabled", enabled.load(), nullptr);
    t.setProperty ("locked", locked.load(), nullptr);
    t.setProperty ("length", length.load(), nullptr);
    t.setProperty ("rate", rate.load(), nullptr);
    t.setProperty ("swing", swing.load(), nullptr);
    juce::String steps;
    for (int i = 0; i < MaxSteps; ++i)
        steps << (on[i].load() ? "1" : "0") << ":" << juce::String (prob[i].load(), 3) << ":" << ratchet[i].load() << ";";
    t.setProperty ("steps", steps, nullptr);
    return t;
}

void SeqData::fromTree (const juce::ValueTree& t)
{
    if (! t.isValid()) return;
    enabled = (bool) t.getProperty ("enabled", false);
    locked = (bool) t.getProperty ("locked", false);
    length = juce::jlimit (1, MaxSteps, (int) t.getProperty ("length", 16));
    rate = juce::jlimit (0, 5, (int) t.getProperty ("rate", 3));
    swing = juce::jlimit (0.0f, 1.0f, (float) t.getProperty ("swing", 0.0f));
    auto items = juce::StringArray::fromTokens (t.getProperty ("steps").toString(), ";", "");
    for (int i = 0; i < MaxSteps && i < items.size(); ++i)
    {
        auto f = juce::StringArray::fromTokens (items[i], ":", "");
        if (f.size() < 3) continue;
        on[i] = f[0] == "1";
        prob[i] = juce::jlimit (0.0f, 1.0f, f[1].getFloatValue());
        ratchet[i] = juce::jlimit (1, 4, f[2].getIntValue());
    }
}

void SeqRuntime::tick (SeqData& d, double ppq, float chaos, dsp::Rng& rng, bool& gate, bool& trig)
{
    trig = false;
    const double stepLen = seqRateBeats (d.rate.load (std::memory_order_relaxed));
    const double swing = d.swing.load (std::memory_order_relaxed);
    const int length = juce::jlimit (1, MaxSteps, d.length.load (std::memory_order_relaxed));

    // swing : le pas impair est retardé
    const double t = ppq / (2.0 * stepLen);
    const auto pair = (int64_t) std::floor (t);
    const double f = t - (double) pair;
    const double split = 0.5 + swing * 0.25;
    int64_t step; double local;
    if (f < split) { step = pair * 2; local = f / split; }
    else { step = pair * 2 + 1; local = (f - split) / (1.0 - split); }

    const int idx = (int) (((step % length) + length) % length);
    if (step != lastStep)
    {
        lastStep = step;
        lastSub = -1;
        float p = d.prob[idx].load (std::memory_order_relaxed);
        bool on = d.on[idx].load (std::memory_order_relaxed);
        // la macro Chaos densifie ou raréfie les pas
        float chance = on ? juce::jlimit (0.0f, 1.0f, p + (chaos - 0.5f) * 0.6f)
                          : juce::jmax (0.0f, (chaos - 0.6f) * 0.5f);
        fires = rng.uni() < chance;
        ratchets = juce::jlimit (1, 4, d.ratchet[idx].load (std::memory_order_relaxed));
        d.current.store (idx, std::memory_order_relaxed);
    }

    const int sub = juce::jmin (ratchets - 1, (int) (local * ratchets));
    const double subLocal = local * ratchets - sub;
    if (fires && sub != lastSub) { trig = true; lastSub = sub; }
    gate = fires && (ratchets == 1 || subLocal < 0.6);
}
} // namespace idg
