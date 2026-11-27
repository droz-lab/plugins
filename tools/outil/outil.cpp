// Outil commun a tous les plugins du depot. Compile une fois par plugin (<cible>_outil),
// il fait tourner le plugin sans DAW :
//
//   <cible>_outil test                      verifications communes (voir plus bas)
//   <cible>_outil params                    liste des parametres
//   <cible>_outil rendu in.wav out.wav      passe un fichier dans le plugin
//         [--param id=0.5 ...] [--bloc 512] [--bpm 140]
//   <cible>_outil rendu - out.wav --notes 57,60,64 --duree 4     (instruments)
//   <cible>_outil capture ui.png [--echelle 2]                   image de l'interface
//
// Ce fichier est partage : toute modification passe par une PR relue, elle touche tous les plugins.

#include <juce_audio_utils/juce_audio_utils.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <random>
#include <string>
#include <vector>


namespace
{
//==============================================================================
// Audio en memoire + WAV minimal (lecture PCM 16/24/32 et float 32, ecriture float 32)
struct Audio
{
    double sampleRate = 48000.0;
    std::vector<std::vector<float>> ch;

    int numSamples() const { return ch.empty() ? 0 : static_cast<int> (ch[0].size()); }
    int numChannels() const { return static_cast<int> (ch.size()); }
};

Audio makeAudio (int channels, int samples, double sr)
{
    Audio a;
    a.sampleRate = sr;
    a.ch.assign (static_cast<size_t> (channels), std::vector<float> (static_cast<size_t> (samples), 0.0f));
    return a;
}

template <typename T>
T readLE (const uint8_t* p)
{
    T v {};
    std::memcpy (&v, p, sizeof (T));
    return v;
}

bool readWav (const std::string& path, Audio& out, std::string& err)
{
    std::ifstream f (path, std::ios::binary);
    if (! f)
    {
        err = "fichier introuvable : " + path;
        return false;
    }
    std::vector<uint8_t> d ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char>());
    if (d.size() < 12 || std::memcmp (d.data(), "RIFF", 4) != 0 || std::memcmp (d.data() + 8, "WAVE", 4) != 0)
    {
        err = "pas un fichier WAV (RIFF/WAVE) : " + path;
        return false;
    }

    int format = 0, channels = 0, bits = 0;
    uint32_t rate = 0;
    const uint8_t* data = nullptr;
    size_t dataSize = 0;

    for (size_t pos = 12; pos + 8 <= d.size();)
    {
        const auto size = static_cast<size_t> (readLE<uint32_t> (d.data() + pos + 4));
        const uint8_t* body = d.data() + pos + 8;
        const size_t avail = std::min (size, d.size() - pos - 8);
        if (std::memcmp (d.data() + pos, "fmt ", 4) == 0 && avail >= 16)
        {
            format = readLE<uint16_t> (body);
            channels = readLE<uint16_t> (body + 2);
            rate = readLE<uint32_t> (body + 4);
            bits = readLE<uint16_t> (body + 14);
            if (format == 0xFFFE && avail >= 26) // WAVE_FORMAT_EXTENSIBLE : le vrai format est plus loin
                format = readLE<uint16_t> (body + 24);
        }
        else if (std::memcmp (d.data() + pos, "data", 4) == 0)
        {
            data = body;
            dataSize = avail;
        }
        pos += 8 + size + (size & 1);
    }

    const bool pcm = format == 1 && (bits == 16 || bits == 24 || bits == 32);
    const bool flt = format == 3 && bits == 32;
    if (data == nullptr || channels < 1 || rate == 0 || ! (pcm || flt))
    {
        err = "WAV non pris en charge (PCM 16/24/32 ou float 32 attendus) : " + path;
        return false;
    }

    const size_t bytes = static_cast<size_t> (bits / 8);
    const size_t frames = dataSize / (bytes * static_cast<size_t> (channels));
    out = makeAudio (channels, static_cast<int> (frames), static_cast<double> (rate));
    for (size_t i = 0; i < frames; ++i)
        for (size_t c = 0; c < static_cast<size_t> (channels); ++c)
        {
            const uint8_t* p = data + (i * static_cast<size_t> (channels) + c) * bytes;
            float v = 0.0f;
            if (flt)
                v = readLE<float> (p);
            else if (bits == 16)
                v = static_cast<float> (readLE<int16_t> (p)) / 32768.0f;
            else if (bits == 24)
            {
                int32_t x = p[0] | (p[1] << 8) | (p[2] << 16);
                if (x & 0x800000)
                    x |= ~0xFFFFFF;
                v = static_cast<float> (x) / 8388608.0f;
            }
            else
                v = static_cast<float> (static_cast<double> (readLE<int32_t> (p)) / 2147483648.0);
            out.ch[c][i] = v;
        }
    return true;
}

