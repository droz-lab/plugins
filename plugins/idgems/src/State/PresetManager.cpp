#include "PresetManager.h"
#include "../PluginProcessor.h"

namespace idg
{
namespace
{
struct Factory { juce::String name, category, tags; };
const Factory factoryList[] {
    { "Init", "Factory", "init" },
    { "Stutter Pump", "Rythmique", "stutter, filtre, pompe" },
    { IDG_T ("CD Brisé"), "Glitch", "corruption, bitcrush, shuffle" },
    { IDG_T ("Poussière Spatiale"), "Texture", "freeze, reverb, ambiance" },
    { IDG_T ("Comb Métal"), "Destruction", IDG_T ("comb, gamme, métal") },
    { IDG_T ("Bande Fatiguée"), "Texture", "tape, wow, lo-fi" },
    { "Choeur Pixel", IDG_T ("Expérimental"), "pixel sort, formant" },
    { "Carnage", "Glitch", IDG_T ("chaos, séquenceurs, random") },
    { "Gate Trance", "Rythmique", "gate, delay, phaser" },
};
}

PresetManager::PresetManager (IDGemsProcessor& p) : proc (p)
{
    loadFavourites();
    rescan();
}

juce::File PresetManager::folder()
{
    auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("IDGems").getChildFile ("Presets");
    f.createDirectory();
    return f;
}

const juce::StringArray& PresetManager::categories()
{
    static const juce::StringArray c { "Glitch", "Destruction", "Texture", "Rythmique", "Espace", IDG_T ("Expérimental"), "Utilisateur" };
    return c;
}

void PresetManager::loadFavourites()
{
    auto f = folder().getChildFile ("favourites.txt");
    favouriteKeys = juce::StringArray::fromLines (f.loadFileAsString());
    favouriteKeys.removeEmptyStrings();
}

void PresetManager::saveFavourites()
{
    folder().getChildFile ("favourites.txt").replaceWithText (favouriteKeys.joinIntoString ("\n"));
}

void PresetManager::rescan()
{
    presets.clear();
    int idx = 0;
    for (const auto& f : factoryList)
    {
        Info i;
        i.name = f.name; i.author = "IDGems"; i.category = f.category; i.tags = f.tags; i.factoryIndex = idx++;
        presets.push_back (i);
    }
    for (const auto& file : folder().findChildFiles (juce::File::findFiles, true, "*.idgems"))
    {
        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr) continue;
        Info i;
        i.file = file;
        i.name = xml->getStringAttribute ("presetName", file.getFileNameWithoutExtension());
        i.author = xml->getStringAttribute ("author");
        i.category = xml->getStringAttribute ("category", "Utilisateur");
        i.tags = xml->getStringAttribute ("tags");
        presets.push_back (i);
    }
    for (auto& p : presets) p.favourite = favouriteKeys.contains (p.key());
    std::stable_sort (presets.begin() + (int) std::size (factoryList), presets.end(),
                      [] (const Info& a, const Info& b) { return a.name.compareIgnoreCase (b.name) < 0; });
    if (onChange) onChange();
}

std::vector<int> PresetManager::filter (const juce::String& search, const juce::String& category, bool favouritesOnly) const
{
    std::vector<int> out;
    for (int i = 0; i < (int) presets.size(); ++i)
    {
        const auto& p = presets[(size_t) i];
        if (favouritesOnly && ! p.favourite) continue;
        if (category.isNotEmpty() && p.category != category) continue;
        if (search.isNotEmpty())
        {
            auto hay = (p.name + " " + p.author + " " + p.tags + " " + p.category).toLowerCase();
            bool all = true;
            for (auto& w : juce::StringArray::fromTokens (search.toLowerCase(), " ", ""))
                if (w.isNotEmpty() && ! hay.contains (w)) { all = false; break; }
            if (! all) continue;
        }
        out.push_back (i);
    }
    return out;
}

