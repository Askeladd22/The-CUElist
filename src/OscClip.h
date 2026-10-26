#pragma once
#include <juce_core/juce_core.h>
#include <cstdint>
#include <vector>

namespace cuelist {
enum class OscValueType { integer, decimal, text };
struct OscValue {
    OscValueType type=OscValueType::integer;
    int integer=0;
    float decimal=0;
    juce::String text;
};
struct OscEvent {
    double time=0;
    juce::String address="/cue";
    std::vector<OscValue> arguments;
};
struct OscClip {
    bool useBeats=false;
    double length=4;
    std::vector<OscEvent> events;
};
// Valid OSC 1.0 messages with int32, float32 and OSC-string parameters.
// Packets have a fixed 1024-byte upper bound for lock-free RT dispatch.
juce::Result validateOscClip(const OscClip&);
std::vector<uint8_t> encodeOscMessage(const OscEvent&);
}
