#pragma once
#include "ControlMapping.h"
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>
#include <algorithm>

namespace cuelist {

// OSC 1.0 numeric/boolean input for Control Mapping. The socket thread only
// parses bounded datagrams and publishes plain POD events; it never touches
// JUCE controls, the Show or the audio callback.
class OscControlInput final : private juce::Thread {
public:
    OscControlInput():juce::Thread("OSC Control Input") {}
    ~OscControlInput() override { stopListening(); }

    struct Packet {
        std::array<char,128> address{};
        int value=0;
    };
    static bool decode(const void* buffer,int length,Packet& packet) {
        if (!buffer || length<8 || length>1024) return false;
        const auto* bytes=static_cast<const uint8_t*>(buffer);
        int end=0;
        while (end<length && bytes[end]!=0 && end<127) {
            // Only canonical printable ASCII paths; no strings from
            // untrusted UDP are passed to the mapping thread.
            if (bytes[end]<33 || bytes[end]>126) return false;
            ++end;
        }
        if (end<2 || end>=length || end>=127 || bytes[end]!=0 ||
            bytes[0]!='/') return false;
        const int tagsStart=(end+4)&~3;
        if (tagsStart>=length || bytes[tagsStart]!=',') return false;
        const int tagEnd=tagsStart+2;
        if (tagEnd>=length || bytes[tagEnd]!=0) return false;
        const char tag=(char)bytes[tagsStart+1];
        const int dataStart=(tagEnd+4)&~3;
        int value=0;
        if (tag=='T') value=127;
        else if (tag=='F') value=0;
        else if (tag=='f' || tag=='i') {
            if (dataStart+4>length) return false;
            const uint32_t bits=((uint32_t)bytes[dataStart]<<24) |
                ((uint32_t)bytes[dataStart+1]<<16) |
                ((uint32_t)bytes[dataStart+2]<<8) | bytes[dataStart+3];
            if (tag=='f') {
                float number=0;
                static_assert(sizeof(float)==sizeof(uint32_t),"IEEE754 required");
                std::memcpy(&number,&bits,sizeof(number));
                if (!std::isfinite(number)) return false;
                // OSC floats can be any finite IEEE-754 value, not only [0,1].
                // Clamp BEFORE rounding/converting to int: a large finite
                // value would otherwise overflow lround and the int cast.
                const float unit=juce::jlimit(0.0f,1.0f,number);
                value=(int)std::lround(unit*127.0f);
            } else {
                const auto number=(int32_t)bits;
                value=juce::jlimit(0,127,(int)number);
            }
        } else return false;
        packet={};
        for (int i=0;i<end;++i) packet.address[(size_t)i]=(char)bytes[i];
        packet.value=value;
        return true;
    }

    bool startListening(int port) {return startListeningPorts({port});}
    bool startListeningPorts(const std::vector<int>& ports) {
        if (ports.empty() || ports.size()>16) return false;
        std::vector<int> unique;
        for (int p:ports) {
            if (p<1 || p>65535 || std::find(unique.begin(),unique.end(),p)!=unique.end())
                return false;
            unique.push_back(p);
        }
        if (unique==listeningPorts && !sockets.empty()) return true;
        // Validate every UDP port before replacing the live listener set.
        std::vector<std::unique_ptr<juce::DatagramSocket>> next;
        for (int p:unique) {
            auto socket=std::make_unique<juce::DatagramSocket>(false);
            if (!socket->bindToPort(p)) return false;
            next.push_back(std::move(socket));
        }
        stopListening();
        listeningPorts=std::move(unique);
        sockets=std::move(next);
        startThread();
        return true;
    }
    void stopListening() {
        signalThreadShouldExit();
        stopThread(1000);
        sockets.clear();
        listeningPorts.clear();
        readIndex.store(0);writeIndex.store(0);
    }
    int port() const noexcept {return listeningPorts.empty()?0:listeningPorts.front();}
    const std::vector<int>& ports() const noexcept {return listeningPorts;}
    uint64_t receivedCount() const noexcept {
        return received.load(std::memory_order_relaxed);
    }
    int droppedCount() const noexcept {return dropped.load();}
    bool poll(ControlEvent& event) {
        const auto r=readIndex.load(std::memory_order_relaxed);
        if (r==writeIndex.load(std::memory_order_acquire)) return false;
        const auto packet=queue[r];
        readIndex.store((r+1)%capacity,std::memory_order_release);
        event.deviceId=juce::String::fromUTF8(packet.address.data());
        event.kind=ControlKind::osc;
        event.channel=1;event.number=0;event.value=packet.value;
        event.isNoteOff=packet.value==0;
        return true;
    }
    void ingestForTest(const void* data,int length) {
        Packet p;
        if (decode(data,length,p)) enqueue(p);
    }
private:
    static constexpr unsigned capacity=256;
    std::array<Packet,capacity> queue{};
    std::atomic<unsigned> readIndex{0},writeIndex{0};
    std::atomic<uint64_t> received{0};
    std::atomic<int> dropped{0};
    std::vector<std::unique_ptr<juce::DatagramSocket>> sockets;
    std::vector<int> listeningPorts;

    void enqueue(const Packet& value) {
        const auto w=writeIndex.load(std::memory_order_relaxed);
        const auto next=(w+1)%capacity;
        if (next==readIndex.load(std::memory_order_acquire)) {
            dropped.fetch_add(1,std::memory_order_relaxed);
            return;
        }
        queue[w]=value;writeIndex.store(next,std::memory_order_release);
    }
    void run() override {
        std::array<uint8_t,1024> buffer{};
        while (!threadShouldExit()) {
            for (const auto& socket:sockets) {
                if (threadShouldExit()) break;
                if (!socket || socket->waitUntilReady(true,10)!=1) continue;
                const int size=socket->read(buffer.data(),(int)buffer.size(),false);
                if (size<=0) continue;
                received.fetch_add(1,std::memory_order_relaxed);
                Packet packet;
                if (decode(buffer.data(),size,packet)) enqueue(packet);
            }
        }
    }
};
} // namespace cuelist
