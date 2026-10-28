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
    const AudioClipLoader& load,std::int64_t maxBytes,
    const std::function<bool()>& cancelled) {
    if (const auto valid=validateShow(show);valid.failed()) return valid;
    if(!load || maxBytes<0) return juce::Result::fail("Invalid media loader or PCM limit.");
    PreparedShowMedia prepared;
    std::map<juce::String,std::shared_ptr<const AudioClip>> cache;
    const auto prepareCell=[&](const Cell& cell)->juce::Result {
        if(cancelled && cancelled()) return juce::Result::fail("Project preparation cancelled.");
        if(cell.file.isEmpty()) return juce::Result::ok();
        if(!juce::File::isAbsolutePath(cell.file))
            return juce::Result::fail("Resolve relative audio paths before preparing a show.");
        const auto found=cache.find(cell.file);
        if(found!=cache.end()) {
            prepared.clips[cell.id]=found->second;
            return juce::Result::ok();
        }
        std::shared_ptr<const AudioClip> clip;
        const auto result=load(juce::File(cell.file),clip);
        if(cancelled && cancelled()) return juce::Result::fail("Project preparation cancelled.");
        if(result.failed() || !clip) {
            prepared.issues.add(result.failed()?result.getErrorMessage():
                                "Decoder produced no audio: "+cell.file);
            return juce::Result::ok();
        }
        prepared.clips[cell.id]=clip;
        cache.emplace(cell.file,clip);
        prepared.pcmBytes=uniquePcmBytes(prepared.clips);
        if(prepared.pcmBytes>maxBytes)
            return juce::Result::fail("Show exceeds the configured PCM memory budget.");
        return juce::Result::ok();
    };
    for (const auto& row:show.rows) for (const auto& cell:row.cells)
        if (auto result=prepareCell(cell);result.failed()) return result;
    for (size_t i=0;i<show.cuelists.size();++i)
        if ((int)i!=show.activeCuelist)
            for (const auto& row:show.cuelists[i].rows)
                for (const auto& cell:row.cells)
                    if (auto result=prepareCell(cell);result.failed()) return result;
    // Only publish a fully prepared result. On failure, the caller's existing
    // prepared state and the running show remain unchanged.
    if(cancelled && cancelled()) return juce::Result::fail("Project preparation cancelled.");
    destination=std::move(prepared);
    return juce::Result::ok();
}

ProjectLoadWorker::ProjectLoadWorker(AudioClipLoader load)
    :decoder(std::move(load)) {
    // Do not let the worker inspect members until construction has completed.
    worker=std::thread([this] {run();});
}
ProjectLoadWorker::~ProjectLoadWorker() {
    {
        std::lock_guard<std::mutex> guard(mutex);
        closing=true;
        generation.fetch_add(1,std::memory_order_relaxed);
        queued.reset();
        ready.reset();
    }
    wake.notify_one();
    if(worker.joinable()) worker.join();
}
void ProjectLoadWorker::request(const juce::File& source) {
    {
        std::lock_guard<std::mutex> guard(mutex);
        if(closing) return;
        generation.fetch_add(1,std::memory_order_relaxed);
        queued=source;
        ready.reset();
    }
    wake.notify_one();
}
void ProjectLoadWorker::cancel() {
    std::lock_guard<std::mutex> guard(mutex);
    generation.fetch_add(1,std::memory_order_relaxed);
    queued.reset();
    ready.reset();
}
bool ProjectLoadWorker::takeCompleted(PreparedProject& destination) {
    std::lock_guard<std::mutex> guard(mutex);
    if(!ready.has_value()) return false;
    destination=std::move(*ready);
    ready.reset();
    return true;
}
void ProjectLoadWorker::run() {
    for(;;) {
        juce::File file;
        std::uint64_t ticket=0;
        {
            std::unique_lock<std::mutex> lock(mutex);
            wake.wait(lock,[this] {return closing || queued.has_value();});
            if(closing) return;
            file=std::move(*queued);
            queued.reset();
            ticket=generation.load(std::memory_order_relaxed);
        }
        auto interrupted=[this,ticket] {
            return generation.load(std::memory_order_relaxed)!=ticket;
        };
        PreparedProject candidate;
        candidate.source=file;
        candidate.status=loadShow(file,candidate.show);
        if(candidate.status.wasOk() && !interrupted())
            candidate.status=prepareShowMedia(candidate.show,candidate.media,decoder,
                                               maxShowPcmBytes,interrupted);
        {
            std::lock_guard<std::mutex> guard(mutex);
            if(!closing && generation.load(std::memory_order_relaxed)==ticket)
                ready=std::move(candidate);
        }
    }
}
}
