#pragma once
#include "ControlMapping.h"
#include "DeviceIdentity.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <array>
#include <atomic>
#include <memory>

namespace cuelist {
// A single explicitly selected hardware input. JUCE calls the callback from
// a MIDI input thread; the callback never calls the mapping engine or the UI.
class MidiInputManager final : private juce::MidiInputCallback {
public:
    ~MidiInputManager() { disconnect(); }
    juce::Result connect(const juce::String& identifier) {
        disconnect();
        if (identifier.isEmpty()) return juce::Result::ok();
        if (!hasExactMidiIdentifier(juce::MidiInput::getAvailableDevices(),identifier))
            return juce::Result::fail("MIDI controller not connected.");
        auto port=juce::MidiInput::openDevice(identifier,this);
        if (!port) return juce::Result::fail("Could not open MIDI input.");
        deviceId=identifier;
        input=std::move(port);
        input->start();
        return juce::Result::ok();
    }
    void disconnect() {
        if (input) { input->stop(); input.reset(); }
        deviceId.clear(); read.store(0); write.store(0);
    }
    bool connected() const { return input!=nullptr; }
    const juce::String& identifier() const { return deviceId; }
    bool stillAvailable() const {
        if (deviceId.isEmpty()) return false;
        return hasExactMidiIdentifier(juce::MidiInput::getAvailableDevices(),deviceId);
    }
    int droppedCount() const noexcept { return dropped.load(); }
    uint64_t receivedCount() const noexcept { return received.load(std::memory_order_relaxed); }
    bool poll(ControlEvent& result) {
        const auto r=read.load(std::memory_order_relaxed);
        if (r==write.load(std::memory_order_acquire)) return false;
        const auto& p=queue[r];
        result.deviceId=deviceId; result.kind=p.kind; result.channel=p.channel;
        result.number=p.number; result.value=p.value; result.isNoteOff=p.noteOff;
        read.store((r+1)%capacity,std::memory_order_release);
        return true;
    }
    void ingestForTest(const juce::MidiMessage& message) { handleIncomingMidiMessage(nullptr,message); }
private:
    struct Packet {
        ControlKind kind=ControlKind::cc;
        int channel=1,number=0,value=0;
        bool noteOff=false;
    };
    static constexpr unsigned capacity=2048;
    std::array<Packet,capacity> queue{};
    std::atomic<unsigned> read{0},write{0};
    std::atomic<int> dropped{0};
    std::atomic<uint64_t> received{0};
    juce::String deviceId;
    std::unique_ptr<juce::MidiInput> input;
    void handleIncomingMidiMessage(juce::MidiInput*,const juce::MidiMessage& message) override {
        Packet p;
        p.channel=message.getChannel();
        if (message.isController()) {
            p.kind=ControlKind::cc; p.number=message.getControllerNumber();
            p.value=message.getControllerValue();
        } else if (message.isNoteOnOrOff()) {
            p.kind=ControlKind::note; p.number=message.getNoteNumber();
            p.noteOff=message.isNoteOff();
            p.value=p.noteOff ? 0 : message.getVelocity();
        } else if (message.isPitchWheel()) {
            p.kind=ControlKind::pitchBend; p.value=message.getPitchWheelValue();
        } else return;
        const auto w=write.load(std::memory_order_relaxed),next=(w+1)%capacity;
        if (next==read.load(std::memory_order_acquire)) {
            dropped.fetch_add(1,std::memory_order_relaxed); return;
        }
        queue[w]=p; write.store(next,std::memory_order_release);
        received.fetch_add(1,std::memory_order_relaxed);
    }
};
}