bool writeWav (const std::string& path, const Audio& a)
{
    std::ofstream f (path, std::ios::binary);
    if (! f)
        return false;
    const auto channels = static_cast<uint16_t> (a.numChannels());
    const auto frames = static_cast<uint32_t> (a.numSamples());
    const uint32_t dataSize = frames * channels * 4u;
    auto put32 = [&f] (uint32_t v) { f.write (reinterpret_cast<const char*> (&v), 4); };
    auto put16 = [&f] (uint16_t v) { f.write (reinterpret_cast<const char*> (&v), 2); };
    f.write ("RIFF", 4);
    put32 (36u + dataSize);
    f.write ("WAVEfmt ", 8);
    put32 (16);
    put16 (3); // float
    put16 (channels);
    put32 (static_cast<uint32_t> (a.sampleRate));
    put32 (static_cast<uint32_t> (a.sampleRate) * channels * 4u);
    put16 (static_cast<uint16_t> (channels * 4));
    put16 (32);
    f.write ("data", 4);
    put32 (dataSize);
    for (uint32_t i = 0; i < frames; ++i)
        for (uint16_t c = 0; c < channels; ++c)
            f.write (reinterpret_cast<const char*> (&a.ch[c][i]), 4);
    return static_cast<bool> (f);
}

//==============================================================================
// Faire tourner le plugin
struct Note
{
    int sample = 0;
    int number = 60;
    bool on = true;
};

bool configure (juce::AudioProcessor& p, int channels, double sr, int block)
{
    auto layout = p.getBusesLayout();
    const auto set = channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
    if (layout.outputBuses.size() > 0)
        layout.outputBuses.getReference (0) = set;
    if (layout.inputBuses.size() > 0)
        layout.inputBuses.getReference (0) = set;
    if (! p.checkBusesLayoutSupported (layout))
        return false;
    p.setBusesLayout (layout);
    p.setRateAndBufferSizeDetails (sr, block);
    p.prepareToPlay (sr, block);
    return true;
}

/** Tete de lecture factice : 140 BPM, 4/4, en lecture. Pour les plugins cales au tempo. */
struct FakePlayHead final : juce::AudioPlayHead
{
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (bpm);
        info.setTimeSignature (TimeSignature { 4, 4 });
        info.setTimeInSamples (samples);
        info.setTimeInSeconds (static_cast<double> (samples) / sampleRate);
        info.setPpqPosition (static_cast<double> (samples) / sampleRate * bpm / 60.0);
        info.setIsPlaying (true);
        return info;
    }

    double bpm = 140.0, sampleRate = 48000.0;
    juce::int64 samples = 0;
};

double renderBpm = 140.0;

/** Passe `in` dans le plugin. `variable` = tailles de bloc irregulieres (comme certains DAW).
    `beforeBlock(position)` permet de bouger des parametres pendant le rendu. */
Audio run (juce::AudioProcessor& p,
           const Audio& in,
           int block,
           bool variable = false,
           const std::vector<Note>& notes = {},
           const std::function<void (int)>& beforeBlock = {})
{
    const int outChannels = std::max (1, p.getTotalNumOutputChannels());
    const int bufChannels = std::max ({ outChannels, p.getTotalNumInputChannels(), in.numChannels() });
    Audio out = makeAudio (outChannels, in.numSamples(), in.sampleRate);

    juce::AudioBuffer<float> buffer (bufChannels, block);
    juce::MidiBuffer midi;
    std::minstd_rand rng (12345);
    FakePlayHead head;
    head.bpm = renderBpm;
    head.sampleRate = in.sampleRate;
    p.setPlayHead (&head);

    for (int pos = 0; pos < in.numSamples();)
    {
        int n = std::min (block, in.numSamples() - pos);
        if (variable)
            n = std::min (n, 1 + static_cast<int> (rng() % static_cast<unsigned> (block)));

        buffer.setSize (bufChannels, n, false, false, true);
        buffer.clear();
        for (int c = 0; c < std::min (bufChannels, in.numChannels()); ++c)
            buffer.copyFrom (c, 0, in.ch[static_cast<size_t> (c)].data() + pos, n);

        midi.clear();
        for (const auto& note : notes)
            if (note.sample >= pos && note.sample < pos + n)
                midi.addEvent (note.on ? juce::MidiMessage::noteOn (1, note.number, 0.8f)
                                       : juce::MidiMessage::noteOff (1, note.number),
                               note.sample - pos);

        if (beforeBlock)
            beforeBlock (pos);

        head.samples = pos;
        p.processBlock (buffer, midi);

        for (int c = 0; c < outChannels; ++c)
            std::memcpy (out.ch[static_cast<size_t> (c)].data() + pos, buffer.getReadPointer (c), sizeof (float) * static_cast<size_t> (n));
        pos += n;
    }
    p.setPlayHead (nullptr);
    return out;
}

