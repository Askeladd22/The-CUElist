#pragma once
#include "ShowModel.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>
#include <map>
#include <memory>

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
struct Voice {
    int cellId = 0, rowId = 0, output = 0;
    std::shared_ptr<const AudioClip> clip;
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
    std::vector<Voice> voices;
    std::vector<RowTransport> rows;
    uint64_t revision = 0;
    uint64_t minimumCommandRevision = 0;
    bool preservePlayback = true;
    void trigger(int id, bool row, bool stop = false) noexcept;
    void seekRow(int rowId, double seconds) noexcept;
    void inheritPlayback(const RenderState&) noexcept;
    void panic() noexcept;
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
    void processBlock(float* const*, int channels, int frames, double sampleRate) noexcept;
    // UI-thread only. Unknown or removed cells return an empty value.
    CellPlaybackInfo getCellPlayback(int cellId) const noexcept;
    juce::AudioDeviceManager devices;
    std::atomic<float> peak {0};
    std::atomic<int> activeVoices {0};
    std::atomic<bool> connected {false};
    std::atomic<int> monitoredRowId {0};
    std::atomic<double> playheadSeconds {0};
    std::atomic<bool> rowPlaying {false};
private:
    struct PlaybackEntry {
        std::shared_ptr<const AudioClip> clip;
        std::shared_ptr<CellPlaybackTelemetry> telemetry;
    };
    std::map<int, PlaybackEntry> playback; // UI-thread owned; no callback map access
    struct Command { int id = 0; bool row = false, stop = false; uint64_t revision = 0, epoch = 0; bool seek = false; double seconds = 0; };
    std::array<Command, 256> commands;
    std::atomic<unsigned> writeIndex {0}, readIndex {0};
    std::atomic<RenderState*> pending {nullptr}, retired {nullptr};
    RenderState* current = nullptr; // accessed only by audio callback, then destructor after removal
    uint64_t revision = 0, handledEpoch = 0, structuralRevision = 0;
    std::atomic<uint64_t> panicEpoch {0};
    double deviceSampleRate = 48000;
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const*, int, int,
                                         const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
};
}
