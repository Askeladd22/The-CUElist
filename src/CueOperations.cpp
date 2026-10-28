#include "MainComponent.h"
#include "ShowEditOperations.h"
#include <algorithm>
#include <sstream>

namespace cuelist {
namespace {
// Keep transient operator rename/tempo dialogs next to cue editing.
void prompt(juce::Component* parent,const juce::String& title,const juce::String& hint,const juce::String& initial,
            bool multiline,std::function<void(juce::String)> result) {
    auto* alert=new juce::AlertWindow(title,hint,juce::MessageBoxIconType::NoIcon,parent);
    alert->addTextEditor("value",initial,{},false); auto* editor=alert->getTextEditor("value");
    if (multiline) { editor->setMultiLine(true); editor->setReturnKeyStartsNewLine(true); editor->setSize(440,180); }
    if (!multiline) editor->onReturnKey=[alert] { alert->exitModalState(1); };
    alert->addButton("Apply",1); alert->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
    alert->enterModalState(true,juce::ModalCallbackFunction::create([alert,result](int answer) {
        if (answer==1) result(alert->getTextEditorContents("value"));
    }),true);
    editor->grabKeyboardFocus(); editor->selectAll();
    juce::MessageManager::callAsync([safe=juce::Component::SafePointer<juce::TextEditor>(editor)] {
        if (safe && safe->isShowing()) { safe->grabKeyboardFocus(); safe->selectAll(); }
    });
}
}

void MainComponent::setRowColour(int row,int colour) {
    if (row<0 || row>=(int)show.rows.size() || colour<0 || colour>7) return;
    if (show.rows[(size_t)row].colour==colour) return;
    show.rows[(size_t)row].colour=colour;
    dirty=true;
    status="Cue colour updated.";
    refresh();
}

void MainComponent::toggleColumnCollapsed(int column) {
    if (column<0 || column>=(int)show.columns.size()) return;
    if (!applyInspector()) return;
    auto& target=show.columns[(size_t)column];
    target.collapsed=!target.collapsed; // visual state only, playback is unchanged
    if (target.collapsed && column==selectedColumn) {
        for (int i=0;i<(int)show.columns.size();++i)
            if (!show.columns[(size_t)i].collapsed) { selectedColumn=i; break; }
    }
    dirty=true;
    status=target.collapsed ? "Column collapsed (read-only preview)." :
                             "Column expanded (editing enabled).";
    refreshControlSettings();
    refresh();
}

void MainComponent::activateCuelist(int index) {
    if (index<0 || index>=(int)show.cuelists.size() ||
        index==show.activeCuelist || !applyInspector()) return;
    // A tab switch never carries live playback, a pending MIDI/OSC editor
    // or a stale mapping preset into a different cue order.
    stopAll();
    closeMidiClip();closeOscClip();
    editHistory.clearUndoHistory();
    show.cuelists[(size_t)show.activeCuelist].rows=show.rows;
    show.rows=show.cuelists[(size_t)index].rows;
    show.activeCuelist=index;
    selectedRow=selectedColumn=nextCue=0;
    copiedRow.reset();copiedRowAudio.clear();copiedCell.reset();copiedAudio.reset();
    resetActiveMapping();
    refreshControlSettings();
    engine.publish(makeRenderState(show,clips));
    dirty=true; // Active tab is persisted with the project.
    status="Cuelist selected: "+show.cuelists[(size_t)index].name+
           ". Playback stopped before switching.";
    viewport.setViewPosition(0,0);
    refresh();
}

void MainComponent::createCuelist() {
    if (show.cuelists.size()>=12) {message("A show supports up to 12 Cuelists.");return;}
    if (!applyInspector()) return;
    const int index=(int)show.cuelists.size();
    auto name="Cuelist "+juce::String(index+1);
    for (int trial=index+1;trial<50;++trial) {
        name="Cuelist "+juce::String(trial+1);
        bool duplicate=false;
        for (const auto& item:show.cuelists)
            if (item.name.equalsIgnoreCase(name)) {duplicate=true;break;}
        if (!duplicate) break;
    }
    Row first;
    first.id=show.nextId++;
    for (size_t i=0;i<show.columns.size();++i) {
        Cell cell;cell.id=show.nextId++;
        first.cells.push_back(std::move(cell));
    }
    Cuelist created;created.name=name;created.rows.push_back(std::move(first));
    show.cuelists.push_back(std::move(created));
    activateCuelist(index);
}

void MainComponent::renameActiveCuelist() {
    if (show.cuelists.empty()) return;
    const int index=show.activeCuelist;
    auto* dialog=new juce::AlertWindow("Rename Cuelist",
        "Rename only this Cuelist; the project and media paths stay unchanged.",
        juce::MessageBoxIconType::NoIcon,this);
    dialog->addTextEditor("name",show.cuelists[(size_t)index].name,"Name");
    dialog->addButton("Rename",1);
    dialog->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true,juce::ModalCallbackFunction::create(
        [safe=juce::Component::SafePointer<MainComponent>(this),dialog,index](int choice) {
            if (!safe || choice!=1 || index>=(int)safe->show.cuelists.size()) return;
            const auto name=dialog->getTextEditorContents("name").trim();
            if (name.isEmpty() || name.length()>64) {
                safe->message("Cuelist names must contain 1–64 characters.");
                return;
            }
            safe->show.cuelists[(size_t)index].name=name;
            safe->dirty=true;
            safe->status="Cuelist renamed. Save to persist.";
            safe->refresh();
        }),true);
    dialog->getTextEditor("name")->selectAll();
}

