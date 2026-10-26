#pragma once
#include "Theme.h"
#include <array>
#include <functional>
#include <cmath>

namespace cuelist {

// An integrated stereo peak-meter and gain fader. No child Slider: the two
// bars, cursor, ticks and hit target use the same calibrated dB-to-Y mapping.
// Fader numbers are relative gain (dB); meter bars represent pre-master dBFS.
class GainMeterFader final : public juce::Component, public juce::SettableTooltipClient {
public:
    static constexpr float minDb=-60.0f, maxDb=12.0f;
    std::function<void(float)> onGainChanged;

    GainMeterFader() {
        setName("columnGainDb");
        setWantsKeyboardFocus(true);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setTooltip("Fader: gain in dB. Barre L/R: picco in dBFS prima del Master. Doppio clic: 0 dB.");
    }

    // Calibrated logarithmic taper: unity sits around 75% of the travel.
    // Both meter marks and the thumb use the same invertible transformation.
    // Saved values remain unchanged dB, converted to linear gain only for PCM.
    static float yForDb(float db,float top,float bottom) noexcept {
        if (bottom<=top) return top;
        const double fraction=(juce::jlimit(minDb,maxDb,db)-minDb)/(maxDb-minDb);
        const double position=std::expm1(fraction*std::log(3.0))/2.0;
        return bottom-(float)position*(bottom-top);
    }
    static float dbForY(float y,float top,float bottom) noexcept {
        if (bottom<=top) return minDb;
        const double position=juce::jlimit(0.0,1.0,(double)(bottom-y)/(bottom-top));
        return (float)(minDb+(maxDb-minDb)*std::log1p(2.0*position)/std::log(3.0));
    }
    float getGainDb() const noexcept { return gainDb; }
    void setGainDb(float value,bool notify=false) {
        if (!std::isfinite(value)) return;
        const auto next=juce::jlimit(minDb,maxDb,std::round(value*10.0f)*0.1f);
        if (std::abs(next-gainDb)<0.001f) return;
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
    float topY() const noexcept { return 12.0f; }
    float bottomY() const noexcept { return (float)std::max(15,getHeight()-35); }
    void setFromMouse(float y) {
        const auto db=std::round(dbForY(y,topY(),bottomY())*10.0f)*0.1f;
        setGainDb(db,true);
    }
};
}
