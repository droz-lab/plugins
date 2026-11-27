#include "PluginEditor.h"

using markovui::fr;
namespace colours = droz::ui::colours;

namespace
{
constexpr int leftX = 16, leftW = 260;
constexpr int midX = 292, midW = 280;
constexpr int rightX = 588, rightW = 516;
constexpr int topY = 56;
constexpr int stepsY = 486;

void setDoubleClickDefault (juce::Slider& s, juce::RangedAudioParameter* p)
{
    if (p != nullptr)
        s.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}
} // namespace

//==============================================================================
MarkovContent::MarkovContent (PluginProcessor& p)
    : processor (p), matrix (p), stepView (p)
{
    auto& apvts = processor.apvts;

    // --- Scale and global
    setupCombo (scaleBox, scaleLabel, "Gamme", fr ("Gamme : les notes que Markov peut jouer. « Personnalisée » : choisir les notes ci-dessous."));
    scaleBox.addItemList (markovui::scaleNames(), 1);
    scaleAttachment = std::make_unique<ComboAttachment> (apvts, "gamme", scaleBox);
    setupCombo (rootBox, rootLabel, "Tonique", fr ("Tonique : première note de la gamme (le degré 1, point de départ de la chaîne)."));
    for (int i = 0; i < 12; ++i)
        rootBox.addItem (markovui::noteName (i, false), i + 1);
    rootAttachment = std::make_unique<ComboAttachment> (apvts, "tonique", rootBox);

    for (int i = 0; i < 12; ++i)
    {
        auto& b = customNotes[(size_t) i];
        b.setClickingTogglesState (true);
        b.setTooltip (fr ("Gamme personnalisée : allumer les notes à garder (à partir de la tonique)."));
        addAndMakeVisible (b);
        customAttachments[(size_t) i] = std::make_unique<ButtonAttachment> (apvts, "perso_" + juce::String (i), b);
    }

    tempoLabel.setTooltip (fr ("Tempo et lecture lus dans le DAW : Markov joue seulement quand le DAW joue."));
    addAndMakeVisible (tempoLabel);

    setupKnob (seed, "Graine", fr ("Graine : même graine et mêmes réglages = même suite de notes. Double-clic = 1."));
    attachKnob (seed, "graine");
    regenerate.setButtonText (fr ("Régénérer"));
    regenerate.setTooltip (fr ("Tire une nouvelle graine au hasard : nouvelle suite de notes (Ctrl+Z dans le DAW pour revenir)."));
    regenerate.onClick = [this] {
        auto* param = processor.apvts.getParameter ("graine");
        const int current = static_cast<int> (param->convertFrom0to1 (param->getValue()));
        int next = current;
        while (next == current)
            next = 1 + juce::Random::getSystemRandom().nextInt (9999);
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (next)));
        param->endChangeGesture();
    };
    addAndMakeVisible (regenerate);

    setupKnob (temperature, fr ("Température"), fr ("Température : basse = prévisible (les transitions fortes dominent), "
                                                    "1 = la matrice telle quelle, haute = plus aléatoire. Double-clic = 1."));
    attachKnob (temperature, "temperature");
    setupCombo (orderBox, orderLabel, "Ordre", fr ("Ordre 1 : la note suivante dépend de la note jouée. "
                                                   "Ordre 2 : des deux dernières (seulement là où l'apprentissage l'a rempli, sinon ordre 1)."));
    orderBox.addItemList ({ "Ordre 1", "Ordre 2" }, 1);
    orderAttachment = std::make_unique<ComboAttachment> (apvts, "ordre", orderBox);
    setupCombo (rhythmBox, rhythmLabel, "Rythme", fr ("Pas à pas : chaque pas joue selon sa chance. "
                                                      "Chaîne : la chance dépend aussi du pas précédent (réglages « après une note » et « après un silence »)."));
    rhythmBox.addItemList ({ fr ("Pas à pas"), fr ("Chaîne") }, 1);
    rhythmAttachment = std::make_unique<ComboAttachment> (apvts, "rythme", rhythmBox);

    preview.setButtonText (fr ("Écoute"));
    preview.setTooltip (fr ("Écoute : un petit son intégré pour entendre les pistes sans synthé. À couper quand les synthés sont branchés."));
    addAndMakeVisible (preview);
    previewAttachment = std::make_unique<ButtonAttachment> (apvts, "ecoute", preview);
    setupKnob (previewLevel, "Volume", fr ("Volume de l'écoute intégrée. Double-clic = -12 dB."));
    attachKnob (previewLevel, "volume_ecoute");

    // --- Learning
    learn.setButtonText ("Apprendre");
    learn.setTooltip (fr ("Apprentissage : jouer des notes MIDI vers Markov. Il compte les transitions et remplit la matrice "
                          "de la piste choisie (les notes sont ramenées à la gamme)."));
    addAndMakeVisible (learn);
    learnAttachment = std::make_unique<ButtonAttachment> (apvts, "apprendre", learn);
    learnStatus.setTooltip (fr ("Nombre de notes apprises et piste qui reçoit la matrice."));
    learnStatus.setColour (juce::Label::textColourId, colours::dim);
    addAndMakeVisible (learnStatus);
    learnReset.setButtonText (fr ("Réinitialiser"));
    learnReset.setTooltip (fr ("Oublie ce qui a été appris et remet la matrice par défaut sur la piste choisie."));
    learnReset.onClick = [this] { processor.resetLearning(); };
    addAndMakeVisible (learnReset);

    // --- Tracks
    for (int t = 0; t < markov::numTracks; ++t)
    {
        rows.push_back (std::make_unique<markovui::TrackRow> (processor, t, [this] (int i) { selectTrack (i); }));
        addAndMakeVisible (*rows.back());
    }
    setupKnob (lo, "Note min", fr ("Note la plus grave de la piste. Double-clic = valeur par défaut."));
    setupKnob (hi, "Note max", fr ("Note la plus aiguë de la piste. Double-clic = valeur par défaut."));
    setupKnob (steps, "Pas", fr ("Nombre de pas de la grille (1 à 32). Double-clic = 16."));
    setupKnob (gate, fr ("Durée"), fr ("Durée de chaque note, en % du pas. Double-clic = 50 %."));
    setupKnob (afterHit, fr ("Si note"), fr ("Mode Chaîne : chance multipliée par cette valeur quand le pas précédent a joué. Double-clic = 60 %."));
    setupKnob (afterRest, fr ("Si silence"), fr ("Mode Chaîne : chance multipliée par cette valeur quand le pas précédent était muet. Double-clic = 90 %."));
    setupCombo (divisionBox, divisionLabel, "Division", fr ("Durée d'un pas, calée sur le tempo du DAW."));
    divisionBox.addItemList (markovui::divisionNames(), 1);

    // --- Matrix and steps
    addAndMakeVisible (matrix);
    matrixDefault.setButtonText (fr ("Par défaut"));
    matrixDefault.setTooltip (fr ("Remet la matrice de la piste par défaut (secondes et tierces favorisées) et oublie l'ordre 2."));
    matrixDefault.onClick = [this] {
        processor.editTrack (track, [] (markov::TrackData& d) { markov::resetMatrix (d); });
        matrix.repaint();
    };
    addAndMakeVisible (matrixDefault);
    addAndMakeVisible (stepView);
    stepsDefault.setButtonText (fr ("Par défaut"));
    stepsDefault.setTooltip (fr ("Remet les chances et vélocités des pas de la piste par défaut."));
    stepsDefault.onClick = [this] {
        const int t = track;
        processor.editTrack (t, [t] (markov::TrackData& d) { markov::resetSteps (d, t); });
        stepView.repaint();
    };
    addAndMakeVisible (stepsDefault);
    stepsFull.setButtonText ("Tout 100 %");
    stepsFull.setTooltip (fr ("Met tous les pas de la piste à 100 % : la piste joue à chaque pas."));
    stepsFull.onClick = [this] {
        processor.editTrack (track, [] (markov::TrackData& d) {
            for (auto& v : d.prob)
                v = 1.0f;
        });
        stepView.repaint();
    };
    addAndMakeVisible (stepsFull);

    for (auto* c : { (juce::Component*) &scaleBox, (juce::Component*) &rootBox, (juce::Component*) &orderBox,
                     (juce::Component*) &rhythmBox, (juce::Component*) &divisionBox })
    {
        c->setColour (juce::ComboBox::backgroundColourId, colours::panel);
        c->setColour (juce::ComboBox::outlineColourId, colours::line);
        c->setColour (juce::ComboBox::textColourId, colours::text);
        c->setColour (juce::ComboBox::arrowColourId, colours::dim);
    }
    for (auto* b : { &regenerate, &learnReset, &matrixDefault, &stepsDefault, &stepsFull })
    {
        b->setColour (juce::TextButton::buttonColourId, colours::panel);
        b->setColour (juce::TextButton::textColourOffId, colours::text);
    }
    for (auto& b : customNotes)
    {
        b.setColour (juce::TextButton::buttonColourId, colours::panel);
        b.setColour (juce::TextButton::buttonOnColourId, colours::text);
        b.setColour (juce::TextButton::textColourOffId, colours::dim);
        b.setColour (juce::TextButton::textColourOnId, colours::background);
    }
    for (auto* b : { &preview, &learn })
    {
        b->setColour (juce::ToggleButton::tickColourId, markovui::accent);
        b->setColour (juce::ToggleButton::tickDisabledColourId, colours::dim);
        b->setColour (juce::ToggleButton::textColourId, colours::text);
    }

    selectTrack (juce::jlimit (0, markov::numTracks - 1, processor.selectedTrack.load()));

    // After adding the controls: this is what makes them (text boxes included) take the look.
    setLookAndFeel (&look);
    setSize (PluginProcessor::baseWidth, PluginProcessor::baseHeight);
    refresh();
}

