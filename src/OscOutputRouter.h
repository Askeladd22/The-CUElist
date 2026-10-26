#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <array>
#include <map>
#include <memory>
#include <vector>
#include <cstdint>

namespace cuelist {
struct OscStream { std::atomic<uint64_t> epoch {0}; int route=-1; };
struct OscEventSink {
    virtual ~OscEventSink()=default;
    virtual void emit(int route,const uint8_t* data,unsigned size,int sampleOffset) noexcept=0;
    virtual void selectStream(OscStream*) noexcept {}
};
class OscOutputRouter final : public OscEventSink, private juce::Thread {
public:
    OscOutputRouter();
    ~OscOutputRouter() override;
    // UI thread only: route identity belongs to column, not to individual events.
    int configure(int columnId,const juce::String& host,int port);
    std::shared_ptr<OscStream> stream(int cellId,int route);
    void beginBlock(double sampleRate) noexcept;
    void selectStream(OscStream* s) noexcept override { selected=s; }
    void emit(int route,const uint8_t* data,unsigned size,int sampleOffset) noexcept override;
    void emergencyStop() noexcept;
    std::atomic<unsigned> dropped {0},sent {0};
private:
    static constexpr unsigned capacity=2048;
    struct Packet {
        int route=-1;
        unsigned size=0;
        std::array<uint8_t,1024> bytes {};
        double due=0;
        OscStream* stream=nullptr;
        uint64_t epoch=0;
    };
    struct Route { int columnId=0; juce::String host; int port=0; };
    std::unique_ptr<std::array<Packet,capacity>> queue;
    std::atomic<unsigned> readIndex {0},writeIndex {0};
    std::atomic<bool> emergency {false};
    juce::CriticalSection mutex;
    std::vector<Route> routes;  // only accessed with mutex except callback indices
    std::map<int,std::shared_ptr<OscStream>> streams;
    OscStream* selected=nullptr;
    double blockTime=0,msPerSample=0;
    void run() override;
};
}
