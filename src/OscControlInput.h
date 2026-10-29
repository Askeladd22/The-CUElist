#pragma once
#include "ControlMapping.h"
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>

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

    bool startListening(int port) {
        if (port<1 || port>65535) return false;
        if (listeningPort==port && socket) return true;
        stopListening();
        auto next=std::make_unique<juce::DatagramSocket>(false);
        if (!next->bindToPort(port)) return false;
        socket=std::move(next);
        listeningPort=port;
        startThread();
        return true;
    }
    void stopListening() {
        signalThreadShouldExit();
        stopThread(1000);
        socket.reset();
        listeningPort=0;
        readIndex.store(0);writeIndex.store(0);
    }
    int port() const noexcept {return listeningPort;}
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
    std::unique_ptr<juce::DatagramSocket> socket;
    int listeningPort=0;

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
            if (!socket || socket->waitUntilReady(true,60)!=1) continue;
            const int size=socket->read(buffer.data(),(int)buffer.size(),false);
            if (size<=0) continue;
            received.fetch_add(1,std::memory_order_relaxed);
            Packet packet;
            if (decode(buffer.data(),size,packet)) enqueue(packet);
        }
    }
};
} // namespace cuelist
