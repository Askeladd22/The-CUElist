#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace cuelist {
namespace palette {
inline const juce::Colour background{0xff080f1b}, panel{0xff0d1828}, raised{0xff132236}, line{0xff23354b};
inline const juce::Colour text{0xffe4edf7}, muted{0xff8498af}, teal{0xff20d9c0}, amber{0xffedbb57}, purple{0xffa78bfa}, red{0xffff6c7e};
inline juce::Colour type(int i) { return i==0 ? teal : i==1 ? amber : i==2 ? purple : juce::Colour(0xff79b8ff); }
}
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
    juce::Font getTextButtonFont(juce::TextButton&,int) override { return font(13,true); }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return font(13); }
    void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour& colour,bool over,bool down) override {
        auto r=b.getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(colour.withMultipliedBrightness(down ? 0.8f : over ? 1.2f : 1.0f)); g.fillRoundedRectangle(r,6);
        g.setColour(palette::line); g.drawRoundedRectangle(r,6,1);
    }
};
}
