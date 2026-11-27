// Sauvegarde / restauration de l'état complet du plugin.
#include "../PluginProcessor.h"

using namespace idg;

juce::ValueTree IDGemsProcessor::captureState (bool forPreset) const
{
    juce::ValueTree root ("IDGEMS");
    root.setProperty ("version", 1, nullptr);
    root.appendChild (const_cast<juce::AudioProcessorValueTreeState&> (apvts).copyState(), nullptr);

    juce::ValueTree ex ("EXTRA");
    // ordre de la chaîne
    juce::String ord;
    for (int v : rackShared.getOrder()) ord << v << ",";
    ex.setProperty ("order", ord, nullptr);

    juce::String solo, trans, fxLock, famSolo;
    for (int i = 0; i < NumFx; ++i)
    {
        solo << (rackShared.solo[i].load() ? "1" : "0");
        trans << (rackShared.transTrig[i].load() ? "1" : "0");
        fxLock << (rackShared.fxLocked[i].load() ? "1" : "0");
    }
    for (int f = 0; f < NumFamilies; ++f) famSolo << (rackShared.famSolo[f].load() ? "1" : "0");
    ex.setProperty ("solo", solo, nullptr);
    ex.setProperty ("trans", trans, nullptr);
    ex.setProperty ("fxlock", fxLock, nullptr);
    ex.setProperty ("famsolo", famSolo, nullptr);
    ex.setProperty ("seqmaster", seqMaster.load(), nullptr);

    // cadenas et plages par paramètre
    juce::ValueTree pl ("PARAMLOCKS");
    const auto& reg = registry();
    for (int i = 0; i < (int) reg.all.size(); ++i)
    {
        const auto& p = bank[i];
        const auto& d = reg.all[(size_t) i];
        const bool customRange = std::abs (p.rmin.load() - d.rmin) > 1.0e-4f || std::abs (p.rmax.load() - d.rmax) > 1.0e-4f;
        if (! p.locked.load() && ! customRange) continue;
        juce::ValueTree c ("P");
        c.setProperty ("id", d.id, nullptr);
        c.setProperty ("lock", p.locked.load(), nullptr);
        c.setProperty ("min", p.rmin.load(), nullptr);
        c.setProperty ("max", p.rmax.load(), nullptr);
        pl.appendChild (c, nullptr);
    }
    ex.appendChild (pl, nullptr);

    // séquenceurs
    juce::ValueTree seqs ("SEQS");
    for (int i = 0; i < NumFx; ++i)
    {
        auto t = seqData[i].toTree();
        t.setProperty ("fx", i, nullptr);
        seqs.appendChild (t, nullptr);
    }
    ex.appendChild (seqs, nullptr);

    // modulations + formes LFO
    ex.appendChild (modShared.toTree(), nullptr);

    // snapshots + arrangement
    juce::ValueTree snaps ("SNAPSHOTS");
    snaps.setProperty ("morph", morphEnabled.load(), nullptr);
    for (int i = 0; i < NumSnapshots; ++i)
    {
        const auto& s = snapshots[i];
        juce::ValueTree t ("SNAP");
        t.setProperty ("index", i, nullptr);
        t.setProperty ("valid", s.valid, nullptr);
        t.setProperty ("x", s.x, nullptr);
        t.setProperty ("y", s.y, nullptr);
        if (s.valid)
        {
            juce::String vals;
            for (size_t k = 0; k < s.values.size(); ++k)
                if (snapshotIncludes (reg.all[k])) vals << reg.all[k].id << "=" << juce::String (s.values[k], 5) << ";";
            t.setProperty ("values", vals, nullptr);
        }
        snaps.appendChild (t, nullptr);
    }
    juce::String arr;
    for (auto& a : arrange) arr << a.snapshot << ":" << a.bars << ";";
    snaps.setProperty ("arrange", arr, nullptr);
    ex.appendChild (snaps, nullptr);

    // random
    ex.appendChild (random.toTree(), nullptr);

    // pads de performance
    juce::String pads;
    for (int v : perfPads) pads << v << ",";
    ex.setProperty ("pads", pads, nullptr);

    if (! forPreset)
    {
        juce::String midiMap;
        for (int cc = 0; cc < 128; ++cc)
            if (ccToParam[cc].load() >= 0) midiMap << cc << "=" << reg.all[(size_t) ccToParam[cc].load()].id << ";";
        ex.setProperty ("midimap", midiMap, nullptr);
        if (presets != nullptr) ex.setProperty ("preset", presets->currentName(), nullptr);
        if (editorScale.load() > 0.0f) ex.setProperty ("uiscale", editorScale.load(), nullptr);
        ex.setProperty ("ui_w", editorW.load(), nullptr);
        ex.setProperty ("ui_h", editorH.load(), nullptr);
    }

    root.appendChild (ex, nullptr);
    return root;
}

