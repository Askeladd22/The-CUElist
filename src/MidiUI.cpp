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
    const int cellId=show.rows[selectedRow].cells[selectedColumn].id;
    if (clipEditor && clipEditorCellId==cellId) {
        if (midiDialog) { midiDialog->toFront(true); return; }
        if (clipEditor->getParentComponent()!=this) addAndMakeVisible(*clipEditor);
        clipEditor->setVisible(true); layoutClipEditor(); return;
    }
    closeMidiEditor();
    clipEditorCellId=cellId;
    const auto value=show.rows[selectedRow].cells[selectedColumn].midi.value_or(MidiClip{});
    clipEditor=std::make_unique<PianoRollEditor>(value,
        [safe=juce::Component::SafePointer<MainComponent>(this),cellId](const MidiClip& clip) {
            if (!safe) return;
            for (auto& row:safe->show.rows)
                for (size_t i=0;i<row.cells.size();++i)
                    if (row.cells[i].id==cellId && safe->show.columns[i].type==ColumnType::midi) {
                        row.cells[i].midi=clip; row.cells[i].file.clear();
                        safe->changed("Clip MIDI aggiornata in tempo reale.",false);
                        return;
                    }
        });
    clipEditor->setName("integratedClipEditor");
    clipEditor->setCloseHandler([safe=juce::Component::SafePointer<MainComponent>(this)] {
        if (safe) safe->closeMidiEditor();
    });
    addAndMakeVisible(*clipEditor);
    layoutClipEditor();
}
void MainComponent::detachMidiEditor() {
    if (!clipEditor || midiDialog) return;
    removeChildComponent(clipEditor.get());
    juce::DialogWindow::LaunchOptions options;
    options.content.setNonOwned(clipEditor.get());
    options.dialogTitle="Clip Editor / Piano Roll MIDI";
    options.dialogBackgroundColour=palette::panel;
    options.useNativeTitleBar=true;
    options.escapeKeyTriggersCloseButton=true;
    options.resizable=true;
    options.componentToCentreAround=this;
    midiDialog=options.launchAsync();
    midiDialog->setResizeLimits(850,420,1800,1200);
    layoutClipEditor();
}
void MainComponent::dockMidiEditor() {
    if (!clipEditor) return;
    if (midiDialog) {
        auto* dialog=midiDialog.getComponent();
        dialog->setContentNonOwned(nullptr,false);
        dialog->exitModalState(0);
        midiDialog=nullptr;
    }
    addAndMakeVisible(*clipEditor);
    layoutClipEditor();
}
void MainComponent::closeMidiEditor() {
    if (midiDialog) {
        auto* dialog=midiDialog.getComponent();
        dialog->setContentNonOwned(nullptr,false);
        dialog->exitModalState(0);
        midiDialog=nullptr;
    }
    if (clipEditor) removeChildComponent(clipEditor.get());
    clipEditor.reset(); clipEditorCellId=-1; layoutClipEditor();
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
