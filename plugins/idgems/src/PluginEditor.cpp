#include "PluginEditor.h"
#include "UI/RackView.h"
#include "UI/EffectPanel.h"
#include "UI/ModPanel.h"
#include "UI/PerfPanel.h"
#include "UI/IOPanels.h"
#include "UI/RightPanel.h"
#include "UI/TopBar.h"
#include "UI/PresetBrowser.h"
#include "UI/Visuals.h"
#include "UI/Knob.h"

using namespace idg;
using namespace idg::ui;

/** Contenu à taille fixe, mis à l'échelle par transformation. */
struct IDGemsEditor::Content : public juce::Component
{
    Content (EditorContext& c)
        : top (c), rack (c), fxPanel (c), mod (c), perf (c), input (c), output (c), recorder (c), right (c), browser (c)
    {
        for (auto* comp : std::initializer_list<juce::Component*> { &top, &rack, &fxPanel, &mod, &perf, &input, &output, &recorder, &right })
            addAndMakeVisible (comp);
        const juce::String names[] { "MODULATION", "PERFORMANCE", IDG_T ("ENTRÉE"), "SORTIE", "ENREG" };
        for (int i = 0; i < 5; ++i)
        {
            auto* b = tabs.add (new juce::TextButton (names[i]));
            b->setClickingTogglesState (true);
            b->setRadioGroupId (501);
            addAndMakeVisible (b);
        }
        addAndMakeVisible (overlay);
        addChildComponent (browser);
    }
    void paint (juce::Graphics& g) override
    {
        if (bg.isNull() || bg.getWidth() != getWidth()) bg = makeBackgroundTexture (getWidth(), getHeight(), Theme::accent);
        g.drawImageAt (bg, 0, 0);
        auto b = bottomArea.toFloat();
        Theme::panelFrame (g, b.reduced (2.0f), Theme::accent, 0.9f);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        top.setBounds (r.removeFromTop (46));
        rack.setBounds (r.removeFromLeft (236));
        right.setBounds (r.removeFromRight (312));
        fxPanel.setBounds (r.removeFromTop (420));
        bottomArea = r;
        auto tr = r.reduced (10, 4).removeFromTop (24).translated (0, 4);
        for (auto* t : tabs) { t->setBounds (tr.removeFromLeft (110)); tr.removeFromLeft (4); }
        auto panel = r.reduced (4, 0).withTrimmedTop (32).withTrimmedBottom (4);
        for (auto* p : std::initializer_list<juce::Component*> { &mod, &perf, &input, &output, &recorder }) p->setBounds (panel);
        overlay.setBounds (getLocalBounds());
        browser.setBounds (getLocalBounds());
    }
    TopBar top;
    RackView rack;
    EffectPanel fxPanel;
    ModPanel mod;
    PerfPanel perf;
    InputPanel input;
    OutputPanel output;
    RecorderPanel recorder;
    RightPanel right;
    PresetBrowser browser;
    GlitchOverlay overlay;
    juce::OwnedArray<juce::TextButton> tabs;
    juce::Rectangle<int> bottomArea;
    juce::Image bg;
    int currentTab = 0;
};