MarkovContent::~MarkovContent()
{
    setLookAndFeel (nullptr);
}

void MarkovContent::setupKnob (Knob& k, const juce::String& label, const juce::String& tooltip)
{
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    k.slider.setTooltip (tooltip);
    k.label.setText (droz::ui::upper (label), juce::dontSendNotification);
    k.label.setFont (droz::ui::caps (10.0f));
    k.label.setColour (juce::Label::textColourId, colours::dim);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setMinimumHorizontalScale (0.6f);
    k.label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);
}

void MarkovContent::attachKnob (Knob& k, const juce::String& paramId)
{
    k.attachment.reset();
    k.attachment = std::make_unique<SliderAttachment> (processor.apvts, paramId, k.slider);
    setDoubleClickDefault (k.slider, processor.apvts.getParameter (paramId));
}

void MarkovContent::setupCombo (juce::ComboBox& box, juce::Label& label, const juce::String& text, const juce::String& tooltip)
{
    box.setTooltip (tooltip);
    label.setText (droz::ui::upper (text), juce::dontSendNotification);
    label.setFont (droz::ui::caps (10.0f));
    label.setColour (juce::Label::textColourId, colours::dim);
    addAndMakeVisible (box);
    addAndMakeVisible (label);
}

void MarkovContent::selectTrack (int t)
{
    track = juce::jlimit (0, markov::numTracks - 1, t);
    processor.selectedTrack = track;
    for (int i = 0; i < (int) rows.size(); ++i)
        rows[(size_t) i]->setSelected (i == track);

    attachKnob (lo, PluginProcessor::trackParam (track, "min"));
    attachKnob (hi, PluginProcessor::trackParam (track, "max"));
    attachKnob (steps, PluginProcessor::trackParam (track, "pas"));
    attachKnob (gate, PluginProcessor::trackParam (track, "duree"));
    attachKnob (afterHit, PluginProcessor::trackParam (track, "apres_note"));
    attachKnob (afterRest, PluginProcessor::trackParam (track, "apres_silence"));
    divisionAttachment.reset();
    divisionAttachment = std::make_unique<ComboAttachment> (processor.apvts, PluginProcessor::trackParam (track, "division"), divisionBox);

    matrix.setTrack (track);
    stepView.setTrack (track);
    repaint();
}

