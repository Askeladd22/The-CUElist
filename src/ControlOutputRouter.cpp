#include "ControlOutputRouter.h"
#include <cmath>

namespace cuelist {
ControlOutputRouter::ControlOutputRouter():juce::Thread("Control OSC output") {startThread();}
ControlOutputRouter::~ControlOutputRouter() {
    signalThreadShouldExit(); notify(); stopThread(-1);
}
bool ControlOutputRouter::sendOscFloat(const juce::String& host,int port,
                                      const juce::String& address,float value) {
    if (host.isEmpty() || port<1 || port>65535 || !address.startsWithChar('/') ||
        !std::isfinite(value)) return false;
    const juce::ScopedLock lock(queueLock);
    if (queue.size()>=maxQueue) {
        dropped.fetch_add(1,std::memory_order_relaxed); return false;
    }
    queue.push_back({host,address,port,value});
    notify(); return true;
}
void ControlOutputRouter::clear() {
    const juce::ScopedLock guard(queueLock); queue.clear();
}
void ControlOutputRouter::run() {
    juce::DatagramSocket udp(false);
    while (!threadShouldExit()) {
        Packet p;
        {
            const juce::ScopedLock lock(queueLock);
            if (!queue.empty()) {p=std::move(queue.front());queue.pop_front();}
        }
        if (p.host.isEmpty()) {wait(25);continue;}
        OscEvent event; event.address=p.address;
        OscValue argument; argument.type=OscValueType::decimal; argument.decimal=p.decimal;
        event.arguments.push_back(std::move(argument));
        auto bytes=encodeOscMessage(event);
        if (!bytes.empty() && bytes.size()<=1024)
            udp.write(p.host,p.port,bytes.data(),(int)bytes.size());
    }
}
}
