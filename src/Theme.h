#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"
#include <cmath>
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
// Small icon buttons share one restrained surface, but retain their own
// glyphs, colours, geometry and event handling. Ordinary text buttons are
// intentionally not forced into this visual treatment.
struct IconButtonState {
    bool enabled=true;
    bool hovered=false;
    bool pressed=false;
    bool active=false;
    bool keyboardFocus=false;
};
inline juce::Colour iconButtonInk(juce::Colour accent,const IconButtonState& state) {
    if (!state.enabled) return palette::muted.withAlpha(.44f);
    return accent.withAlpha(state.active || state.hovered || state.pressed ? 1.0f : .90f);
}
inline void drawIconButtonSurface(juce::Graphics& g,juce::Rectangle<float> bounds,
                                  juce::Colour accent,const IconButtonState& state,
                                  float radius=4.5f) {
    if (bounds.isEmpty()) return;
    const auto rect=bounds.reduced(.65f);
    const auto fill=state.active ? accent.withAlpha(state.pressed ? .18f : .12f) :
                    state.pressed ? palette::raised.withAlpha(.82f) :
                    palette::raised.withAlpha(state.hovered ? .68f : .47f);
    g.setColour(state.enabled ? fill : fill.withMultipliedAlpha(.40f));
    g.fillRoundedRectangle(rect,radius);
    const auto stroke=state.keyboardFocus ? palette::teal :
        state.active ? accent.withAlpha(.58f) :
        state.hovered ? palette::line.withAlpha(.48f) :
                        palette::line.withAlpha(.27f);
    g.setColour(state.enabled ? stroke : stroke.withMultipliedAlpha(.45f));
    g.drawRoundedRectangle(rect,radius,state.keyboardFocus ? 1.65f :
                                         state.active ? .95f : .8f);
}
// Embedded OFL typefaces are registered once and kept alive for the app's
// lifetime. No user-installed fonts, startup network or Windows font state.
struct AppTypefaces {
    juce::Typeface::Ptr exo2=juce::Typeface::createSystemTypefaceFor(
        BinaryData::Exo2_ttf,BinaryData::Exo2_ttfSize);
    juce::Typeface::Ptr hind=juce::Typeface::createSystemTypefaceFor(
        BinaryData::HindRegular_ttf,BinaryData::HindRegular_ttfSize);
    juce::Typeface::Ptr hindSemibold=juce::Typeface::createSystemTypefaceFor(
        BinaryData::HindSemiBold_ttf,BinaryData::HindSemiBold_ttfSize);
    // Noto families are embedded for explicit math/symbol rendering.
    // Do not rely on finding system-installed typefaces.
    juce::Typeface::Ptr notoMath=juce::Typeface::createSystemTypefaceFor(
        BinaryData::NotoSansMathRegular_ttf,BinaryData::NotoSansMathRegular_ttfSize);
    juce::Typeface::Ptr notoSymbols2=juce::Typeface::createSystemTypefaceFor(
        BinaryData::NotoSansSymbols2Regular_ttf,BinaryData::NotoSansSymbols2Regular_ttfSize);
};
inline const AppTypefaces& appTypefaces() {
    static const AppTypefaces faces;
    return faces;
}

// Historical design sizes are JUCE logical pixels, not typographic points.
// 1 pt = 4/3 logical px at the Windows 96-DPI reference scale. Preserve the
// intended layout while enforcing Exo 2 at >=10 actual points for small text.
inline constexpr float smallTextCutoff=14.5f;
inline juce::Font font(float size,bool bold=false) {
    const auto& faces=appTypefaces();
    const bool small=size<=smallTextCutoff;
    const auto face=small ? faces.exo2 : (bold ? faces.hindSemibold : faces.hind);
    const auto pt=juce::jmax(10.0f,size*0.75f);
    if (face!=nullptr) {
        const juce::Font embedded(juce::FontOptions(face)
            .withPointHeight(pt)
            // Noto fonts are selected explicitly for special glyph runs.
            // Ordinary UI text always retains Exo 2 or Hind.
            .withFallbacks({"Segoe UI Symbol"}));
        // JUCE 8 FontOptions has no withStyleFlags() member. Bold the
        // embedded Exo 2 face using the supported juce::Font API.
        return small && bold ? embedded.boldened() : embedded;
    }
    // Last resort only (e.g. a corrupt embedded font), never default behavior.
    return juce::Font(juce::FontOptions("Segoe UI",size,
        bold ? juce::Font::bold : juce::Font::plain).withPointHeight(pt)
        .withFallbacks({"Segoe UI Symbol"}));
}
// Explicit fonts for special glyphs. Never use these for ordinary headings,
 // labels or transport iconography; those retain Hind/Exo 2 or vector paths.
