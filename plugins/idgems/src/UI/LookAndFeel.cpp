#include "LookAndFeel.h"

namespace idg::ui
{
IDGLookAndFeel::IDGLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Theme::bg);
    setColour (juce::PopupMenu::backgroundColourId, Theme::panel);
    setColour (juce::PopupMenu::textColourId, Theme::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::accent.withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    setColour (juce::PopupMenu::headerTextColourId, Theme::accent);
    setColour (juce::ComboBox::textColourId, Theme::text);
    setColour (juce::ComboBox::arrowColourId, Theme::accent);
    setColour (juce::ComboBox::backgroundColourId, Theme::panelHi);
    setColour (juce::ComboBox::outlineColourId, Theme::line);
    setColour (juce::TextButton::textColourOffId, Theme::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::TextButton::buttonColourId, Theme::panelHi);
    setColour (juce::TextButton::buttonOnColourId, Theme::accent);
    setColour (juce::Label::textColourId, Theme::text);
    setColour (juce::TextEditor::textColourId, Theme::text);
    setColour (juce::TextEditor::backgroundColourId, Theme::panelHi);
    setColour (juce::TextEditor::highlightColourId, Theme::accent.withAlpha (0.4f));
    setColour (juce::TextEditor::outlineColourId, Theme::line);
    setColour (juce::TextEditor::focusedOutlineColourId, Theme::accent);
    setColour (juce::CaretComponent::caretColourId, Theme::accent);
    setColour (juce::Slider::thumbColourId, Theme::accent);
    setColour (juce::Slider::trackColourId, Theme::accent);
    setColour (juce::Slider::backgroundColourId, Theme::line);
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::thumbColourId, Theme::accent.withAlpha (0.5f));
    setColour (juce::TooltipWindow::backgroundColourId, Theme::panel);
    setColour (juce::TooltipWindow::textColourId, Theme::text);
    setColour (juce::AlertWindow::backgroundColourId, Theme::panel);
    setColour (juce::AlertWindow::textColourId, Theme::text);
    setColour (juce::AlertWindow::outlineColourId, Theme::accent);
}

void IDGLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    auto c = b.findColour (on ? juce::TextButton::buttonOnColourId : juce::TextButton::buttonColourId);
    if (! b.isEnabled()) c = c.withAlpha (0.3f);
    if (down) c = c.brighter (0.2f);
    else if (over) c = c.brighter (0.08f);
    juce::Path p;
    const float cut = juce::jmin (5.0f, r.getHeight() * 0.3f);
    p.startNewSubPath (r.getX() + cut, r.getY());
    p.lineTo (r.getRight(), r.getY());
    p.lineTo (r.getRight(), r.getBottom() - cut);
    p.lineTo (r.getRight() - cut, r.getBottom());
    p.lineTo (r.getX(), r.getBottom());
    p.lineTo (r.getX(), r.getY() + cut);
    p.closeSubPath();
    g.setColour (c);
    g.fillPath (p);
    g.setColour (on ? c.brighter (0.4f) : Theme::line.brighter (over ? 0.4f : 0.0f));
    g.strokePath (p, juce::PathStrokeType (1.0f));
    if (on)
    {
        // légère aberration sur le contour
        g.setColour (Theme::blue.withAlpha (0.35f));
        g.strokePath (p, juce::PathStrokeType (1.0f), juce::AffineTransform::translation (1.2f, 0.0f));
    }
}

void IDGLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    auto c = b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId);
    if (! b.isEnabled()) c = c.withAlpha (0.35f);
    g.setColour (c);
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (3, 1), juce::Justification::centred, 1, 0.8f);
}

void IDGLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    auto box = r.removeFromLeft (r.getHeight()).reduced (3.0f);
    g.setColour (Theme::panelHi);
    g.fillRect (box);
    g.setColour (b.getToggleState() ? Theme::accent : Theme::line.brighter (over ? 0.5f : 0.0f));
    g.drawRect (box, 1.0f);
    if (b.getToggleState()) g.fillRect (box.reduced (3.0f));
    g.setColour (Theme::text.withAlpha (b.isEnabled() ? 1.0f : 0.4f));
    g.setFont (Theme::mono (11.0f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (4), juce::Justification::centredLeft, true);
}

void IDGLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRect (r);
    g.setColour (box.hasKeyboardFocus (false) ? Theme::accent : Theme::line);
    g.drawRect (r, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 10.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha (box.isEnabled() ? 1.0f : 0.3f));
    g.fillPath (arrow);
}

void IDGLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (2, 0, box.getWidth() - 16, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

void IDGLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (Theme::panel);
    g.setColour (Theme::accent.withAlpha (0.6f));
    g.drawRect (0, 0, w, h, 1);
}

void IDGLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                       juce::Slider::SliderStyle style, juce::Slider& s)
{
    auto c = s.findColour (juce::Slider::trackColourId);
    if (style == juce::Slider::LinearVertical)
    {
        auto r = juce::Rectangle<float> ((float) x + w * 0.5f - 3.0f, (float) y, 6.0f, (float) h);
        g.setColour (Theme::line);
        g.fillRect (r);
        g.setColour (c);
        g.fillRect (r.withTop (pos));
        g.fillRect (r.getX() - 3.0f, pos - 1.5f, 12.0f, 3.0f);
        return;
    }
    auto r = juce::Rectangle<float> ((float) x, (float) y + h * 0.5f - 3.0f, (float) w, 6.0f);
    g.setColour (Theme::line);
    g.fillRect (r);
    g.setColour (c.withAlpha (s.isEnabled() ? 1.0f : 0.3f));
    g.fillRect (r.withRight (pos));
    g.setColour (Theme::text);
    g.fillRect (pos - 1.5f, r.getY() - 3.0f, 3.0f, 12.0f);
}

void IDGLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    g.fillAll (Theme::panel.withAlpha (0.97f));
    g.setColour (Theme::accent);
    g.drawRect (0, 0, w, h, 1);
    g.fillRect (0, 0, 3, h);
    g.setColour (Theme::text);
    g.setFont (Theme::sans (13.0f));
    g.drawFittedText (text, juce::Rectangle<int> (8, 4, w - 12, h - 8), juce::Justification::centredLeft, 6);
}

juce::Rectangle<int> IDGLookAndFeel::getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    juce::AttributedString s;
    s.append (text, Theme::sans (13.0f));
    juce::TextLayout tl;
    tl.createLayout (s, 320.0f);
    const int w = (int) tl.getWidth() + 24, h = (int) tl.getHeight() + 14;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 14,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 6) : pos.y + 8, w, h)
        .constrainedWithin (parent);
}

void IDGLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int ts, int tsz, bool over, bool)
{
    auto thumb = vertical ? juce::Rectangle<int> (x + w - 4, ts, 3, tsz) : juce::Rectangle<int> (ts, y + h - 4, tsz, 3);
    g.setColour (Theme::accent.withAlpha (over ? 0.8f : 0.45f));
    g.fillRect (thumb);
}

void IDGLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (Theme::panelHi);
    g.fillRect (0, 0, w, h);
}

void IDGLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& te)
{
    g.setColour (te.hasKeyboardFocus (true) ? Theme::accent : Theme::line);
    g.drawRect (0, 0, w, h, 1);
}

void IDGLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& l)
{
    if (l.isBeingEdited()) return;
    g.setColour (l.findColour (juce::Label::textColourId).withAlpha (l.isEnabled() ? 1.0f : 0.4f));
    g.setFont (l.getFont());
    g.drawFittedText (l.getText(), l.getLocalBounds().reduced (2, 0), l.getJustificationType(), 2, 0.8f);
}
} // namespace idg::ui