Audio noise (int channels, double seconds, double sr, float amp = 0.5f)
{
    Audio a = makeAudio (channels, static_cast<int> (seconds * sr), sr);
    std::minstd_rand rng (4242);
    std::uniform_real_distribution<float> dist (-amp, amp);
    for (auto& c : a.ch)
        for (auto& s : c)
            s = dist (rng);
    return a;
}

Audio sine (int channels, double seconds, double sr, double freq, float amp)
{
    Audio a = makeAudio (channels, static_cast<int> (seconds * sr), sr);
    for (auto& c : a.ch)
        for (size_t i = 0; i < c.size(); ++i)
            c[i] = amp * static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * freq * static_cast<double> (i) / sr));
    return a;
}

std::vector<Note> arpeggio (const std::vector<int>& numbers, double seconds, double sr)
{
    std::vector<Note> notes;
    if (numbers.empty())
        return notes;
    const int step = static_cast<int> (0.5 * sr);
    const int total = static_cast<int> (seconds * sr);
    size_t k = 0;
    for (int t = static_cast<int> (0.1 * sr); t + step < total; t += step, ++k)
    {
        const int n = numbers[k % numbers.size()];
        notes.push_back ({ t, n, true });
        notes.push_back ({ t + step - static_cast<int> (0.05 * sr), n, false });
    }
    return notes;
}

bool allFinite (const Audio& a)
{
    for (const auto& c : a.ch)
        for (float s : c)
            if (! std::isfinite (s))
                return false;
    return true;
}

float peak (const Audio& a, int from = 0)
{
    float m = 0.0f;
    for (const auto& c : a.ch)
        for (size_t i = static_cast<size_t> (std::max (0, from)); i < c.size(); ++i)
            m = std::max (m, std::abs (c[i]));
    return m;
}

float maxStep (const Audio& a)
{
    float m = 0.0f;
    for (const auto& c : a.ch)
        for (size_t i = 1; i < c.size(); ++i)
            m = std::max (m, std::abs (c[i] - c[i - 1]));
    return m;
}

double toDb (double x) { return x <= 1.0e-12 ? -240.0 : 20.0 * std::log10 (x); }

juce::String paramId (const juce::AudioProcessorParameter& p)
{
    if (auto* withId = dynamic_cast<const juce::AudioProcessorParameterWithID*> (&p))
        return withId->paramID;
    return "#" + juce::String (p.getParameterIndex());
}

juce::AudioProcessorParameter* findParam (juce::AudioProcessor& proc, const juce::String& id)
{
    for (auto* p : proc.getParameters())
        if (paramId (*p) == id)
            return p;
    return nullptr;
}

std::unique_ptr<juce::AudioProcessor> make()
{
    return std::unique_ptr<juce::AudioProcessor> (createPluginFilter());
}

/** Ouvre l'interface comme le fait un DAW, et previent le plugin avant de la fermer. */
struct ScopedEditor
{
    explicit ScopedEditor (juce::AudioProcessor& p) : processor (p), editor (p.createEditorAndMakeActive()) {}
    ~ScopedEditor()
    {
        if (editor != nullptr)
            processor.editorBeingDeleted (editor.get());
    }

    juce::AudioProcessor& processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
};

//==============================================================================
// Tests communs
int failures = 0, warnings = 0;

void ok (const char* name, const juce::String& detail = {})
{
    std::printf ("  OK     %-14s %s\n", name, detail.toRawUTF8());
}
void fail (const char* name, const juce::String& detail)
{
    ++failures;
    std::printf ("  ECHEC  %-14s %s\n", name, detail.toRawUTF8());
}
void warn (const char* name, const juce::String& detail)
{
    ++warnings;
    std::printf ("  ALERTE %-14s %s\n", name, detail.toRawUTF8());
}

