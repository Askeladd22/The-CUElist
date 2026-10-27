#pragma once
#include "Theme.h"
#include <array>
#include <functional>
#include <cmath>
#include <limits>

namespace cuelist {

// An integrated stereo peak-meter and gain fader. No child Slider: the two
// bars, cursor, ticks and hit target use the same calibrated dB-to-Y mapping.
// Fader numbers are relative gain (dB); meter bars represent pre-master dBFS.
class GainMeterFader final : public juce::Component, public juce::SettableTooltipClient {
public:
    static constexpr float minDb=-60.0f, maxDb=6.0f;
    static constexpr float silenceDb=-std::numeric_limits<float>::infinity();
    static float fromLinear(float gain) noexcept { return gain<=0 ? silenceDb : 20.0f*std::log10(gain); }
    static float toLinear(float db) noexcept { return db==silenceDb ? 0.0f : juce::Decibels::decibelsToGain(db); }
    float maximumDb() const noexcept {return ceilingDb;}
    void setMaximumDb(int ceiling) { const float next=ceiling==0 ? 0.0f : 6.0f; if (ceilingDb!=next) {ceilingDb=next;repaint();} }
    std::function<void(float)> onGainChanged;

    GainMeterFader() {
        setName("columnGainDb");
        setWantsKeyboardFocus(true);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setTooltip("Fader gain in dB; L/R peak meters in dBFS. Double-click to reset to 0 dB.");
    }

    // Calibrated logarithmic taper: unity sits around 75% of the travel.
    // Both meter marks and the thumb use the same invertible transformation.
    // Saved values remain unchanged dB, converted to linear gain only for PCM.
    static float yForDb(float db,float top,float bottom,float ceilingDb=maxDb) noexcept {
        if (bottom<=top) return top;
        const double fraction=(juce::jlimit(minDb,ceilingDb,db)-minDb)/(ceilingDb-minDb);
        const double position=std::expm1(fraction*std::log(3.0))/2.0;
        return bottom-(float)position*(bottom-top);
    }
    static float dbForY(float y,float top,float bottom,float ceilingDb=maxDb) noexcept {
        if (bottom<=top) return minDb;
        const double position=juce::jlimit(0.0,1.0,(double)(bottom-y)/(bottom-top));
        if (position<=0.0) return silenceDb;
        return (float)(minDb+(ceilingDb-minDb)*std::log1p(2.0*position)/std::log(3.0));
    }
    bool isCompact() const noexcept {return compact;}
    void setCompact(bool value) {if(compact!=value){compact=value;repaint();}}
    float getGainDb() const noexcept { return gainDb; }
    // Programmatic sync preserves historical gain even above the selected
    // ceiling. Only an operator edit is constrained to the visible range.
    void setGainDb(float value,bool notify=false) {
        if (std::isnan(value) || value==std::numeric_limits<float>::infinity()) return;
        // Incoming project gain is never modified merely by changing range.
        const auto next=value==silenceDb ? silenceDb :
            std::round((notify ? juce::jlimit(minDb,ceilingDb,value) : value)*10.0f)*0.1f;
        if (next==gainDb || (std::isfinite(next) && std::isfinite(gainDb) && std::abs(next-gainDb)<0.001f)) return;
        gainDb=next; repaint();
        if (notify && onGainChanged) onGainChanged(gainDb);
    }
    void setStereoPeaks(float l,float r) noexcept {
        const float values[2]={l,r};
        for (int i=0;i<2;++i) {
            const float raw=std::isfinite(values[i]) && values[i]>0
                ? 20.0f*std::log10(values[i]) : minDb;
            const float db=juce::jlimit(minDb,maxDb,raw);
            auto& peak=peaks[(size_t)i];
            peak.current=std::max(db,peak.current-1.6f);
            if (db>=peak.held) { peak.held=db; peak.holdTicks=13; }
            else if (peak.holdTicks>0) --peak.holdTicks;
            else peak.held=std::max(peak.current,peak.held-1.1f);
        }
        repaint();
    }
    float getDisplayedPeakDb(int channel) const noexcept {
        return channel>=0 && channel<2 ? peaks[(size_t)channel].current : minDb;
    }
    float getHeldPeakDb(int channel) const noexcept {
        return channel>=0 && channel<2 ? peaks[(size_t)channel].held : minDb;
    }
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent&) override { setGainDb(0,true); }
    bool keyPressed(const juce::KeyPress& key) override;
private:
    struct Peak { float current=minDb,held=minDb; int holdTicks=0; };
    std::array<Peak,2> peaks {};
    float gainDb=0.0f;
    bool compact=false;
    float ceilingDb=6.0f;
    float topY() const noexcept { return compact ? 7.0f : 12.0f; }
    float bottomY() const noexcept { return (float)std::max(15,getHeight()-(compact ? 19 : 35)); }
    void setFromMouse(float y) {
        const auto raw=dbForY(y,topY(),bottomY(),ceilingDb);
        const auto db=raw==silenceDb ? raw : std::round(raw*10.0f)*0.1f;
        setGainDb(db,true);
    }
};
}
