#pragma once
#include "../Core/JuceIncludes.h"

class IDGemsProcessor;

namespace idg
{
/** Gestion des presets façon Vital : catégories, tags, auteur, favoris, recherche. */
class PresetManager
{
public:
    struct Info
    {
        juce::String name, author, category, tags;
        juce::File file;          // vide pour les presets d'usine
        int factoryIndex = -1;
        bool favourite = false;
        juce::String key() const { return factoryIndex >= 0 ? "factory:" + name : file.getFullPathName(); }
    };

    explicit PresetManager (IDGemsProcessor& p);

    static juce::File folder();
    static const juce::StringArray& categories();

    void rescan();
    const std::vector<Info>& getAll() const { return presets; }
    std::vector<int> filter (const juce::String& search, const juce::String& category, bool favouritesOnly) const;

    bool load (int index);
    bool save (const juce::String& name, const juce::String& author, const juce::String& category, const juce::String& tags);
    void toggleFavourite (int index);
    void next (int delta);

    juce::String currentName() const { return current; }
    int currentIndex() const { return currentIdx; }
    std::function<void()> onChange;

private:
    void loadFavourites();
    void saveFavourites();
    juce::ValueTree buildFactory (int index, juce::String& name, juce::String& category, juce::String& tags);

    IDGemsProcessor& proc;
    std::vector<Info> presets;
    juce::StringArray favouriteKeys;
    juce::String current { "Init" };
    int currentIdx = -1;
};
} // namespace idg
