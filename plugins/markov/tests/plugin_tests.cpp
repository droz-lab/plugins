// Whole-plugin tests (host sync, note-offs, reproducibility, learning). Run by ctest as markov_plugin.
#include "PluginProcessor.h"

#include <cstdio>
#include <map>
#include <set>
#include <vector>

namespace
{
int failures = 0;

void check (bool cond, const char* name, const juce::String& detail = {})
{
    std::printf ("  %s  %s %s\n", cond ? "OK   " : "ECHEC", name, detail.toRawUTF8());
    if (! cond)
        ++failures;
}

struct Head final : juce::AudioPlayHead
{
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (bpm);
        info.setTimeSignature (TimeSignature { 4, 4 });
        info.setPpqPosition (ppq);
        info.setIsPlaying (playing);
        return info;
    }
    double bpm = 120.0, ppq = 0.0;
    bool playing = true;
};

struct Event
{
    int64_t sample;
    int channel, note, velocity;
    bool on;
    bool operator== (const Event& o) const
    {
        return sample == o.sample && channel == o.channel && note == o.note && velocity == o.velocity && on == o.on;
    }
};

/** Drives the plugin like a host: transport, blocks, collects the MIDI it produces. */
struct Rig
{
    explicit Rig (int block = 512, double sr = 48000.0) : blockSize (block), sampleRate (sr)
    {
        plugin.reset (static_cast<PluginProcessor*> (createPluginFilter()));
        plugin->setPlayHead (&head);
        plugin->setRateAndBufferSizeDetails (sr, block);
        plugin->prepareToPlay (sr, block);
    }

    void set (const juce::String& id, float plainValue)
    {
        auto* p = plugin->apvts.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
    }

    /** Plays `samples` from the current transport position (advancing it if playing). */
    void play (int64_t samples, int block = 0, const juce::MidiBuffer* input = nullptr)
    {
        if (block <= 0)
            block = blockSize;
        juce::AudioBuffer<float> audio (2, block);
        juce::MidiBuffer midi;
        for (int64_t done = 0; done < samples;)
        {
            const int n = static_cast<int> (std::min<int64_t> (block, samples - done));
            audio.setSize (2, n, false, false, true);
            midi.clear();
            if (input != nullptr && done == 0)
                midi = *input;
            plugin->processBlock (audio, midi);
            collect (midi, n);
            if (head.playing)
                head.ppq += n / (sampleRate * 60.0 / head.bpm);
            done += n;
        }
    }

    void bypassedBlock()
    {
        juce::AudioBuffer<float> audio (2, blockSize);
        juce::MidiBuffer midi;
        plugin->processBlockBypassed (audio, midi);
        collect (midi, blockSize);
    }

    void collect (const juce::MidiBuffer& midi, int n)
    {
        int last = -1;
        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            if (meta.samplePosition < last || meta.samplePosition < 0 || meta.samplePosition >= n)
                unsorted = true;
            last = meta.samplePosition;
            if (! m.isNoteOnOrOff())
                continue;
            const Event e { position + meta.samplePosition, m.getChannel(), m.getNoteNumber(), m.getVelocity(), m.isNoteOn() };
            events.push_back (e);
            const auto key = std::make_pair (e.channel, e.note);
            if (e.on)
            {
                if (active.count (key))
                    doubleOn = true;
                active.insert (key);
            }
            else if (active.erase (key) == 0)
                strayOff = true;
        }
        position += n;
    }

    std::vector<Event> onsBetween (int64_t from, int64_t to) const
    {
        std::vector<Event> out;
        for (const auto& e : events)
            if (e.on && e.sample >= from && e.sample < to)
                out.push_back ({ e.sample - from, e.channel, e.note, e.velocity, true });
        return out;
    }

    int blockSize;
    double sampleRate;
    Head head;
    std::unique_ptr<PluginProcessor> plugin;
    std::vector<Event> events;
    std::set<std::pair<int, int>> active;
    int64_t position = 0;
    bool unsorted = false, doubleOn = false, strayOff = false;
};

int countOns (const std::vector<Event>& ev)
{
    int n = 0;
    for (const auto& e : ev)
        n += e.on;
    return n;
}

