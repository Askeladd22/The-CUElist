#include "MainComponent.h"
#include "PianoRoll.h"

namespace cuelist {
namespace {
bool sameMidiClip(const MidiClip& a,const MidiClip& b) {
    if (a.lengthBeats!=b.lengthBeats || a.notes.size()!=b.notes.size()) return false;
    for (size_t i=0;i<a.notes.size();++i) {
        const auto& x=a.notes[i]; const auto& y=b.notes[i];
        if (x.beat!=y.beat || x.length!=y.length || x.pitch!=y.pitch
            || x.velocity!=y.velocity || x.channel!=y.channel) return false;
    }
    return true;
}

class FloatingPianoRoll final : public juce::DocumentWindow {
public:
    explicit FloatingPianoRoll(std::function<void()> onClose)
        : juce::DocumentWindow("The Cuelist - Piano Roll",palette::panel,
                               juce::DocumentWindow::closeButton,true),
          close(std::move(onClose)) {}
    void closeButtonPressed() override { if (close) close(); }
private:
    std::function<void()> close;
};
}

// This history action records the logical gesture, not its intermediate drag frames.
// The model already contains the new state when perform() is called the first time.
class MidiUndoAction final : public juce::UndoableAction {
public:
    MidiUndoAction(MainComponent& p,int cell,MidiClip oldValue,MidiClip newValue)
        : owner(&p),id(cell),before(std::move(oldValue)),after(std::move(newValue)) {}
    bool perform() override {
        if (first) { first=false; return true; }
        if (!owner) return false;
        owner->restoreMidiClip(id,after); return true;
    }
    bool undo() override {
        if (!owner) return false;
        owner->restoreMidiClip(id,before); return true;
    }
    int getSizeInUnits() override { return 1+(int)(before.notes.size()+after.notes.size())/64; }
private:
    juce::Component::SafePointer<MainComponent> owner;
    int id;
    MidiClip before,after;
    bool first=true;
};

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
    auto& cell=show.rows[selectedRow].cells[selectedColumn];
    if (!cell.midi) {
        cell.midi=MidiClip{};
        dirty=true; engine.publish(makeRenderState(show,clips));
    }
    const int id=cell.id;
    if (clipEditor) {
        if (clipEditorCellId!=id) {
            clipEditorCellId=id; clipEditor->loadClip(*cell.midi);
        }
        clipCollapsed=false;
        clipCollapseButton.setButtonText("Riduci");
        if (clipWindow) clipWindow->toFront(true);
        else { resized(); repaint(); }
        return;
    }
    clipEditorCellId=id;
    auto safe=juce::Component::SafePointer<MainComponent>(this);
    clipEditor=std::make_unique<PianoRollEditor>(*cell.midi,
        [safe,id](const MidiClip& before,const MidiClip& after,bool complete) {
            if (safe) safe->midiClipEdited(id,before,after,complete);
        });
    clipEditor->onDetach=[safe] {
        if (!safe) return;
        if (safe->isClipEditorFloating()) safe->dockMidiClip();
        else safe->detachMidiClip();
    };
    clipEditor->onUndo=[safe] { if (safe) safe->undoEdit(); };
    clipEditor->onRedo=[safe] { if (safe) safe->redoEdit(); };
    clipEditor->setFloating(false);
    addAndMakeVisible(*clipEditor);
    clipCollapsed=false; clipCollapseButton.setButtonText("Riduci");
    resized(); repaint();
}
void MainComponent::syncClipEditor() {
    if (!clipEditor) return;
    if (show.columns[selectedColumn].type!=ColumnType::midi) { closeMidiClip(); return; }
    const auto& cell=show.rows[selectedRow].cells[selectedColumn];
    if (clipEditorCellId==cell.id) return;
    clipEditorCellId=cell.id;
    clipEditor->loadClip(cell.midi.value_or(MidiClip{}));
}
void MainComponent::detachMidiClip() {
    if (!clipEditor || clipWindow) return;
    if (clipEditor->getParentComponent()==this) removeChildComponent(clipEditor.get());
    auto safe=juce::Component::SafePointer<MainComponent>(this);
    auto* window=new FloatingPianoRoll([safe] {
        juce::MessageManager::callAsync([safe] { if (safe) safe->dockMidiClip(); });
    });
    clipWindow.reset(window);
    window->setUsingNativeTitleBar(true);
    window->setResizable(true,true);
    window->setResizeLimits(760,320,1800,1200);
    window->setContentNonOwned(clipEditor.get(),true);
    window->centreWithSize(1000,620);
    clipEditor->setFloating(true);
    window->setVisible(true); window->toFront(true);
    resized(); repaint();
}
void MainComponent::dockMidiClip() {
    if (!clipEditor || !clipWindow) return;
    clipWindow->clearContentComponent();
    clipWindow->setVisible(false);
    clipWindow.reset();
    addAndMakeVisible(*clipEditor);
    clipEditor->setFloating(false);
    clipCollapsed=false; clipCollapseButton.setButtonText("Riduci");
    resized(); repaint();
}
void MainComponent::closeMidiClip() {
    if (clipWindow) {
        clipWindow->clearContentComponent();
        clipWindow->setVisible(false);
        clipWindow.reset();
    }
    if (clipEditor) {
        if (clipEditor->getParentComponent()==this) removeChildComponent(clipEditor.get());
        clipEditor.reset();
    }
    clipEditorCellId=-1;
    clipCollapsed=false;
    resized(); repaint();
}
void MainComponent::midiClipEdited(int cellId,const MidiClip& before,const MidiClip& after,bool complete) {
    if (sameMidiClip(before,after) || validateMidiClip(after).failed()) return;
    for (auto& row:show.rows)
        for (size_t c=0;c<row.cells.size();++c) {
            auto& cell=row.cells[c];
            if (cell.id!=cellId || show.columns[c].type!=ColumnType::midi) continue;
            if (!cell.midi || !sameMidiClip(*cell.midi,after)) {
                cell.midi=after; cell.file.clear(); dirty=true;
                engine.publish(makeRenderState(show,clips));
                status="Clip MIDI aggiornata. Salva lo show per conservarla.";
                timeline.repaint(); grid.repaint(); repaint();
            }
            if (complete) editHistory.perform(new MidiUndoAction(*this,cellId,before,after),"Modifica clip MIDI");
            return;
        }
}
void MainComponent::restoreMidiClip(int cellId,const MidiClip& snapshot) {
    for (auto& row:show.rows) for (size_t c=0;c<row.cells.size();++c) {
        if (row.cells[c].id!=cellId || show.columns[c].type!=ColumnType::midi) continue;
        row.cells[c].midi=snapshot;
        dirty=true; engine.publish(makeRenderState(show,clips));
        if (clipEditor && clipEditorCellId==cellId) clipEditor->loadClip(snapshot);
        status="Modifica MIDI ripristinata."; grid.repaint(); timeline.repaint(); repaint();
        return;
    }
}
bool MainComponent::undoEdit() {
    if (!editHistory.canUndo()) return false;
    return editHistory.undo();
}
bool MainComponent::redoEdit() {
    if (!editHistory.canRedo()) return false;
    return editHistory.redo();
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
