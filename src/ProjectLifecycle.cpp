#include "MainComponent.h"
#include "ProjectSafety.h"
#include "ProjectMediaLoader.h"

namespace cuelist {
// Project I/O, safe activation, recovery and operator preflight remain
// UI-thread-owned; background media preparation stays in ProjectLoadWorker.

void MainComponent::withDiscardConfirmation(std::function<void()> action) {
    if (!hasUnsavedChanges()) { action(); return; }
    juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon)
        .withTitle("Unsaved changes").withMessage("Continuing will discard unsaved changes. To save them, cancel and click Save.")
        .withButton("Discard and continue").withButton("Cancel").withAssociatedComponent(this),
        [safe=juce::Component::SafePointer<MainComponent>(this),action](int result) { if (safe && result==1) action(); });
}

void MainComponent::requestQuit() { withDiscardConfirmation([this] { stopAll(); juce::JUCEApplication::getInstance()->quit(); }); }

void MainComponent::saveProject() {
    if (projectLoader) projectLoader->cancel();
    if (!applyInspector()) return;
    chooser=std::make_unique<juce::FileChooser>("Save show",projectFile==juce::File{} ? juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("Show.cuelist") : projectFile,"*.cuelist");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,
        [safe=juce::Component::SafePointer<MainComponent>(this)](const juce::FileChooser& fc) {
            if (!safe || fc.getResult()==juce::File{}) return;
            auto file=fc.getResult(); auto result=saveShow(safe->show,file);
            if (result.failed()) { safe->message(result.getErrorMessage()); return; }
            Show portable; if (loadShow(file,portable).wasOk()) safe->show=std::move(portable);
            safe->projectFile=file; safe->dirty=false; safe->status="Saved: "+file.getFullPathName();
            if (safe->recoveryEnabled) {
                if (safe->recoveryWriter) safe->recoveryWriter->flush();
                ProjectRecovery::discard(ProjectRecovery::defaultDirectory());
                safe->preserveDeferredRecovery=false;
            }
            safe->repaint();
        });
}

void MainComponent::showPreflight() {
    juce::StringArray outputs;
    for(const auto& d:juce::MidiOutput::getAvailableDevices()) outputs.add(d.identifier);
    const auto report=preflightShow(show,engine.connected.load(),outputs,midiInput.connected());
    const auto icon=report.criticalCount()>0 ? juce::MessageBoxIconType::WarningIcon :
        report.warningCount()>0 ? juce::MessageBoxIconType::InfoIcon :
        juce::MessageBoxIconType::NoIcon;
    juce::AlertWindow::showMessageBoxAsync(icon,"Show preflight",report.describe(),"Close",this);
    status=report.criticalCount()>0 ? "Preflight: critical problems detected." :
           report.warningCount()>0 ? "Preflight: warnings to review." :
           "Preflight complete: no critical project errors.";
    repaint();
}

void MainComponent::showDiagnostics() {
    const auto stats=engine.diagnostics();
    juce::String details="ENGINE DIAGNOSTICS\n";
    details+="Audio device: "+juce::String(engine.connected.load()?"ONLINE":"OFFLINE");
    details+="\nActive audio/MIDI/OSC cells: "+juce::String(engine.activeVoices.load());
    details+="\nAudio callback commands dropped: "+juce::String(stats.audioCommandsDropped);
    details+="\nMIDI input: "+juce::String(midiInput.connected()?"CONNECTED":"DISCONNECTED");
    details+="\nMIDI input messages dropped: "+juce::String(midiInput.droppedCount());
    details+="\nMIDI input reconnect attempts: "+juce::String(midiInputReconnectAttempts);
    details+="\nMIDI output messages sent: "+juce::String(stats.midiOutputSent);
    details+="\nMIDI output messages dropped: "+juce::String(stats.midiOutputDropped);
    details+="\nMIDI dispatches >10 ms late: "+juce::String(stats.midiLateOver10ms);
    details+="\nOSC UDP packets sent: "+juce::String(stats.oscSent);
    details+="\nOSC UDP packets dropped / send failures: "+juce::String(stats.oscDropped);
    details+="\nOSC dispatches >10 ms late: "+juce::String(stats.oscLateOver10ms);
    details+="\nRecovery I/O failures: "+juce::String(
        recoveryWriter ? recoveryWriter->failedWrites() : 0u);
    details+="\n\nThese counters are local to the current process. UDP sent does not prove receiver delivery.";
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
        "Diagnostics",details,"Close",this);
}