inline juce::Font mathFont(float size) {
    const auto& face=appTypefaces().notoMath;
    const auto pt=juce::jmax(10.0f,size*0.75f);
    return face!=nullptr
        ? juce::Font(juce::FontOptions(face).withPointHeight(pt))
        : font(size);
}
inline juce::Font symbolsFont(float size) {
    const auto& face=appTypefaces().notoSymbols2;
    const auto pt=juce::jmax(10.0f,size*0.75f);
    return face!=nullptr
        ? juce::Font(juce::FontOptions(face).withPointHeight(pt))
        : font(size);
}
// Draw a symbol only when it actually exists in the embedded Symbols 2 font.
// Do not ask JUCE to substitute an OS font or an emoji. Callers must supply a
// small vector fallback if this glyph is absent from the pinned font.
inline bool drawNotoSymbol(juce::Graphics& g,juce::juce_wchar codepoint,
                           juce::Rectangle<int> area,float size,juce::Colour ink) {
    const auto& symbols=appTypefaces().notoSymbols2;
    if (symbols==nullptr ||
        !symbols->getNominalGlyphForCodepoint(codepoint).has_value())
        return false;
    g.saveState();
    g.setColour(ink);
    g.setFont(symbolsFont(size));
    g.drawText(juce::String::charToString(codepoint),area,
               juce::Justification::centred,false);
    g.restoreState();
    return true;
}
// The Fader Range menu mixes two scripts: draw only the mathematical
// minus/infinity pair with Noto Sans Math; the rest remains Exo 2.
inline juce::String faderMinusInfinity() {
    return juce::String::charToString((juce::juce_wchar)0x2212)
         + juce::String::charToString((juce::juce_wchar)0x221e);
}
inline juce::String faderRangeMenuTitle(int maximumDb) {
    return faderMinusInfinity()+" dB / "+(maximumDb>0?"+":"")
           +juce::String(maximumDb)+" dB";
}
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
        setColour(juce::PopupMenu::headerTextColourId,palette::teal);
        setColour(juce::PopupMenu::highlightedBackgroundColourId,palette::raised);
        setColour(juce::PopupMenu::highlightedTextColourId,palette::text);
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
        // JUCE DocumentWindow exposes textColourId only; its titlebar
        // background comes from ResizableWindow and the LookAndFeel.
        setColour(juce::DocumentWindow::textColourId,palette::text);
        setColour(juce::ScrollBar::backgroundColourId,palette::raised);
        setColour(juce::AlertWindow::backgroundColourId,palette::panel);
        setColour(juce::AlertWindow::textColourId,palette::text);
        // Quiet left-edge scrollbar: readable on hover, but not a glowing
        // white line competing with cues and the cyan BPM boundary.
        setColour(juce::ScrollBar::thumbColourId,
                  palette::muted.withMultipliedBrightness(0.72f));
    }
    juce::Font getTextButtonFont(juce::TextButton&,int height) override {
        return font(height<26 ? 12.5f : 15.0f,true);
    }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return font(14.5f); }
    juce::Font getLabelFont(juce::Label& l) override {
        return font(l.getFont().getHeight(),l.getFont().isBold());
    }
    juce::Font getAlertWindowTitleFont() override { return font(22.0f,true); }
    juce::Font getAlertWindowMessageFont() override { return font(15.0f); }
    juce::Font getAlertWindowFont() override { return font(14.0f); }
    juce::Font getSliderPopupFont(juce::Slider&) override { return font(14.0f); }
    void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour& colour,bool over,bool down) override {
        const auto caption=b.getButtonText();
        // Only icon-only JUCE TextButtons. Keep full-text controls, the
        // emergency PANIC action and the GO button visually unchanged.
        if (caption=="+" || caption=="x" || caption=="..." || caption=="⋯") {
            const IconButtonState state{b.isEnabled(),over,down,
                b.getToggleState(),b.hasKeyboardFocus(false)};
            drawIconButtonSurface(g,b.getLocalBounds().toFloat(),
                                  caption=="x" ? palette::red : palette::teal,state);
            return;
        }
        const auto r=b.getLocalBounds().toFloat().reduced(0.8f);
        const auto fill=colour.withMultipliedBrightness(down ? 0.87f : over ? 1.14f : 1.0f);
        g.setColour(b.isEnabled() ? fill : fill.withAlpha(0.46f));
        g.fillRoundedRectangle(r,6.0f);
        g.setColour(b.hasKeyboardFocus(false) ? palette::teal :
                    over && b.isEnabled() ? palette::muted : palette::line);
        g.drawRoundedRectangle(r,6.0f,b.hasKeyboardFocus(false) ? 1.6f : 1.0f);
    }

    // JUCE PopupMenu is a separate desktop window. Explicitly attach this
    // LookAndFeel when constructing a menu (not only to its target button),
    // otherwise Windows can render it using the default grey theme.
    juce::Font getPopupMenuFont() override { return font(13.0f); }

    void drawPopupMenuBackground(juce::Graphics& g,int width,int height) override {
        g.fillAll(palette::panel);
        g.setColour(palette::line.withAlpha(.9f));
        g.drawRect(0,0,width,height,1);
    }

    void drawPopupMenuItem(juce::Graphics& g,const juce::Rectangle<int>& area,
                           bool separator,bool active,bool highlighted,bool ticked,
                           bool hasSubMenu,const juce::String& title,
                           const juce::String& shortcut,const juce::Drawable* icon,
                           const juce::Colour* customTextColour) override {
        if (separator) {
            g.setColour(palette::line.withAlpha(.65f));
            g.fillRect(area.getX()+12,area.getCentreY(),
                       juce::jmax(1,area.getWidth()-24),1);
            return;
        }

        const auto hover=highlighted && active;
        if (hover) {
            const auto surface=area.reduced(3,1).toFloat();
            g.setColour(palette::raised);
            g.fillRoundedRectangle(surface,4.0f);
            g.setColour(palette::teal);
            g.fillRoundedRectangle((float)area.getX()+3.0f,
                                   (float)area.getY()+4.0f,
                                   2.5f,(float)juce::jmax(1,area.getHeight()-8),1.0f);
        }

        const auto ink=customTextColour!=nullptr ? *customTextColour : palette::text;
        g.setColour(active ? (hover ? palette::text : ink)
                           : palette::muted.withAlpha(.48f));
        g.setFont(font(13.0f));

        auto content=area.reduced(10,0);
        const auto iconArea=content.removeFromLeft(20).toFloat();
        if (icon!=nullptr)
            icon->drawWithin(g,iconArea,
                juce::RectanglePlacement::centred |
                juce::RectanglePlacement::onlyReduceInSize,active ? 1.0f : .48f);
        else if (ticked) {
            g.setColour(active ? palette::teal : palette::muted.withAlpha(.48f));
            juce::Path tick;
            tick.startNewSubPath(iconArea.getX()+3.0f,iconArea.getCentreY());
            tick.lineTo(iconArea.getX()+8.0f,iconArea.getCentreY()+4.0f);
            tick.lineTo(iconArea.getX()+16.0f,iconArea.getCentreY()-5.0f);
            g.strokePath(tick,juce::PathStrokeType(1.9f,
                juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        }
        content.removeFromLeft(4);
        if (hasSubMenu) {
            const float cx=(float)content.getRight()-7.0f;
            const float cy=(float)content.getCentreY();
            juce::Path chevron;
            chevron.startNewSubPath(cx-3.0f,cy-4.0f);
            chevron.lineTo(cx+1.0f,cy);
            chevron.lineTo(cx-3.0f,cy+4.0f);
            g.setColour(active ? palette::teal : palette::muted.withAlpha(.48f));
            g.strokePath(chevron,juce::PathStrokeType(1.6f,
                juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
            content.removeFromRight(19);
        }
        if (shortcut.isNotEmpty()) content.removeFromRight(70);
        g.setColour(active ? (hover ? palette::text : ink)
                           : palette::muted.withAlpha(.48f));
        if (title.startsWith(faderMinusInfinity())) {
            const auto prefix=faderMinusInfinity();
            const auto mathematical=mathFont(13.0f);
            const int prefixWidth=(int)std::ceil(
                juce::GlyphArrangement::getStringWidth(mathematical,prefix))+2;
            const auto prefixArea=content.removeFromLeft(prefixWidth);
            g.setFont(mathematical);
            g.drawText(prefix,prefixArea,juce::Justification::centredLeft,false);
            g.setFont(font(13.0f));
            g.drawFittedText(title.substring(prefix.length()),content,
                             juce::Justification::centredLeft,1);
        } else {
            g.setFont(font(13.0f));
            g.drawFittedText(title,content,juce::Justification::centredLeft,1);
        }
        if (shortcut.isNotEmpty()) {
            g.setColour(active ? palette::muted : palette::muted.withAlpha(.48f));
            g.setFont(font(10.5f));
            const auto shortcutArea=area.reduced(12,0).removeFromRight(69);
            g.drawFittedText(shortcut,shortcutArea,
                             juce::Justification::centredRight,1);
        }
    }

    void drawPopupMenuSectionHeader(juce::Graphics& g,
                                    const juce::Rectangle<int>& area,
                                    const juce::String& section) override {
        g.setColour(palette::teal);
        g.setFont(font(11.0f,true));
        g.drawFittedText(section,area.reduced(12,2),
                         juce::Justification::centredLeft,1);
    }

    void getIdealPopupMenuItemSize(const juce::String& title,bool separator,
                                    int standardHeight,int& width,int& height) override {
        if (separator) {
            width=28;
            height=9;
            return;
        }
        height=juce::jmax(34,standardHeight);
        if (title.startsWith(faderMinusInfinity())) {
            const auto prefix=faderMinusInfinity();
            width=juce::jmax(142,(int)std::ceil(
                juce::GlyphArrangement::getStringWidth(mathFont(13.0f),prefix)
              + juce::GlyphArrangement::getStringWidth(
                    font(13.0f),title.substring(prefix.length())))+73);
        } else {
            width=juce::jmax(110,
                (int)std::ceil(juce::GlyphArrangement::getStringWidth(
                    font(13.0f),title))+65);
        }
    }

    void drawComboBox(juce::Graphics& g,int width,int height,bool isButtonDown,
                      int buttonX,int buttonY,int buttonW,int buttonH,
                      juce::ComboBox& combo) override {
        const auto r=juce::Rectangle<float>(0.5f,0.5f,
                          (float)width-1.0f,(float)height-1.0f);
        g.setColour(combo.isEnabled() ? palette::input
                                      : palette::input.withAlpha(.5f));
        g.fillRoundedRectangle(r,5.0f);
        g.setColour(combo.hasKeyboardFocus(true) ? palette::teal :
                    isButtonDown ? palette::muted : palette::line);
        g.drawRoundedRectangle(r,5.0f,
                               combo.hasKeyboardFocus(true) ? 1.4f : 1.0f);
        const auto cx=(float)buttonX+(float)buttonW*.5f;
        const auto cy=(float)buttonY+(float)buttonH*.5f;
        juce::Path arrow;
        arrow.startNewSubPath(cx-4.0f,cy-2.0f);
        arrow.lineTo(cx,cy+2.0f);
        arrow.lineTo(cx+4.0f,cy-2.0f);
        g.setColour(combo.isEnabled() ? palette::teal : palette::muted.withAlpha(.5f));
        g.strokePath(arrow,juce::PathStrokeType(1.6f,
            juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    }
};
}
