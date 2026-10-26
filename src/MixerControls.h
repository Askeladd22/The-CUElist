#pragma once
#include "Theme.h"
#include <cmath>

namespace cuelist {

// Shared channel/master mixer controls: identical pan geometry and mute icon.
class UtilityIcon final : public juce::ToggleButton {
public:
    explicit UtilityIcon(bool invert) : phase(invert) {
        setButtonText(invert ? "Inversione di fase" : "Mute");
    }
    void paintButton(juce::Graphics& g,bool hover,bool down) override {
        const auto box=getLocalBounds().toFloat().reduced(2);
        const auto active=phase ? palette::teal : palette::red;
        g.setColour(getToggleState() ? active.withAlpha(.18f) : palette::raised);
        g.fillRoundedRectangle(box,4);
        g.setColour(getToggleState() ? active : hover || down ? palette::text : palette::muted);
        g.drawRoundedRectangle(box,4,getToggleState() ? 1.4f : .7f);
        const auto icon=juce::Rectangle<float>(20,20).withCentre(box.getCentre());
        if (phase) {
            g.drawEllipse(icon.reduced(2),2.3f);
            g.drawLine(icon.getX()+3,icon.getBottom(),icon.getRight()-3,icon.getY(),2.8f);
        } else {
            const float x=icon.getX(),y=icon.getY();
            juce::Path speaker; speaker.startNewSubPath(x+2,y+7);
            speaker.lineTo(x+7,y+7); speaker.lineTo(x+14,y+2);
            speaker.lineTo(x+14,y+18); speaker.lineTo(x+7,y+13);
            speaker.lineTo(x+2,y+13); speaker.closeSubPath(); g.fillPath(speaker);
            g.drawLine(x+1,y+1,x+19,y+19,3.0f);
        }
    }
private:
    bool phase;
};

inline void configureMixerPan(juce::Slider& pan) {
    pan.setDoubleClickReturnValue(true,0);
    pan.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    pan.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
    pan.setRange(-1,1,0.01);
    pan.textFromValueFunction=[](double value) {
        return std::abs(value)<0.005 ? juce::String("C")
            : juce::String(value<0 ? "L " : "R ")
                  +juce::String((int)std::round(std::abs(value)*100));
    };
    pan.valueFromTextFunction=[](const juce::String& text) {
        return text.containsIgnoreCase("L") ? -std::abs(text.retainCharacters("0123456789.").getDoubleValue())/100
             : text.containsIgnoreCase("R") ? std::abs(text.retainCharacters("0123456789.").getDoubleValue())/100
             : text.getDoubleValue();
    };
    pan.updateText();
}
}
