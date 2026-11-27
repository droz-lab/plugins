#include "RightPanel.h"

namespace idg::ui
{
struct RightPanel::BigRandomButton : public juce::Component, public juce::SettableTooltipClient
{
    std::function<void()> onClick;
    bool pending = false;
    float flashAmt = 0.0f;
    juce::Random rng;
    BigRandomButton() { setTooltip (IDG_T ("RANDOM (touche R). Dosage, portée et synchro réglables dessous.")); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1);
        g.setColour (Theme::accent.withAlpha (0.12f + flashAmt * 0.5f));
        g.fillRect (r);
        g.setColour (Theme::accent);
        g.drawRect (r, 1.5f);
        // fractures
        for (int i = 0; i < 4; ++i)
        {
            float y = r.getY() + rng.nextFloat() * r.getHeight();
            g.setColour ((i % 2 ? Theme::blue : Theme::accent).withAlpha (0.25f + flashAmt * 0.4f));
            g.fillRect (r.getX() + rng.nextFloat() * r.getWidth() * 0.5f, y, r.getWidth() * rng.nextFloat() * 0.6f, 1.5f);
        }
        Theme::cutText (g, pending ? "EN ATTENTE" : "RANDOM", r, pending ? 13.0f : 18.0f, juce::Colour (0xffeeeae6),
                        juce::Justification::centred, 0.35f + flashAmt * 0.6f, true);
    }
    void mouseDown (const juce::MouseEvent&) override { flashAmt = 1.0f; if (onClick) onClick(); repaint(); }
    void tick (bool isPending)
    {
        if (flashAmt > 0.0f || isPending != pending) { flashAmt = juce::jmax (0.0f, flashAmt - 0.08f); pending = isPending; repaint(); }
    }
};

