#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
namespace cuelist {
struct MidiNote { double beat=0, length=1; int pitch=60, velocity=100, channel=1; };
struct MidiClip { double lengthBeats=4; std::vector<MidiNote> notes; };
juce::Result validateMidiClip(const MidiClip&);
juce::Result loadMidiClip(const juce::File&, MidiClip&, juce::String& warning);
}
