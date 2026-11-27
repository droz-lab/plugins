#pragma once
#include "../Core/ParamDefs.h"

class IDGemsProcessor;

namespace idg
{
/** Système de random (thread interface). */
class RandomEngine
{
public:
    enum class Scope { All, Family, Effect, Sequencers };

    struct Settings
    {
        float amount = 0.5f;      // dosage : léger -> chaos total
        int sync = 0;             // 0 immédiat, 1 prochain temps, 2 prochaine mesure
        float glideMs = 0.0f;     // 0 = coupure sèche
        bool learn = false;       // penche vers les favoris
        bool mutate = false;
        int mutateBars = 2;       // index : 1, 2, 4, 8, 16 mesures
        float mutateAmount = 0.2f;
        float driftAmount = 0.0f; // mode Drift (0 = désactivé)
        float driftSpeed = 0.3f;
        bool driftOn = false;
        bool doOrder = true;      // le random général peut réordonner la chaîne
        bool doSeq = false;       // le random touche aux séquenceurs (désactivé par défaut)
    };

    struct Entry
    {
        uint32_t seed = 0;
        juce::String label;
        bool favourite = false;
        juce::ValueTree state;
    };

    explicit RandomEngine (IDGemsProcessor& p) : proc (p) {}

    /** Demande un random (respecte la synchro au temps / à la mesure). */
    void request (Scope scope, int target, uint32_t seed = 0);
    void timerTick();           // ~30 Hz
    bool hasPending() const { return pending.active; }

    // Historique
    const std::vector<Entry>& getHistory() const { return history; }
    int getCursor() const { return cursor; }
    void recall (int index);
    void back() { if (cursor > 0) recall (cursor - 1); }
    void forward() { if (cursor + 1 < (int) history.size()) recall (cursor + 1); }
    void toggleFavourite();
    bool currentIsFavourite() const { return cursor >= 0 && cursor < (int) history.size() && history[(size_t) cursor].favourite; }
    uint32_t lastSeed() const { return seedShown; }

    Settings settings;
    std::function<void()> onChange;

    juce::ValueTree toTree() const;
    void fromTree (const juce::ValueTree&);

    static juce::String scopeLabel (Scope s, int target);

private:
public:
    /** Exécute immédiatement (utilisé aussi par les tests). */
    void perform (Scope scope, int target, uint32_t seed, const juce::String& label);
    void perform (Scope scope, int target, uint32_t seed, const juce::String& label, float amount);
private:
    void addHistory (uint32_t seed, const juce::String& label);
    float pickTarget (int paramIndex, juce::Random& r);

    IDGemsProcessor& proc;
    std::vector<Entry> history;
    int cursor = -1;
    uint32_t seedShown = 0;

    struct Pending { bool active = false; Scope scope = Scope::All; int target = 0; uint32_t seed = 0; int64_t counterAtRequest = 0; } pending;

    struct Glide { bool active = false; double start = 0, duration = 0; std::vector<std::pair<int, std::pair<float, float>>> moves; } glide;
    int64_t lastMutateBar = -1;
    juce::String glideLabel;
    uint32_t glideSeed = 0;
};
} // namespace idg
