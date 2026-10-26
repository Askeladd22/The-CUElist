#pragma once
#include <juce_core/juce_core.h>
#include <cstdint>
#include <vector>

namespace cuelist {
enum class OscValueType { integer, decimal, text, boolean };
struct OscValue {
    OscValueType type=OscValueType::integer;
    int integer=0;
    float decimal=0;
    juce::String text;
    bool boolean=false;
};
struct OscEvent {
    double time=0;
    juce::String address="/cue";
    std::vector<OscValue> arguments;
};
// Point times are offsets from an automation's start, in clip time units.
struct OscControlPoint { double time=0, value=0; };
struct OscAutomation {
    OscEvent message; // address and full ordered argument template
    int argumentIndex=0; // the only writable numeric argument
    double time=0, duration=1;
    std::vector<OscControlPoint> points;
};
struct OscClip {
    bool useBeats=false;
    double length=4;
    std::vector<OscEvent> events;
    std::vector<OscAutomation> automations;
};
// Valid OSC 1.0 messages with i, f, s, T and F type tags.
// Packets have a fixed 1024-byte upper bound for lock-free RT dispatch.
juce::Result validateOscClip(const OscClip&);
std::vector<uint8_t> encodeOscMessage(const OscEvent&);
}
