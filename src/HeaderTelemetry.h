#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <cmath>

namespace cuelist {
// One shared geometry contract for painting, status-button hitboxes,
// responsive operator menus and the Cuelist tab overflow.
// The entire console remains within the original 72px global header.
struct HeaderTelemetryLayout final {
    juce::Rectangle<int> clock,usage,activity,audio;
    juce::Rectangle<int> timeText,meridiemText,cpuLabel,cpuValue,ramLabel,ramValue;
    juce::Rectangle<float> cpuMeter,ramMeter;
    juce::Rectangle<int> activityHeading,audioHeading,audioStatus;
    juce::Rectangle<int> midiLearn,oscLearn;
    std::array<juce::Rectangle<int>,4> activityLabels{};
    std::array<juce::Rectangle<int>,4> activityDots{};

    static HeaderTelemetryLayout atWidth(int windowWidth) {
        // Grow horizontally, not vertically. Three profiles keep at least
        // File + More accessible down to the existing 1024px test width.
        const bool wide=windowWidth>=1550;
        const bool medium=windowWidth>=1250 && !wide;
        const int cw=wide?168:medium?154:134;
        const int uw=wide?156:medium?144:130;
        const int iw=wide?258:medium?238:218;
        const int aw=wide?184:medium?164:150;
        constexpr int gap=10;
        const int x=windowWidth-10-(cw+uw+iw+aw+gap*3);
        HeaderTelemetryLayout t;
        t.clock={x,7,cw,58};
        t.usage={t.clock.getRight()+gap,7,uw,58};
        t.activity={t.usage.getRight()+gap,7,iw,58};
        t.audio={t.activity.getRight()+gap,7,aw,58};

        t.timeText={x+4,10,cw-8,38};
        t.meridiemText={x+cw-49,49,45,13};
        t.cpuLabel={t.usage.getX()+9,16,35,19};
        t.cpuValue={t.usage.getX()+44,16,45,19};
        t.ramLabel={t.usage.getX()+9,39,35,19};
        t.ramValue={t.usage.getX()+44,39,45,19};
        t.cpuMeter={(float)t.usage.getRight()-40.f,22.f,30.f,7.f};
        t.ramMeter={(float)t.usage.getRight()-40.f,45.f,30.f,7.f};

        t.activityHeading={t.activity.getX()+9,8,iw<=220?78:94,19};
        const int learnWidth=iw>=245?67:61;
        const int learnX=t.activity.getRight()-2*learnWidth-7;
        t.midiLearn={learnX,9,learnWidth,22};
        t.oscLearn={learnX+learnWidth+4,9,learnWidth,22};
        const int leftX=t.activity.getX()+9;
        const int rightX=t.activity.getX()+iw/2+3;
        t.activityDots={juce::Rectangle<int>{leftX,36,10,10},
                        juce::Rectangle<int>{rightX,36,10,10},
                        juce::Rectangle<int>{leftX,53,10,10},
                        juce::Rectangle<int>{rightX,53,10,10}};
        t.activityLabels={juce::Rectangle<int>{leftX+15,30,iw/2-25,20},
                          juce::Rectangle<int>{rightX+15,30,iw/2-23,20},
                          juce::Rectangle<int>{leftX+15,46,iw/2-25,18},
                          juce::Rectangle<int>{rightX+15,46,iw/2-23,18}};
        t.audioHeading={t.audio.getX()+12,11,aw-22,20};
        t.audioStatus={t.audio.getX()+30,31,aw-40,26};
        return t;
    }

    static juce::String percentage(float value,bool cpu) {
        if (!std::isfinite(value) || value<0) return "N/A";
        return juce::String(value,cpu ? 1 : 0)+"%";
    }
};
} // namespace cuelist