void testRobust (bool synth)
{
    // Frequences et tailles de bloc variees, blocs irreguliers, bloc vide : jamais de NaN ni d'explosion.
    int runs = 0;
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (int block : { 16, 64, 512, 1024 })
            for (bool variable : { false, true })
            {
                auto p = make();
                if (! configure (*p, 2, sr, block))
                {
                    fail ("robustesse", "disposition stereo refusee");
                    return;
                }
                // Une configuration sur deux avec tous les parametres tires au hasard.
                if (runs % 2 == 1)
                {
                    std::minstd_rand rng (static_cast<unsigned> (1000 + runs));
                    std::uniform_real_distribution<float> dist (0.0f, 1.0f);
                    for (auto* param : p->getParameters())
                        param->setValueNotifyingHost (dist (rng));
                }
                const auto in = synth ? makeAudio (2, static_cast<int> (sr), sr) : noise (2, 1.0, sr);
                const auto notes = synth ? arpeggio ({ 45, 57, 64 }, 1.0, sr) : std::vector<Note>();
                const auto out = run (*p, in, block, variable, notes);

                juce::AudioBuffer<float> empty (std::max (p->getTotalNumInputChannels(), p->getTotalNumOutputChannels()), 0);
                juce::MidiBuffer midi;
                p->processBlock (empty, midi);

                if (! allFinite (out) || peak (out) > 100.0f)
                {
                    fail ("robustesse", juce::String::formatted ("sortie invalide a %.0f Hz, bloc %d%s (crete %.1f dBFS)", sr, block, variable ? " irregulier" : "", toDb (peak (out))));
                    return;
                }
                p->releaseResources();
                ++runs;
            }
    ok ("robustesse", juce::String (runs) + " configurations (dont reglages au hasard), sortie toujours finie");
}

void testSilence (bool synth)
{
    // Apres le son, le plugin doit revenir au silence (pas de bruit de fond, pas d'oscillation).
    const double sr = 48000.0;
    auto p = make();
    configure (*p, 2, sr, 512);
    const double tail = std::min (10.0, std::max (0.0, p->getTailLengthSeconds()));
    const double total = 1.0 + tail + 3.0;
    Audio in = makeAudio (2, static_cast<int> (total * sr), sr);
    if (! synth)
    {
        const auto n = noise (2, 1.0, sr);
        for (size_t c = 0; c < 2; ++c)
            std::copy (n.ch[c].begin(), n.ch[c].end(), in.ch[c].begin());
    }
    const auto notes = synth ? arpeggio ({ 57 }, 1.0, sr) : std::vector<Note>();
    const auto out = run (*p, in, 512, false, notes);
    const float end = peak (out, out.numSamples() - static_cast<int> (0.5 * sr));
    if (end > 1.0e-5f)
        fail ("silence", juce::String::formatted ("encore %.1f dBFS en sortie 2,5 s apres la fin du son", toDb (end)));
    else
        ok ("silence", juce::String::formatted ("retour au silence (%.0f dBFS)", toDb (end)));
}

void testState()
{
    // Sauvegarde puis rechargement dans une instance neuve : memes parametres. Etat corrompu : pas de plantage.
    auto a = make();
    configure (*a, 2, 48000.0, 512);
    std::minstd_rand rng (777);
    std::uniform_real_distribution<float> dist (0.0f, 1.0f);
    for (auto* p : a->getParameters())
        p->setValueNotifyingHost (dist (rng));

    juce::MemoryBlock saved;
    a->getStateInformation (saved);

    auto b = make();
    configure (*b, 2, 48000.0, 512);
    b->setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));

    const auto& pa = a->getParameters();
    const auto& pb = b->getParameters();
    for (int i = 0; i < pa.size(); ++i)
        if (std::abs (pa[i]->getValue() - pb[i]->getValue()) > 1.0e-4f)
        {
            fail ("etat", "le parametre '" + paramId (*pa[i]) + "' ne revient pas apres sauvegarde / rechargement");
            return;
        }

    auto c = make();
    configure (*c, 2, 48000.0, 512);
    std::vector<uint8_t> junk (257);
    for (auto& x : junk)
        x = static_cast<uint8_t> (rng());
    c->setStateInformation (junk.data(), static_cast<int> (junk.size()));
    c->setStateInformation (junk.data(), 0);
    const auto out = run (*c, noise (2, 0.1, 48000.0), 512);
    if (! allFinite (out))
    {
        fail ("etat", "sortie invalide apres un etat corrompu");
        return;
    }
    ok ("etat", juce::String (pa.size()) + " parametre(s) retrouves, etat corrompu ignore sans plantage");
}

