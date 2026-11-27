#include "EffectPanel.h"
#include "Icons.h"

namespace idg::ui
{
/** Zone de scratch à la souris : position horizontale = position du disque. */
struct EffectPanel::ScratchPad : public juce::Component, public juce::SettableTooltipClient
{
    ScratchPad (EditorContext& c) : ctx (c) { idx = c.proc.indexOf (fxParamId (FxScratch, "pos")); setTooltip (IDG_T ("Scratch à la souris : glissez horizontalement")); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Theme::panelHi.withAlpha (0.8f));
        g.fillRect (r);
        g.setColour (Theme::accent.withAlpha (0.5f));
        g.drawRect (r, 1.0f);
        float v = ctx.proc.getNorm (idx);
        for (int i = 0; i < 24; ++i)
        {
            float x = r.getX() + std::fmod (v * 400.0f + i * 18.0f, r.getWidth());
            g.setColour (Theme::line.brighter (0.4f));
            g.drawVerticalLine ((int) x, r.getY() + 4, r.getBottom() - 4);
        }
        g.setColour (Theme::accent);
        g.fillRect (r.getX() + v * r.getWidth() - 2.0f, r.getY(), 4.0f, r.getHeight());
        g.setFont (Theme::mono (10.0f, true));
        g.drawText ("SCRATCH PAD", r.reduced (6), juce::Justification::topLeft);
    }
    void mouseDown (const juce::MouseEvent& e) override { ctx.proc.param (idx)->beginChangeGesture(); mouseDrag (e); }
    void mouseDrag (const juce::MouseEvent& e) override { ctx.proc.setNorm (idx, (float) e.x / (float) getWidth()); repaint(); }
    void mouseUp (const juce::MouseEvent&) override { ctx.proc.param (idx)->endChangeGesture(); }
    EditorContext& ctx;
    int idx;
};

EffectPanel::EffectPanel (EditorContext& c) : ctx (c), seq (c)
{
    spectrum.showGrid = false;
    spectrum.fillAlpha = 0.12f;
    spectrum.lineAlpha = 0.3f;
    addAndMakeVisible (spectrum);
    addAndMakeVisible (scope);
    scope.setInterceptsMouseClicks (false, false);
    for (auto* b : { &onBtn, &holdBtn, &soloBtn, &rndBtn, &lockBtn, &initBtn, &copyBtn, &pasteBtn, &transBtn })
        addAndMakeVisible (b);
    addAndMakeVisible (seq);

    onBtn.setTooltip (IDG_T ("Allume / éteint l'effet (un effet éteint ne consomme aucun CPU)"));
    holdBtn.setTooltip (IDG_T ("Pad momentané : maintenir = effet actif, relâcher = coupé. En mode Throw : envoi momentané"));
    soloBtn.setTooltip ("Solo de l'effet");
    rndBtn.setTooltip ("Random de cet effet (Maj + R)");
    lockBtn.setTooltip ("Verrouille l'effet : le random ne le touche jamais");
    initBtn.setTooltip (IDG_T ("Remet l'effet à ses valeurs par défaut"));
    copyBtn.setTooltip (IDG_T ("Copie les réglages de l'effet"));
    pasteBtn.setTooltip (IDG_T ("Colle les réglages copiés"));
    transBtn.setTooltip (IDG_T ("Relance l'effet à chaque transitoire détectée (activer la détection dans l'onglet ENTRÉE)"));
    soloBtn.setClickingTogglesState (true);
    lockBtn.setClickingTogglesState (true);
    lockBtn.setColour (juce::TextButton::buttonOnColourId, Theme::red);
    transBtn.setClickingTogglesState (true);

    holdBtn.onStateChange = [this] { ctx.proc.setMomentary (fx, holdBtn.isDown()); };
    soloBtn.onClick = [this] { ctx.proc.rackShared.solo[fx] = soloBtn.getToggleState(); };
    lockBtn.onClick = [this] { ctx.proc.rackShared.fxLocked[fx] = lockBtn.getToggleState(); };
    transBtn.onClick = [this] { ctx.proc.rackShared.transTrig[fx] = transBtn.getToggleState(); };
    rndBtn.onClick = [this] { ctx.proc.random.request (RandomEngine::Scope::Effect, fx); };
    initBtn.onClick = [this] { ctx.proc.pushUndo(); ctx.proc.initEffect (fx); ctx.proc.seqData[fx].clear(); seq.refresh(); };
    copyBtn.onClick = [this] { ctx.fxClipboard = ctx.proc.copyEffect (fx); if (ctx.flash) ctx.flash (IDG_T ("Réglages copiés")); };
    pasteBtn.onClick = [this] { if (ctx.fxClipboard.isValid()) { ctx.proc.pushUndo(); ctx.proc.pasteEffect (fx, ctx.fxClipboard); seq.refresh(); } };
    setEffect (ctx.selectedFx);
}