RightPanel::RightPanel (EditorContext& c) : ctx (c), meters (c.proc)
{
    auto& proc = ctx.proc;
    chaos = std::make_unique<Knob> (ctx, proc.indexOf (G::chaos), Theme::red, "CHAOS");
    chaos->setBig (true);
    addAndMakeVisible (*chaos);
    for (int i = 0; i < NumMacros; ++i)
    {
        auto* k = macros.add (new Knob (ctx, proc.indexOf (G::macro (i)), sourceColour (SrcMacro0), "M" + juce::String (i + 1)));
        addAndMakeVisible (k);
        auto* h = macroHandles.add (new ModSourceButton (ctx, SrcMacro0 + i));
        addAndMakeVisible (h);
    }
    bigRandom = std::make_unique<BigRandomButton>();
    bigRandom->onClick = [this] { doRandom(); };
    addAndMakeVisible (*bigRandom);

    auto& s = proc.random.settings;
    scope.addItemList ({ IDG_T ("Général"), IDG_T ("Famille sélectionnée"), IDG_T ("Effet sélectionné"), IDG_T ("Séquenceurs") }, 1);
    scope.setSelectedItemIndex (0, juce::dontSendNotification);
    scope.setTooltip (IDG_T ("Portée du bouton RANDOM"));
    sync.addItemList ({ IDG_T ("Immédiat"), "Prochain temps", "Prochaine mesure" }, 1);
    sync.setSelectedItemIndex (s.sync, juce::dontSendNotification);
    sync.setTooltip (IDG_T ("Random calé sur le temps"));
    sync.onChange = [this] { ctx.proc.random.settings.sync = sync.getSelectedItemIndex(); };
    mutateBars.addItemList ({ "1 mes.", "2 mes.", "4 mes.", "8 mes.", "16 mes." }, 1);
    mutateBars.setTooltip ("Intervalle du Mutate auto");
    mutateBars.onChange = [this] { ctx.proc.random.settings.mutateBars = mutateBars.getSelectedItemIndex(); };

    auto setupSlider = [this] (juce::Slider& sl, double lo, double hi, const juce::String& tip, std::function<void (double)> fn)
    {
        sl.setSliderStyle (juce::Slider::LinearBar);
        sl.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        sl.setRange (lo, hi, 0.0);
        sl.setTooltip (tip);
        sl.setColour (juce::Slider::trackColourId, Theme::accent.withAlpha (0.65f));
        sl.onValueChange = [&sl, fn] { fn (sl.getValue()); };
        addAndMakeVisible (sl);
    };
    setupSlider (amount, 0.0, 1.0, IDG_T ("Dosage du random : légère variation -> chaos total"), [this] (double v) { ctx.proc.random.settings.amount = (float) v; repaint (randomArea); });
    setupSlider (glide, 0.0, 4000.0, IDG_T ("Glide du random : coupure sèche -> transition lissée (ms)"), [this] (double v) { ctx.proc.random.settings.glideMs = (float) v; repaint (randomArea); });
    glide.setSkewFactorFromMidPoint (600.0);
    setupSlider (driftAmt, 0.0, 1.0, IDG_T ("Amplitude du Drift : les valeurs dérivent autour de leur position"), [this] (double v) { ctx.proc.random.settings.driftAmount = (float) v; });
    setupSlider (driftSpeed, 0.0, 1.0, "Vitesse du Drift", [this] (double v) { ctx.proc.random.settings.driftSpeed = (float) v; });
    setupSlider (mutateAmt, 0.0, 1.0, "Dosage du Mutate auto", [this] (double v) { ctx.proc.random.settings.mutateAmount = (float) v; });

    seed.setFont (Theme::mono (13.0f, true));
    seed.setJustification (juce::Justification::centred);
    seed.setInputRestrictions (7, "0123456789");
    seed.setTooltip (IDG_T ("Seed du dernier random. Tapez un numéro + Entrée pour rejouer ce tirage"));
    seed.onReturnKey = [this]
    {
        auto v = (uint32_t) seed.getText().getLargeIntValue();
        if (v > 0)
        {
            const int sc = scope.getSelectedItemIndex();
            ctx.proc.random.request ((RandomEngine::Scope) sc, sc == 1 ? (int) effectDefs()[(size_t) ctx.selectedFx].family : ctx.selectedFx, v);
        }
        seed.giveAwayKeyboardFocus();
    };
    addAndMakeVisible (seed);

    back.onClick = [this] { ctx.proc.random.back(); };
    fwd.onClick = [this] { ctx.proc.random.forward(); };
    star.onClick = [this] { ctx.proc.random.toggleFavourite(); refresh(); };
    back.setTooltip (IDG_T ("Random précédent"));
    fwd.setTooltip ("Random suivant");
    star.setTooltip ("Favori (le mode APPREND s'en inspire)");
    history.setTooltip ("Liste des randoms");
    history.onClick = [this]
    {
        juce::PopupMenu m;
        const auto& h = ctx.proc.random.getHistory();
        for (int i = (int) h.size() - 1; i >= 0 && i >= (int) h.size() - 40; --i)
            m.addItem (i + 1, (h[(size_t) i].favourite ? "* " : "   ") + juce::String (i + 1) + ". " + h[(size_t) i].label + "  #" + juce::String (h[(size_t) i].seed),
                       true, i == ctx.proc.random.getCursor());
        if (h.empty()) m.addItem (-1, "(vide)", false);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&history), [this] (int r) { if (r > 0) ctx.proc.random.recall (r - 1); });
    };
    for (auto* b : { &drift, &mutate, &learn, &order, &seqs }) { b->setClickingTogglesState (true); addAndMakeVisible (b); }
    drift.setTooltip (IDG_T ("Mode Drift : les paramètres dérivent lentement autour de leur valeur au lieu de sauter"));
    mutate.setTooltip ("Mutate auto : re-randomise toutes les X mesures selon le dosage");
    learn.setTooltip ("Random qui apprend : les tirages penchent vers vos favoris");
    order.setTooltip (IDG_T ("Le random général peut réordonner la chaîne d'effets"));
    seqs.setTooltip (IDG_T ("Le random général touche aussi aux séquenceurs"));
    drift.onClick = [this] { ctx.proc.random.settings.driftOn = drift.getToggleState(); };
    mutate.onClick = [this] { ctx.proc.random.settings.mutate = mutate.getToggleState(); };
    learn.onClick = [this] { ctx.proc.random.settings.learn = learn.getToggleState(); };
    order.onClick = [this] { ctx.proc.random.settings.doOrder = order.getToggleState(); };
    seqs.onClick = [this] { ctx.proc.random.settings.doSeq = seqs.getToggleState(); };
    for (auto* c : std::initializer_list<juce::Component*> { &scope, &sync, &mutateBars, &back, &fwd, &star, &history })
        addAndMakeVisible (c);
    addAndMakeVisible (meters);
    ctx.proc.random.onChange = [this] { refresh(); };
    refresh();
}

RightPanel::~RightPanel() = default;

void RightPanel::doRandom()
{
    const int sc = scope.getSelectedItemIndex();
    const int target = sc == 1 ? (int) effectDefs()[(size_t) ctx.selectedFx].family : ctx.selectedFx;
    ctx.proc.random.request ((RandomEngine::Scope) sc, target);
}

void RightPanel::refresh()
{
    const auto& s = ctx.proc.random.settings;
    amount.setValue (s.amount, juce::dontSendNotification);
    glide.setValue (s.glideMs, juce::dontSendNotification);
    driftAmt.setValue (s.driftAmount, juce::dontSendNotification);
    driftSpeed.setValue (s.driftSpeed, juce::dontSendNotification);
    mutateAmt.setValue (s.mutateAmount, juce::dontSendNotification);
    sync.setSelectedItemIndex (s.sync, juce::dontSendNotification);
    mutateBars.setSelectedItemIndex (s.mutateBars, juce::dontSendNotification);
    drift.setToggleState (s.driftOn, juce::dontSendNotification);
    mutate.setToggleState (s.mutate, juce::dontSendNotification);
    learn.setToggleState (s.learn, juce::dontSendNotification);
    order.setToggleState (s.doOrder, juce::dontSendNotification);
    seqs.setToggleState (s.doSeq, juce::dontSendNotification);
    if (! seed.hasKeyboardFocus (true)) seed.setText (ctx.proc.random.lastSeed() > 0 ? juce::String (ctx.proc.random.lastSeed()) : "-", false);
    star.setToggleState (ctx.proc.random.currentIsFavourite(), juce::dontSendNotification);
    star.setColour (juce::TextButton::buttonOnColourId, Theme::accent);
    const auto& h = ctx.proc.random.getHistory();
    historyText = h.empty() ? "-" : juce::String (ctx.proc.random.getCursor() + 1) + "/" + juce::String ((int) h.size());
    history.setButtonText ("HISTO " + historyText);
    if (ctx.refreshAll) ctx.refreshAll();
    repaint();
}

