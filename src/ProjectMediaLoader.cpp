#include "ProjectMediaLoader.h"
#include <map>
#include <set>

namespace cuelist {
std::int64_t uniquePcmBytes(const ClipMap& clips) noexcept {
    std::int64_t bytes=0;
    std::set<const AudioClip*> seen;
    for(const auto& item:clips) if(item.second && seen.insert(item.second.get()).second)
        bytes+=(std::int64_t)item.second->samples.getNumSamples()*
               item.second->samples.getNumChannels()*sizeof(float);
    return bytes;
}
bool withinPcmBudget(const ClipMap& clips,std::int64_t maxBytes) noexcept {
    return maxBytes>=0 && uniquePcmBytes(clips)<=maxBytes;
}
juce::Result prepareShowMedia(const Show& show,PreparedShowMedia& destination,
    const AudioClipLoader& load,std::int64_t maxBytes) {
    if (const auto valid=validateShow(show);valid.failed()) return valid;
    if(!load || maxBytes<0) return juce::Result::fail("Invalid media loader or PCM limit.");
    PreparedShowMedia prepared;
    std::map<juce::String,std::shared_ptr<const AudioClip>> cache;
    for(const auto& row:show.rows) for(const auto& cell:row.cells) {
        if(cell.file.isEmpty()) continue;
        if(!juce::File::isAbsolutePath(cell.file))
            return juce::Result::fail("Resolve relative audio paths before preparing a show.");
        const auto found=cache.find(cell.file);
        if(found!=cache.end()) {
            prepared.clips[cell.id]=found->second;
            continue;
        }
        std::shared_ptr<const AudioClip> clip;
        const auto result=load(juce::File(cell.file),clip);
        if(result.failed() || !clip) {
            prepared.issues.add(result.failed()?result.getErrorMessage():
                                "Decoder produced no audio: "+cell.file);
            continue;
        }
        prepared.clips[cell.id]=clip;
        cache.emplace(cell.file,clip);
        prepared.pcmBytes=uniquePcmBytes(prepared.clips);
        if(prepared.pcmBytes>maxBytes)
            return juce::Result::fail("Show exceeds the configured PCM memory budget.");
    }
    // Only publish a fully prepared result. On failure, the caller's existing
    // prepared state and the running show remain unchanged.
    destination=std::move(prepared);
    return juce::Result::ok();
}
}
