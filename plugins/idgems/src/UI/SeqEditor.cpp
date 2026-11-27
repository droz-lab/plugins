#include "SeqEditor.h"

namespace idg::ui
{
SeqEditor::SeqEditor (EditorContext& c) : ctx (c)
{
    for (auto* b : { &enable, &rnd, &lock, &clear, &fill }) addAndMakeVisible (b);
    enable.setClickingTogglesState (true);
    lock.setClickingTogglesState (true);
    lock.setColour (juce::TextButton::buttonOnColourId, Theme::red);
    enable.setTooltip (IDG_T ("Active le séquenceur on/off de cet effet (calé sur le tempo)"));
    rnd.setTooltip (IDG_T ("Random du séquenceur de cet effet"));
    lock.setTooltip (IDG_T ("Verrouille ce séquenceur : le random n'y touche plus"));
    clear.setTooltip ("Efface tous les pas");
    fill.setTooltip ("Allume tous les pas");
    enable.onClick = [this] { ctx.proc.seqData[fx].enabled = enable.getToggleState(); };
    lock.onClick = [this] { ctx.proc.seqData[fx].locked = lock.getToggleState(); };
    rnd.onClick = [this]
    {
        auto& s = ctx.proc.seqData[fx];
        if (s.locked.load()) return;
        juce::Random r;
        const float density = 0.25f + r.nextFloat() * 0.6f;
        for (int k = 0; k < MaxSteps; ++k)
        {
            s.on[k] = r.nextFloat() < density;
            s.prob[k] = r.nextFloat() < 0.3f ? 0.4f + r.nextFloat() * 0.6f : 1.0f;
            s.ratchet[k] = r.nextFloat() < 0.15f ? 2 + r.nextInt (3) : 1;
        }
        s.enabled = true;
        refresh();
    };
    clear.onClick = [this] { auto& s = ctx.proc.seqData[fx]; for (int k = 0; k < MaxSteps; ++k) { s.on[k] = false; s.ratchet[k] = 1; s.prob[k] = 1.0f; } repaint(); };
    fill.onClick = [this] { auto& s = ctx.proc.seqData[fx]; for (int k = 0; k < MaxSteps; ++k) s.on[k] = true; repaint(); };

    rate.addItemList (seqRateNames(), 1);
    rate.setTooltip (IDG_T ("Résolution du séquenceur (jusqu'au 1/32)"));
    rate.onChange = [this] { ctx.proc.seqData[fx].rate = rate.getSelectedItemIndex(); };
    addAndMakeVisible (rate);

    for (auto* s : { &length, &swing })
    {
        s->setSliderStyle (juce::Slider::LinearBar);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setColour (juce::Slider::trackColourId, Theme::blue.withAlpha (0.6f));
        addAndMakeVisible (s);
    }
    length.setRange (1, MaxSteps, 1);
    length.setTooltip (IDG_T ("Longueur du motif (indépendante pour chaque effet : polyrythmie)"));
    length.onValueChange = [this] { ctx.proc.seqData[fx].length = (int) length.getValue(); lengthLabel.setText ("LEN " + juce::String ((int) length.getValue()), juce::dontSendNotification); repaint(); };
    swing.setRange (0.0, 1.0, 0.01);
    swing.setTooltip ("Swing");
    swing.onValueChange = [this] { ctx.proc.seqData[fx].swing = (float) swing.getValue(); swingLabel.setText ("SWING " + juce::String (juce::roundToInt (swing.getValue() * 100)) + "%", juce::dontSendNotification); };
    for (auto* l : { &lengthLabel, &swingLabel })
    {
        l->setFont (Theme::mono (10.0f, true));
        l->setInterceptsMouseClicks (false, false);
        l->setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    }
}

void SeqEditor::setEffect (int f) { fx = f; refresh(); }

void SeqEditor::refresh()
{
    auto& s = ctx.proc.seqData[fx];
    enable.setToggleState (s.enabled.load(), juce::dontSendNotification);
    lock.setToggleState (s.locked.load(), juce::dontSendNotification);
    rate.setSelectedItemIndex (s.rate.load(), juce::dontSendNotification);
    length.setValue (s.length.load(), juce::dontSendNotification);
    swing.setValue (s.swing.load(), juce::dontSendNotification);
    lengthLabel.setText ("LEN " + juce::String (s.length.load()), juce::dontSendNotification);
    swingLabel.setText ("SWING " + juce::String (juce::roundToInt (s.swing.load() * 100)) + "%", juce::dontSendNotification);
    enable.setColour (juce::TextButton::buttonOnColourId, familyColour (effectDefs()[(size_t) fx].family));
    repaint();
}

void SeqEditor::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (22);
    enable.setBounds (top.removeFromLeft (50)); top.removeFromLeft (6);
    rate.setBounds (top.removeFromLeft (70)); top.removeFromLeft (6);
    length.setBounds (top.removeFromLeft (90)); lengthLabel.setBounds (length.getBounds()); top.removeFromLeft (6);
    swing.setBounds (top.removeFromLeft (100)); swingLabel.setBounds (swing.getBounds()); top.removeFromLeft (6);
    lock.setBounds (top.removeFromRight (50)); top.removeFromRight (4);
    rnd.setBounds (top.removeFromRight (44)); top.removeFromRight (4);
    clear.setBounds (top.removeFromRight (52)); top.removeFromRight (4);
    fill.setBounds (top.removeFromRight (44));
    r.removeFromTop (6);
    grid = r;
}