void testStop()
{
    Rig rig;
    rig.play (48000 * 3);
    const int ons = countOns (rig.events);
    rig.head.playing = false;
    rig.play (512 * 4);
    check (ons > 20 && rig.active.empty() && ! rig.doubleOn && ! rig.strayOff,
           "note-off a l'arret", juce::String (ons) + " notes jouees, " + juce::String ((int) rig.active.size()) + " encore actives");
    check (! rig.unsorted, "evenements tries et dans le bloc");

    // Restart from the beginning after a stop: same notes as the first time.
    const auto first = rig.onsBetween (0, 48000);
    rig.head.ppq = 0.0;
    rig.head.playing = true;
    const auto restartAt = rig.position;
    rig.play (48000);
    check (rig.onsBetween (restartAt, restartAt + 48000) == first, "relecture depuis le debut : meme sequence");
}

void testLoop()
{
    // Host loop of 2 bars (8 beats) played 3 times, with an odd block size.
    Rig rig (300);
    const double loopEnd = 8.0;
    const double samplesPerBeat = 48000.0 * 60.0 / 120.0;
    std::vector<int64_t> starts;
    for (int pass = 0; pass < 3; ++pass)
    {
        rig.head.ppq = 0.0;
        starts.push_back (rig.position);
        rig.play (static_cast<int64_t> (loopEnd * samplesPerBeat));
    }
    const auto len = static_cast<int64_t> (loopEnd * samplesPerBeat);
    const auto a = rig.onsBetween (starts[0], starts[0] + len);
    const auto b = rig.onsBetween (starts[1], starts[1] + len);
    const auto c = rig.onsBetween (starts[2], starts[2] + len);
    rig.head.playing = false;
    rig.play (300);
    check (a == b && b == c && ! a.empty(), "boucle : chaque passage rejoue la meme sequence", juce::String ((int) a.size()) + " notes par passage");
    check (rig.active.empty() && ! rig.doubleOn && ! rig.strayOff, "boucle : aucune note bloquee");
}

void testReproducibility()
{
    auto render = [] (int block, int seed) {
        Rig rig (block);
        rig.set ("graine", (float) seed);
        rig.set ("rythme", 1.0f);
        rig.set ("p6_actif", 1.0f);
        rig.play (48000 * 4);
        return rig.events;
    };
    const auto a = render (512, 77), b = render (512, 77), c = render (64, 77), d = render (512, 78);
    check (a == b, "meme graine, memes reglages : memes evenements MIDI");
    check (a == c, "taille de bloc sans effet (64 / 512), a l'echantillon pres");
    check (! (a == d), "autre graine : autre sequence");
}

void testSampleAccuracy()
{
    // 120 BPM, 48 kHz, 1/16: one step = 6000 samples. Every note-on starts exactly on a step.
    Rig rig (480);
    for (int t = 1; t < 8; ++t)
        rig.set (PluginProcessor::trackParam (t, "actif"), 0.0f);
    rig.play (48000 * 4);
    bool onGrid = true;
    for (const auto& e : rig.events)
        if (e.on)
            onGrid = onGrid && e.sample % 6000 == 0 && e.channel == 1;
    // Gate 50 %: each note-off 3000 samples after its note-on.
    bool gateOk = true;
    std::map<int, int64_t> lastOn;
    for (const auto& e : rig.events)
        if (e.on)
            lastOn[e.note] = e.sample;
        else
            gateOk = gateOk && e.sample - lastOn[e.note] == 3000;
    for (const auto& e : rig.events)
        if (e.on && e.sample % 6000 != 0)
            std::printf ("    hors grille : echantillon %lld canal %d\n", (long long) e.sample, e.channel);
    check (onGrid && countOns (rig.events) > 10, "notes placees a l'echantillon pres, piste 1 sur canal 1");
    check (gateOk, "duree de note respectee (50 % du pas)");
}

