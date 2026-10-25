#include "MainComponent.h"
#include "PianoRoll.h"

namespace cuelist {
bool MainComponent::hasClip(int row,int column) const {
    if (row<0 || column<0 || row>=(int)show.rows.size() || column>=(int)show.columns.size()) return false;
    const auto& cell=show.rows[(size_t)row].cells[(size_t)column];
    return show.columns[(size_t)column].type==ColumnType::midi ? cell.midi.has_value() : clips.count(cell.id)>0;
}
double MainComponent::clipDuration(int row,int column) const {
    if (!hasClip(row,column)) return 0;
    const auto& cue=show.rows[(size_t)row]; const auto& cell=cue.cells[(size_t)column];
    if (cell.midi) return secondsAtBeat(cue.tempo,cell.midi->lengthBeats);
    const auto clip=clips.find(cell.id);
    return clip!=clips.end() && clip->second->sampleRate>0 ? clip->second->samples.getNumSamples()/clip->second->sampleRate : 0;
}
void MainComponent::editMidiClip() {
    if (show.columns[selectedColumn].type!=ColumnType::midi || !applyInspector()) return;
    if (midiDialog) { midiDialog->toFront(true); return; }
    stopAll();
    const auto& cell=show.rows[selectedRow].cells[selectedColumn]; const int id=cell.id;
    auto* editor=new PianoRollEditor(cell.midi.value_or(MidiClip{}),[safe=juce::Component::SafePointer<MainComponent>(this),id](const MidiClip& clip) {
        if (!safe) return;
        for (auto& row:safe->show.rows) for (size_t c=0;c<row.cells.size();++c) if (row.cells[c].id==id && safe->show.columns[c].type==ColumnType::midi) {
            row.cells[c].midi=clip; row.cells[c].file.clear();
            safe->changed("Clip MIDI aggiornata. Scegli l'uscita MIDI nelle utility.",true); return;
        }
    });
    juce::DialogWindow::LaunchOptions options; options.content.setOwned(editor);
    options.dialogTitle="Piano roll MIDI"; options.dialogBackgroundColour=palette::panel;
    options.useNativeTitleBar=true; options.escapeKeyTriggersCloseButton=true; options.resizable=true;
    options.componentToCentreAround=this; midiDialog=options.launchAsync();
    midiDialog->setResizeLimits(850,420,1800,1200);
}
void MainComponent::setMidiOutput(int column,const juce::String& identifier) {
    if (column<0 || column>=(int)show.columns.size() || show.columns[column].type!=ColumnType::midi) return;
    if (identifier.isNotEmpty()) {
        engine.panic();
        auto ready=engine.setMidiOutput(identifier,true);
        if (ready.failed()) { message(ready.getErrorMessage()); utilities.refresh(); return; }
    }
    show.columns[column].midiOutputId=identifier;
    changed(identifier.isEmpty() ? "Uscita MIDI disattivata." : "Uscita MIDI della colonna aggiornata.",true);
}
}
