#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace cuelist {
namespace palette {
// Midnight Slate: balanced between the darker redesign reference and the
// brighter original. Five restrained luminance steps preserve panel hierarchy
// without making the empty cue workspace pure black or the mixer too blue.
// These roles are shared by Cue List, Inspector, mixer, Timeline and dialogs.
inline const juce::Colour background{0xff071725}, // main canvas
                          input{0xff0b1d2c},      // editable fields
                          panel{0xff102536},      // sidebar, mixer, Inspector
                          toolbar{0xff162c40},    // global navigation
                          raised{0xff1c354a},     // buttons, cards, cue headers
                          line{0xff36536b};       // muted structural separators

// Functional accents must remain readily distinguishable in a live show.
inline const juce::Colour text{0xffedf5fa}, muted{0xffabc2d2},
                          teal{0xff27ddc8}, amber{0xfff9c76e},
                          purple{0xffb49cf0}, red{0xfffb7e8b},
                          go{0xff0b786f}, panic{0xff913d4e},
                          selectedTab{0xff0b686c};
inline juce::Colour type(int i) {
    return i==0 ? teal : i==1 ? amber : i==2 ? purple :
           juce::Colour(0xff82baf4);
}
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
        setColour(juce::ComboBox::backgroundColourId,palette::input);
        setColour(juce::ComboBox::textColourId,palette::text);
        setColour(juce::ComboBox::outlineColourId,palette::line);
        setColour(juce::PopupMenu::backgroundColourId,palette::panel);
        setColour(juce::PopupMenu::textColourId,palette::text);
        setColour(juce::PopupMenu::highlightedBackgroundColourId,palette::raised);
        setColour(juce::TextEditor::backgroundColourId,palette::input);
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