void IDGemsProcessor::restoreState (const juce::ValueTree& root, bool fromPreset)
{
    if (! root.hasType ("IDGEMS")) return;
    const auto& reg = registry();

    auto p = root.getChildWithName (apvts.state.getType());
    if (p.isValid())
    {
        // les paramètres absents gardent leur valeur par défaut
        for (int i = 0; i < (int) params.size(); ++i)
        {
            auto child = p.getChildWithProperty ("id", reg.all[(size_t) i].id);
            float v = child.isValid() ? (float) child.getProperty ("value") : params[(size_t) i]->convertFrom0to1 (params[(size_t) i]->getDefaultValue());
            setNorm (i, params[(size_t) i]->convertTo0to1 (v));
        }
    }

    auto ex = root.getChildWithName ("EXTRA");
    if (! ex.isValid()) return;

    {
        auto items = juce::StringArray::fromTokens (ex.getProperty ("order").toString(), ",", "");
        items.removeEmptyStrings();
        if (items.size() == NumFx)
        {
            std::array<int, NumFx> o {};
            for (int i = 0; i < NumFx; ++i) o[(size_t) i] = items[i].getIntValue();
            rackShared.setOrder (o);
        }
    }
    auto bits = [] (const juce::var& v, int i, bool def) { auto s = v.toString(); return i < s.length() ? s[i] == '1' : def; };
    for (int i = 0; i < NumFx; ++i)
    {
        rackShared.solo[i] = bits (ex.getProperty ("solo"), i, false);
        rackShared.transTrig[i] = bits (ex.getProperty ("trans"), i, i <= FxScratch);
        rackShared.fxLocked[i] = bits (ex.getProperty ("fxlock"), i, false);
    }
    for (int f = 0; f < NumFamilies; ++f) rackShared.famSolo[f] = bits (ex.getProperty ("famsolo"), f, false);
    seqMaster = (bool) ex.getProperty ("seqmaster", true);

    for (int i = 0; i < (int) reg.all.size(); ++i)
    {
        bank[i].locked = false;
        bank[i].rmin = reg.all[(size_t) i].rmin;
        bank[i].rmax = reg.all[(size_t) i].rmax;
    }
    for (auto c : ex.getChildWithName ("PARAMLOCKS"))
    {
        const int i = reg.find (c.getProperty ("id").toString());
        if (i < 0) continue;
        bank[i].locked = (bool) c.getProperty ("lock", false);
        bank[i].rmin = juce::jlimit (0.0f, 1.0f, (float) c.getProperty ("min", 0.0f));
        bank[i].rmax = juce::jlimit (0.0f, 1.0f, (float) c.getProperty ("max", 1.0f));
    }

    for (int i = 0; i < NumFx; ++i) { seqData[i].clear(); seqData[i].enabled = false; seqData[i].locked = false; }
    for (auto t : ex.getChildWithName ("SEQS"))
    {
        const int fx = t.getProperty ("fx", -1);
        if (fx >= 0 && fx < NumFx) seqData[fx].fromTree (t);
    }

    modShared.fromTree (ex.getChildWithName ("MODS"));

    auto snaps = ex.getChildWithName ("SNAPSHOTS");
    if (snaps.isValid())
    {
        morphEnabled = (bool) snaps.getProperty ("morph", false);
        for (auto t : snaps)
        {
            const int i = t.getProperty ("index", -1);
            if (i < 0 || i >= NumSnapshots) continue;
            auto& s = snapshots[i];
            s.valid = (bool) t.getProperty ("valid", false);
            s.x = (float) t.getProperty ("x", s.x);
            s.y = (float) t.getProperty ("y", s.y);
            s.values.clear();
            if (s.valid)
            {
                s.values.resize (reg.all.size());
                for (size_t k = 0; k < reg.all.size(); ++k) s.values[k] = getNorm ((int) k);
                for (auto& item : juce::StringArray::fromTokens (t.getProperty ("values").toString(), ";", ""))
                {
                    const int k = reg.find (item.upToFirstOccurrenceOf ("=", false, false));
                    if (k >= 0) s.values[(size_t) k] = item.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
                }
            }
        }
        auto arr = juce::StringArray::fromTokens (snaps.getProperty ("arrange").toString(), ";", "");
        for (int i = 0; i < NumArrangeSlots; ++i)
        {
            arrange[i] = {};
            if (i < arr.size() && arr[i].contains (":"))
            {
                arrange[i].snapshot = juce::jlimit (-1, NumSnapshots - 1, arr[i].upToFirstOccurrenceOf (":", false, false).getIntValue());
                arrange[i].bars = juce::jlimit (1, 64, arr[i].fromFirstOccurrenceOf (":", false, false).getIntValue());
            }
        }
    }

    random.fromTree (ex.getChildWithName ("RANDOM"));

    {
        auto pads = juce::StringArray::fromTokens (ex.getProperty ("pads").toString(), ",", "");
        for (int i = 0; i < 8 && i < pads.size(); ++i)
            if (pads[i].isNotEmpty()) perfPads[i] = juce::jlimit (0, NumFx - 1, pads[i].getIntValue());
    }

    if (! fromPreset)
    {
        if (ex.hasProperty ("uiscale")) editorScale = juce::jlimit (0.6f, 2.0f, (float) ex.getProperty ("uiscale"));
        editorW = (int) ex.getProperty ("ui_w", 0);
        editorH = (int) ex.getProperty ("ui_h", 0);
        for (auto& c : ccToParam) c = -1;
        for (auto& item : juce::StringArray::fromTokens (ex.getProperty ("midimap").toString(), ";", ""))
        {
            const int cc = item.upToFirstOccurrenceOf ("=", false, false).getIntValue();
            const int k = reg.find (item.fromFirstOccurrenceOf ("=", false, false));
            if (cc >= 0 && cc < 128 && k >= 0 && item.contains ("=")) ccToParam[cc] = k;
        }
    }

    if (onStateRestored) onStateRestored();
}