bool MainComponent::activateProject(Show candidate,const juce::File& source,bool recovering) {
    if (projectLoader) projectLoader->cancel();
    PreparedShowMedia prepared;
    const auto preparedResult=prepareShowMedia(candidate,prepared);
    if(preparedResult.failed()) {
        message(preparedResult.getErrorMessage()+
                " The current show has not been replaced.");
        return false;
    }
    return commitPreparedProject(std::move(candidate),source,recovering,std::move(prepared));
}

bool MainComponent::commitPreparedProject(Show candidate,const juce::File& source,
                                           bool recovering,PreparedShowMedia prepared) {
    // Only the UI thread may interrupt playback or touch the AudioEngine.
    // Decode and memory-budget checks have already completed transactionally.
    auto loaded=std::move(prepared.clips);
    auto issues=std::move(prepared.issues);
    // Only after all memory checks pass do we stop the previous show.
    engine.panic();
    for(const auto& column:candidate.columns)
        if(column.type==ColumnType::midi && column.midiOutputId.isNotEmpty()) {
            const auto opened=engine.setMidiOutput(column.midiOutputId);
            if(opened.failed()) issues.add(opened.getErrorMessage());
        }
    closeMidiClip();closeOscClip();editHistory.clearUndoHistory();
    show=std::move(candidate);clips=std::move(loaded);
    projectFile=recovering ? juce::File{} : source;dirty=recovering;
    selectedRow=selectedColumn=nextCue=0;
    resetActiveMapping();refreshControlSettings();
    engine.publish(makeRenderState(show,clips));refresh();
    status=recovering ? "Recovered unsaved show. Use Save to choose a project filename." :
                        "Project opened: "+source.getFileName();
    if(!recovering && recoveryEnabled) {
        if (recoveryWriter) recoveryWriter->flush();
        ProjectRecovery::discard(ProjectRecovery::defaultDirectory());
        preserveDeferredRecovery=false;
    }
    if(!issues.isEmpty())
        message(juce::String(issues.size())+" missing media or MIDI output issues.\n\n"+
                issues.joinIntoString("\n").substring(0,1500));
    return true;
}

void MainComponent::offerCrashRecovery() {
    if(!ProjectRecovery::exists(ProjectRecovery::defaultDirectory())) return;
    auto safe=juce::Component::SafePointer<MainComponent>(this);
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon)
            .withTitle("Recover an unsaved show?")
            .withMessage("A recovery checkpoint from an interrupted session was found. "
                         "Restoring does not overwrite the original project or any media. "
                         "Playback will not start automatically.")
            .withButton("Restore").withButton("Discard checkpoint").withButton("Later")
            .withAssociatedComponent(this),[safe](int answer) {
        if(!safe) return;
        if(answer==1) {
            Show candidate;
            const auto result=ProjectRecovery::restore(ProjectRecovery::defaultDirectory(),candidate);
            if(result.failed()) safe->message(result.getErrorMessage());
            else safe->activateProject(std::move(candidate),{},true);
        } else if(answer==2) {
            if (safe->recoveryWriter) safe->recoveryWriter->flush();
            ProjectRecovery::discard(ProjectRecovery::defaultDirectory());
        }
        else {
            safe->preserveDeferredRecovery=true;
            safe->status="Recovery deferred: previous checkpoint preserved until you decide.";
            safe->repaint();
        }
    });
}

void MainComponent::openProject() {
    chooser=std::make_unique<juce::FileChooser>("Open show",projectFile,"*.cuelist;*.json;*.bak");
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
        [safe=juce::Component::SafePointer<MainComponent>(this)](const juce::FileChooser& fc) {
            if(!safe || fc.getResult()==juce::File{}) return;
            const auto file=fc.getResult();
            if(!safe->projectLoader)
                safe->projectLoader=std::make_unique<ProjectLoadWorker>();
            safe->projectLoader->request(file);
            safe->status="Preparing project in background: "+file.getFileName()+
                ". Current show stays loaded until preparation succeeds.";
            safe->repaint();
        });
}

} // namespace cuelist
