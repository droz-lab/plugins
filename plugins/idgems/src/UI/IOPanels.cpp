#include "IOPanels.h"
#include "../Effects/Rack.h"

namespace idg::ui
{
namespace
{
template <typename T> T* own (juce::Component& parent, juce::OwnedArray<juce::Component>& arr, T* c)
{
    arr.add (c);
    parent.addAndMakeVisible (c);
    return c;
}
}

InputPanel::InputPanel (EditorContext& c) : ctx (c)
{
    auto k = [&] (const juce::String& id, const juce::String& l, juce::Colour col = Theme::accent) { return own (*this, comps, new Knob (ctx, ctx.proc.indexOf (id), col, l)); };
    auto b = [&] (const juce::String& id, const juce::String& l, juce::Colour col = Theme::accent) { return own (*this, comps, new ParamButton (ctx, id, l, col)); };
    auto cb = [&] (const juce::String& id) { return own (*this, comps, new ParamCombo (ctx, id)); };
    // ordre de création = ordre de placement (voir resized)
    b (G::transOn, IDG_T ("DÉTECTION"));             // 0
    k (G::transSens, IDG_T ("SENSIBILITÉ"));          // 1
    cb (G::transSrc);                         // 2
    b (G::capture, "CAPTURE", Theme::red);    // 3
    cb (G::captureLen);                       // 4
    k (G::fbAmount, IDG_T ("QUANTITÉ"), Theme::red);  // 5
    k (G::fbTime, "TEMPS", Theme::red);       // 6
    b (G::scaleOn, "GAMME", Theme::blue);     // 7
    cb (G::scaleKey);                         // 8
    cb (G::scaleType);                        // 9
    b (G::pitchSnap, "SNAP PITCH", Theme::blue); // 10
    b (G::zeroLat, "SANS LATENCE", Theme::blue); // 11
    cb (G::destOS);                           // 12
    panic.setColour (juce::TextButton::buttonColourId, Theme::red.withAlpha (0.7f));
    panic.setTooltip (IDG_T ("Coupe instantanément les feedbacks qui s'emballent (vide tous les buffers)"));
    panic.onClick = [this] { ctx.proc.panic(); };
    addAndMakeVisible (panic);
    comps[3]->setHelpText (IDG_T ("Attrape une mesure du son entrant et continue à la glitcher en boucle, même quand la piste s'arrête"));
    if (auto* bt = dynamic_cast<juce::TextButton*> (comps[3])) bt->setTooltip (IDG_T ("Capture : attrape une mesure (au prochain temps) et la boucle, même quand la piste s'arrête"));
    if (auto* bt = dynamic_cast<juce::TextButton*> (comps[0])) bt->setTooltip (IDG_T ("Détecte les attaques pour relancer les effets glitch (bouton TRANS de chaque effet) et les LFO"));
    if (auto* bt = dynamic_cast<juce::TextButton*> (comps[11])) bt->setTooltip (IDG_T ("Mode sans latence : limiteur sans anticipation, pitch/formant simplifiés, Pixel Sort indisponible"));
}

void InputPanel::resized()
{
    sections.clear();
    auto r = getLocalBounds().reduced (8, 6);
    const int colW = r.getWidth() / 4;
    auto col = [&] (const juce::String& title) { auto c = r.removeFromLeft (colW).reduced (4, 0); sections.push_back ({ title, c }); return c.withTrimmedTop (18); };
    auto btn = [] (juce::Component* c, juce::Rectangle<int>& a) { c->setBounds (a.removeFromTop (24)); a.removeFromTop (6); };
    auto knobPair = [] (juce::Component* a, juce::Component* b, juce::Rectangle<int>& area)
    {
        auto row = area.removeFromTop (82);
        a->setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (2, 0));
        if (b) b->setBounds (row.reduced (2, 0));
        area.removeFromTop (4);
    };

    auto c1 = col ("// TRANSITOIRES / SIDECHAIN");
    btn (comps[0], c1);
    btn (comps[2], c1);
    knobPair (comps[1], nullptr, c1);

    auto c2 = col ("// CAPTURE + FEEDBACK");
    auto cr = c2.removeFromTop (24);
    comps[3]->setBounds (cr.removeFromLeft (cr.getWidth() / 2 - 2));
    cr.removeFromLeft (4);
    comps[4]->setBounds (cr);
    c2.removeFromTop (22);
    knobPair (comps[5], comps[6], c2);
    panic.setBounds (c2.removeFromTop (26));

