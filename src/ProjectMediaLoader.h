#pragma once
#include "AudioEngine.h"
#include <cstdint>
#include <functional>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

namespace cuelist {
// The media loader is independent of the JUCE view hierarchy. Tests may inject
// an in-memory decoder; production continues to use loadClip().
using AudioClipLoader=std::function<juce::Result(const juce::File&,
                                                std::shared_ptr<const AudioClip>&)>;
struct PreparedShowMedia {
    ClipMap clips;
    juce::StringArray issues; // absent/unreadable clips remain nonfatal, as in 0.7
    std::int64_t pcmBytes=0;
};
inline constexpr std::int64_t maxShowPcmBytes=512LL*1024*1024;
std::int64_t uniquePcmBytes(const ClipMap&) noexcept;
bool withinPcmBudget(const ClipMap&,std::int64_t maxBytes=maxShowPcmBytes) noexcept;
juce::Result prepareShowMedia(const Show&,PreparedShowMedia&,
    const AudioClipLoader& load=loadClip,std::int64_t maxBytes=maxShowPcmBytes,
    const std::function<bool()>& cancelled={});

// No JUCE Component or AudioEngine crosses this boundary. Results are polled
// by the UI and committed only after full validation, media decoding and PCM
// budgeting succeed. New requests supersede older ones.
struct PreparedProject {
    juce::File source;
    Show show;
    PreparedShowMedia media;
    juce::Result status=juce::Result::ok();
};
class ProjectLoadWorker final {
public:
    explicit ProjectLoadWorker(AudioClipLoader load=loadClip);
    ~ProjectLoadWorker();
    void request(const juce::File& source);
    void cancel();
    bool takeCompleted(PreparedProject& destination);
private:
    void run();
    AudioClipLoader decoder;
    std::mutex mutex;
    std::condition_variable wake;
    std::optional<juce::File> queued;
    std::optional<PreparedProject> ready;
    std::thread worker;
    std::atomic<std::uint64_t> generation{0};
    bool closing=false;
};
}
