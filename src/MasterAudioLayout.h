#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>

namespace cuelist {

// The Master Audio is a fixed 318px sibling of the horizontally scrollable
// audio/MIDI/OSC column strips, NOT a child of their scrollable container.
// This geometry is shared by its paint, controls and JUCE regression tests.
// Coordinates are local to ColumnUtilities, not to the cue viewport.
struct MasterAudioLayout final {
    static constexpr int laneWidth=318;
    static constexpr int pinnedX=0;
    static constexpr int accentWidth=3;

    juce::Rectangle<int> card, fader, panKnob, muteButton;
    juce::Rectangle<int> output, limiter;
    juce::Rectangle<int> title, outputCaption, panCaption, panValue;
    juce::Rectangle<int> muteCaption, limiterCaption, gainCaption;
    juce::Rectangle<int> headerRule;

    static MasterAudioLayout atHeight(int panelHeight) {
        const int height=std::max(180,panelHeight);
        // At compact heights the lower action row moves up rather than
        // escaping the card. The fader scales continuously with the panel.
        // Leave a real 55px buffer under the 43px pan pot so its numeric
        // readout and the MUTE caption never overlap. At 180px the final
        // button still ends within the inner card (y=173, bottom=175).
        const int actionTop=std::min(148,height-38);
        return {
            {8,5,laneWidth-16,height-10},
            {17,37,82,std::max(92,height-45)},
            {244,43,43,43},
            {248,actionTop,34,31},
            {110,65,100,25},
            {109,actionTop,105,31},
            {19,12,280,21},
            {112,45,97,17},
            {238,25,57,16},
            {242,91,52,17},
            {241,actionTop-21,59,17},
            {110,actionTop-21,105,17},
            {110,height-25,174,16},
            // Stop the header rule before the high PAN caption.
            {19,36,198,1}
        };
    }
};
} // namespace cuelist