void RightPanel::tick()
{
    bigRandom->tick (ctx.proc.random.hasPending());
    meters.tick();
}

void RightPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 6);
    chaosArea = r.removeFromTop (128);
    chaos->setBounds (chaosArea.withTrimmedTop (14).withSizeKeepingCentre (110, 114));
    macroArea = r.removeFromTop (150);
    auto m = macroArea.withTrimmedTop (16);
    const int mw = m.getWidth() / 4;
    for (int i = 0; i < NumMacros; ++i)
    {
        auto cell = juce::Rectangle<int> (m.getX() + (i % 4) * mw, m.getY() + (i / 4) * 66, mw, 66);
        macros[i]->setBounds (cell.removeFromTop (50).reduced (2, 0));
        macroHandles[i]->setBounds (cell.reduced (4, 1));
    }
    randomArea = r.removeFromTop (300);
    auto ra = randomArea.withTrimmedTop (16);
    bigRandom->setBounds (ra.removeFromTop (44));
    ra.removeFromTop (6);
    auto row = [&ra] (int h = 22) { auto x = ra.removeFromTop (h); ra.removeFromTop (5); return x; };
    auto r1 = row();
    scope.setBounds (r1.removeFromLeft (r1.getWidth() / 2 + 20)); r1.removeFromLeft (4); sync.setBounds (r1);
    auto r2 = row (20); r2.removeFromLeft (64); amount.setBounds (r2);
    auto r3 = row (20); r3.removeFromLeft (64); glide.setBounds (r3);
    auto r4 = row();
    seed.setBounds (r4.removeFromLeft (84)); r4.removeFromLeft (4);
    back.setBounds (r4.removeFromLeft (24)); r4.removeFromLeft (2);
    fwd.setBounds (r4.removeFromLeft (24)); r4.removeFromLeft (2);
    star.setBounds (r4.removeFromLeft (24)); r4.removeFromLeft (4);
    history.setBounds (r4);
    auto r5 = row (20);
    drift.setBounds (r5.removeFromLeft (60)); r5.removeFromLeft (4);
    driftAmt.setBounds (r5.removeFromLeft (r5.getWidth() / 2 - 2)); r5.removeFromLeft (4); driftSpeed.setBounds (r5);
    auto r6 = row (20);
    mutate.setBounds (r6.removeFromLeft (60)); r6.removeFromLeft (4);
    mutateBars.setBounds (r6.removeFromLeft (70)); r6.removeFromLeft (4); mutateAmt.setBounds (r6);
    auto r7 = row (20);
    const int bw = r7.getWidth() / 3;
    learn.setBounds (r7.removeFromLeft (bw - 2)); r7.removeFromLeft (3);
    order.setBounds (r7.removeFromLeft (bw - 2)); r7.removeFromLeft (3);
    seqs.setBounds (r7);
    r.removeFromTop (4);
    meters.setBounds (r);
}

void RightPanel::paint (juce::Graphics& g)
{
    Theme::sectionTitle (g, "// CHAOS", chaosArea.toFloat().withHeight (14), Theme::red);
    Theme::sectionTitle (g, IDG_T ("// MACROS (glisser ≡ sur un potard)"), macroArea.toFloat().withHeight (14), sourceColour (SrcMacro0));
    Theme::sectionTitle (g, "// RANDOM", randomArea.toFloat().withHeight (14), Theme::accent);
    g.setFont (Theme::mono (10.0f, true));
    g.setColour (Theme::textDim);
    auto lbl = [&] (juce::Slider& s, const juce::String& t)
    {
        g.drawText (t, juce::Rectangle<int> (s.getX() - 64, s.getY(), 62, s.getHeight()), juce::Justification::centredLeft);
    };
    lbl (amount, "DOSAGE");
    lbl (glide, "GLIDE");
    g.setColour (Theme::text);
    g.drawText (juce::String (juce::roundToInt (amount.getValue() * 100)) + "%", amount.getBounds(), juce::Justification::centred);
    g.drawText (glide.getValue() < 1 ? "SEC" : juce::String (juce::roundToInt (glide.getValue())) + " ms", glide.getBounds(), juce::Justification::centred);
    g.setFont (Theme::mono (9.0f));
    g.setColour (Theme::textDim);
    g.drawText ("AMPL.", driftAmt.getBounds(), juce::Justification::centred);
    g.drawText ("VITESSE", driftSpeed.getBounds(), juce::Justification::centred);
    g.drawText ("DOSAGE", mutateAmt.getBounds(), juce::Justification::centred);
}
} // namespace idg::ui
