#pragma once
#include "ShowModel.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>
#include <map>
#include <memory>

namespace cuelist {
struct AudioClip { juce::AudioBuffer<float> samples; double sampleRate = 48000; };
struct Voice {
    int cellId = 0, rowId = 0, output = 0;
    std::shared_ptr<const AudioClip> clip;
    bool followsRow = true, loop = false, playing = false;
    Behaviour behaviour = Behaviour::retrigger;
    float gain = 0.8f;
    double position = 0;
};
struct RenderState {
    std::vector<Voice> voices;
    uint64_t revision = 0;
    void trigger(int id, bool row, bool stop = false) noexcept;
    void panic() noexcept;
    void render(float* const* outputs, int channels, int frames, double sampleRate) noexcept;
};
using ClipMap = std::map<int, std::shared_ptr<const AudioClip>>;
juce::Result loadClip(const juce::File&, std::shared_ptr<const AudioClip>&);
std::unique_ptr<RenderState> makeRenderState(const Show&, const ClipMap&);

class AudioEngine final : private juce::AudioIODeviceCallback {
public:
    AudioEngine();
    ~AudioEngine() override;
    juce::String initialise();
    void publish(std::unique_ptr<RenderState>);
    void collectRetired();
    bool send(int id, bool row, bool stop = false) noexcept;
    void panic() noexcept;
    void processBlock(float* const*, int channels, int frames, double sampleRate) noexcept;
    juce::AudioDeviceManager devices;
    std::atomic<float> peak {0};
    std::atomic<int> activeVoices {0};
    std::atomic<bool> connected {false};
private:
    struct Command { int id = 0; bool row = false, stop = false; uint64_t revision = 0, epoch = 0; };
    std::array<Command, 256> commands;
    std::atomic<unsigned> writeIndex {0}, readIndex {0};
    std::atomic<RenderState*> pending {nullptr}, retired {nullptr};
    RenderState* current = nullptr; // accessed only by audio callback, then destructor after removal
    uint64_t revision = 0, handledEpoch = 0;
    std::atomic<uint64_t> panicEpoch {0};
    double deviceSampleRate = 48000;
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const*, int, int,
                                         const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
};
}
