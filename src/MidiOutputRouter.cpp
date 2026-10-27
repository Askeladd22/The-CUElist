#include "MidiOutputRouter.h"
#include "TransportTiming.h"
#include "DeviceIdentity.h"
namespace cuelist {
MidiOutputRouter::MidiOutputRouter():Thread("MIDI output dispatcher"),queue(std::make_unique<std::array<Packet,capacity>>()) { startThread(); }
MidiOutputRouter::~MidiOutputRouter() { signalThreadShouldExit(); stopThread(-1); }
void MidiOutputRouter::closeRoute(int route) {
    auto& output=devices[(size_t)route];
    if (output) for (int ch=1;ch<=16;++ch) {
        output->sendMessageNow(juce::MidiMessage::allNotesOff(ch));
        output->sendMessageNow(juce::MidiMessage::allSoundOff(ch));
    }
    output.reset(); sounding[(size_t)route].fill(0);
    resetTimes[(size_t)route]=juce::Time::getMillisecondCounterHiRes();
    for (auto& pair:streams) if (pair.second->route==route) {
        auto& s=*pair.second; s.workerEpoch=s.epoch.fetch_add(1)+1; s.sounding.fill(0);
    }
}
void MidiOutputRouter::refresh() {
    const auto available=juce::MidiOutput::getAvailableDevices();
    const juce::ScopedLock lock(deviceLock);
    for (const auto& route:routes) {
        const bool found=hasExactMidiIdentifier(available,route.first);
        const int index=route.second;
        if (!found && devices[(size_t)index]) closeRoute(index);
        else if (found && !devices[(size_t)index]) {
            // Reconnect ONLY the operator-configured device ID. Never silently
            // substitute another MIDI port. Old packets are invalidated by
            // resetTimes and stream epochs.
            devices[(size_t)index]=juce::MidiOutput::openDevice(route.first);
            if (devices[(size_t)index]) {
                resetTimes[(size_t)index]=juce::Time::getMillisecondCounterHiRes();
                for (auto& pair:streams) if (pair.second->route==index) {
                    pair.second->epoch.fetch_add(1,std::memory_order_release);
                }
            }
        }
    }
}
juce::Result MidiOutputRouter::open(const juce::String& id,bool reconnect) {
    if (id.isEmpty()) return juce::Result::ok();
    const bool available=hasExactMidiIdentifier(juce::MidiOutput::getAvailableDevices(),id);
    const juce::ScopedLock lock(deviceLock);
    const auto found=routes.find(id);
    if (!available) {
        if (found!=routes.end() && devices[(size_t)found->second]) closeRoute(found->second);
        return juce::Result::fail("MIDI output disconnected or unavailable.");
    }
    if (found!=routes.end()) {
        const auto slot=(size_t)found->second;
        if (devices[slot] && !reconnect) return juce::Result::ok();
        closeRoute((int)slot);
        devices[slot]=juce::MidiOutput::openDevice(id);
        resetTimes[slot]=juce::Time::getMillisecondCounterHiRes();
        return devices[slot] ? juce::Result::ok() : juce::Result::fail("Could not reopen MIDI output; output muted.");
    }
    auto output=juce::MidiOutput::openDevice(id);
    if (!output) return juce::Result::fail("MIDI output unavailable.");
    routes.emplace(id,(int)devices.size()); devices.push_back(std::move(output)); sounding.emplace_back();
    resetTimes.push_back(juce::Time::getMillisecondCounterHiRes());
    return juce::Result::ok();
}
bool MidiOutputRouter::queueMappedCc(const juce::String& id,int channel,int cc,int value) {
    if (id.isEmpty() || channel<1 || channel>16 || cc<0 || cc>127 || value<0 || value>127)
        return false;
    if (open(id).failed()) return false;
    const juce::ScopedLock lock(deviceLock);
    const auto it=routes.find(id);
    if (it==routes.end() || mappedCommands.size()>=512) {
        dropped.fetch_add(1,std::memory_order_relaxed); return false;
    }
    mappedCommands.push_back({it->second,channel,cc,value});
    return true;
}
int MidiOutputRouter::route(const juce::String& id) const {
    const auto it=routes.find(id); return it==routes.end() ? -1 : it->second;
}
std::shared_ptr<MidiStream> MidiOutputRouter::stream(int cellId,int output) {
    const juce::ScopedLock lock(deviceLock);
    auto& value=streams[cellId]; if (!value) value=std::make_shared<MidiStream>();
    // A fresh identity for route changes; retain old identity until router destruction.
    if (value->route!=output && value->route>=0) {
        static int retiredId=-1; streams[retiredId--]=value; value=std::make_shared<MidiStream>();
    }
    value->route=output; return value;
}
void MidiOutputRouter::beginBlock(double rate,double sharedOriginMs) noexcept {
    blockTime=sharedOriginMs>=0 ? sharedOriginMs : juce::Time::getMillisecondCounterHiRes();
    sampleMillis=rate>0 ? 1000.0/rate : 0;
}
void MidiOutputRouter::emit(int route,int status,int a,int b,int offset) noexcept {
    if (route<0 || emergency.load(std::memory_order_relaxed)) return;
    const auto w=write.load(std::memory_order_relaxed);
    const auto next=(w+1)%capacity;
    if (next==read.load(std::memory_order_acquire)) {
        dropped.fetch_add(1,std::memory_order_relaxed);
        emergency.store(true); return;
    }
    (*queue)[w]={route,status,a,b,scheduleOutputMillis(blockTime,offset,sampleMillis),selected,selected ? selected->epoch.load() : 0,blockTime}; write.store(next,std::memory_order_release);
}
void MidiOutputRouter::emergencyStop() noexcept { emergency.store(true); }
void MidiOutputRouter::run() {
    std::vector<Packet> pending; pending.reserve(capacity);
    for (;;) {
        const bool closing=threadShouldExit();
        {
            const juce::ScopedLock lock(deviceLock);
            if (closing || emergency.exchange(false)) {
                read.store(write.load(std::memory_order_acquire),std::memory_order_release);
                pending.clear();
                mappedCommands.clear();
                for (auto& counts:sounding) counts.fill(0);
                for (auto& pair:streams) { pair.second->sounding.fill(0); pair.second->workerEpoch=pair.second->epoch.load(); }
                for (auto& d:devices) if (d) for (int ch=1;ch<=16;++ch) {
                    d->sendMessageNow(juce::MidiMessage::allNotesOff(ch));
                    d->sendMessageNow(juce::MidiMessage::allSoundOff(ch));
                }
            } else {
                for (const auto& message:mappedCommands)
                    if (message.route>=0 && (size_t)message.route<devices.size() &&
                        devices[(size_t)message.route])
                        {
                            devices[(size_t)message.route]->sendMessageNow(
                                juce::MidiMessage::controllerEvent(message.channel,message.cc,message.value));
                            sent.fetch_add(1,std::memory_order_relaxed);
                        }
                mappedCommands.clear();
                for (auto& pair:streams) {
                    auto& s=*pair.second; const auto epoch=s.epoch.load();
                    if (s.workerEpoch==epoch) continue;
                    if (s.route>=0 && (size_t)s.route<devices.size() && devices[(size_t)s.route]) for (size_t key=0;key<s.sounding.size();++key) if (s.sounding[key]) {
                        auto& count=sounding[(size_t)s.route][key]; count=count>=s.sounding[key] ? count-s.sounding[key] : 0;
                        if (!count) devices[(size_t)s.route]->sendMessageNow(juce::MidiMessage::noteOff(1+(int)(key/128),(int)(key%128)));
                    }
                    s.sounding.fill(0); s.workerEpoch=epoch;
                }
                auto r=read.load(std::memory_order_relaxed); const auto w=write.load(std::memory_order_acquire);
                while (r!=w) {
                    if (pending.size()<capacity) pending.push_back((*queue)[r]);
                    else {dropped.fetch_add(1,std::memory_order_relaxed);emergency.store(true);}
                    r=(r+1)%capacity;
                }
                read.store(r,std::memory_order_release);
                std::stable_sort(pending.begin(),pending.end(),[](const auto& a,const auto& b){ return a.due<b.due; });
                const auto now=juce::Time::getMillisecondCounterHiRes();
                auto end=pending.begin();
                while (end!=pending.end() && end->due<=now) {
                    const auto& p=*end++;
                    if (p.route<0 || (size_t)p.route>=devices.size() || !devices[(size_t)p.route]
                        || p.blockOrigin<=resetTimes[(size_t)p.route] || (p.stream && p.stream->epoch.load()!=p.epoch)) continue;
                    if (juce::Time::getMillisecondCounterHiRes()-p.due>10.0)
                        lateOver10ms.fetch_add(1,std::memory_order_relaxed);
                    const size_t key=(size_t)((p.status&15)*128+p.a); auto& total=sounding[(size_t)p.route][key];
                    if ((p.status&0xf0)==0x90) {
                        ++total; if (p.stream) ++p.stream->sounding[key];
                        devices[(size_t)p.route]->sendMessageNow(juce::MidiMessage(p.status,p.a,p.b));
                        sent.fetch_add(1,std::memory_order_relaxed);
                    } else if (p.stream && p.stream->sounding[key]) {
                        total-=std::min(total,(unsigned)p.stream->sounding[key]); p.stream->sounding[key]=0;
                        if (!total) {
                            devices[(size_t)p.route]->sendMessageNow(juce::MidiMessage(p.status,p.a,p.b));
                            sent.fetch_add(1,std::memory_order_relaxed);
                        }
                    }
                }
                pending.erase(pending.begin(),end);
            }
        }
        if (closing) break;
        wait(1);
    }
}
}
