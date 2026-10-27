#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <array>
#include <atomic>
#include <map>
#include <deque>
namespace cuelist {
struct MidiStream {
    std::atomic<uint64_t> epoch {0};
    uint64_t workerEpoch=0;
    int route=-1;
    std::array<unsigned short,2048> sounding {};
};
// The callback may only enqueue bounded short messages. Implementations must not allocate or block.
struct MidiEventSink {
    virtual ~MidiEventSink() = default;
    virtual void emit(int route, int status, int data1, int data2, int sampleOffset) noexcept=0;
    virtual void selectStream(MidiStream*) noexcept {}
};
class MidiOutputRouter final : public MidiEventSink, private juce::Thread {
public:
    MidiOutputRouter();
    ~MidiOutputRouter() override;
    juce::Result open(const juce::String& identifier, bool reconnect=false);
    // UI-originated MIDI controller commands use the existing device output
    // worker, never opening a second handle to the same physical port.
    bool queueMappedCc(const juce::String& identifier,int channel,int cc,int value);
    void refresh(); // UI thread: invalidate handles for disconnected devices; never choose a substitute.
    int route(const juce::String&) const;
    void emit(int,int,int,int,int) noexcept override;
    void emergencyStop() noexcept;
    std::atomic<unsigned> dropped{0}, sent{0};
    std::shared_ptr<MidiStream> stream(int cellId,int route);
    void selectStream(MidiStream* value) noexcept override { selected=value; }
    void beginBlock(double rate,double sharedOriginMs=-1) noexcept;
private:
    struct Packet { int route,status,a,b; double due=0; MidiStream* stream=nullptr; uint64_t epoch=0; double blockOrigin=0; };
    static constexpr unsigned capacity=65536;
    std::unique_ptr<std::array<Packet,capacity>> queue;
    std::atomic<unsigned> read {0},write {0};
    std::atomic<bool> emergency {false};
    juce::CriticalSection deviceLock;
    struct MappedCc { int route=0,channel=1,cc=0,value=0; };
    std::deque<MappedCc> mappedCommands;
    std::map<juce::String,int> routes; // UI thread only
    std::vector<std::unique_ptr<juce::MidiOutput>> devices;
    std::vector<std::array<unsigned,2048>> sounding;
    std::vector<double> resetTimes;
    std::map<int,std::shared_ptr<MidiStream>> streams;
    MidiStream* selected=nullptr;
    double blockTime=0, sampleMillis=0;
    void run() override;
    void closeRoute(int route); // deviceLock held, UI thread only
};
}