void IDGemsProcessor::resetToInit()
{
    for (int i = 0; i < (int) params.size(); ++i) setNorm (i, params[(size_t) i]->getDefaultValue());
    std::array<int, NumFx> o {};
    for (int i = 0; i < NumFx; ++i)
    {
        o[(size_t) i] = i;
        rackShared.solo[i] = false;
        rackShared.fxLocked[i] = false;
        rackShared.transTrig[i] = i <= FxScratch;
        seqData[i].clear();
        seqData[i].enabled = false;
        seqData[i].locked = false;
    }
    rackShared.setOrder (o);
    for (auto& f : rackShared.famSolo) f = false;
    const auto& reg = registry();
    for (int i = 0; i < (int) reg.all.size(); ++i)
    {
        bank[i].locked = false;
        bank[i].rmin = reg.all[(size_t) i].rmin;
        bank[i].rmax = reg.all[(size_t) i].rmax;
    }
    modShared.clearAll();
    for (int i = 0; i < NumLfos; ++i) modShared.setShape (i, LfoShape::preset (0));
    for (int i = 0; i < NumSnapshots; ++i) clearSnapshot (i);
    for (auto& a : arrange) a = {};
    morphEnabled = false;
    if (onStateRestored) onStateRestored();
}

void IDGemsProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = captureState().createXml()) copyXmlToBinary (*xml, destData);
}

void IDGemsProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (tree.isValid()) restoreState (tree);
    }
}