juce::Rectangle<float> SeqEditor::cell (int i) const
{
    const float w = (float) grid.getWidth() / MaxSteps;
    return { (float) grid.getX() + i * w, (float) grid.getY(), w - 2.0f, (float) grid.getHeight() - 12.0f };
}

int SeqEditor::stepAt (juce::Point<int> p) const
{
    if (! grid.contains (p)) return -1;
    return juce::jlimit (0, MaxSteps - 1, (int) ((p.x - grid.getX()) / ((float) grid.getWidth() / MaxSteps)));
}

void SeqEditor::tick()
{
    int cur = ctx.proc.seqData[fx].current.load();
    if (cur != lastCurrent) { lastCurrent = cur; repaint (grid); }
}

void SeqEditor::paint (juce::Graphics& g)
{
    auto& s = ctx.proc.seqData[fx];
    const auto fc = familyColour (effectDefs()[(size_t) fx].family);
    const int len = s.length.load();
    const int cur = s.current.load();
    const bool en = s.enabled.load();
    for (int i = 0; i < MaxSteps; ++i)
    {
        auto c = cell (i);
        const bool inLen = i < len;
        g.setColour (Theme::panelHi.withAlpha (inLen ? 1.0f : 0.35f));
        g.fillRect (c);
        if (i % 4 == 0) { g.setColour (Theme::line.brighter (0.3f)); g.fillRect (c.getX(), c.getBottom() + 3, c.getWidth(), 2.0f); }
        if (s.on[i].load())
        {
            const float p = s.prob[i].load();
            auto f = c.withTrimmedTop (c.getHeight() * (1.0f - p));
            g.setColour (fc.withAlpha ((en ? 0.9f : 0.4f) * (inLen ? 1.0f : 0.3f)));
            g.fillRect (f);
            const int rt = s.ratchet[i].load();
            if (rt > 1)
            {
                g.setColour (Theme::bg.withAlpha (0.8f));
                for (int k = 1; k < rt; ++k) g.fillRect (c.getX(), f.getY() + f.getHeight() * k / rt - 1.0f, c.getWidth(), 2.0f);
                g.setColour (Theme::text);
                g.setFont (Theme::mono (9.0f, true));
                g.drawText ("x" + juce::String (rt), c.withHeight (12.0f), juce::Justification::centred);
            }
        }
        if (en && i == cur)
        {
            g.setColour (Theme::text.withAlpha (0.85f));
            g.drawRect (c.expanded (1.0f), 1.5f);
        }
    }
    g.setColour (Theme::textDim);
    g.setFont (Theme::mono (9.0f));
    g.drawText (IDG_T ("clic : on/off   glisser : probabilité   clic droit : ratchets   ctrl+clic : longueur"),
                grid.withTop (grid.getBottom() - 10).toFloat(), juce::Justification::centredRight);
}

void SeqEditor::mouseDown (const juce::MouseEvent& e)
{
    const int i = stepAt (e.getPosition());
    if (i < 0) return;
    auto& s = ctx.proc.seqData[fx];
    if (e.mods.isPopupMenu()) { s.ratchet[i] = s.ratchet[i].load() % 4 + 1; s.on[i] = true; repaint(); return; }
    if (e.mods.isCommandDown() || e.mods.isCtrlDown()) { s.length = i + 1; refresh(); return; }
    s.on[i] = ! s.on[i].load();
    if (s.on[i].load() && s.prob[i].load() < 0.05f) s.prob[i] = 1.0f;
    dragStep = i;
    dragStartProb = s.prob[i].load();
    repaint();
}

void SeqEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStep < 0 || std::abs (e.getDistanceFromDragStartY()) < 3) return;
    auto& s = ctx.proc.seqData[fx];
    s.on[dragStep] = true;
    s.prob[dragStep] = juce::jlimit (0.05f, 1.0f, dragStartProb - (float) e.getDistanceFromDragStartY() / (float) grid.getHeight());
    repaint();
}
} // namespace idg::ui
