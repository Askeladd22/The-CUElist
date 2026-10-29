#include "MainComponent.h"
#include "OperatorTextPrompt.h"
#include "ShowEditOperations.h"
#include <algorithm>
#include <sstream>
#include <set>

namespace cuelist {
namespace {
// Shared prompt UI; editing, validation and destructive model changes
// remain in the existing owner callbacks.
void prompt(juce::Component* parent,juce::LookAndFeel& theme,
            const juce::String& title,const juce::String& hint,
            const juce::String& field,const juce::String& initial,bool multiline,
            std::function<void(juce::String)> result) {
    showOperatorTextPrompt(*parent,theme,title,hint,field,initial,multiline,
                           "Apply",std::move(result));
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
    // or a stale mapping preset into a different cue order. A background
    // project-open request must not replace the newly selected Cuelist later.
    if (projectLoader) projectLoader->cancel();
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
    // Fill the first free sequence number, starting at 1. The previous
    // algorithm started at index+2 and created Cuelist 3 after Cuelist 1.
    juce::String name;
    for (int number=1;number<=50;++number) {
        const auto candidate="Cuelist "+juce::String(number);
        const auto used=std::any_of(show.cuelists.begin(),show.cuelists.end(),
            [&](const Cuelist& item){return item.name.equalsIgnoreCase(candidate);});
        if (!used) {name=candidate;break;}
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

void MainComponent::deleteCuelist(int index) {
    // Deleting a list is a structural, undo-boundary operation. It never
    // deletes source media files; only its cue data and decoded PCM cache.
    if (index<0 || index>=(int)show.cuelists.size() ||
        show.cuelists.size()<=1 || !applyInspector()) return;

    Show candidate=show;
    candidate.cuelists[(size_t)candidate.activeCuelist].rows=candidate.rows;
    const auto removedName=candidate.cuelists[(size_t)index].name;
    std::set<int> removedRows,removedCells;
    for (const auto& row:candidate.cuelists[(size_t)index].rows) {
        removedRows.insert(row.id);
        for (const auto& cell:row.cells) removedCells.insert(cell.id);
    }

    candidate.cuelists.erase(candidate.cuelists.begin()+index);
    if (index<candidate.activeCuelist) --candidate.activeCuelist;
    else if (index==candidate.activeCuelist)
        candidate.activeCuelist=std::min(index,(int)candidate.cuelists.size()-1);
    candidate.rows=candidate.cuelists[(size_t)candidate.activeCuelist].rows;

    auto& bindings=candidate.controls.bindings;
    bindings.erase(std::remove_if(bindings.begin(),bindings.end(),
        [&](const ControlBinding& binding) {
            return binding.scope==ControlScope::cue &&
                   removedRows.count(binding.scopeId)!=0;
        }),bindings.end());
    for (auto& column:candidate.columns)
        if (removedRows.count(column.controlDefaultSourceRowId)!=0)
            column.controlDefaultSourceRowId=0;

    if (auto valid=validateShow(candidate);valid.failed()) {
        message("Cannot delete Cuelist: "+valid.getErrorMessage());
        return; // The original show and its audio remain untouched.
    }

    if (projectLoader) projectLoader->cancel();
    stopAll();closeMidiClip();closeOscClip();
    editHistory.clearUndoHistory();
    show=std::move(candidate);
    for (int cellId:removedCells) clips.erase(cellId);
    selectedRow=selectedColumn=nextCue=0;
    copiedRow.reset();copiedRowAudio.clear();copiedCell.reset();copiedAudio.reset();
    resetActiveMapping();
    refreshControlSettings();
    engine.publish(makeRenderState(show,clips));
    viewport.setViewPosition(0,0);
    dirty=true;
    status="Deleted "+removedName+". Original media files retained.";
    refresh();
}

void MainComponent::requestDeleteCuelist(int index) {
    if (index<0 || index>=(int)show.cuelists.size() ||
        show.cuelists.size()<=1) return;
    const auto name=show.cuelists[(size_t)index].name;
    auto* dialog=new juce::AlertWindow(
        "Delete "+name+"?",
        "Delete this Cuelist and all its cues from the show? This cannot be "
        "undone. Source audio files on disk will not be removed.",
        juce::MessageBoxIconType::WarningIcon,this);
    dialog->addButton("Delete Cuelist",1);
    dialog->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true,juce::ModalCallbackFunction::create(
        [safe=juce::Component::SafePointer<MainComponent>(this),index](int choice) {
            if (safe && choice==1) safe->deleteCuelist(index);
        }),true);
}

bool MainComponent::setCuelistName(int index,const juce::String& proposed) {
    if (index<0 || index>=(int)show.cuelists.size()) return false;
    const auto name=proposed.trim();
    if (name.isEmpty() || name.length()>64) return false;
    for (int i=0;i<(int)show.cuelists.size();++i)
        if (i!=index && show.cuelists[(size_t)i].name.equalsIgnoreCase(name))
            return false;
    if (show.cuelists[(size_t)index].name==name) return true;
    // Both tab labels and the pinned Cue List header render directly from
    // this single persisted name. Never rename an independent UI copy.
    show.cuelists[(size_t)index].name=name;
    dirty=true;
    status="Cuelist renamed to "+name+". Save to persist.";
    refreshCuelistTabs();
    cueHeaders.repaint();
    grid.repaint();
    repaint();
    return true;
}

void MainComponent::renameActiveCuelist() {
    renameCuelist(show.activeCuelist);
}

void MainComponent::renameCuelist(int index) {
    if (index<0 || index>=(int)show.cuelists.size()) return;
    showOperatorTextPrompt(*this,theme,"Rename Cuelist",
        "Update the tab and its Cue List header together.",
        "Cuelist name",show.cuelists[(size_t)index].name,false,"Rename",
        [safe=juce::Component::SafePointer<MainComponent>(this),index](juce::String name) {
            if (!safe) return;
            if (!safe->setCuelistName(index,name))
                safe->message("Use a unique Cuelist name of 1-64 characters.");
        });
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
    menu.setLookAndFeel(&theme);
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
    prompt(this,theme,"Cue tempo map",
        "Each line contains beat and BPM. Beats count quarter notes from zero. Audio file playback speed is unchanged.",
        "Tempo changes",text,true,
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
    prompt(this,theme,column ? "Rename column" : "Rename cue",
        "Give this "+juce::String(column ? "column" : "cue")+" a name (or leave it empty).",
        column ? "Column name" : "Cue name",
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
    prompt(this,theme,"Cue BPM","Set the initial BPM (20–400). Later tempo changes stay unchanged.",
        "Initial BPM",juce::String(show.rows[(size_t)index].tempo.front().bpm,1),false,
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