void MarkovContent::updateCustomNotes()
{
    const int root = static_cast<int> (processor.apvts.getRawParameterValue ("tonique")->load());
    const bool custom = static_cast<int> (processor.apvts.getRawParameterValue ("gamme")->load()) == markov::custom;
    for (int i = 0; i < 12; ++i)
    {
        auto& b = customNotes[(size_t) i];
        const auto name = markovui::noteName ((root + i) % 12, false);
        if (b.getButtonText() != name)
            b.setButtonText (name);
        b.setEnabled (custom);
        b.setAlpha (custom ? 1.0f : 0.35f);
    }
}

void MarkovContent::refresh()
{
    ++tick;
    for (int t = 0; t < markov::numTracks; ++t)
    {
        const auto n = processor.notesPlayed[(size_t) t].load();
        leds[(size_t) t] = n != lastNotes[(size_t) t] ? 1.0f : leds[(size_t) t] * 0.8f;
        lastNotes[(size_t) t] = n;
        rows[(size_t) t]->setLed (leds[(size_t) t] < 0.02f ? 0.0f : leds[(size_t) t]);
    }

    const double bpm = processor.hostBpm.load();
    tempoLabel.setText ((bpm > 0.0 ? juce::String (bpm, 1) + " BPM" : juce::String ("-- BPM"))
                            + (processor.hostPlaying.load() ? fr (" · lecture") : fr (" · arrêt")) + fr (" (DAW)"),
                        juce::dontSendNotification);

    const bool chain = processor.apvts.getRawParameterValue ("rythme")->load() > 0.5f;
    for (auto* k : { &afterHit, &afterRest })
    {
        k->slider.setEnabled (chain);
        k->slider.setAlpha (chain ? 1.0f : 0.35f);
        k->label.setAlpha (chain ? 1.0f : 0.35f);
    }

    learnStatus.setText (juce::String (processor.learnedNotes.load()) + fr (" notes apprises → piste ") + juce::String (track + 1),
                         juce::dontSendNotification);
    updateCustomNotes();

    // Matrix and steps follow the playhead and the learning: redraw at ~15 Hz.
    if (tick % 2 == 0)
    {
        matrix.repaint();
        stepView.repaint();
    }
}