    auto c3 = col ("// GAMME GLOBALE");
    btn (comps[7], c3);
    auto kr = c3.removeFromTop (24);
    comps[8]->setBounds (kr.removeFromLeft (60));
    kr.removeFromLeft (4);
    comps[9]->setBounds (kr);
    c3.removeFromTop (6);
    btn (comps[10], c3);

    auto c4 = col (IDG_T ("// LATENCE / QUALITÉ"));
    btn (comps[11], c4);
    btn (comps[12], c4);
}

void InputPanel::tick()
{
    const juce::String states[] { "inactive", IDG_T ("armée (prochain temps)"), "enregistre...", "EN BOUCLE" };
    juce::String t = juce::String ("Capture : ") + states[juce::jlimit (0, 3, ctx.proc.getCaptureState())];
    const bool zl = ctx.proc.zeroLatency();
    const int lat = Rack::rackLatency (zl) + (zl ? 0 : (int) std::round (0.0015 * ctx.proc.getSampleRate()));
    juce::String l = IDG_T ("Latence déclarée : ") + juce::String (lat) + IDG_T (" échantillons");
    if (ctx.proc.getSampleRate() > 0) l << " (" << juce::String (lat * 1000.0 / ctx.proc.getSampleRate(), 1) << " ms)";
    if (zl) l << IDG_T ("\nModules dégradés : Pitch Shift, Formant (algo simplifié), Pixel Sort (coupé), limiteur sans anticipation");
    if (t != captureText || l != latencyText) { captureText = t; latencyText = l; repaint(); }
}

void InputPanel::paint (juce::Graphics& g)
{
    for (auto& [title, r] : sections) Theme::sectionTitle (g, title, r.toFloat().withHeight (14), Theme::accent);
    if (sections.size() >= 4)
    {
        g.setFont (Theme::mono (10.0f));
        g.setColour (ctx.proc.getCaptureState() == 3 ? Theme::red : Theme::textDim);
        g.drawText (captureText, sections[1].second.withTrimmedTop (44).withHeight (16).toFloat(), juce::Justification::centredLeft);
        g.setColour (ctx.proc.zeroLatency() ? Theme::blue : Theme::textDim);
        g.drawFittedText (latencyText, sections[3].second.withTrimmedTop (80).withHeight (90), juce::Justification::topLeft, 6);
        g.setColour (Theme::textDim);
        g.drawFittedText (IDG_T ("Sidechain : activez l'entrée sidechain de IDGems dans Ableton (menu Sidechain du plugin). ") +
                          IDG_T ("Notes MIDI à partir de C1 : déclenchent les effets (C1 Stutter, C#1 Beat Repeat, D1 Reverse...)."),
                          sections[0].second.withTrimmedTop (190).withHeight (80), juce::Justification::topLeft, 6);
        g.drawFittedText ("Le Snap Pitch arrondit au demi-ton les modulations de hauteur. Avec la gamme active, Pitch Shift, " +
                          IDG_T ("Freeze, Comb, Ring Mod et Vocoder restent dans la gamme, même en random."),
                          sections[2].second.withTrimmedTop (110).withHeight (80), juce::Justification::topLeft, 6);
    }
}

// ---------------------------------------------------------------------------
OutputPanel::OutputPanel (EditorContext& c) : ctx (c)
{
    const juce::String titles[] { "1 / GAIN + PASSE-HAUT", "2 / GRAVE MONO", "3 / OTT", "4 / SOFT CLIP", "5 / LIMITEUR", "SPECTRE", "SORTIE" };
    const juce::Colour cols[] { Theme::blue, Theme::blue, Theme::accent, Theme::red, Theme::red, Theme::accent, Theme::accent };
    for (int i = 0; i < 7; ++i) addAndMakeVisible (boxes.add (new ModuleBox (titles[i], cols[i])));
    const OutputGraph::Kind kinds[] { OutputGraph::HighPass, OutputGraph::Mono, OutputGraph::Ott, OutputGraph::Clip, OutputGraph::Limiter };
    for (auto k : kinds) addAndMakeVisible (graphs.add (new OutputGraph (c.proc, k)));
    addAndMakeVisible (spectrum);

    auto k = [&] (const juce::String& id, const juce::String& l, juce::Colour col) { return own (*this, comps, new Knob (ctx, ctx.proc.indexOf (id), col, l)); };
    k (G::inGain, "GAIN IN", Theme::blue);      // 0
    k (G::hpFreq, IDG_T ("FRÉQ"), Theme::blue);          // 1
    own (*this, comps, new ParamButton (ctx, G::monoOn, "MONO", Theme::blue)); // 2
    k (G::monoFreq, IDG_T ("FRÉQ"), Theme::blue);        // 3
    k (G::ottDepth, "PROF.", Theme::accent);     // 4
    k (G::ottTime, "TEMPS", Theme::accent);      // 5
    k (G::ottUp, "UP", Theme::accent);           // 6
    k (G::ottDown, "DOWN", Theme::accent);       // 7
    k (G::ottGain, "GAIN", Theme::accent);       // 8
    k (G::clipDrive, "DRIVE", Theme::red);       // 9
    k (G::ceiling, "PLAFOND", Theme::red);       // 10
    k (G::limRelease, "RELEASE", Theme::red);    // 11
    k (G::outGain, "GAIN OUT", Theme::accent);   // 12
    k (G::globalMix, "DRY/WET", Theme::accent);  // 13
    for (auto* b : boxes) b->toBack();
}