EffectPanel::~EffectPanel() = default;

void EffectPanel::setEffect (int f)
{
    fx = f;
    rebuild();
}

void EffectPanel::rebuild()
{
    auto& proc = ctx.proc;
    const auto& reg = registry();
    const auto& d = effectDefs()[(size_t) fx];
    const auto fc = familyColour (d.family);
    paramComps.clear();
    scratchPad.reset();

    onAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, fxParamId (fx, "on"), onBtn);
    onBtn.setClickingTogglesState (true);
    onBtn.setColour (juce::TextButton::buttonOnColourId, fc);
    holdBtn.setColour (juce::TextButton::buttonColourId, Theme::panelHi);

    mixKnob = std::make_unique<Knob> (ctx, reg.fxMix[fx], fc, "MIX");
    mixKnob->setBig (true);
    addAndMakeVisible (*mixKnob);

    for (int k = 0; k < reg.fxCount[fx]; ++k)
    {
        const int i = reg.fxFirst[fx] + k;
        const auto& pd = reg.all[(size_t) i];
        if (pd.type == PType::Bool)
        {
            auto* b = new ParamButton (ctx, pd.id, pd.name.fromFirstOccurrenceOf (" ", false, false).toUpperCase(), fc);
            paramComps.add (b);
            addAndMakeVisible (b);
        }
        else
        {
            auto* kb = new Knob (ctx, i, fc);
            paramComps.add (kb);
            addAndMakeVisible (kb);
        }
    }
    if (fx == FxScratch)
    {
        scratchPad = std::make_unique<ScratchPad> (ctx);
        addAndMakeVisible (*scratchPad);
    }
    transBtn.setVisible (d.triggerable);
    seq.setEffect (fx);
    updateHeader();
    resized();
    repaint();
}

void EffectPanel::updateHeader()
{
    auto& proc = ctx.proc;
    const auto& d = effectDefs()[(size_t) fx];
    soloBtn.setToggleState (proc.rackShared.solo[fx].load(), juce::dontSendNotification);
    lockBtn.setToggleState (proc.rackShared.fxLocked[fx].load(), juce::dontSendNotification);
    transBtn.setToggleState (proc.rackShared.transTrig[fx].load(), juce::dontSendNotification);
    const bool zl = proc.zeroLatency();
    juce::String w;
    if (zl && d.noZeroLatency) w = "INDISPONIBLE EN MODE SANS LATENCE";
    else if (zl && d.stft) w = IDG_T ("MODE SANS LATENCE : ALGORITHME SIMPLIFIÉ");
    else if (! zl && d.stft) w = "ALGO SPECTRAL (LATENCE " + juce::String (768) + IDG_T (" ÉCH., COMPENSÉE)");
    if (w != warning) { warning = w; repaint(); }
    const bool throwP = (fx == FxDelay && proc.getValue (fxParamId (FxDelay, "throw")) > 0.5f)
                     || (fx == FxReverb && proc.getValue (fxParamId (FxReverb, "throw")) > 0.5f);
    holdBtn.setButtonText (throwP ? "THROW" : "HOLD");
}

void EffectPanel::tick()
{
    updateHeader();
    seq.tick();
    scope.repaint();
    spectrum.repaint();
}

void EffectPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 6);
    header = r.removeFromTop (56);
    auto controls = header.withTrimmedLeft (286).reduced (0, 4);
    auto row1 = controls.removeFromTop (22);
    controls.removeFromTop (4);
    auto row2 = controls.removeFromTop (22);
    auto place = [] (juce::Rectangle<int>& row, juce::Component& c, int w) { c.setBounds (row.removeFromLeft (w)); row.removeFromLeft (4); };
    place (row1, onBtn, 44);
    place (row1, holdBtn, 60);
    place (row1, soloBtn, 50);
    if (transBtn.isVisible()) place (row1, transBtn, 54);
    place (row2, rndBtn, 70);
    place (row2, lockBtn, 50);
    place (row2, initBtn, 46);
    place (row2, copyBtn, 64);
    place (row2, pasteBtn, 64);

    auto seqArea = r.removeFromBottom (128);
    seq.setBounds (seqArea);
    r.removeFromBottom (6);
    body = r;
    scope.setBounds (body.reduced (2));
    spectrum.setBounds (body.reduced (2));

    auto kr = body.reduced (4, 10);
    if (scratchPad != nullptr) { scratchPad->setBounds (kr.removeFromBottom (44)); kr.removeFromBottom (6); }
    mixKnob->setBounds (kr.removeFromLeft (92).withSizeKeepingCentre (88, juce::jmin (106, kr.getHeight())));
    kr.removeFromLeft (8);
    const int n = paramComps.size();
    if (n == 0) return;
    const int cols = juce::jmin (n, 8);
    const int rows = (n + cols - 1) / cols;
    const int cw = kr.getWidth() / juce::jmax (cols, 6);
    const int ch = juce::jmin (90, kr.getHeight() / rows);
    for (int i = 0; i < n; ++i)
    {
        const int row = i / cols, col = i % cols;
        juce::Rectangle<int> cellR (kr.getX() + col * cw, kr.getY() + row * ch + (kr.getHeight() - rows * ch) / 2, cw, ch);
        if (dynamic_cast<Knob*> (paramComps[i]) != nullptr) paramComps[i]->setBounds (cellR.reduced (4, 0));
        else paramComps[i]->setBounds (cellR.withSizeKeepingCentre (cw - 12, 24));
    }
}

void EffectPanel::paint (juce::Graphics& g)
{
    const auto& d = effectDefs()[(size_t) fx];
    const auto fc = familyColour (d.family);
    Theme::panelFrame (g, getLocalBounds().toFloat().reduced (2.0f), fc);
    auto h = header.toFloat();
    // logo de l'effet dans un cadre décalé
    auto iconBox = h.withWidth (50.0f).withHeight (50.0f).translated (0, 3);
    g.setColour (Theme::bg.withAlpha (0.8f));
    g.fillRect (iconBox);
    g.setColour (fc.withAlpha (0.6f));
    g.drawRect (iconBox, 1.0f);
    g.setColour (juce::Colour (0xffff1e3c).withAlpha (0.3f));
    g.drawRect (iconBox.translated (-2.0f, 1.0f), 1.0f);
    drawEffectIcon (g, fx, iconBox.reduced (8.0f), fc, 2.2f, true);
    auto txt = h.withTrimmedLeft (58.0f).withWidth (220.0f);
    Theme::tapeLabel (g, familyNames()[d.family], txt.getX(), txt.getY() + 2.0f, fc, 12.0f);
    Theme::cutText (g, d.name.toUpperCase(), txt.withTrimmedTop (16).withHeight (26), 16.0f, juce::Colour (0xffeeeae6),
                    juce::Justification::centredLeft, 0.3f, true);
    g.setFont (Theme::sans (10.5f));
    g.setColour (Theme::textDim);
    g.drawText (d.help, txt.withTrimmedTop (42), juce::Justification::topLeft);
    if (warning.isNotEmpty())
    {
        g.setFont (Theme::mono (10.0f, true));
        g.setColour (warning.startsWith ("INDISPO") ? Theme::red : Theme::blue);
        g.drawText (warning, body.toFloat().reduced (8, 4), juce::Justification::topRight);
    }
}
} // namespace idg::ui
