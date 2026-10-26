#pragma once
#include "OscClip.h"
#include <juce_core/juce_core.h>
#include <atomic>
#include <deque>

// Async OSC output for MIDI-controller mappings. MIDI CC output is delegated
// to the existing MidiOutputRouter so that Windows devices are opened once.
namespace cuelist {
class ControlOutputRouter final : private juce::Thread {
public:
    ControlOutputRouter();
    ~ControlOutputRouter() override;
    bool sendOscFloat(const juce::String& host,int port,const juce::String& address,float value);
    void clear();
    int droppedCount() const noexcept { return dropped.load(); }
private:
    struct Packet {
        juce::String host,address;
        int port=9000;
        float decimal=0;
    };
    juce::CriticalSection queueLock;
    std::deque<Packet> queue;
    std::atomic<int> dropped{0};
    static constexpr size_t maxQueue=512;
    void run() override;
};
}