void MainComponent::setAllColumnsCollapsed(bool collapsed) {
    if (!applyInspector()) return;
    bool modified=false;
    for (auto& column:show.columns) if(column.collapsed!=collapsed) {
        column.collapsed=collapsed;
        modified=true;
    }
    if (!modified) return;
    // Layout-only state: do not stop playing cues, invalidate media, or
    // change mixer routing. Preserve selection and each column's type.
    dirty=true;
    status=collapsed ? "All columns collapsed; playback unchanged." :
                       "All columns expanded; playback unchanged.";
    refreshControlSettings();
    refresh();
}

void MainComponent::setAutoStop(int row,bool enabled) {
    if (row<0 || row>=(int)show.rows.size()) return;
    show.rows[row].stopOnNext=enabled; dirty=true; engine.publish(makeRenderState(show,clips));
    status=enabled ? "The row will stop on another cue's GO." : "The row will continue after another cue's GO.";
    grid.repaint(); cueMetrics.repaint(); repaint();
}

void MainComponent::setCellAutoStop(int row,int column,bool enabled) {
    if (row<0 || row>=(int)show.rows.size() || column<0 || column>=(int)show.columns.size()) return;
    auto& cell=show.rows[(size_t)row].cells[(size_t)column];
    if (cell.stopOnNext==enabled) return;
    cell.stopOnNext=enabled;
    dirty=true;
    engine.publish(makeRenderState(show,clips));
    if (row==selectedRow && column==selectedColumn) cellAutoStop.setToggleState(enabled,juce::dontSendNotification);
    status=enabled ? "Cell included in cue Auto Stop." : "Cell excluded from Auto Stop: continues after the next GO.";
    grid.repaint(); repaint();
}

void MainComponent::addCue() {
    if (!applyInspector()) return;
    addRow(show,{});
    selectedRow=nextCue=(int)show.rows.size()-1;
    changed("Cue added. Playback stopped.",true);
}

void MainComponent::showColumnMenu(juce::Point<int> screenPoint) {
    if (!applyInspector()) return;
    juce::PopupMenu menu;
    menu.addItem(1,"Audio"); menu.addItem(2,"MIDI");
    menu.addItem(3,"OSC / UDP"); menu.addItem(4,"Control");
    auto safe=juce::Component::SafePointer<MainComponent>(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(
        {screenPoint.x,screenPoint.y,1,1}),[safe](int choice) {
        if (!safe || choice==0) return;
        addColumn(safe->show,(ColumnType)(choice-1),{});
        safe->selectedColumn=(int)safe->show.columns.size()-1;
        safe->changed("Column added. Playback stopped.",true);
    });
}

