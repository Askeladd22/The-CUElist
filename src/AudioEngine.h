#pragma once
#include "ShowModel.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>
#include <map>
#include <memory>
#include "MidiOutputRouter.h"
#include "OscOutputRouter.h"

namespace cuelist {
struct AudioClip { juce::AudioBuffer<float> samples; double sampleRate = 48000; };
struct CellPlaybackInfo {
    double positionSeconds = 0, durationSeconds = 0;
    bool playing = false, waiting = false, paused = false;
};
// Single audio-thread writer; UI readers never inspect mutable Voice state.
struct CellPlaybackTelemetry {
    std::atomic<double> positionSeconds {0}, durationSeconds {0};
    std::atomic<unsigned> flags {0};
    std::atomic<uint64_t> sequence {0};
    CellPlaybackInfo read() const noexcept;
    void write(CellPlaybackInfo) noexcept;
};
// Column meter receives post-strip, pre-master peak samples from the audio callback.
// No allocations, locks, or UI lookups occur on the real-time thread.
struct ColumnPeakTelemetry {
    static constexpr int maxMeterFrames=8192;
    // The scratch arrays are allocated on the UI thread when creating a render
    // state, never inside audioDeviceIOCallback. They preserve stereo sums
    // across simultaneous voices on the same column.
    std::array<std::vector<float>,2> mix;
    std::array<float,2> oversizeVoicePeak {{0,0}};
    std::atomic<float> peak {0}, left {0}, right {0};
    ColumnPeakTelemetry() {
        for (auto& channel:mix) channel.resize(maxMeterFrames,0.0f);
    }
};
struct Voice {
    int cellId = 0, rowId = 0, output = 0, columnIndex = -1, columnId = 0;
    std::shared_ptr<ColumnPeakTelemetry> columnMeter;
    std::shared_ptr<const AudioClip> clip;
    struct NoteEvent { double seconds=0; int status=0, pitch=0, velocity=0; };
    std::vector<NoteEvent> midiEvents;
    bool isMidi=false, isOsc=false;
    double midiDuration=0;
    struct OscPreparedEvent { double seconds=0; std::vector<uint8_t> bytes; };
    std::vector<OscPreparedEvent> oscEvents;
    double oscDuration=0;
    size_t oscCursor=0;
    bool oscNeedsSync=true;
    uint64_t oscHash=0;
    int oscColumnId=0,oscRoute=-1,oscPort=9000;
    juce::String oscHost;
    OscEventSink* oscSink=nullptr;
    std::shared_ptr<OscStream> oscStream;
    uint64_t midiHash=0;
    juce::String midiOutputId;
    int midiRoute=-1;
    MidiEventSink* midiSink=nullptr;
    std::shared_ptr<MidiStream> midiStream;
    std::array<unsigned char,2048> heldVelocity {};
    std::array<unsigned short,2048> heldNotes {};
    size_t midiCursor=0;
    bool midiNeedsSync=true;
    bool followsRow = true, loop = false, playing = false, paused = false;
    bool stopOnNext = true; // immutable row policy copied into this render snapshot
    std::shared_ptr<CellPlaybackTelemetry> telemetry;
    Behaviour behaviour = Behaviour::retrigger;
    float gain = 0.8f;
    double position = 0;
    double startSeconds = 0, delaySeconds = 0;
    float rowGain = 1;
    float columnGain = 1, columnPan = 0;
    bool columnMuted = false, columnPhaseInverted = false;
    bool masterSend=false, directOutputEnabled=true, directPostFader=true;
    bool rowMuted = false, rowLoop = false, overrideBehaviour = false;
    Behaviour rowBehaviour = Behaviour::retrigger;
    bool rowMode = false, pendingSeek = false;
};
struct RowTransport {
    int id = 0;
    double seconds = 0;
    bool playing = false, pendingSeek = false, ended = false;
    bool stopOnNext = true;
};
struct RenderState {
    float masterGain=1.0f, masterPan=0.0f;
    bool masterMuted=false;
    int masterOutput=0;
    bool safetyLimiterEnabled=true;
    float limiterGain=1.0f;
    bool limiterReducing=false;
    float preLimiterPeakLeft=0.0f, preLimiterPeakRight=0.0f;
    static constexpr int maxMixerFrames=65536;
    // Allocated before publication, never on the audio callback.
    std::array<std::vector<float>,2> masterMix;
    RenderState() {for(auto& channel:masterMix) channel.resize(maxMixerFrames,0.0f);}
    std::vector<std::pair<int,std::shared_ptr<ColumnPeakTelemetry>>> columnMeters;
    std::vector<Voice> voices;
    std::vector<RowTransport> rows;
    uint64_t revision = 0;
    uint64_t minimumCommandRevision = 0;
    bool preservePlayback = true;
    void trigger(int id, bool row, bool stop = false) noexcept;
    void seekRow(int rowId, double seconds) noexcept;
    void inheritPlayback(const RenderState&) noexcept;
    void panic() noexcept;
    void releaseMidi(Voice&, int sampleOffset=0, bool cancelPending=true) noexcept;
    void render(float* const* outputs, int channels, int frames, double sampleRate) noexcept;
    void updateTelemetry() noexcept;
};
using ClipMap = std::map<int, std::shared_ptr<const AudioClip>>;
juce::Result loadClip(const juce::File&, std::shared_ptr<const AudioClip>&);
std::unique_ptr<RenderState> makeRenderState(const Show&, const ClipMap&);

class AudioEngine final : private juce::AudioIODeviceCallback {
public:
    AudioEngine();
    ~AudioEngine() override;
    juce::String initialise();
    void publish(std::unique_ptr<RenderState>, bool preservePlayback = true);
    void collectRetired();
    bool send(int id, bool row, bool stop = false) noexcept;
    bool seekRow(int rowId, double seconds) noexcept;
    void panic() noexcept;
    juce::Result setMidiOutput(const juce::String& identifier, bool reconnect=false);
    bool sendMappedMidiCc(const juce::String& id,int channel,int number,int value);
    void refreshMidiOutputs();
    void processBlock(float* const*, int channels, int frames, double sampleRate) noexcept;
    // UI-thread only. Unknown or removed cells return an empty value.
    CellPlaybackInfo getCellPlayback(int cellId) const noexcept;
    float getColumnPeak(int columnId) const noexcept;
    // Returns post-cell/row/strip, pre-master L/R peaks (linear amplitude).
    std::array<float,2> getColumnStereoPeak(int columnId) const noexcept;
    uint64_t getMidiColumnSent(int columnId) {return midiRouter.getColumnSent(columnId);}
    std::atomic<bool> limiterActive{false};
    juce::AudioDeviceManager devices;
    std::atomic<float> peak {0};
    std::atomic<float> masterPeakLeft {0}, masterPeakRight {0};
    std::atomic<int> activeVoices {0};
    std::atomic<bool> connected {false};
    // Last row GO actually processed in the callback. A queued GO does not
    // change the active control mapping until acknowledged by transport.
    std::atomic<int> acknowledgedCueId {0};
    std::atomic<int> monitoredRowId {0};
    std::atomic<double> playheadSeconds {0};
    std::atomic<bool> rowPlaying {false};
    struct Diagnostics {
        unsigned audioCommandsDropped=0, midiOutputDropped=0, midiOutputSent=0,
                 oscDropped=0, oscSent=0, midiLateOver10ms=0, oscLateOver10ms=0;
    };
    Diagnostics diagnostics() const noexcept {
        return {commandDrops.load(std::memory_order_relaxed),
                midiRouter.dropped.load(std::memory_order_relaxed),
                midiRouter.sent.load(std::memory_order_relaxed),
                oscRouter.dropped.load(std::memory_order_relaxed),
                oscRouter.sent.load(std::memory_order_relaxed),
                midiRouter.lateOver10ms.load(std::memory_order_relaxed),
                oscRouter.lateOver10ms.load(std::memory_order_relaxed)};
    }
private:
    MidiOutputRouter midiRouter;
    OscOutputRouter oscRouter;
    struct PlaybackEntry {
        std::shared_ptr<const AudioClip> clip;
        std::shared_ptr<CellPlaybackTelemetry> telemetry;
        uint64_t midiHash=0, oscHash=0;
    };
    std::map<int, PlaybackEntry> playback; // UI-thread owned; no callback map access
    std::map<int,std::shared_ptr<ColumnPeakTelemetry>> columnPeaks; // UI-thread owned
    struct Command { int id = 0; bool row = false, stop = false; uint64_t revision = 0, epoch = 0; bool seek = false; double seconds = 0; };
    std::array<Command, 256> commands;
    std::atomic<unsigned> writeIndex {0}, readIndex {0};
    std::atomic<RenderState*> pending {nullptr}, retired {nullptr};
    RenderState* current = nullptr; // accessed only by audio callback, then destructor after removal
    uint64_t revision = 0, handledEpoch = 0, structuralRevision = 0;
    std::atomic<uint64_t> panicEpoch {0};
    std::atomic<unsigned> commandDrops {0};
    double deviceSampleRate = 48000;
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const*, int, int,
                                         const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
};
}