void OutputPanel::tick()
{
    for (auto* g : graphs) g->tick();
    spectrum.setSampleRate (ctx.proc.getSampleRate());
    spectrum.repaint();
}

void OutputPanel::resized()
{
    auto r = getLocalBounds().reduced (4, 2);
    auto top = r.removeFromTop (196);
    r.removeFromTop (4);
    const int widths[] { 134, 104, 232, 112, 124 };
    const int gap = (top.getWidth() - (134 + 104 + 232 + 112 + 124)) / 4;
    const int kw = 46, kh = 62;
    for (int i = 0; i < 5; ++i)
    {
        auto b = top.removeFromLeft (widths[i]);
        top.removeFromLeft (gap);
        boxes[i]->setBounds (b);
        auto c = boxes[i]->content().translated (b.getX(), b.getY());
        graphs[i]->setBounds (c.removeFromTop (c.getHeight() - kh - 4));
        c.removeFromTop (4);
        auto knobsRow = c;
        auto place = [&] (int idx, int w) { comps[idx]->setBounds (knobsRow.removeFromLeft (w).withHeight (kh)); };
        switch (i)
        {
            case 0: place (0, 60); place (1, 60); break;
            case 1: comps[2]->setBounds (knobsRow.removeFromLeft (44).withSizeKeepingCentre (44, 22)); place (3, 50); break;
            case 2: for (int j = 4; j <= 8; ++j) place (j, 44); break;
            case 3: place (9, 100); break;
            case 4: place (10, 56); place (11, 56); break;
        }
    }
    auto spec = r.removeFromLeft (r.getWidth() - 136);
    r.removeFromLeft (8);
    boxes[5]->setBounds (spec);
    spectrum.setBounds (boxes[5]->content().translated (spec.getX(), spec.getY()));
    boxes[6]->setBounds (r);
    auto oc = boxes[6]->content().translated (r.getX(), r.getY());
    comps[12]->setBounds (oc.removeFromLeft (oc.getWidth() / 2).withHeight (juce::jmin (70, oc.getHeight())));
    comps[13]->setBounds (oc.withHeight (juce::jmin (70, oc.getHeight())));
}

// ---------------------------------------------------------------------------
struct RecorderPanel::DragTile : public juce::Component, public juce::SettableTooltipClient
{
    DragTile() { setTooltip ("Glissez ce fichier directement dans Ableton (ou l'explorateur)"); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1);
        const bool has = file.existsAsFile();
        g.setColour (has ? Theme::accent.withAlpha (0.18f) : Theme::panelHi);
        g.fillRect (r);
        g.setColour (has ? Theme::accent : Theme::line);
        float dash[] { 6.0f, 4.0f };
        juce::Path p;
        p.addRectangle (r);
        juce::Path d;
        juce::PathStrokeType (1.5f).createDashedStroke (d, p, dash, 2);
        g.fillPath (d);
        g.setColour (has ? Theme::text : Theme::textDim);
        g.setFont (Theme::mono (12.0f, true));
        g.drawFittedText (has ? IDG_T ("≡ ") + file.getFileName() + "\nGLISSER VERS ABLETON" : "Aucun enregistrement pour l'instant",
                          r.reduced (10).toNearestInt(), juce::Justification::centred, 3);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! file.existsAsFile() || e.getDistanceFromDragStart() < 5 || dragging) return;
        dragging = true;
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this, [this] { dragging = false; });
    }
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
    juce::File file;
    bool dragging = false;
};