void testTracksAndChannels()
{
    Rig rig;
    for (int t = 0; t < 8; ++t)
        rig.set (PluginProcessor::trackParam (t, "actif"), 1.0f);
    rig.play (48000 * 4);
    std::set<int> channels;
    bool inRange = true;
    for (const auto& e : rig.events)
    {
        channels.insert (e.channel);
        const int t = e.channel - 1;
        const auto lo = rig.plugin->apvts.getRawParameterValue (PluginProcessor::trackParam (t, "min"))->load();
        const auto hi = rig.plugin->apvts.getRawParameterValue (PluginProcessor::trackParam (t, "max"))->load();
        inRange = inRange && e.note >= lo && e.note <= hi;
    }
    check (channels.size() == 8 && *channels.begin() == 1 && *channels.rbegin() == 8, "8 pistes sur les canaux 1 a 8");
    check (inRange, "chaque piste reste dans sa tessiture");

    // Switching a track off while it plays: its note stops at once.
    for (int i = 0; i < 40; ++i)
    {
        rig.play (64, 64);
        bool trackOneOn = false;
        for (const auto& k : rig.active)
            trackOneOn = trackOneOn || k.first == 1;
        if (trackOneOn)
            break;
    }
    rig.set ("p1_actif", 0.0f);
    rig.play (64, 64);
    bool stillOn = false;
    for (const auto& k : rig.active)
        stillOn = stillOn || k.first == 1;
    check (! stillOn, "piste desactivee : sa note s'arrete");

    // Changing division and tempo while playing: no stuck note.
    rig.set ("p2_division", 1.0f);
    rig.head.bpm = 87.0;
    rig.play (48000 * 2);
    rig.head.bpm = 171.0;
    rig.play (48000 * 2);
    rig.bypassedBlock();
    check (rig.active.empty() && ! rig.doubleOn && ! rig.strayOff, "changements de tempo, de division, contournement : aucune note bloquee");
}

void testNoTransport()
{
    Rig rig;
    rig.plugin->setPlayHead (nullptr);
    rig.play (48000);
    check (rig.events.empty(), "sans tete de lecture : silence MIDI (jamais de minuterie systeme)");
    rig.plugin->setPlayHead (&rig.head);
    rig.head.ppq = -2.0; // pre-roll
    rig.play (24000);
    check (rig.events.empty(), "pre-roll (position negative) : rien avant le debut");
}

void testLearning()
{
    Rig rig;
    rig.set ("apprendre", 1.0f);
    rig.plugin->selectedTrack = 2;
    juce::MidiBuffer input;
    // C major: Do Ré Mi Do Ré Mi Do Mi  -> 0->1 x2, 0->2 x1, 1->2 x2, 2->0 x2
    int at = 0;
    for (int note : { 60, 62, 64, 60, 62, 64, 60, 64 })
        input.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), at++);
    rig.head.playing = false;
    rig.play (512, 512, &input);
    rig.plugin->applyLearning();
    const auto t = rig.plugin->getTrack (2);
    check (t.edited && std::abs (t.w[0][1] - 2.0f / 3.0f) < 1e-5f && std::abs (t.w[0][2] - 1.0f / 3.0f) < 1e-5f
               && std::abs (t.w[2][0] - 1.0f) < 1e-5f,
           "apprentissage par le MIDI entrant", juce::String (rig.plugin->learnedNotes.load()) + " notes apprises");
    check (rig.events.empty(), "le MIDI entrant n'est pas renvoye");

    rig.plugin->resetLearning();
    rig.play (512);
    check (! rig.plugin->getTrack (2).edited && rig.plugin->learnedNotes.load() == 0, "reinitialisation de l'apprentissage");
}

void testState()
{
    Rig a;
    a.plugin->editTrack (4, [] (markov::TrackData& d) {
        markov::materialise (d, 7);
        d.w[1][3] = 0.9f;
        d.prob[5] = 0.33f;
        d.velocity[7] = 12;
        d.spread[7] = 40;
    });
    juce::MemoryBlock saved;
    a.plugin->getStateInformation (saved);
    Rig b;
    b.plugin->setStateInformation (saved.getData(), (int) saved.getSize());
    const auto d = b.plugin->getTrack (4);
    check (d.edited && std::abs (d.w[1][3] - 0.9f) < 1e-4f && std::abs (d.prob[5] - 0.33f) < 1e-4f && d.velocity[7] == 12 && d.spread[7] == 40,
           "matrice et pas gardes dans l'etat");
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    std::printf ("Markov : tests du plugin\n");
    testStop();
    testLoop();
    testReproducibility();
    testSampleAccuracy();
    testTracksAndChannels();
    testNoTransport();
    testLearning();
    testState();
    std::printf ("%d echec(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