void MarkovContent::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
    droz::ui::drawHeader (g, getLocalBounds(), "Markov");

    const auto title = [&g] (const juce::String& text, int x, int y, int w) {
        g.setColour (colours::text);
        g.setFont (droz::ui::caps (12.0f));
        g.drawText (droz::ui::upper (text), x, y, w, 18, juce::Justification::centredLeft);
        g.setColour (colours::line);
        g.fillRect (x, y + 20, w, 1);
    };
    title ("Gamme et global", leftX, topY, leftW);
    title ("Apprentissage", leftX, 600, leftW);
    title ("Pistes", midX, topY, midW);
    title (fr ("Matrice · piste ") + juce::String (track + 1), rightX, topY, rightW);
    title (fr ("Pas · piste ") + juce::String (track + 1), midX, stepsY, rightX + rightW - midX);

    g.setColour (colours::line);
    g.fillRect (leftX + leftW + 7, topY, 1, getHeight() - topY - 16);
}

void MarkovContent::resized()
{
    // --- Left column
    int y = topY + 28;
    const auto comboRow = [&] (juce::Label& label, juce::ComboBox& box, int x, int w) {
        label.setBounds (x, y, w, 14);
        box.setBounds (x, y + 15, w, 24);
    };
    comboRow (scaleLabel, scaleBox, leftX, 160);
    comboRow (rootLabel, rootBox, leftX + 170, 90);
    y += 48;
    for (int i = 0; i < 12; ++i)
        customNotes[(size_t) i].setBounds (leftX + (i % 6) * 44, y + (i / 6) * 28, 40, 24);
    y += 62;
    tempoLabel.setBounds (leftX, y, leftW, 20);
    y += 28;

    const auto knob = [] (Knob& k, int x, int ky, int w, int h) {
        k.label.setBounds (x, ky, w, 14);
        k.slider.setBounds (x, ky + 14, w, h - 14);
    };
    knob (seed, leftX, y, 84, 92);
    knob (temperature, leftX + 88, y, 84, 92);
    regenerate.setBounds (leftX + 178, y + 30, 82, 28);
    y += 100;
    comboRow (orderLabel, orderBox, leftX, 125);
    comboRow (rhythmLabel, rhythmBox, leftX + 135, 125);
    y += 52;
    preview.setBounds (leftX, y + 30, 100, 26);
    knob (previewLevel, leftX + 120, y, 84, 92);

    y = 600 + 30;
    learn.setBounds (leftX, y, 130, 26);
    learnReset.setBounds (leftX + leftW - 100, y, 100, 26);
    learnStatus.setBounds (leftX, y + 32, leftW, 22);

    // --- Middle column: track list, then the selected track's settings
    y = topY + 28;
    for (auto& row : rows)
    {
        row->setBounds (midX, y, midW, 26);
        y += 29;
    }
    y += 6;
    const int kw = midW / 4;
    knob (lo, midX, y, kw, 84);
    knob (hi, midX + kw, y, kw, 84);
    knob (steps, midX + 2 * kw, y, kw, 84);
    knob (gate, midX + 3 * kw, y, kw, 84);
    y += 88;
    divisionLabel.setBounds (midX, y, 110, 14);
    divisionBox.setBounds (midX, y + 15, 110, 24);
    knob (afterHit, midX + 2 * kw, y - 4, kw, 84);
    knob (afterRest, midX + 3 * kw, y - 4, kw, 84);

    // --- Right column: matrix
    matrix.setBounds (rightX, topY + 26, rightW, stepsY - topY - 66);
    matrixDefault.setBounds (rightX, stepsY - 36, 100, 24);

    // --- Bottom: steps
    const int stepsW = rightX + rightW - midX;
    stepView.setBounds (midX, stepsY + 28, stepsW, getHeight() - stepsY - 28 - 44);
    stepsDefault.setBounds (midX + stepsW - 100, getHeight() - 36, 100, 24);
    stepsFull.setBounds (midX + stepsW - 210, getHeight() - 36, 100, 24);
}

//==============================================================================
PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processor (p), content (p)
{
    addAndMakeVisible (content);

    // Read the kept size BEFORE setResizeLimits: it resizes the (still empty) window to the minimum,
    // and resized() would overwrite the kept size. Trap checked by the "interface" test.
    const int width = processor.editorWidth.load(), height = processor.editorHeight.load();
    setResizable (true, true);
    setResizeLimits (PluginProcessor::baseWidth / 2, PluginProcessor::baseHeight / 2,
                     PluginProcessor::baseWidth * 2, PluginProcessor::baseHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (static_cast<double> (PluginProcessor::baseWidth) / PluginProcessor::baseHeight);
    setSize (width, height);
    ready = true;
    startTimerHz (30);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
}

void PluginEditor::resized()
{
    if (ready)
    {
        processor.editorWidth = getWidth();
        processor.editorHeight = getHeight();
    }
    // The content is drawn at its base size and scaled as a whole (ratio kept by the constrainer).
    const float s = juce::jmin (getWidth() / static_cast<float> (PluginProcessor::baseWidth),
                                getHeight() / static_cast<float> (PluginProcessor::baseHeight));
    content.setTransform (juce::AffineTransform::scale (s));
}