RecorderPanel::RecorderPanel (EditorContext& c) : ctx (c)
{
    tile = std::make_unique<DragTile>();
    addAndMakeVisible (*tile);
    seconds.setSliderStyle (juce::Slider::LinearBar);
    seconds.setRange (5.0, 60.0, 1.0);
    seconds.setTextValueSuffix (" s");
    seconds.setColour (juce::Slider::trackColourId, Theme::accent.withAlpha (0.6f));
    seconds.setColour (juce::Slider::textBoxTextColourId, Theme::text);
    float saved = 30.0f;
    if (auto* f = ctx.proc.appProps.getUserSettings()) saved = (float) f->getDoubleValue ("rollingSeconds", 30.0);
    seconds.setValue (saved, juce::dontSendNotification);
    seconds.onValueChange = [this] { if (auto* f = ctx.proc.appProps.getUserSettings()) f->setValue ("rollingSeconds", seconds.getValue()); };
    seconds.setTooltip (IDG_T ("Durée du rolling recorder : il garde en permanence les dernières secondes jouées"));
    addAndMakeVisible (seconds);
    saveLast.setTooltip (IDG_T ("Écrit les dernières secondes dans un WAV (nommé avec le BPM et la date)"));
    saveLast.onClick = [this]
    {
        auto f = ctx.proc.recorder.saveRolling ((float) seconds.getValue(), ctx.proc.getBpm());
        if (f.existsAsFile()) setLastFile (f);
        else if (ctx.flash) ctx.flash (IDG_T ("Rien à sauver pour l'instant"));
    };
    rec.setClickingTogglesState (true);
    rec.setColour (juce::TextButton::buttonOnColourId, Theme::red);
    rec.setTooltip ("Enregistrement manuel de la sortie");
    rec.onClick = [this]
    {
        if (rec.getToggleState()) { if (! ctx.proc.recorder.startRecording (ctx.proc.getBpm())) rec.setToggleState (false, juce::dontSendNotification); }
        else setLastFile (ctx.proc.recorder.stopRecording());
    };
    openFolder.onClick = [] { idg::Recorder::folder().startAsProcess(); };
    for (auto* b : { &saveLast, &rec, &openFolder }) addAndMakeVisible (b);
    startTimerHz (5);
}

RecorderPanel::~RecorderPanel() { stopTimer(); }

void RecorderPanel::setLastFile (const juce::File& f)
{
    tile->file = f;
    tile->repaint();
    if (ctx.flash && f.existsAsFile()) ctx.flash (IDG_T ("Enregistré : ") + f.getFileName());
}

void RecorderPanel::timerCallback()
{
    if (ctx.proc.recorder.isRecording()) repaint (recArea);
}

void RecorderPanel::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    auto left = r.removeFromLeft (r.getWidth() / 2 - 6);
    rollArea = left.removeFromTop (100);
    auto ra = rollArea.withTrimmedTop (18);
    seconds.setBounds (ra.removeFromTop (24));
    ra.removeFromTop (8);
    saveLast.setBounds (ra.removeFromTop (28));
    left.removeFromTop (8);
    recArea = left.removeFromTop (90);
    auto rr = recArea.withTrimmedTop (18);
    rec.setBounds (rr.removeFromLeft (90).withHeight (40));
    left.removeFromTop (8);
    openFolder.setBounds (left.removeFromTop (24).withWidth (160));
    r.removeFromLeft (12);
    tileArea = r;
    tile->setBounds (r.withTrimmedTop (18));
}

void RecorderPanel::paint (juce::Graphics& g)
{
    Theme::sectionTitle (g, "// ROLLING RECORDER (TOUJOURS ACTIF)", rollArea.toFloat().withHeight (14), Theme::accent);
    Theme::sectionTitle (g, "// ENREGISTREMENT MANUEL", recArea.toFloat().withHeight (14), Theme::accent);
    Theme::sectionTitle (g, "// DERNIER FICHIER", tileArea.toFloat().withHeight (14), Theme::accent);
    if (ctx.proc.recorder.isRecording())
    {
        g.setColour (Theme::red);
        g.setFont (Theme::mono (14.0f, true));
        g.drawText (IDG_T ("● ") + juce::String (ctx.proc.recorder.recordedSeconds(), 1) + " s", recArea.withTrimmedTop (18).withTrimmedLeft (100).toFloat(), juce::Justification::centredLeft);
    }
    g.setColour (Theme::textDim);
    g.setFont (Theme::mono (9.5f));
    g.drawFittedText ("Dossier : " + idg::Recorder::folder().getFullPathName(), recArea.withTrimmedTop (64).withHeight (24), juce::Justification::topLeft, 2);
}
} // namespace idg::ui
