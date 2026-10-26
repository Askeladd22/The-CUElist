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
