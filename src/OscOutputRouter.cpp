#include "OscOutputRouter.h"
#include "TransportTiming.h"
#include <algorithm>
#include <cstring>
namespace cuelist {
OscOutputRouter::OscOutputRouter()
    : juce::Thread("OSC UDP dispatcher"),queue(std::make_unique<std::array<Packet,capacity>>()) { startThread(); }
OscOutputRouter::~OscOutputRouter() { signalThreadShouldExit(); stopThread(-1); }
int OscOutputRouter::configure(int columnId,const juce::String& host,int port) {
    const juce::ScopedLock lock(mutex);
    for (size_t i=0;i<routes.size();++i) if (routes[i].columnId==columnId) {
        if (routes[i].host!=host || routes[i].port!=port) {
            routes[i].host=host; routes[i].port=port;
            for (auto& s:streams) if (s.second->route==(int)i) s.second->epoch.fetch_add(1);
        }
        return (int)i;
    }
    routes.push_back({columnId,host,port});
    return (int)routes.size()-1;
}
std::shared_ptr<OscStream> OscOutputRouter::stream(int cellId,int route) {
    const juce::ScopedLock lock(mutex);
    auto& s=streams[cellId];
    if (!s) s=std::make_shared<OscStream>();
    if (s->route!=route) { s->route=route; s->epoch.fetch_add(1); }
    if(route>=0 && (size_t)route<routes.size()) {
        auto& counter=sentByColumn[routes[(size_t)route].columnId];
        if(!counter) counter=std::make_shared<std::atomic<uint64_t>>(0);
        s->columnSent=counter;
    }
    return s;
}
uint64_t OscOutputRouter::getColumnSent(int columnId) {
    const juce::ScopedLock lock(mutex);
    auto it=sentByColumn.find(columnId);
    return it==sentByColumn.end()?0:it->second->load(std::memory_order_relaxed);
}
void OscOutputRouter::beginBlock(double rate,double sharedOriginMs) noexcept {
    blockTime=sharedOriginMs>=0 ? sharedOriginMs : juce::Time::getMillisecondCounterHiRes();
    msPerSample=rate>0 ? 1000.0/rate : 0;
}
void OscOutputRouter::emit(int route,const uint8_t* data,unsigned size,int offset) noexcept {
    if (route<0 || !data || size==0 || size>1024 || emergency.load(std::memory_order_relaxed)) return;
    const auto w=writeIndex.load(std::memory_order_relaxed);
    const auto next=(w+1)%capacity;
    if (next==readIndex.load(std::memory_order_acquire)) { dropped.fetch_add(1); return; }
    auto& p=(*queue)[w];
    p.route=route; p.size=size; p.due=scheduleOutputMillis(blockTime,offset,msPerSample);
    p.stream=selected; p.epoch=selected ? selected->epoch.load() : 0;
    std::memcpy(p.bytes.data(),data,size);
    writeIndex.store(next,std::memory_order_release);
}
void OscOutputRouter::emergencyStop() noexcept { emergency.store(true); }
void OscOutputRouter::run() {
    juce::DatagramSocket socket(false);
    std::vector<Packet> pending;
    pending.reserve(capacity);
    for (;;) {
        const bool closing=threadShouldExit();
        {
            const juce::ScopedLock lock(mutex);
            if (closing || emergency.exchange(false)) {
                readIndex.store(writeIndex.load(std::memory_order_acquire),std::memory_order_release);
                pending.clear();
            } else {
                auto r=readIndex.load(std::memory_order_relaxed);
                const auto w=writeIndex.load(std::memory_order_acquire);
                while (r!=w) {
                    if (pending.size()<capacity) pending.push_back((*queue)[r]);
                    else dropped.fetch_add(1);
                    r=(r+1)%capacity;
                }
                readIndex.store(r,std::memory_order_release);
                std::stable_sort(pending.begin(),pending.end(),[](const Packet& a,const Packet& b){return a.due<b.due;});
                const auto now=juce::Time::getMillisecondCounterHiRes();
                auto end=pending.begin();
                while (end!=pending.end() && end->due<=now) {
                    const auto& p=*end++;
                    if (p.route<0 || (size_t)p.route>=routes.size() || (p.stream && p.stream->epoch.load()!=p.epoch)) continue;
                    const auto& route=routes[(size_t)p.route];
                    if (route.host.isEmpty() || route.port<1 || route.port>65535) continue;
                    if (juce::Time::getMillisecondCounterHiRes()-p.due>10.0)
                        lateOver10ms.fetch_add(1,std::memory_order_relaxed);
                    if (socket.write(route.host,route.port,p.bytes.data(),(int)p.size)==(int)p.size) {
                        sent.fetch_add(1);
                        if(p.stream && p.stream->columnSent)
                            p.stream->columnSent->fetch_add(1,std::memory_order_relaxed);
                    } else dropped.fetch_add(1);
                }
                pending.erase(pending.begin(),end);
            }
        }
        if (closing) break;
        wait(1);
    }
}
}
