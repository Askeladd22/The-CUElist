#pragma once
#include "Theme.h"
#include <cmath>

namespace cuelist {

// Shared channel/master mixer controls: identical pan geometry and mute icon.
class UtilityIcon final : public juce::ToggleButton {
public:
    explicit UtilityIcon(bool invert) : phase(invert) {
        setButtonText(invert ? "Phase inversion" : "Mute");
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

// Graphic language shared by the column and Master pan knobs.
class MixerPanLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    void drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,
        float position,float start,float end,juce::Slider&) override {
        const auto r=juce::Rectangle<float>((float)x,(float)y,(float)width,(float)height).reduced(5);
        const auto centre=r.getCentre();
        const float radius=std::min(r.getWidth(),r.getHeight())*0.38f;
        juce::Path track;
        track.addCentredArc(centre.x,centre.y,radius,radius,0,start,end,true);
        g.setColour(palette::line);g.strokePath(track,juce::PathStrokeType(3.5f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        juce::Path active;
        const auto angle=start+(end-start)*position,mid=(start+end)*.5f;
        active.addCentredArc(centre.x,centre.y,radius,radius,0,std::min(mid,angle),std::max(mid,angle),true);
        g.setColour(palette::teal);g.strokePath(active,juce::PathStrokeType(4.0f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        g.setColour(palette::raised);g.fillEllipse(centre.x-radius*.68f,centre.y-radius*.68f,radius*1.36f,radius*1.36f);
        g.setColour(palette::line);g.drawEllipse(centre.x-radius*.68f,centre.y-radius*.68f,radius*1.36f,radius*1.36f,1.1f);
        const auto x2=centre.x+std::sin(angle)*radius*.65f;
        const auto y2=centre.y-std::cos(angle)*radius*.65f;
        g.setColour(palette::text);g.drawLine(centre.x,centre.y,x2,y2,2.3f);
        const auto centreX=centre.x+std::sin(mid)*radius*1.18f;
        const auto centreY=centre.y-std::cos(mid)*radius*1.18f;
        g.setColour(palette::muted);g.fillEllipse(centreX-1.6f,centreY-1.6f,3.2f,3.2f);
    }
};
inline MixerPanLookAndFeel& sharedMixerPanLookAndFeel() {
    static MixerPanLookAndFeel lookAndFeel;
    return lookAndFeel;
}
class MasterSendButton final : public juce::ToggleButton {
public:
    MasterSendButton() {setButtonText("L/R");}
    void paintButton(juce::Graphics& g,bool hover,bool) override {
        const auto circle=getLocalBounds().toFloat().reduced(2);
        const bool on=getToggleState();
        g.setColour(on ? palette::teal.withAlpha(.25f) : palette::raised);
        g.fillEllipse(circle);
        g.setColour(on ? palette::teal : hover ? palette::text : palette::line);
        g.drawEllipse(circle,on ? 2.0f : 1.2f);
        g.setColour(on ? palette::text : palette::muted);
        g.setFont(font(10.5f,true));
        g.drawFittedText("L/R",getLocalBounds(),juce::Justification::centred,1);
    }
};
class LimiterIndicator final : public juce::Component {
public:
    void setActive(bool value) {if(value!=active){active=value;repaint();}}
    void paint(juce::Graphics& g) override {
        const auto area=getLocalBounds().toFloat().reduced(3);
        g.setColour(active ? palette::red : palette::muted.withAlpha(.18f));
        g.fillEllipse(area);
        g.setColour(active ? palette::red : palette::line);
        g.drawEllipse(area,1.0f);
    }
private:
    bool active=false;
};

inline void configureMixerPan(juce::Slider& pan) {
    pan.setLookAndFeel(&sharedMixerPanLookAndFeel());
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
