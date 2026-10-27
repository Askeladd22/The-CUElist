#include "MainComponent.h"
#include <algorithm>
#include <cmath>

namespace cuelist {
// Cell clipboard, swaps and clip timing edits retain their existing
// transport/engine publication semantics. No audio callback changes.

void MainComponent::moveClip(int sr,int sc,int tr,int tc) {
    if (sc>=0 && tc>=0 && sc<(int)show.columns.size() && tc<(int)show.columns.size() &&
        (show.columns[(size_t)sc].collapsed || show.columns[(size_t)tc].collapsed))
        return;
    if (sc>=0 && sc<(int)show.columns.size() && tr>=0 && tr<(int)show.rows.size() &&
        tc>=0 && tc<(int)show.columns.size() &&
        ((show.columns[(size_t)sc].type==ColumnType::control &&
          show.columns[(size_t)sc].controlLocked) ||
         (show.columns[(size_t)tc].type==ColumnType::control &&
          show.columns[(size_t)tc].controlLocked))) {
        status="Unlock the Control Column before moving its cue-specific sets.";
        repaint(); return;
    }
    if (!applyInspector()) return;
    auto result=moveCell(show,sr,sc,tr,tc); if (result.failed()) { status=result.getErrorMessage(); repaint(); return; }
    selectedRow=tr; selectedColumn=tc; nextCue=tr;
    refreshControlSettings(); changed("Cells swapped.",true);
}

void MainComponent::setCellLoop(int row,int column,bool enabled) {
    if(row<0 || row>=(int)show.rows.size() || column<0 ||
       column>=(int)show.columns.size() || show.columns[(size_t)column].collapsed) return;
    if(!applyInspector()) return;
    auto& cell=show.rows[(size_t)row].cells[(size_t)column];
    if(cell.loop==enabled) return;
    cell.loop=enabled;
    changed(enabled ? "Cell loop enabled." : "Cell loop disabled.");
}

void MainComponent::setCellFollowsRow(int row,int column,bool enabled) {
    if(row<0 || row>=(int)show.rows.size() || column<0 ||
       column>=(int)show.columns.size() || show.columns[(size_t)column].collapsed) return;
    if(!applyInspector()) return;
    auto& cell=show.rows[(size_t)row].cells[(size_t)column];
    if(cell.followsRow==enabled) return;
    cell.followsRow=enabled;
    changed(enabled ? "Cell included in Row GO." : "Cell excluded from Row GO.");
}

bool MainComponent::deleteSelectedCell() {
    if(selectedRow<0 || selectedRow>=(int)show.rows.size() ||
       selectedColumn<0 || selectedColumn>=(int)show.columns.size() ||
       show.columns[(size_t)selectedColumn].collapsed ||
       (show.columns[(size_t)selectedColumn].type==ColumnType::control &&
        show.columns[(size_t)selectedColumn].controlLocked) || !applyInspector()) return false;
    auto& cell=show.rows[(size_t)selectedRow].cells[(size_t)selectedColumn];
    const int id=cell.id;
    clips.erase(id);
    // Clear content and settings with a stable cell identity.
    cell=Cell{};cell.id=id;
    closeMidiClip();closeOscClip();
    refreshControlSettings();
    changed("Cell deleted. Source files were not touched.");
    return true;
}

bool MainComponent::cutSelectedCell() {
    if(!copySelectedCell()) return false;
    return deleteSelectedCell();
}

bool MainComponent::copySelectedCell() {
    if (selectedRow<0 || selectedRow>=(int)show.rows.size() ||
        selectedColumn<0 || selectedColumn>=(int)show.columns.size()) return false;
    if (show.columns[(size_t)selectedColumn].type==ColumnType::control &&
        show.columns[(size_t)selectedColumn].controlLocked) {
        status="Locked Control Column cells cannot be copied individually.";
        repaint();return false;
    }
    const auto& source=show.rows[(size_t)selectedRow].cells[(size_t)selectedColumn];
    copiedCell=source;
    copiedType=show.columns[(size_t)selectedColumn].type;
    const auto found=clips.find(source.id);
    copiedAudio=found==clips.end() ? nullptr : found->second;
    status="Cell copied: press Ctrl+V on another cell of the same type."; repaint();
    return true;
}

bool MainComponent::pasteSelectedCell() {
    if (selectedColumn>=0 && selectedColumn<(int)show.columns.size() &&
        show.columns[(size_t)selectedColumn].collapsed) return false;
    if (!copiedCell || selectedRow<0 || selectedRow>=(int)show.rows.size() ||
        selectedColumn<0 || selectedColumn>=(int)show.columns.size()) return false;
    if (show.columns[(size_t)selectedColumn].type==ColumnType::control &&
        show.columns[(size_t)selectedColumn].controlLocked) {
        status="Unlock this Control Column before pasting cue-specific sets.";
        repaint();return false;
    }
    if (show.columns[(size_t)selectedColumn].type!=copiedType) {
        message("Copy and paste require columns of the same type."); return false;
    }
    if (!applyInspector()) return false;
    auto updatedClips=clips;
    const int id=show.rows[(size_t)selectedRow].cells[(size_t)selectedColumn].id;
    if (copiedAudio) updatedClips[id]=copiedAudio;
    else updatedClips.erase(id);
    if (!memoryFits(updatedClips)) {
        message("Cannot paste: media memory limit exceeded."); return false;
    }
    auto candidate=show;
    auto& target=candidate.rows[(size_t)selectedRow].cells[(size_t)selectedColumn];
    target=*copiedCell;
    target.id=id; // Stable destination cell identity.
    if (copiedType==ColumnType::control && target.controlSet)
        for (auto& binding:target.controlSet->bindings)
            binding.id=candidate.controls.nextId++; // never alias another set
    if (auto result=validateShow(candidate);result.failed()) {
        message(result.getErrorMessage()); return false;
    }
    closeMidiClip(); closeOscClip();
    show=std::move(candidate);
    clips=std::move(updatedClips);
    if (copiedType==ColumnType::control) refreshControlSettings();
    changed("Cell content and settings pasted.");
    return true;
}

void MainComponent::setClipDuration(int column,double seconds) {
    if (column<0 || column>=(int)show.columns.size() ||
        selectedRow<0 || selectedRow>=(int)show.rows.size() || !std::isfinite(seconds)) return;
    auto& row=show.rows[(size_t)selectedRow];
    auto& cell=row.cells[(size_t)column];
    if (!cell.midi && !cell.osc) return;
    const auto& tempo=row.tempo;
    const auto beatAt=[&](double duration) {
        double low=0,high=std::max(1.0,duration*4);
        while (secondsAtBeat(tempo,high)<duration && high<1000000.0) high*=2;
        for (int i=0;i<48;++i) {
            double mid=(low+high)*.5;
            if (secondsAtBeat(tempo,mid)<duration) low=mid; else high=mid;
        }
        return (low+high)*.5;
    };
    double length=cell.midi || cell.osc->useBeats ? beatAt(std::max(.01,seconds)) : std::max(.01,seconds);
    if (cell.midi) {
        for (const auto& note:cell.midi->notes) length=std::max(length,note.beat+note.length+.0001);
        length=juce::jlimit(.25,4096.0,length);
        if (std::abs(cell.midi->lengthBeats-length)<.0001) return;
        cell.midi->lengthBeats=length;
    } else {
        for (const auto& event:cell.osc->events) length=std::max(length,event.time+.001);
        for (const auto& automation:cell.osc->automations)
            length=std::max(length,automation.time+automation.duration);
        length=juce::jlimit(.01,86400.0,length);
        if (std::abs(cell.osc->length-length)<.0001) return;
        cell.osc->length=length;
    }
    dirty=true;
    engine.publish(makeRenderState(show,clips));
    if (cell.midi && clipEditor && clipEditorCellId==cell.id) clipEditor->loadClip(*cell.midi);
    if (cell.osc && oscEditor && oscEditorCellId==cell.id) oscEditor->loadClip(*cell.osc);
    status="Clip length updated in timeline.";
    timeline.repaint(); grid.repaint(); repaint();
}

void MainComponent::setClipStart(int column,double seconds) {
    if (column<0 || column>=(int)show.columns.size() || !std::isfinite(seconds)) return;
    auto& cell=show.rows[selectedRow].cells[column]; seconds=juce::jlimit(0.0,86400.0,std::round(seconds*100)/100);
    if (cell.startSeconds==seconds) return;
    cell.startSeconds=seconds; dirty=true; engine.publish(makeRenderState(show,clips));
    status="Clip start: "+juce::String(seconds,2)+" s"; timeline.repaint(); repaint();
}

void MainComponent::seekTimeline(double seconds) {
    if (!std::isfinite(seconds)) return;
    if (!engine.seekRow(show.rows[selectedRow].id,juce::jlimit(0.0,86400.0,seconds))) { status="Command queue full."; }
    timeline.repaint();
}

} // namespace cuelist
