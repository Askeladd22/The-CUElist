#pragma once
#include "ShowModel.h"
#include <juce_core/juce_core.h>
#include <vector>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace cuelist {
enum class PreflightLevel { info, warning, critical };
struct PreflightIssue {
    PreflightLevel level=PreflightLevel::info;
    juce::String category, detail;
};
struct PreflightReport {
    std::vector<PreflightIssue> issues;
    int audioFiles=0,missingFiles=0,midiClips=0,oscClips=0,controlSets=0;
    int criticalCount() const noexcept;
    int warningCount() const noexcept;
    juce::String describe() const;
};
// Pure project inspection except for metadata-only file existence checks.
// OSC over UDP is deliberately reported as unverified, never "connected".
PreflightReport preflightShow(const Show&,bool audioAvailable,
    const juce::StringArray& availableMidiOutputs,
    bool midiInputConnected);

// Crash-recovery checkpoint: stores an *unsaved* validated show snapshot using
// absolute media references (already resolved by loadShow). It never copies
// or changes user media, the .cuelist file, or its .bak generation.
class ProjectRecovery final {
public:
    static juce::File defaultDirectory();
    static juce::File snapshotFile(const juce::File& directory);
    static juce::Result checkpoint(const Show&,const juce::File& directory);
    static juce::Result restore(const juce::File& directory,Show&);
    static bool exists(const juce::File& directory);
    static bool discard(const juce::File& directory);
};
 // Background disk writer. Show validation/serialization remains on the caller
 // thread; disk creation/replacement is isolated from the UI and audio callback.
 // A single pending slot bounds memory usage and coalesces repeated snapshots.
class RecoveryCheckpointWriter final {
public:
    explicit RecoveryCheckpointWriter(juce::File directory);
    ~RecoveryCheckpointWriter();
    juce::Result enqueue(const Show& show);
    void flush(); // complete outstanding writes before save/discard/exit
    unsigned failedWrites() const noexcept {return failures.load();}
private:
    void run();
    juce::File dir;
    std::mutex mutex;
    std::condition_variable wake;
    std::thread worker;
    juce::String pending;
    bool hasPending=false,writing=false,stopping=false;
    std::atomic<unsigned> failures{0};
};

}
