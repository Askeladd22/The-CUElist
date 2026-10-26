#pragma once
#include "OscClip.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_core/juce_core.h>
#include <deque>
#include <map>

namespace cuelist {
// Standalone controller-output worker. Never called from the audio callback.
// A capped GUI-producer queue isolates hardware output and UDP DNS/write stalls.
class ControlOutputRouter final : private juce::Thread {
public:
    ControlOutputRouter();
    ~ControlOutputRouter() override;
    bool sendMidiCc(const juce::String& device,int channel,int cc,int value);
    bool sendOscFloat(const juce::String& host,int port,const juce::String& address,float value);
    void clear();
    int droppedCount() const noexcept {return dropped.load();}
private:
    struct Packet {
        enum class Kind {midiCc,oscFloat} kind=Kind::midiCc;
        juce::String device,host,address;
        int channel=1,number=0,value=0,port=9000;
        float decimal=0;
    };
    juce::CriticalSection queueLock;
    std::deque<Packet> queue;
    std::atomic<int> dropped{0};
    static constexpr size_t maxQueue=512;
    bool enqueue(Packet);
    void run() override;
};
}
