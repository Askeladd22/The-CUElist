#include "MainComponent.h"
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
    std::swap(show.rows[index],show.rows[target]); selectedRow=nextCue=target; changed("Cue order updated.",true);
}

void MainComponent::reorderColumn(int index,int delta) {
    int target=index+delta;
    if (index<0 || index>=(int)show.columns.size() || target<0 || target>=(int)show.columns.size() || !applyInspector()) return;
    std::swap(show.columns[index],show.columns[target]);
    for (auto& row : show.rows) std::swap(row.cells[index],row.cells[target]);
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
    if ((int64_t)show.nextId+1+(int64_t)source.cells.size()>=1000000000) {
        message("Show identifier limit reached."); return false;
    }
    auto candidate=show;
    auto media=clips;
    Row inserted=source;
    inserted.id=candidate.nextId++;
    for (size_t c=0;c<inserted.cells.size();++c) {
        inserted.cells[c].id=candidate.nextId++;
        if (sourceAudio[c]) media[inserted.cells[c].id]=sourceAudio[c];
    }
    for (auto& cell:inserted.cells) if (cell.controlSet)
        for (auto& binding:cell.controlSet->bindings) binding.id=candidate.controls.nextId++;
    candidate.rows.insert(candidate.rows.begin()+index+1,std::move(inserted));
    const auto valid=validateShow(candidate);
    if (valid.failed()) { message(valid.getErrorMessage()); return false; }
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
    const int removedId=show.rows[(size_t)index].id;
    for (auto& c : show.rows[index].cells) clips.erase(c.id);
    show.rows.erase(show.rows.begin()+index); if (show.rows.empty()) addRow(show,{});
    auto& bindings=show.controls.bindings;
    bindings.erase(std::remove_if(bindings.begin(),bindings.end(),
        [removedId](const ControlBinding& b) {
            return b.scope==ControlScope::cue && b.scopeId==removedId;
        }),bindings.end());
    refreshControlSettings();
    selectedRow=nextCue=std::min(index,(int)show.rows.size()-1); changed("Cue deleted. Original files retained.",true);
}

void MainComponent::deleteColumn(int index) {
    if (index<0 || index>=(int)show.columns.size() || !applyInspector()) return;
    const int removedId=show.columns[(size_t)index].id;
    for (auto& row : show.rows) { clips.erase(row.cells[index].id); row.cells.erase(row.cells.begin()+index); }
    show.columns.erase(show.columns.begin()+index); if (show.columns.empty()) addColumn(show,ColumnType::audio,{});
    auto& bindings=show.controls.bindings;
    bindings.erase(std::remove_if(bindings.begin(),bindings.end(),
        [removedId](const ControlBinding& b) {
            return (b.scope==ControlScope::column || b.target==ControlTarget::columnGain ||
                b.target==ControlTarget::midiCc || b.target==ControlTarget::oscFloat)
                    && b.targetColumnId==removedId;
        }),bindings.end());
    // A deleted target cannot remain in any other Control Set.
    const auto purge=[removedId](ControlSet& set) {
        auto& bindings=set.bindings;
        bindings.erase(std::remove_if(bindings.begin(),bindings.end(),
            [removedId](const ControlBinding& b){
                return b.targetColumnId==removedId &&
                    (b.target==ControlTarget::columnGain ||
                     b.target==ControlTarget::midiCc || b.target==ControlTarget::oscFloat);
            }),bindings.end());
    };
    for (auto& col:show.columns) purge(col.controlDefaultSet);
    for (auto& row:show.rows) for (auto& cell:row.cells)
        if (cell.controlSet) purge(*cell.controlSet);
    for (auto& saved:show.controls.templates) purge(saved.set);
    refreshControlSettings();
    selectedColumn=std::min(index,(int)show.columns.size()-1); changed("Column deleted. Original files retained.",true);
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