void testDeterminism (bool synth)
{
    // Deux rendus identiques = export identique a l'ecoute. Et la taille de bloc ne doit pas changer le son.
    const double sr = 48000.0;
    const auto in = synth ? makeAudio (2, static_cast<int> (sr), sr) : noise (2, 1.0, sr);
    const auto notes = synth ? arpeggio ({ 45, 57 }, 1.0, sr) : std::vector<Note>();

    auto render = [&] (int block) {
        auto p = make();
        configure (*p, 2, sr, block);
        return run (*p, in, block, false, notes);
    };
    const auto a = render (512), b = render (512), c = render (64);

    float sameDiff = 0.0f, blockDiff = 0.0f;
    for (size_t ch = 0; ch < a.ch.size(); ++ch)
        for (size_t i = 0; i < a.ch[ch].size(); ++i)
        {
            sameDiff = std::max (sameDiff, std::abs (a.ch[ch][i] - b.ch[ch][i]));
            blockDiff = std::max (blockDiff, std::abs (a.ch[ch][i] - c.ch[ch][i]));
        }

    if (sameDiff > 0.0f)
        warn ("determinisme", juce::String::formatted ("deux rendus identiques different de %.1f dBFS (aleatoire non seme ?)", toDb (sameDiff)));
    else
        ok ("determinisme", "deux rendus identiques au bit pres");

    if (blockDiff > 1.0e-3f)
        warn ("taille de bloc", juce::String::formatted ("le son change avec la taille de bloc (ecart %.1f dBFS entre 64 et 512)", toDb (blockDiff)));
    else
        ok ("taille de bloc", juce::String::formatted ("ecart entre blocs de 64 et 512 : %.1f dBFS", toDb (blockDiff)));
}

void testClicks (bool synth)
{
    // Chaque parametre continu saute du minimum au maximum et retour pendant une sinusoide :
    // aucun saut brutal en sortie. Les interrupteurs et listes ne sont pas testes ici.
    if (synth)
    {
        ok ("clics", "instrument : non teste par l'outil commun");
        return;
    }
    const double sr = 48000.0;
    const int block = 256;
    const auto in = sine (2, 1.5, sr, 220.0, 0.25f);
    int tested = 0;

    auto probe = make();
    const int count = probe->getParameters().size();
    for (int index = 0; index < count; ++index)
    {
        auto p = make();
        configure (*p, 2, sr, block);
        auto* param = p->getParameters()[index];
        if (param->isDiscrete() || param->isBoolean() || ! param->isAutomatable())
            continue;

        param->setValueNotifyingHost (0.0f);
        const int half = static_cast<int> (0.5 * sr);
        const auto out = run (*p, in, block, false, {}, [&] (int pos) {
            if (pos >= half && pos < half + block)
                param->setValueNotifyingHost (1.0f);
            if (pos >= 2 * half && pos < 2 * half + block)
                param->setValueNotifyingHost (0.0f);
        });
        ++tested;
        const float step = maxStep (out), pk = std::max (peak (out), 1.0e-3f);
        if (step > 0.2f * pk)
        {
            fail ("clics", "'" + paramId (*param) + juce::String::formatted ("' : saut de %.1f dBFS quand il passe d'un extreme a l'autre (lisser le parametre)", toDb (step)));
            return;
        }
    }
    ok ("clics", juce::String (tested) + " parametre(s) continu(s) sans saut");
}

void testMono()
{
    auto p = make();
    if (! configure (*p, 1, 48000.0, 512))
    {
        ok ("mono", "disposition mono non proposee");
        return;
    }
    const auto out = run (*p, noise (1, 0.5, 48000.0), 512);
    if (! allFinite (out))
        fail ("mono", "sortie invalide en mono");
    else
        ok ("mono", "fonctionne en mono");
}