IDGemsEditor::IDGemsEditor (IDGemsProcessor& p) : AudioProcessorEditor (p), proc (p), ctx (p)
{
    // Taille mémorisée lue AVANT setResizable / limites : ces appels déclenchent resized()
    // qui écraserait sinon la valeur sauvegardée (fenêtre qui revient petite à la réouverture).
    float remembered = proc.editorScale.load();
    const int rememberedW = proc.editorW.load(), rememberedH = proc.editorH.load();
    if (remembered <= 0.0f)
        if (auto* f = proc.appProps.getUserSettings()) remembered = (float) f->getDoubleValue ("uiScale", 0.85);
    Theme::accent = proc.getAccent();
    if (auto* f = proc.appProps.getUserSettings())
    {
        Theme::tooltips = f->getBoolValue ("tooltips", true);
        Theme::glitchAmount = (float) f->getDoubleValue ("glitch", 0.5);
    }
    applyAccent (Theme::accent);
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);

    ctx.selectEffect = [this] (int fx) { selectEffect (fx); };
    ctx.selectSource = [this] (int s) { content->mod.selectSource (s); selectTab (0); };
    ctx.refreshAll = [this] { refreshAll(); };
    ctx.flash = [this] (const juce::String& m) { if (content) content->top.flash (m); };

    content = std::make_unique<Content> (ctx);
    addAndMakeVisible (*content);
    content->setSize (baseW, baseH);

    for (int i = 0; i < content->tabs.size(); ++i) content->tabs[i]->onClick = [this, i] { selectTab (i); };
    content->top.onOpenBrowser = [this] { content->browser.open(); };
    content->top.onSave = [this] { content->browser.openSave(); };
    content->top.onSettings = [this] { showSettings(); };
    content->top.onScale = [this] (float s) { setScale (s); };
    if (proc.presets) proc.presets->onChange = [this] { refreshAll(); };

    juce::Component::SafePointer<IDGemsEditor> safe (this);
    proc.onStateRestored = [safe]
    {
        juce::MessageManager::callAsync ([safe]
        {
            if (safe != nullptr)
            {
                safe->content->mod.selectSource (safe->ctx.selectedSource);
                safe->content->right.refresh();
                safe->refreshAll();
            }
        });
    };

    tooltip = std::make_unique<juce::TooltipWindow> (this, 700);
    selectTab (0);

    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) baseW / baseH);
        c->setSizeLimits (baseW * 6 / 10, baseH * 6 / 10, baseW * 2, baseH * 2);
    }
    const float scale = juce::jlimit (0.6f, 2.0f, remembered > 0.0f ? remembered : 0.85f);
    // Taille du projet, au pixel pres, si elle a ete sauvegardee ; sinon l'echelle retenue.
    if (rememberedW > 0 && rememberedH > 0)
        setSize (rememberedW, rememberedH);
    else
        setSize ((int) (baseW * scale), (int) (baseH * scale));
    sizeReady = true;
    setWantsKeyboardFocus (true);
    startTimerHz (30);
}

