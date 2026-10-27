#include "ProjectSafety.h"
#include <set>

namespace cuelist {
int PreflightReport::criticalCount() const noexcept {
    int n=0; for(const auto& i:issues) if(i.level==PreflightLevel::critical) ++n;
    return n;
}
int PreflightReport::warningCount() const noexcept {
    int n=0; for(const auto& i:issues) if(i.level==PreflightLevel::warning) ++n;
    return n;
}
juce::String PreflightReport::describe() const {
    juce::String result="SHOW PREFLIGHT\n";
    result+="Audio media: "+juce::String(audioFiles)+" | Missing: "+juce::String(missingFiles)+
        " | MIDI clips: "+juce::String(midiClips)+" | OSC clips: "+juce::String(oscClips)+
        " | Control Sets: "+juce::String(controlSets)+"\n";
    result+="Critical: "+juce::String(criticalCount())+" | Warnings: "+juce::String(warningCount())+"\n\n";
    for(const auto& issue:issues) {
        const char* tag=issue.level==PreflightLevel::critical ? "[CRITICAL] " :
                        issue.level==PreflightLevel::warning ? "[WARNING] " : "[INFO] ";
        result+=juce::String(tag)+issue.category+": "+issue.detail+"\n";
    }
    if(issues.empty()) result+="No problems detected in the inspected project.\n";
    result+="\nThis check does not guarantee delivery to OSC/UDP receivers or live hardware reliability.";
    return result;
}
PreflightReport preflightShow(const Show& show,bool audioAvailable,
    const juce::StringArray& availableMidiOutputs,bool midiInputConnected) {
    PreflightReport result;
    if(const auto check=validateShow(show);check.failed())
        result.issues.push_back({PreflightLevel::critical,"Project",check.getErrorMessage()});
    if(!audioAvailable)
        result.issues.push_back({PreflightLevel::warning,"Audio","No running audio device was detected."});
    std::set<juce::String> checked;
    for(const auto& col:show.columns) {
        if(col.type==ColumnType::midi && col.midiOutputId.isNotEmpty() &&
           !availableMidiOutputs.contains(col.midiOutputId))
            result.issues.push_back({PreflightLevel::warning,"MIDI output",
                col.name+": configured device is disconnected."});
        if(col.type==ColumnType::osc && checked.insert(col.oscHost+":"+juce::String(col.oscPort)).second)
            result.issues.push_back({PreflightLevel::info,"OSC/UDP",
                col.oscHost+":"+juce::String(col.oscPort)+
                " configured; receiver availability cannot be confirmed without a response."});
        if(col.type==ColumnType::control && col.controlLocked)
            ++result.controlSets;
    }
    if(show.controls.midiInputId.isNotEmpty()&&!midiInputConnected)
        result.issues.push_back({PreflightLevel::warning,"MIDI input",
            "The controller selected in the project is not connected."});
    for(const auto& row:show.rows) for(size_t i=0;i<row.cells.size();++i) {
        const auto& cell=row.cells[i];
        if(cell.midi) ++result.midiClips;
        if(cell.osc) ++result.oscClips;
        if(cell.controlSet) ++result.controlSets;
        if(cell.file.isEmpty()) continue;
        ++result.audioFiles;
        const juce::File file(cell.file);
        if(!file.existsAsFile()) {
            ++result.missingFiles;
            result.issues.push_back({PreflightLevel::critical,"Missing media",
                row.name+": "+file.getFullPathName()});
        } else if(file.getSize()==0) {
            result.issues.push_back({PreflightLevel::critical,"Empty media",
                row.name+": "+file.getFileName()});
        }
    }
    return result;
}
juce::File ProjectRecovery::defaultDirectory() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("The Cuelist").getChildFile("Recovery");
}
juce::File ProjectRecovery::snapshotFile(const juce::File& dir) {
    return dir.getChildFile("last-unsaved-session.cuelist");
}
bool ProjectRecovery::exists(const juce::File& dir) { return snapshotFile(dir).existsAsFile(); }
bool ProjectRecovery::discard(const juce::File& dir) {
    const auto file=snapshotFile(dir);
    return !file.exists() || file.deleteFile();
}
namespace {
juce::Result writeRecoverySnapshot(const juce::String& text,const juce::File& dir) {
    if(dir.createDirectory().failed())
        return juce::Result::fail("Cannot create recovery directory.");
    const auto file=ProjectRecovery::snapshotFile(dir);
    juce::TemporaryFile staged(file);
    if(!staged.getFile().replaceWithText(text) ||
       !staged.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Recovery checkpoint could not be written.");
    return juce::Result::ok();
}
juce::Result validateRecoverySource(const Show& show) {
    if(const auto valid=validateShow(show);valid.failed()) return valid;
    for(const auto& row:show.rows) for(const auto& cell:row.cells)
        if(cell.file.isNotEmpty() && !juce::File::isAbsolutePath(cell.file))
            return juce::Result::fail("Recovery requires resolved absolute media paths.");
    return juce::Result::ok();
}
}
juce::Result ProjectRecovery::checkpoint(const Show& show,const juce::File& dir) {
    if(const auto check=validateRecoverySource(show);check.failed()) return check;
    return writeRecoverySnapshot(encodeShow(show),dir);
}
RecoveryCheckpointWriter::RecoveryCheckpointWriter(juce::File directory)
    :dir(std::move(directory)),worker([this] {run();}) {}
RecoveryCheckpointWriter::~RecoveryCheckpointWriter() {
    flush();
    {
        std::lock_guard<std::mutex> guard(mutex);
        stopping=true;
    }
    wake.notify_all();
    if(worker.joinable()) worker.join();
}
juce::Result RecoveryCheckpointWriter::enqueue(const Show& show) {
    if(const auto check=validateRecoverySource(show);check.failed()) return check;
    // Serialization uses an immutable snapshot on the GUI thread, avoiding
    // races with edits, while filesystem access is always on the worker.
    auto text=encodeShow(show);
    {
        std::lock_guard<std::mutex> guard(mutex);
        if(stopping) return juce::Result::fail("Recovery writer is closing.");
        pending=std::move(text);
        hasPending=true;
    }
    wake.notify_one();
    return juce::Result::ok();
}
void RecoveryCheckpointWriter::flush() {
    std::unique_lock<std::mutex> lock(mutex);
    wake.wait(lock,[this] {return !hasPending && !writing;});
}
void RecoveryCheckpointWriter::run() {
    for(;;) {
        juce::String snapshot;
        {
            std::unique_lock<std::mutex> lock(mutex);
            wake.wait(lock,[this] {return hasPending || stopping;});
            if(stopping && !hasPending) break;
            snapshot=std::move(pending);
            pending.clear();
            hasPending=false;
            writing=true;
        }
        if(writeRecoverySnapshot(snapshot,dir).failed())
            failures.fetch_add(1,std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> guard(mutex);
            writing=false;
        }
        wake.notify_all();
    }
}
juce::Result ProjectRecovery::restore(const juce::File& dir,Show& destination) {
    const auto file=snapshotFile(dir);
    if(!file.existsAsFile() || file.getSize()>32*1024*1024)
        return juce::Result::fail("Recovery checkpoint missing or too large.");
    Show recovered;
    if(auto result=decodeShow(file.loadFileAsString(),recovered);result.failed()) return result;
    for(const auto& row:recovered.rows) for(const auto& cell:row.cells)
        if(cell.file.isNotEmpty() && !juce::File::isAbsolutePath(cell.file))
            return juce::Result::fail("Recovery references an unsafe relative media path.");
    destination=std::move(recovered);
    return juce::Result::ok();
}
}