void testEditor()
{
    auto p = make();
    configure (*p, 2, 48000.0, 512);
    if (! p->hasEditor())
    {
        ok ("interface", "pas d'interface");
        return;
    }
    for (int i = 0; i < 2; ++i) // ouvrir, fermer, rouvrir : c'est la que ca plante d'habitude
    {
        ScopedEditor scoped (*p);
        auto& ed = scoped.editor;
        if (ed == nullptr || ed->getWidth() <= 0 || ed->getHeight() <= 0)
        {
            fail ("interface", "editeur vide ou de taille nulle");
            return;
        }
        const auto img = ed->createComponentSnapshot (ed->getLocalBounds());
        if (! img.isValid())
        {
            fail ("interface", "impossible de dessiner l'interface");
            return;
        }
    }

    // La taille choisie par l'utilisateur doit revenir apres sauvegarde / rechargement du projet.
    {
        int w = 0, h = 0;
        juce::MemoryBlock saved;
        {
            ScopedEditor scoped (*p);
            if (scoped.editor->isResizable())
            {
                scoped.editor->setSize (scoped.editor->getWidth() + 60, scoped.editor->getHeight() + 40);
                w = scoped.editor->getWidth();
                h = scoped.editor->getHeight();
            }
        }
        if (w > 0)
        {
            p->getStateInformation (saved);
            auto q = make();
            configure (*q, 2, 48000.0, 512);
            q->setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
            ScopedEditor reopened (*q);
            if (reopened.editor->getWidth() != w || reopened.editor->getHeight() != h)
            {
                fail ("interface", juce::String::formatted ("taille de fenetre perdue : %d x %d demande, %d x %d au rechargement", w, h, reopened.editor->getWidth(), reopened.editor->getHeight()));
                return;
            }
        }
    }
    ok ("interface", "s'ouvre, se ferme, se rouvre, garde sa taille");
}

void testSpeed (bool synth)
{
    const double sr = 48000.0, seconds = 10.0;
    auto p = make();
    configure (*p, 2, sr, 512);
    const auto in = synth ? makeAudio (2, static_cast<int> (seconds * sr), sr) : noise (2, seconds, sr);
    const auto notes = synth ? arpeggio ({ 45, 57, 64 }, seconds, sr) : std::vector<Note>();
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    run (*p, in, 512, false, notes);
    const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
    const double load = 100.0 * (ms / 1000.0) / seconds;
    if (load > 25.0)
        warn ("charge", juce::String::formatted ("%.1f %% d'un coeur a 48 kHz / 512 (machine de test)", load));
    else
        ok ("charge", juce::String::formatted ("%.2f %% d'un coeur a 48 kHz / 512 (machine de test)", load));
}

int commandTest()
{
    auto p = make();
    const bool synth = p->acceptsMidi() && p->getTotalNumInputChannels() == 0;
    std::printf ("%s %s : tests communs\n", p->getName().toRawUTF8(), synth ? "(instrument)" : "(effet)");
    std::printf ("  latence annoncee : %d echantillons\n", p->getLatencySamples());
    p.reset();

    testRobust (synth);
    testSilence (synth);
    testState();
    testDeterminism (synth);
    testClicks (synth);
    testMono();
    testEditor();
    testSpeed (synth);

    std::printf ("%d echec(s), %d alerte(s)\n", failures, warnings);
    return failures == 0 ? 0 : 1;
}

//==============================================================================
int commandParams()
{
    auto p = make();
    configure (*p, 2, 48000.0, 512);
    std::printf ("%-20s %-24s %-14s %s\n", "id", "nom", "defaut", "type");
    for (auto* param : p->getParameters())
        std::printf ("%-20s %-24s %-14s %s\n",
                     paramId (*param).toRawUTF8(),
                     param->getName (64).toRawUTF8(),
                     (param->getText (param->getDefaultValue(), 32) + " " + param->getLabel()).toRawUTF8(),
                     param->isBoolean() ? "interrupteur" : (param->isDiscrete() ? "liste" : "continu"));
    return 0;
}

