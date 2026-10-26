#include "MainComponent.h"
#include "OscEditor.h"

namespace cuelist {
namespace {
class OscUndoAction final : public juce::UndoableAction {
public:
    OscUndoAction(MainComponent& parent,int cell,OscClip original,OscClip replacement)
        : owner(&parent),id(cell),before(std::move(original)),after(std::move(replacement)) {}
    bool perform() override {
        if (first) { first=false; return true; }
        if (!owner) return false;
        owner->restoreOscClip(id,after); return true;
    }
    bool undo() override {
        if (!owner) return false;
        owner->restoreOscClip(id,before); return true;
    }
    int getSizeInUnits() override { return 1+(int)(before.events.size()+after.events.size()
        +before.automations.size()*4+after.automations.size()*4)/32; }
private:
    juce::Component::SafePointer<MainComponent> owner;
    int id=0;
    OscClip before,after;
    bool first=true;
};
class FloatingOscWindow final : public juce::DocumentWindow {
public:
    explicit FloatingOscWindow(std::function<void()> callback)
      : juce::DocumentWindow("The Cuelist - OSC",palette::panel,juce::DocumentWindow::closeButton,true),
        close(std::move(callback)) {}
    void closeButtonPressed() override { if (close) close(); }
private:
    std::function<void()> close;
};
}
void MainComponent::editOscClip() {
    if (show.columns[selectedColumn].type!=ColumnType::osc || !applyInspector()) return;
    closeMidiClip();
    auto& cell=show.rows[selectedRow].cells[selectedColumn];
    if (!cell.osc) {
        cell.osc=OscClip{};
        dirty=true; engine.publish(makeRenderState(show,clips));
    }
    const int id=cell.id;
    if (oscEditor) {
        oscEditor->setTempo(show.rows[selectedRow].tempo);
        if (oscEditorCellId!=id) { oscEditorCellId=id; oscEditor->loadClip(*cell.osc); }
        clipCollapsed=false; clipCollapseButton.setButtonText("Riduci");
        if (oscWindow) oscWindow->toFront(true); else resized();
        return;
    }
    oscEditorCellId=id;
    auto safe=juce::Component::SafePointer<MainComponent>(this);
    oscEditor=std::make_unique<OscClipEditor>(*cell.osc,show.rows[selectedRow].tempo,
        [safe](const OscClip& before,const OscClip& updated,bool complete) {
            if (safe) safe->oscClipEdited(safe->oscEditorCellId,before,updated,complete);
        });
    oscEditor->onUndo=[safe] { if (safe) safe->undoEdit(); };
    oscEditor->onRedo=[safe] { if (safe) safe->redoEdit(); };
    oscEditor->onDetach=[safe] {
        if (!safe) return;
        if (safe->oscWindow) safe->dockOscClip(); else safe->detachOscClip();
    };
    oscEditor->setFloating(false);
    addAndMakeVisible(*oscEditor);
    clipCollapsed=false; clipCollapseButton.setButtonText("Riduci");
    resized(); repaint();
}
void MainComponent::oscClipEdited(int cellId,const OscClip& before,const OscClip& updated,bool complete) {
    if (validateOscClip(updated).failed()) return;
    for (auto& row:show.rows) for (size_t c=0;c<row.cells.size();++c) {
        auto& cell=row.cells[c];
        if (cell.id!=cellId || show.columns[c].type!=ColumnType::osc) continue;
        cell.osc=updated; cell.file.clear();
        dirty=true; engine.publish(makeRenderState(show,clips));
        status="OSC aggiornato. Salva lo show per conservarlo.";
        grid.repaint(); timeline.repaint(); repaint();
        if (complete) {
            editHistory.beginNewTransaction("Modifica clip OSC");
            editHistory.perform(new OscUndoAction(*this,cellId,before,updated));
        }
        return;
    }
}
void MainComponent::restoreOscClip(int cellId,const OscClip& snapshot) {
    for (auto& row:show.rows) for (size_t c=0;c<row.cells.size();++c) {
        if (row.cells[c].id!=cellId || show.columns[c].type!=ColumnType::osc) continue;
        row.cells[c].osc=snapshot; dirty=true;
        engine.publish(makeRenderState(show,clips));
        if (oscEditor && oscEditorCellId==cellId) oscEditor->loadClip(snapshot);
        status="Modifica OSC ripristinata.";
        grid.repaint(); timeline.repaint(); repaint();
        return;
    }
}
void MainComponent::syncOscEditor() {
    if (!oscEditor) return;
    if (show.columns[selectedColumn].type!=ColumnType::osc) { closeOscClip(); return; }
    const auto& cell=show.rows[selectedRow].cells[selectedColumn];
    oscEditor->setTempo(show.rows[selectedRow].tempo);
    if (oscEditorCellId==cell.id) return;
    oscEditorCellId=cell.id;
    oscEditor->loadClip(cell.osc.value_or(OscClip{}));
}
void MainComponent::detachOscClip() {
    if (!oscEditor || oscWindow) return;
    if (oscEditor->getParentComponent()==this) removeChildComponent(oscEditor.get());
    auto safe=juce::Component::SafePointer<MainComponent>(this);
    oscWindow=std::make_unique<FloatingOscWindow>([safe] {
        juce::MessageManager::callAsync([safe] { if (safe) safe->dockOscClip(); });
    });
    oscWindow->setUsingNativeTitleBar(true);
    oscWindow->setResizable(true,true);
    oscWindow->setResizeLimits(760,320,1800,1200);
    oscWindow->setContentNonOwned(oscEditor.get(),true);
    oscWindow->centreWithSize(1000,500);
    oscEditor->setFloating(true);
    oscWindow->setVisible(true); oscWindow->toFront(true);
    resized(); repaint();
}
void MainComponent::dockOscClip() {
    if (!oscEditor || !oscWindow) return;
    oscWindow->clearContentComponent();
    oscWindow->setVisible(false); oscWindow.reset();
    addAndMakeVisible(*oscEditor);
    oscEditor->setFloating(false);
    clipCollapsed=false; clipCollapseButton.setButtonText("Riduci");
    resized(); repaint();
}
void MainComponent::closeOscClip() {
    if (oscWindow) {
        oscWindow->clearContentComponent();
        oscWindow->setVisible(false); oscWindow.reset();
    }
    if (oscEditor) {
        if (oscEditor->getParentComponent()==this) removeChildComponent(oscEditor.get());
        oscEditor.reset();
    }
    oscEditorCellId=-1;
    clipCollapsed=false; resized(); repaint();
}
void MainComponent::setOscDestination(int column,const juce::String& host,int port) {
    if (column<0 || column>=(int)show.columns.size() || show.columns[(size_t)column].type!=ColumnType::osc) return;
    auto candidate=show;
    candidate.columns[(size_t)column].oscHost=host.trim();
    candidate.columns[(size_t)column].oscPort=port;
    if (const auto valid=validateShow(candidate); valid.failed()) { message(valid.getErrorMessage()); return; }
    show=std::move(candidate);
    changed("Destinazione OSC UDP aggiornata: "+host+":"+juce::String(port));
}
}
