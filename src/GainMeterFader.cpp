#include "GainMeterFader.h"

namespace cuelist {
void GainMeterFader::paint(juce::Graphics& g) {
    const float top=topY(),bottom=bottomY();
    const float length=bottom-top;
    if(compact) {
        const float mid=getWidth()*.5f;
        const auto rail=juce::Rectangle<float>(mid-10.0f,top,20.0f,length);
        g.setColour(palette::raised);g.fillRoundedRectangle(rail,3.0f);
        g.setColour(palette::line);g.drawRoundedRectangle(rail,3.0f,1.0f);
        for(int ch=0;ch<2;++ch) {
            const float value=peaks[(size_t)ch].current;
            if(value>minDb+.01f) {
                const float y=yForDb(value,top,bottom,ceilingDb);
                g.setColour(value>=0 ? palette::red : value>=-9 ? palette::amber : palette::teal);
                g.fillRect(mid-7.0f+ch*8.0f,y,6.0f,bottom-y);
            }
        }
        const float y=yForDb(gainDb,top,bottom,ceilingDb);
        g.setColour(palette::background);
        g.fillRoundedRectangle(mid-15.0f,y-5.0f,30.0f,10.0f,2.0f);
        g.setColour(palette::text);
        g.fillRoundedRectangle(mid-13.0f,y-3.0f,26.0f,6.0f,1.7f);
        if(hasKeyboardFocus(false)) {
            g.setColour(palette::teal);
            g.drawRoundedRectangle(mid-15.0f,y-5.0f,30.0f,10.0f,2.0f,1.0f);
        }
        g.setColour(palette::muted);
        g.setFont(font(9.0f,true));
        g.drawText(gainDb==silenceDb ? juce::String("-inf") : juce::String((int)std::round(gainDb)),0,getHeight()-18,
                   getWidth(),17,juce::Justification::centred,false);
        return;
    }
    const juce::Rectangle<float> backdrop(12.0f,top,28.0f,length);
    g.setColour(palette::raised); g.fillRoundedRectangle(backdrop,3.0f);
    g.setColour(palette::line); g.drawRoundedRectangle(backdrop,3.0f,1.0f);

    // Gain and both dBFS readings use exactly the same vertical coordinate
    // transform; labels describe different quantities rather than gain=signal.
    const float xs[2]={15.0f,29.0f};
    for (int ch=0;ch<2;++ch) {
        const auto db=peaks[(size_t)ch].current;
        if (db>minDb+0.01f) {
            const float y=yForDb(db,top,bottom,ceilingDb);
            const auto fill=juce::Rectangle<float>(xs[ch],y,10.0f,bottom-y);
            g.setColour(db>=0 ? palette::red.withAlpha(.80f)
                : db>=-9 ? palette::amber.withAlpha(.85f) : palette::teal.withAlpha(.85f));
            g.fillRect(fill);
        }
        if (peaks[(size_t)ch].held>minDb+0.01f) {
            const float mark=yForDb(peaks[(size_t)ch].held,top,bottom,ceilingDb);
            g.setColour(palette::text);
            g.fillRect(xs[ch],mark,10.0f,1.8f);
        }
    }

    constexpr int ticks[]={6,0,-6,-12,-18,-24,-36,-48,-60};
    float previousLabel=-10000.0f;
    for (const int tick:ticks) {
        if (tick>ceilingDb) continue;
        const float y=yForDb((float)tick,top,bottom,ceilingDb);
        const bool major=tick==(int)ceilingDb || tick==0 || tick==-12 || tick==-24
            || tick==-36 || tick==-48 || tick==-60;
        g.setColour(tick==0 ? palette::teal : palette::muted);
        g.drawHorizontalLine((int)std::round(y),42.0f,major ? 50.0f : 46.0f);
        // Retain all tick marks but skip conflicting labels in short strips.
        if (y-previousLabel>=13.0f || previousLabel<0) {
            g.setFont(font(10.0f));
            g.drawText(tick>0 ? "+"+juce::String(tick) : juce::String(tick),
                       53,(int)std::round(y-7),36,14,juce::Justification::centredLeft,false);
            previousLabel=y;
        }
    }

    // A single, visible gain thumb overlaps both L/R bars. No child slider.
    const float thumbY=yForDb(gainDb,top,bottom,ceilingDb);
    const juce::Rectangle<float> thumb(8.0f,thumbY-5.0f,37.0f,10.0f);
    g.setColour(palette::background); g.fillRoundedRectangle(thumb.expanded(1),2.5f);
    g.setColour(palette::text); g.fillRoundedRectangle(thumb,2.2f);
    g.setColour(palette::background); g.fillRect(14.0f,thumbY-.65f,25.0f,1.3f);
    if (hasKeyboardFocus(false)) {
        g.setColour(palette::teal); g.drawRoundedRectangle(thumb.expanded(2),3.0f,1.1f);
    }

    g.setColour(palette::muted);
    g.setFont(font(11.0f));
    g.drawText("L",14,(int)bottom+3,12,14,juce::Justification::centred,false);
    g.drawText("R",29,(int)bottom+3,12,14,juce::Justification::centred,false);
    g.setColour(palette::text);
    g.setFont(font(12.0f,true));
    g.drawText(gainDb==silenceDb ? juce::String("-inf dB") : juce::String(gainDb,1)+" dB",4,getHeight()-22,
               std::max(70,getWidth()-8),20,juce::Justification::centredLeft,false);
}
void GainMeterFader::mouseDown(const juce::MouseEvent& e) {
    grabKeyboardFocus();
    setFromMouse(e.position.y);
}
void GainMeterFader::mouseDrag(const juce::MouseEvent& e) { setFromMouse(e.position.y); }
bool GainMeterFader::keyPressed(const juce::KeyPress& key) {
    const int k=key.getKeyCode();
    if (k==juce::KeyPress::upKey || k==juce::KeyPress::rightKey) {
        setGainDb(gainDb+(key.getModifiers().isShiftDown() ? 1.0f : 0.1f),true); return true;
    }
    if (k==juce::KeyPress::downKey || k==juce::KeyPress::leftKey) {
        setGainDb(gainDb-(key.getModifiers().isShiftDown() ? 1.0f : 0.1f),true); return true;
    }
    if (k==juce::KeyPress::homeKey) { setGainDb(0,true); return true; }
    return false;
}
}