void MainComponent::editTempo() {
    if (!applyInspector()) return;
    stopAll();
    const int rowId=show.rows[selectedRow].id; juce::String text;
    for (const auto& p : show.rows[selectedRow].tempo) text+=juce::String(p.beat)+" "+juce::String(p.bpm)+"\n";
    prompt(this,"Cue tempo map","One line per change: beat BPM. Beats count quarter notes from zero. Does not change WAV playback speed.",text,true,
        [safe=juce::Component::SafePointer<MainComponent>(this),rowId](juce::String input) {
            if (!safe) return;
            std::vector<TempoPoint> points;
            for (const auto& line : juce::StringArray::fromLines(input)) {
                if (line.trim().isEmpty()) continue;
                std::istringstream stream(line.toStdString()); TempoPoint p; std::string extra;
                if (!(stream>>p.beat>>p.bpm) || stream>>extra) { safe->message("Required format: 0 120. One line per tempo change."); return; }
                points.push_back(p);
            }
            auto candidate=safe->show; for (auto& row : candidate.rows) if (row.id==rowId) row.tempo=points;
            auto valid=validateShow(candidate); if (valid.failed()) { safe->message(valid.getErrorMessage()); return; }
            safe->show=std::move(candidate); safe->changed("Tempo map saved for this cue. Playback stopped.");
        });
}

void MainComponent::renameTitle(bool column,int index) {
    if (!applyInspector()) return;
    const int id=column ? show.columns[index].id : show.rows[index].id;
    prompt(this,column ? "Rename column" : "Rename cue","Press Enter to confirm; the name may be empty.",
        column ? show.columns[index].name : show.rows[index].name,false,
        [safe=juce::Component::SafePointer<MainComponent>(this),column,id](juce::String name) {
            if (!safe) return;
            if (column) { for (auto& c : safe->show.columns) if (c.id==id) c.name=name.trim(); }
            else { for (auto& r : safe->show.rows) if (r.id==id) r.name=name.trim(); }
            safe->changed("Title updated.");
        });
}

void MainComponent::reorderRow(int index,int delta) {
    int target=index+delta;
    if (index<0 || index>=(int)show.rows.size() || target<0 || target>=(int)show.rows.size() || !applyInspector()) return;
    if (auto result=swapCueRows(show,index,target);result.failed()) {
        message(result.getErrorMessage()); return;
    }
    selectedRow=nextCue=target; changed("Cue order updated.",true);
}

void MainComponent::reorderColumn(int index,int delta) {
    int target=index+delta;
    if (index<0 || index>=(int)show.columns.size() || target<0 || target>=(int)show.columns.size() || !applyInspector()) return;
    if (auto result=swapCueColumns(show,index,target);result.failed()) {
        message(result.getErrorMessage()); return;
    }
    selectedColumn=target;
    // Equal-priority Control Column conflicts are resolved by visible order.
    // Rebuild the layered mapping immediately when the columns are reordered.
    refreshControlSettings();
    changed("Column order updated.",true);
}

bool MainComponent::copyRow(int index) {
    if (index<0 || index>=(int)show.rows.size() || !applyInspector()) return false;
    const auto& row=show.rows[(size_t)index];
    copiedRow=row;
    copiedRowAudio.clear();
    copiedRowAudio.reserve(row.cells.size());
    for (const auto& cell:row.cells) {
        const auto it=clips.find(cell.id);
        copiedRowAudio.push_back(it==clips.end() ? nullptr : it->second);
    }
    status="Cue copied. Right-click a row to paste below.";
    repaint();
    return true;
}