int commandRender (const juce::StringArray& args)
{
    if (args.size() < 3)
    {
        std::printf ("usage : rendu <entree.wav|-> <sortie.wav> [--param id=0..1] [--bloc 512] [--bpm 140] [--notes 57,60 --duree 4]\n");
        return 2;
    }
    int block = 512;
    double seconds = 4.0;
    std::vector<int> numbers;
    std::vector<std::pair<juce::String, float>> params;
    for (int i = 3; i < args.size(); ++i)
    {
        if (args[i] == "--bloc" && i + 1 < args.size())
            block = juce::jlimit (1, 8192, args[++i].getIntValue());
        else if (args[i] == "--bpm" && i + 1 < args.size())
            renderBpm = juce::jlimit (20.0, 400.0, args[++i].getDoubleValue());
        else if (args[i] == "--duree" && i + 1 < args.size())
            seconds = juce::jlimit (0.1, 600.0, args[++i].getDoubleValue());
        else if (args[i] == "--notes" && i + 1 < args.size())
            for (const auto& n : juce::StringArray::fromTokens (args[++i], ",", ""))
                numbers.push_back (juce::jlimit (0, 127, n.getIntValue()));
        else if (args[i] == "--param" && i + 1 < args.size())
        {
            const auto kv = args[++i];
            params.emplace_back (kv.upToFirstOccurrenceOf ("=", false, false),
                                 juce::jlimit (0.0f, 1.0f, kv.fromFirstOccurrenceOf ("=", false, false).getFloatValue()));
        }
        else
        {
            std::printf ("option inconnue : %s\n", args[i].toRawUTF8());
            return 2;
        }
    }

    Audio in;
    if (args[1] == "-")
        in = makeAudio (2, static_cast<int> (seconds * 48000.0), 48000.0);
    else
    {
        std::string err;
        if (! readWav (args[1].toStdString(), in, err))
        {
            std::printf ("%s\n", err.c_str());
            return 1;
        }
    }

    auto p = make();
    // Parametres regles avant le demarrage, comme un projet qu'on rouvre : pas de rampe en debut de rendu.
    for (const auto& [id, value] : params)
    {
        if (auto* param = findParam (*p, id))
            param->setValueNotifyingHost (value);
        else
        {
            std::printf ("parametre inconnu : %s (voir la commande params)\n", id.toRawUTF8());
            return 1;
        }
    }
    if (! configure (*p, in.numChannels() == 1 ? 1 : 2, in.sampleRate, block)
        && ! configure (*p, 2, in.sampleRate, block))
    {
        std::printf ("disposition de canaux refusee par le plugin\n");
        return 1;
    }

    const auto out = run (*p, in, block, false, arpeggio (numbers, in.numSamples() / in.sampleRate, in.sampleRate));
    if (! writeWav (args[2].toStdString(), out))
    {
        std::printf ("ecriture impossible : %s\n", args[2].toRawUTF8());
        return 1;
    }
    std::printf ("%s : %.2f s, %d canal(aux), %.0f Hz, crete %.1f dBFS\n",
                 args[2].toRawUTF8(), out.numSamples() / out.sampleRate, out.numChannels(), out.sampleRate, toDb (peak (out)));
    return 0;
}

int commandCapture (const juce::StringArray& args)
{
    if (args.size() < 2)
    {
        std::printf ("usage : capture <image.png> [--echelle 2]\n");
        return 2;
    }
    float scale = 2.0f;
    for (int i = 2; i + 1 < args.size(); ++i)
        if (args[i] == "--echelle")
            scale = juce::jlimit (0.5f, 4.0f, args[i + 1].getFloatValue());

    auto p = make();
    configure (*p, 2, 48000.0, 512);
    if (! p->hasEditor())
    {
        std::printf ("ce plugin n'a pas d'interface\n");
        return 1;
    }
    ScopedEditor scoped (*p);
    auto& ed = scoped.editor;
    if (ed == nullptr)
    {
        std::printf ("l'interface ne s'ouvre pas\n");
        return 1;
    }
    const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);

    juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile (args[1]);
    file.deleteFile();
    juce::FileOutputStream stream (file);
    if (! stream.openedOk() || ! juce::PNGImageFormat().writeImageToStream (img, stream))
    {
        std::printf ("ecriture impossible : %s\n", file.getFullPathName().toRawUTF8());
        return 1;
    }
    std::printf ("%s : %d x %d px\n", file.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    return 0;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add (juce::String::fromUTF8 (argv[i]));

    const auto cmd = args.isEmpty() ? juce::String() : args[0];
    if (cmd == "test")
        return commandTest();
    if (cmd == "params")
        return commandParams();
    if (cmd == "rendu")
        return commandRender (args);
    if (cmd == "capture")
        return commandCapture (args);

    std::printf ("commandes : test | params | rendu <in.wav|-> <out.wav> [...] | capture <image.png>\n");
    return 2;
}
