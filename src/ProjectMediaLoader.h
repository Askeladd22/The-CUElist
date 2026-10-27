#pragma once
#include "AudioEngine.h"
#include <cstdint>
#include <functional>

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
    const AudioClipLoader& load=loadClip,std::int64_t maxBytes=maxShowPcmBytes);
}
