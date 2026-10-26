#include "MainComponent.h"
#include "OscEditor.h"

namespace cuelist {
namespace {
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
        if (oscEditorCellId!=id) { oscEditorCellId=id; oscEditor->loadClip(*cell.osc); }
        clipCollapsed=false; clipCollapseButton.setButtonText("Riduci");
        if (oscWindow) oscWindow->toFront(true); else resized();
        return;
    }
    oscEditorCellId=id;
    auto safe=juce::Component::SafePointer<MainComponent>(this);
    oscEditor=std::make_unique<OscClipEditor>(*cell.osc,show.rows[selectedRow].tempo,[safe,id](const OscClip& updated) {
        if (!safe || validateOscClip(updated).failed()) return;
        for (auto& row:safe->show.rows)
            for (size_t c=0;c<row.cells.size();++c) if (row.cells[c].id==id && safe->show.columns[c].type==ColumnType::osc) {
                row.cells[c].osc=updated; row.cells[c].file.clear();
                safe->dirty=true; safe->engine.publish(makeRenderState(safe->show,safe->clips));
                safe->status="Messaggi OSC aggiornati: salva lo show.";
                safe->grid.repaint(); safe->timeline.repaint(); safe->repaint();
                return;
            }
    });
    oscEditor->onDetach=[safe] {
        if (!safe) return;
        if (safe->oscWindow) safe->dockOscClip(); else safe->detachOscClip();
    };
    oscEditor->setFloating(false);
    addAndMakeVisible(*oscEditor);
    clipCollapsed=false; clipCollapseButton.setButtonText("Riduci");
    resized(); repaint();
}
void MainComponent::syncOscEditor() {
    if (!oscEditor) return;
    if (show.columns[selectedColumn].type!=ColumnType::osc) { closeOscClip(); return; }
    const auto& cell=show.rows[selectedRow].cells[selectedColumn];
    if (oscEditorCellId==cell.id) return;
    oscEditorCellId=cell.id;
    oscEditor->loadClip(cell.osc.value_or(OscClip{}));
}
}