bool MainComponent::insertCopiedRowAfter(int index,const Row& source,
        const std::vector<std::shared_ptr<const AudioClip>>& sourceAudio) {
    if (index<0 || index>=(int)show.rows.size() || !applyInspector()
        || source.cells.size()!=show.columns.size() || sourceAudio.size()!=source.cells.size())
        return false;
    Show candidate;
    const auto valid=prepareInsertedCueCopy(show,index,source,candidate);
    if (valid.failed()) { message(valid.getErrorMessage()); return false; }
    auto media=clips;
    const auto& inserted=candidate.rows[(size_t)index+1];
    for (size_t c=0;c<inserted.cells.size();++c)
        if (sourceAudio[c]) media[inserted.cells[c].id]=sourceAudio[c];
    if (!memoryFits(media)) { message("Show PCM memory limit reached."); return false; }
    closeMidiClip(); closeOscClip();
    show=std::move(candidate);
    clips=std::move(media);
    refreshControlSettings();
    selectedRow=nextCue=index+1;
    changed("Cue inserted with new row and cell identities.",true);
    return true;
}

bool MainComponent::pasteRowAfter(int index) {
    if (!copiedRow) return false;
    return insertCopiedRowAfter(index,*copiedRow,copiedRowAudio);
}

bool MainComponent::duplicateRow(int index) {
    if (index<0 || index>=(int)show.rows.size() || !applyInspector()) return false;
    Row snapshot=show.rows[(size_t)index];
    snapshot.name=snapshot.name.isEmpty() ? "Cue (copy)" : snapshot.name+" (copy)";
    std::vector<std::shared_ptr<const AudioClip>> media;
    media.reserve(snapshot.cells.size());
    for (const auto& cell:snapshot.cells) {
        const auto it=clips.find(cell.id);
        media.push_back(it==clips.end() ? nullptr : it->second);
    }
    return insertCopiedRowAfter(index,snapshot,media);
}

void MainComponent::deleteRow(int index) {
    if (index<0 || index>=(int)show.rows.size() || !applyInspector()) return;
    Show candidate;
    std::vector<int> removedCellIds;
    const auto result=prepareDeletedCue(show,index,candidate,removedCellIds);
    if (result.failed()) { message(result.getErrorMessage()); return; }

    // No media or live playback state is mutated before model validation.
    for (int cellId:removedCellIds) clips.erase(cellId);
    show=std::move(candidate);
    refreshControlSettings();
    selectedRow=nextCue=std::min(index,(int)show.rows.size()-1);
    changed("Cue deleted. Original files retained.",true);
}

void MainComponent::deleteColumn(int index) {
    if (index<0 || index>=(int)show.columns.size() || !applyInspector()) return;
    Show candidate;
    std::vector<int> removedCellIds;
    const auto result=prepareDeletedColumn(show,index,candidate,removedCellIds);
    if (result.failed()) { message(result.getErrorMessage()); return; }

    for (int cellId:removedCellIds) clips.erase(cellId);
    show=std::move(candidate);
    refreshControlSettings();
    selectedColumn=std::min(index,(int)show.columns.size()-1);
    changed("Column deleted. Original files retained.",true);
}

void MainComponent::editRowBpm(int index) {
    if (index<0 || index>=(int)show.rows.size() || !applyInspector()) return;
    const int id=show.rows[(size_t)index].id;
    prompt(this,"Cue BPM","New initial BPM (20–400). Any later tempo changes remain unchanged.",
        juce::String(show.rows[(size_t)index].tempo.front().bpm,1),false,
        [safe=juce::Component::SafePointer<MainComponent>(this),id](juce::String text) {
            if (!safe) return;
            const auto value=text.trim();
            if (value.isEmpty() || value.retainCharacters("0123456789.")!=value) {
                safe->message("Invalid BPM."); return;
            }
            auto candidate=safe->show;
            bool changed=false;
            for (auto& row:candidate.rows) if (row.id==id) {
                row.tempo.front().bpm=value.getDoubleValue(); changed=true; break;
            }
            if (!changed) return;
            if (auto result=validateShow(candidate); result.failed()) {
                safe->message(result.getErrorMessage()); return;
            }
            safe->show=std::move(candidate);
            safe->changed("Cue initial BPM updated.",true);
        });
}

} // namespace cuelist
