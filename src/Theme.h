#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace cuelist {
namespace palette {
// Midnight Console: deliberate luminance steps keep adjacent dark surfaces legible.
inline const juce::Colour background{0xff09131f}, panel{0xff16283a}, raised{0xff253d54}, line{0xff3a536a};
inline const juce::Colour text{0xfff1f6fa}, muted{0xffb1c4d4}, teal{0xff30ead2}, amber{0xffffc96d}, purple{0xffb399f4}, red{0xffff7881};
inline juce::Colour type(int i) { return i==0 ? teal : i==1 ? amber : i==2 ? purple : juce::Colour(0xff79b8ff); }
}
// Segoe UI is shipped with supported Windows versions, with JUCE fallback.
inline juce::Font font(float size, bool bold=false) { return juce::Font(juce::FontOptions("Segoe UI",size,bold ? juce::Font::bold : juce::Font::plain)); }
inline void label(juce::Graphics& g,const juce::String& s,juce::Rectangle<int> r,float size=14,juce::Colour colour=palette::text,bool bold=false,int align=juce::Justification::centredLeft) {
    g.setColour(colour); g.setFont(font(size,bold)); g.drawText(s,r,align,true);
}
class Theme final : public juce::LookAndFeel_V4 {
public:
    Theme() {
        setColour(juce::TextButton::buttonColourId,palette::raised);
        setColour(juce::TextButton::textColourOffId,palette::text);
        setColour(juce::ComboBox::backgroundColourId,palette::background);
        setColour(juce::ComboBox::textColourId,palette::text);
        setColour(juce::ComboBox::outlineColourId,palette::line);
        setColour(juce::PopupMenu::backgroundColourId,palette::panel);
        setColour(juce::PopupMenu::textColourId,palette::text);
        setColour(juce::PopupMenu::highlightedBackgroundColourId,palette::raised);
        setColour(juce::TextEditor::backgroundColourId,palette::background);
        setColour(juce::TextEditor::textColourId,palette::text);
        setColour(juce::TextEditor::outlineColourId,palette::line);
        setColour(juce::TextEditor::focusedOutlineColourId,palette::teal);
        setColour(juce::Label::textColourId,palette::text);
        setColour(juce::ToggleButton::textColourId,palette::text);
        setColour(juce::ToggleButton::tickColourId,palette::teal);
        setColour(juce::Slider::thumbColourId,palette::teal);
        setColour(juce::Slider::trackColourId,palette::teal);
        setColour(juce::Slider::backgroundColourId,palette::line);
        setColour(juce::Slider::textBoxTextColourId,palette::text);
        setColour(juce::Slider::textBoxOutlineColourId,palette::line);
        setColour(juce::ResizableWindow::backgroundColourId,palette::background);
        setColour(juce::AlertWindow::backgroundColourId,palette::panel);
        setColour(juce::AlertWindow::textColourId,palette::text);
        setColour(juce::ScrollBar::thumbColourId,palette::line.brighter(0.3f));
    }
    juce::Font getTextButtonFont(juce::TextButton&,int height) override {
        return font(height<26 ? 12.5f : 15.0f,true);
    }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return font(14.5f); }
    void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour& colour,bool over,bool down) override {
        const auto r=b.getLocalBounds().toFloat().reduced(0.8f);
        const auto fill=colour.withMultipliedBrightness(down ? 0.87f : over ? 1.14f : 1.0f);
        g.setColour(b.isEnabled() ? fill : fill.withAlpha(0.46f));
        g.fillRoundedRectangle(r,6.0f);
        g.setColour(b.hasKeyboardFocus(false) ? palette::teal :
                    over && b.isEnabled() ? palette::muted : palette::line);
        g.drawRoundedRectangle(r,6.0f,b.hasKeyboardFocus(false) ? 1.6f : 1.0f);
    }
};
}
