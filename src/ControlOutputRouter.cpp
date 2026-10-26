#include "ControlOutputRouter.h"
#include <juce_audio_devices/juce_audio_devices.h>

namespace cuelist {
ControlOutputRouter::ControlOutputRouter():juce::Thread("Control Mapping output") {startThread();}
ControlOutputRouter::~ControlOutputRouter() {
    signalThreadShouldExit(); notify(); stopThread(-1);
}
bool ControlOutputRouter::enqueue(Packet packet) {
    const juce::ScopedLock guard(queueLock);
    if (queue.size()>=maxQueue) {
        dropped.fetch_add(1,std::memory_order_relaxed); return false;
    }
    queue.push_back(std::move(packet)); notify(); return true;
}
bool ControlOutputRouter::sendMidiCc(const juce::String& id,int channel,int cc,int value) {
    if (id.isEmpty() || channel<1 || channel>16 || cc<0 || cc>127 || value<0 || value>127)
        return false;
    Packet p; p.kind=Packet::Kind::midiCc; p.device=id;
    p.channel=channel; p.number=cc; p.value=value;
    return enqueue(std::move(p));
}
bool ControlOutputRouter::sendOscFloat(const juce::String& host,int port,
                                      const juce::String& address,float value) {
    if (host.isEmpty() || port<1 || port>65535 || !address.startsWithChar('/') ||
        !std::isfinite(value)) return false;
    Packet p; p.kind=Packet::Kind::oscFloat; p.host=host; p.port=port;
    p.address=address; p.decimal=value; return enqueue(std::move(p));
}
void ControlOutputRouter::clear() {
    const juce::ScopedLock guard(queueLock); queue.clear();
}
void ControlOutputRouter::run() {
    juce::DatagramSocket udp(false);
    std::map<juce::String,std::unique_ptr<juce::MidiOutput>> midi;
    while (!threadShouldExit()) {
        Packet p;
        {
            const juce::ScopedLock lock(queueLock);
            if (queue.empty()) {p.device.clear(); p.host.clear();}
            else {p=std::move(queue.front()); queue.pop_front();}
        }
        if (p.device.isEmpty() && p.host.isEmpty()) {
            wait(25); continue;
        }
        if (p.kind==Packet::Kind::midiCc) {
            auto& out=midi[p.device];
            if (!out) {
                for (const auto& device:juce::MidiOutput::getAvailableDevices())
                    if (device.identifier==p.device) {
                        out=juce::MidiOutput::openDevice(p.device);break;
                    }
            }
            if (out) out->sendMessageNow(
                juce::MidiMessage::controllerEvent(p.channel,p.number,p.value));
        } else {
            OscEvent event; event.address=p.address;
            OscValue argument; argument.type=OscValueType::decimal; argument.decimal=p.decimal;
            event.arguments.push_back(std::move(argument));
            auto bytes=encodeOscMessage(event);
            if (!bytes.empty() && bytes.size()<=1024)
                udp.write(p.host,p.port,bytes.data(),(int)bytes.size());
        }
    }
}
}
