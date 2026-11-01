#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace cuelist {
// Header telemetry cards with consistent vertical rhythm and a 14px gutter.
struct HeaderTelemetryLayout final {
    juce::Rectangle<int> clock,usage;
    juce::Rectangle<int> timeText,meridiemText,cpuLabel,cpuValue,ramLabel,ramValue;
    juce::Rectangle<float> cpuMeter,ramMeter;
    static HeaderTelemetryLayout atWidth(int windowWidth) {
        const int cx=windowWidth-652, ux=windowWidth-514;
        return {
            {cx,8,124,56},{ux,8,132,56},
            // 24h/12h digits use almost the full clock tile. In 12h
            // mode the AM/PM suffix has its own smaller lower-right slot.
            {cx+4,12,116,40},{cx+75,49,45,13},
            {ux+10,16,28,19},{ux+39,16,48,19},
            {ux+10,39,28,19},{ux+39,39,48,19},
            {(float)ux+95,23.5f,27.0f,6.0f},
            {(float)ux+95,46.5f,27.0f,6.0f}
        };
    }
    static juce::String percentage(float value,bool cpu) {
        if (!std::isfinite(value) || value<0) return "N/A";
        return juce::String(value,cpu ? 1 : 0)+"%";
    }
};
} // namespace cuelist