bool PresetManager::load (int index)
{
    if (index < 0 || index >= (int) presets.size()) return false;
    const auto& info = presets[(size_t) index];
    proc.pushUndo();
    if (info.factoryIndex >= 0)
    {
        proc.resetToInit();
        auto set = [this] (int fx, const char* key, float v) { proc.setValue (fxParamId (fx, key), v); };
        auto on = [&] (int fx, float mix = -1.0f) { set (fx, "on", 1.0f); if (mix >= 0.0f) set (fx, "mix", mix); };
        auto& reg = registry();
        juce::ignoreUnused (reg);
        switch (info.factoryIndex)
        {
            case 1:
                on (FxStutter); set (FxStutter, "div", (float) divisionIndex ("1/16")); set (FxStutter, "retrig", 0);
                proc.seqData[FxStutter].enabled = true;
                for (int k = 0; k < MaxSteps; ++k) { proc.seqData[FxStutter].on[k] = (k % 8 == 6) || (k % 16 == 14); proc.seqData[FxStutter].ratchet[k] = (k % 16 == 14) ? 3 : 1; }
                on (FxFilter); set (FxFilter, "cutoff", 1200.0f); set (FxFilter, "reso", 0.45f);
                proc.modShared.addConnection (SrcLfo0, proc.indexOf (fxParamId (FxFilter, "cutoff")), 0.35f, true);
                proc.setValue (lfoId (0, "div"), (float) divisionIndex ("1/2"));
                on (FxReverb, 0.18f);
                break;
            case 2:
                on (FxCorruption); set (FxCorruption, "type", 3); set (FxCorruption, "rate", 5.0f);
                on (FxBitcrush, 0.6f); set (FxBitcrush, "bits", 10.0f); set (FxBitcrush, "rate", 3.0f);
                on (FxShuffle); set (FxShuffle, "chance", 0.35f);
                break;
            case 3:
                on (FxFreeze, 0.7f); set (FxFreeze, "pitch", 12.0f); set (FxFreeze, "spray", 0.6f); set (FxFreeze, "density", 6.0f);
                on (FxReverb, 0.5f); set (FxReverb, "size", 0.9f); set (FxReverb, "decay", 8.0f);
                on (FxChorus, 0.4f);
                break;
            case 4:
                proc.setValue (G::scaleOn, 1.0f); proc.setValue (G::scaleType, 1.0f);
                on (FxComb, 0.7f); set (FxComb, "freq", 110.0f); set (FxComb, "chord", 3.0f); set (FxComb, "fb", 0.9f);
                on (FxFreqShift, 0.4f); set (FxFreqShift, "shift", 30.0f);
                on (FxDistortion); set (FxDistortion, "curve", 2.0f); set (FxDistortion, "drive", 18.0f);
                break;
            case 5:
                on (FxTapeStop); set (FxTapeStop, "auto", 2.0f);
                on (FxDelay, 0.3f); set (FxDelay, "wow", 0.6f);
                on (FxBitcrush, 0.5f); set (FxBitcrush, "bits", 12.0f);
                on (FxFilter); set (FxFilter, "cutoff", 6000.0f);
                break;
            case 6:
                on (FxPixelSort); set (FxPixelSort, "thresh", 0.3f);
                on (FxFormant); set (FxFormant, "shift", 5.0f);
                on (FxReverb, 0.35f);
                break;
            case 7:
                proc.setValue (G::chaos, 0.85f);
                proc.random.perform (RandomEngine::Scope::All, 0, 666, "Carnage", 1.0f);
                break;
            case 8:
                on (FxGate); set (FxGate, "pattern", 2.0f);
                on (FxDelay, 0.35f); set (FxDelay, "pingpong", 1.0f);
                on (FxPhaser, 0.6f);
                break;
            default: break;
        }
    }
    else
    {
        auto xml = juce::XmlDocument::parse (info.file);
        if (xml == nullptr) return false;
        auto tree = juce::ValueTree::fromXml (*xml);
        proc.restoreState (tree, true);
    }
    current = info.name;
    currentIdx = index;
    if (onChange) onChange();
    return true;
}

bool PresetManager::save (const juce::String& name, const juce::String& author, const juce::String& category, const juce::String& tags)
{
    auto state = proc.captureState (true);
    auto xml = state.createXml();
    if (xml == nullptr) return false;
    xml->setAttribute ("presetName", name);
    xml->setAttribute ("author", author);
    xml->setAttribute ("category", category);
    xml->setAttribute ("tags", tags);
    auto file = folder().getChildFile (juce::File::createLegalFileName (name) + ".idgems");
    if (! xml->writeTo (file)) return false;
    current = name;
    rescan();
    for (int i = 0; i < (int) presets.size(); ++i) if (presets[(size_t) i].file == file) currentIdx = i;
    if (onChange) onChange();
    return true;
}

void PresetManager::toggleFavourite (int index)
{
    if (index < 0 || index >= (int) presets.size()) return;
    auto& p = presets[(size_t) index];
    p.favourite = ! p.favourite;
    if (p.favourite) favouriteKeys.addIfNotAlreadyThere (p.key());
    else favouriteKeys.removeString (p.key());
    saveFavourites();
    if (onChange) onChange();
}

void PresetManager::next (int delta)
{
    if (presets.empty()) return;
    int i = currentIdx < 0 ? 0 : (currentIdx + delta + (int) presets.size()) % (int) presets.size();
    load (i);
}

juce::ValueTree PresetManager::buildFactory (int, juce::String&, juce::String&, juce::String&) { return {}; }
} // namespace idg