IDGemsEditor::~IDGemsEditor()
{
    stopTimer();
    proc.onStateRestored = nullptr;
    proc.random.onChange = nullptr;
    if (proc.presets) proc.presets->onChange = nullptr;
    tooltip.reset();
    content.reset();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void IDGemsEditor::paint (juce::Graphics& g) { g.fillAll (Theme::bg); }

void IDGemsEditor::resized()
{
    const float s = (float) getWidth() / (float) baseW;
    content->setTransform (juce::AffineTransform::scale (s));
    if (! sizeReady) return; // ne pas sauvegarder les tailles transitoires de la construction
    proc.editorScale = s;
    proc.editorW = getWidth();
    proc.editorH = getHeight();
    if (auto* f = proc.appProps.getUserSettings()) f->setValue ("uiScale", s);
}

void IDGemsEditor::setScale (float s) { setSize ((int) (baseW * s), (int) (baseH * s)); }

void IDGemsEditor::selectEffect (int fx)
{
    ctx.selectedFx = fx;
    content->fxPanel.setEffect (fx);
    content->rack.repaint();
}

void IDGemsEditor::selectTab (int t)
{
    content->currentTab = t;
    content->tabs[t]->setToggleState (true, juce::dontSendNotification);
    content->mod.setVisible (t == 0);
    content->perf.setVisible (t == 1);
    content->input.setVisible (t == 2);
    content->output.setVisible (t == 3);
    content->recorder.setVisible (t == 4);
}

void IDGemsEditor::refreshAll()
{
    if (! content) return;
    content->rack.repaint();
    content->fxPanel.refresh();
    content->perf.refresh();
    content->mod.refreshConnections();
    content->mod.repaint();
    content->top.repaint();
}

void IDGemsEditor::timerCallback()
{
    // oscilloscope
    int s1, n1, s2, n2;
    const int ready = proc.scopeFifo.getNumReady();
    proc.scopeFifo.prepareToRead (ready, s1, n1, s2, n2);
    int n = 0;
    for (int i = 0; i < n1; ++i) scopeTmp[(size_t) n++] = proc.scopeData[(size_t) (s1 + i)];
    for (int i = 0; i < n2; ++i) scopeTmp[(size_t) n++] = proc.scopeData[(size_t) (s2 + i)];
    proc.scopeFifo.finishedRead (n1 + n2);
    if (n > 0)
    {
        content->fxPanel.scope.push (scopeTmp.data(), n);
        content->fxPanel.spectrum.setSampleRate (proc.getSampleRate());
        content->fxPanel.spectrum.push (scopeTmp.data(), n);
        content->output.spectrum.push (scopeTmp.data(), n);
    }

    const float level = proc.outLevel.load();
    const float chaos = proc.getValue (G::chaos);
    content->fxPanel.scope.setIntensity (juce::jlimit (0.0f, 1.0f, level * (0.3f + chaos) * Theme::glitchAmount * 1.5f));
    content->overlay.setLevel (level * (0.4f + chaos));

    for (auto* k : ctx.knobs) if (k->isShowing()) k->tick();
    content->rack.tick();
    content->fxPanel.tick();
    content->right.tick();
    content->top.tick();
    switch (content->currentTab)
    {
        case 0: content->mod.tick(); break;
        case 1: content->perf.tick(); break;
        case 2: content->input.tick(); break;
        case 3: content->output.tick(); break;
        default: break;
    }
}

bool IDGemsEditor::keyPressed (const juce::KeyPress& k)
{
    const auto mods = k.getModifiers();
    const auto c = juce::CharacterFunctions::toLowerCase (k.getTextCharacter());
    if (mods.isCommandDown() && k.getKeyCode() == 'Z') { if (mods.isShiftDown()) proc.redo(); else proc.undo(); return true; }
    if (mods.isCommandDown() && k.getKeyCode() == 'Y') { proc.redo(); return true; }
    if (mods.isCommandDown() && k.getKeyCode() == 'C') { ctx.fxClipboard = proc.copyEffect (ctx.selectedFx); ctx.flash (IDG_T ("Effet copié")); return true; }
    if (mods.isCommandDown() && k.getKeyCode() == 'V') { proc.pushUndo(); proc.pasteEffect (ctx.selectedFx, ctx.fxClipboard); refreshAll(); return true; }
    if (k.getKeyCode() == juce::KeyPress::escapeKey && content->browser.isVisible()) { content->browser.setVisible (false); return true; }
    if (mods.isCommandDown() || mods.isAltDown()) return false;
    if (c == 'r')
    {
        if (mods.isShiftDown()) proc.random.request (RandomEngine::Scope::Effect, ctx.selectedFx);
        else content->right.doRandom();
        return true;
    }
    if (c == 'l' && ctx.hoveredParam >= 0)
    {
        auto& b = proc.bank[ctx.hoveredParam];
        b.locked = ! b.locked.load();
        ctx.flash ((b.locked.load() ? IDG_T ("Verrouillé : ") : IDG_T ("Déverrouillé : ")) + ctx.def (ctx.hoveredParam).name);
        for (auto* kn : ctx.knobs) kn->repaint();
        return true;
    }
    if (c == 'p') { proc.panic(); ctx.flash ("PANIC"); return true; }
    if (k.getKeyCode() == juce::KeyPress::leftKey && proc.presets) { proc.presets->next (-1); return true; }
    if (k.getKeyCode() == juce::KeyPress::rightKey && proc.presets) { proc.presets->next (1); return true; }
    return false;
}

void IDGemsEditor::applyAccent (juce::Colour c)
{
    Theme::accent = c;
    lnf.setColour (juce::PopupMenu::highlightedBackgroundColourId, c.withAlpha (0.85f));
    lnf.setColour (juce::PopupMenu::headerTextColourId, c);
    lnf.setColour (juce::ComboBox::arrowColourId, c);
    lnf.setColour (juce::TextButton::buttonOnColourId, c);
    lnf.setColour (juce::TextEditor::focusedOutlineColourId, c);
    lnf.setColour (juce::Slider::thumbColourId, c);
    lnf.setColour (juce::Slider::trackColourId, c);
    lnf.setColour (juce::CaretComponent::caretColourId, c);
    if (content)
    {
        content->bg = {};
        content->repaint();
    }
}

void IDGemsEditor::showSettings()
{
    juce::PopupMenu m;
    juce::PopupMenu accent;
    const std::pair<juce::String, juce::uint32> colours[] {
        { IDG_T ("Orange (défaut)"), 0xffff6a00 }, { "Rouge", 0xffff2d55 }, { "Magenta", 0xffff2fd0 }, { "Cyan", 0xff2fe6ff },
        { "Vert acide", 0xff9dff2f }, { "Or", 0xffffc21a }, { "Blanc", 0xffeeeeee } };
    int id = 100;
    for (auto& [name, col] : colours) accent.addItem (id++, name, true, Theme::accent == juce::Colour (col));
    accent.addItem (150, IDG_T ("Couleur personnalisée..."));
    m.addSubMenu ("Couleur d'accent", accent);
    m.addItem (1, "Bulles d'aide", true, Theme::tooltips);
    juce::PopupMenu glitch;
    const float levels[] { 0.0f, 0.25f, 0.5f, 1.0f };
    const juce::String glitchNames[] { IDG_T ("Désactivé"), IDG_T ("Léger"), "Moyen", "Fort" };
    for (int i = 0; i < 4; ++i) glitch.addItem (200 + i, glitchNames[i], true, std::abs (Theme::glitchAmount - levels[i]) < 0.01f);
    m.addSubMenu (IDG_T ("Glitch visuel (réaction au son)"), glitch);
    m.addSeparator();
    m.addItem (2, "Ouvrir le dossier des presets");
    m.addItem (3, "Ouvrir le dossier des enregistrements");
    m.addSeparator();
    m.addItem (4, IDG_T ("Tout réinitialiser (Init)"));
    m.addItem (5, "Raccourcis clavier...");

    std::vector<juce::uint32> cols;
    for (auto& c : colours) cols.push_back (c.second);
    m.showMenuAsync (juce::PopupMenu::Options(), [this, cols, levels] (int r)
    {
        auto* f = proc.appProps.getUserSettings();
        if (r >= 100 && r < 150)
        {
            juce::Colour c (cols[(size_t) (r - 100)]);
            applyAccent (c);
            proc.setAccent (c);
        }
        else if (r == 150)
        {
            auto sel = std::make_unique<juce::ColourSelector> (juce::ColourSelector::showColourspace | juce::ColourSelector::showSliders);
            sel->setCurrentColour (Theme::accent);
            sel->setSize (280, 260);
            auto* raw = sel.get();
            juce::Component::SafePointer<IDGemsEditor> safe (this);
            struct Listener : juce::ChangeListener
            {
                std::function<void()> fn;
                void changeListenerCallback (juce::ChangeBroadcaster*) override { fn(); }
            };
            auto* l = new Listener();
            l->fn = [safe, raw] { if (safe) { safe->applyAccent (raw->getCurrentColour()); safe->proc.setAccent (raw->getCurrentColour()); } };
            raw->addChangeListener (l);
            struct Holder : juce::Component
            {
                std::unique_ptr<juce::ColourSelector> s; std::unique_ptr<Listener> lis;
                void resized() override { s->setBounds (getLocalBounds()); }
                ~Holder() override { s->removeAllChangeListeners(); }
            };
            auto h = std::make_unique<Holder>();
            h->s = std::move (sel);
            h->lis.reset (l);
            h->addAndMakeVisible (*h->s);
            h->setSize (280, 260);
            juce::CallOutBox::launchAsynchronously (std::move (h), content->top.getScreenBounds().withWidth (80).withX (getScreenX() + getWidth() - 100), nullptr);
        }
        else if (r == 1) { Theme::tooltips = ! Theme::tooltips; if (f) f->setValue ("tooltips", Theme::tooltips); }
        else if (r >= 200 && r < 204) { Theme::glitchAmount = levels[r - 200]; if (f) f->setValue ("glitch", Theme::glitchAmount); }
        else if (r == 2) PresetManager::folder().startAsProcess();
        else if (r == 3) Recorder::folder().startAsProcess();
        else if (r == 4) { proc.pushUndo(); proc.resetToInit(); }
        else if (r == 5)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Raccourcis clavier",
                IDG_T ("R : random (portée choisie)\nMaj + R : random de l'effet sélectionné\nL : verrouiller / déverrouiller le paramètre survolé\n") +
                IDG_T ("P : panic\nCtrl + Z / Ctrl + Y : annuler / rétablir\nCtrl + C / Ctrl + V : copier / coller les réglages de l'effet\n") +
                IDG_T ("Flèches gauche / droite : preset précédent / suivant\nÉchap : fermer le navigateur\n\n") +
                IDG_T ("Sur un potard : double-clic = défaut, clic droit = menu, Alt + glisser = profondeur de modulation."));
        if (f) f->saveIfNeeded();
        repaint();
    });
}
