#include "MainComponent.h"
#include <cmath>

namespace cuelist {
namespace {
juce::String columnTypeName(ColumnType type) { return type==ColumnType::audio ? "AUDIO" : type==ColumnType::midi ? "MIDI" : type==ColumnType::osc ? "OSC" : "CONTROL"; }
juce::String timeText(double seconds) {
    const auto whole=(int)seconds;
    return juce::String(whole/60).paddedLeft('0',2)+":"+juce::String(whole%60).paddedLeft('0',2);
}
void smallButton(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> box, bool enabled, bool danger=false) {
    const auto colour=danger ? palette::red : palette::muted;
    g.setColour(palette::raised.withAlpha(enabled ? 1.0f : .45f));
    g.fillRoundedRectangle(box.toFloat(),5);
    g.setColour(palette::line.withAlpha(enabled ? 1.0f : .4f));
    g.drawRoundedRectangle(box.toFloat().reduced(.5f),5,1);
    label(g,text,box,12,colour.withAlpha(enabled ? 1.0f : .45f),true,juce::Justification::centred);
}
void transportButton(juce::Graphics& g,juce::Rectangle<int> box,bool stop,bool enabled) {
    g.setColour((stop ? palette::red : palette::teal).withAlpha(enabled ? .16f : .06f));
    g.fillRoundedRectangle(box.toFloat(),5);
    g.setColour((stop ? palette::red : palette::teal).withAlpha(enabled ? .75f : .30f));
    g.drawRoundedRectangle(box.toFloat().reduced(.5f),5,1);
    g.setColour((stop ? palette::red : palette::teal).withAlpha(enabled ? 1.0f : .25f));
    const float x=(float)box.getCentreX(),y=(float)box.getCentreY();
    if (stop) g.fillRect(x-4,y-4,8.0f,8.0f);
    else { juce::Path triangle; triangle.addTriangle(x-3,y-5,x-3,y+5,x+5,y); g.fillPath(triangle); }
}
}

void CueGrid::paint(juce::Graphics& g) {
    g.fillAll(palette::background);
    g.setColour(palette::raised); g.fillRect(0,0,getWidth(),headerHeight);
    g.setColour(palette::line); g.drawHorizontalLine(headerHeight-1,0,(float)getWidth());
    g.setColour(palette::teal); g.fillRoundedRectangle(12.0f,9.0f,3.0f,50.0f,1.5f);
    label(g,"CUE LIST",{25,10,170,22},14,palette::teal,true);
    label(g,"SEQUENCE   /   LIVE CUES",{25,37,nameWidth-40,23},12,palette::muted,true);
    const auto& show=owner.show;
    for (int c=0;c<(int)show.columns.size();++c) {
        const int x=nameWidth+c*columnWidth; const auto colour=palette::type((int)show.columns[c].type);
        g.setColour(colour); g.fillRect(x,0,columnWidth-1,5);
        g.setColour(colour.withAlpha(.09f));
        g.fillRect(x,5,columnWidth-1,headerHeight-6);
        label(g,columnTypeName(show.columns[c].type),{x+10,11,78,20},12,colour,true);
        smallButton(g,"<",{x+columnWidth-72,11,20,20},c>0);
        smallButton(g,">",{x+columnWidth-48,11,20,20},c+1<(int)show.columns.size());
        smallButton(g,"x",{x+columnWidth-24,11,20,20},true,true);
        // Older projects may still contain unnamed columns: show meaningful
        // type-specific numbering without rewriting their saved metadata.
        int ordinal=0;
        for (int n=0;n<=c;++n) if (show.columns[n].type==show.columns[c].type) ++ordinal;
        const auto prefix=show.columns[c].type==ColumnType::audio ? "Audio" :
            show.columns[c].type==ColumnType::midi ? "MIDI" :
            show.columns[c].type==ColumnType::osc ? "OSC" : "Control";
        const auto name=show.columns[c].name.isEmpty() ? juce::String(prefix)+" "+juce::String(ordinal) : show.columns[c].name;
        label(g,name,{x+10,36,columnWidth-20,25},15,palette::text,true);
        if (show.columns[c].type==ColumnType::control && show.columns[c].controlLocked)
            label(g,"LOCKED",{x+75,12,70,17},10,palette::amber,true);
        g.setColour(palette::line); g.drawVerticalLine(x,0,(float)getHeight());
    }
    const int tail=nameWidth+(int)show.columns.size()*columnWidth;
    label(g,"BPM (click)",{tail+12,31,92,24},13,palette::text,true);
    label(g,"AUTO STOP",{tail+105,31,96,24},12,palette::text,true);
    const auto clip=g.getClipBounds();
    const int first=std::max(0,(clip.getY()-headerHeight)/rowHeight), last=std::min((int)show.rows.size(),(clip.getBottom()-headerHeight)/rowHeight+1);
    for (int r=first;r<last;++r) {
        const int y=headerHeight+r*rowHeight; const bool selected=r==owner.selectedRow, next=r==owner.nextCueIndex();
        const auto& row=show.rows[r];
        bool playing=false,paused=false;
        if (owner.engine.connected.load()) for (const auto& cell:row.cells) {
            const auto state=owner.engine.getCellPlayback(cell.id);
            playing=playing||state.playing;
            paused=paused||state.paused;
        }
        g.setColour(selected ? palette::raised.withAlpha(.94f) :
                    playing ? palette::teal.withAlpha(.17f) :
                    next ? palette::amber.withAlpha(.11f) : palette::panel);
        g.fillRect(0,y,getWidth(),rowHeight);
        if (next || playing || selected) {
            g.setColour(playing ? palette::teal : next ? palette::amber : palette::muted);
            g.fillRect(0,y,5,rowHeight);
        }
        g.setColour(palette::line.withAlpha(.66f));
        g.drawHorizontalLine(y+rowHeight-1,0,(float)getWidth());
        g.saveState();
        g.addTransform(juce::AffineTransform::translation(0.0f,(float)rowContentInset));
        smallButton(g,"^",{6,y+17,20,21},r>0); smallButton(g,"v",{30,y+17,20,21},r+1<(int)show.rows.size());
        smallButton(g,"x",{54,y+17,20,21},true,true);
        smallButton(g,"...",{nameWidth-70,y+17,28,21},true);
        g.setColour(next ? palette::amber : palette::teal);
        g.fillRoundedRectangle(84.0f,(float)y+12,48,33,5);
        label(g,"GO",{84,y+12,48,33},15,palette::background,true,juce::Justification::centred);
        label(g,row.name.isEmpty() ? "Untitled cue" : row.name,
              {142,y+5,nameWidth-215,28},17,palette::text,true);
        const juce::String cueState=playing ? "PLAYING" : paused ? "PAUSED" :
                                    next ? "NEXT CUE" : row.muted ? "MUTED" : "STANDBY";
        const auto stateColour=playing ? palette::teal : next || paused ? palette::amber : palette::muted;
        label(g,juce::String(r+1).paddedLeft('0',2)+"  /  "+cueState,
              {142,y+32,nameWidth-215,19},12,stateColour,true);
        transportButton(g,{nameWidth-36,y+17,26,22},true,true);
        for (int c=0;c<(int)show.columns.size();++c) {
            const int x=nameWidth+c*columnWidth; const auto& cell=row.cells[c];
            const bool loaded=owner.hasClip(r,c), missing=cell.file.isNotEmpty() && !loaded;
            const auto colour=missing ? palette::red : palette::type((int)show.columns[c].type);
            const auto box=juce::Rectangle<float>((float)x+5,(float)y+1,(float)columnWidth-10,62.0f);
            g.setColour(loaded ? colour.withAlpha(.24f) : palette::background.withAlpha(.65f));
            g.fillRoundedRectangle(box,7);
            const bool drop=dragging && dropRow==r && dropColumn==c;
            g.setColour(drop ? palette::amber : selected && c==owner.selectedColumn ? colour : palette::line.withAlpha(.65f));
            g.drawRoundedRectangle(box,7,drop || (selected && c==owner.selectedColumn) ? 2.0f : 1.0f);
            if (show.columns[c].type==ColumnType::control) {
                const auto& col=show.columns[c];
                const ControlSet* set=col.controlLocked ? &col.controlDefaultSet :
                    (cell.controlSet ? &*cell.controlSet : nullptr);
                const int count=set ? (int)set->bindings.size() : 0;
                label(g,col.controlLocked ? "DEFAULT / LOCKED" :
                      count>0 ? "CONTROL SET" : "+ Control Set",
                      {x+12,y+10,columnWidth-22,20},12,
                      col.controlLocked ? palette::amber : count>0 ? palette::teal : palette::muted,
                      count>0);
                if (count>0) label(g,juce::String(count)+" mappings",
                    {x+12,y+32,columnWidth-22,17},11,palette::muted);
            } else if (loaded) {
                auto playback=owner.engine.getCellPlayback(cell.id);
                if (!owner.engine.connected.load()) playback.playing=playback.waiting=playback.paused=false;
                label(g,cell.name.isEmpty() ? (cell.midi ? "MIDI" : cell.osc ? "OSC" : juce::File(cell.file).getFileNameWithoutExtension()) : cell.name,{x+13,y+9,columnWidth-68,24},14,palette::text,true);
                const auto seconds=owner.clipDuration(r,c);
                label(g,playback.waiting ? "WAITING" : timeText(playback.positionSeconds)+" / "+timeText(seconds),
                      {x+13,y+35,columnWidth-72,19},12,colour);
                const auto progress=seconds>0 ? juce::jlimit(0.0,1.0,playback.positionSeconds/seconds) : 0;
                g.setColour(palette::line); g.fillRect(x+8,y+56,columnWidth-16,4);
                g.setColour(playback.paused || playback.waiting ? palette::amber : colour);
                g.fillRect((float)x+8,(float)y+56,(float)((columnWidth-16)*progress),4.0f);
                if (playback.playing || playback.paused) {
                    g.setColour(playback.paused ? palette::amber : palette::teal);
                    g.fillEllipse((float)x+columnWidth-19,(float)y+13,8,8);
                }
            } else label(g,missing ? "! File unavailable" : show.columns[c].type==ColumnType::audio ? "+ WAV / MP3" : show.columns[c].type==ColumnType::midi ? "+ MIDI / Piano Roll" : "+ OSC / messages",
                         {x+10,y+8,columnWidth-20,24},12,missing ? palette::red : palette::muted,false,juce::Justification::centred);
            // Auto Stop is a per-cell choice. The row's switch remains the
            // master gate; an OFF cell survives future GO commands.
            if (autoStopVisibleRows.count(row.id)) {
                const bool effective=row.stopOnNext && cell.stopOnNext;
                const auto badge=juce::Rectangle<float>((float)x+columnWidth-52,(float)y+7,44,18);
                g.setColour(effective ? palette::teal.withAlpha(.22f)
                    : (!cell.stopOnNext ? palette::amber.withAlpha(.16f) : palette::raised));
                g.fillRoundedRectangle(badge,3.0f);
                label(g,cell.stopOnNext ? "AS ON" : "AS OFF",
                    {x+columnWidth-52,y+7,44,18},10,
                    effective ? palette::teal : !cell.stopOnNext ? palette::amber : palette::muted,
                    true,juce::Justification::centred);
            }
            if (show.columns[c].type!=ColumnType::control) {
                transportButton(g,{x+columnWidth-54,y+28,21,18},false,loaded);
                transportButton(g,{x+columnWidth-29,y+28,21,18},true,loaded);
            }
        }
        const auto tempo=row.tempo.front();
        label(g,juce::String(tempo.bpm,1),{tail+12,y+14,79,25},17,palette::text,true);
        g.setColour(row.stopOnNext ? palette::teal.withAlpha(.25f) : palette::raised);
        g.fillRoundedRectangle((float)tail+115,(float)y+8,34,19,9.5f);
        g.setColour(row.stopOnNext ? palette::teal : palette::muted);
        g.fillEllipse((float)tail+(row.stopOnNext ? 134 : 118),(float)y+11,13,13);
        label(g,row.stopOnNext ? "ON" : "OFF",{tail+154,y+8,38,20},11,row.stopOnNext ? palette::teal : palette::muted);
        smallButton(g,autoStopVisibleRows.count(row.id) ? "−" : "+",{tail+118,y+32,28,19},true);
        g.restoreState();
        g.setColour(palette::line.withAlpha(.65f));
        g.drawHorizontalLine(y+rowHeight-1,0,(float)getWidth());
    }
}

void CueGrid::showRowMenu(int row) {
    if (row<0 || row>=(int)owner.show.rows.size()) return;
    const int rowId=owner.show.rows[(size_t)row].id;
    juce::PopupMenu menu;
    menu.addItem(1,"Copy cue");
    menu.addItem(2,"Paste cue below",owner.copiedRow.has_value()
        && owner.copiedRow->cells.size()==owner.show.columns.size());
    menu.addItem(3,"Duplicate cue");
    menu.addSeparator();
    menu.addItem(4,"Delete cue");
    auto safe=juce::Component::SafePointer<MainComponent>(&owner);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),[safe,rowId](int item) {
        if (!safe || item==0) return;
        for (int index=0;index<(int)safe->show.rows.size();++index)
            if (safe->show.rows[(size_t)index].id==rowId) {
                if (item==1) safe->copyRow(index);
                else if (item==2) safe->pasteRowAfter(index);
                else if (item==3) safe->duplicateRow(index);
                else if (item==4) safe->deleteRow(index);
                return;
            }
    });
}

void CueGrid::mouseDown(const juce::MouseEvent& e) {
    dragRow=dragColumn=dropRow=dropColumn=-1; dragging=false;
    if (e.mods.isPopupMenu() && e.x>=nameWidth && e.y>=headerHeight) {
        const int row=(e.y-headerHeight)/rowHeight,col=(e.x-nameWidth)/columnWidth;
        if (row>=0 && row<(int)owner.show.rows.size() && col>=0 && col<(int)owner.show.columns.size() && owner.select(row,col)) {
            juce::PopupMenu menu;
            const bool locked=owner.show.columns[(size_t)col].type==ColumnType::control &&
                owner.show.columns[(size_t)col].controlLocked;
            if (locked) menu.addItem(3,"Edit default Control Set");
            menu.addItem(1,"Copy cell  Ctrl+C",owner.hasClip(row,col) && !locked);
            menu.addItem(2,"Paste cell  Ctrl+V",owner.copiedCell.has_value() &&
                owner.copiedType==owner.show.columns[(size_t)col].type && !locked);
            auto safe=juce::Component::SafePointer<MainComponent>(&owner);
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),[safe](int item) {
                if (safe) {
                    if (item==1) safe->copySelectedCell();
                    else if (item==2) safe->pasteSelectedCell();
                    else if (item==3) safe->editControlSet(safe->selectedRow,safe->selectedColumn);
                }
            });
        }
        return;
    }
    if (e.mods.isPopupMenu() && e.x>=0 && e.x<nameWidth && e.y>=headerHeight) {
        const int row=(e.y-headerHeight)/rowHeight;
        if (row>=0 && row<(int)owner.show.rows.size()) showRowMenu(row);
        return;
    }
    if (!e.mods.isLeftButtonDown()) return;
    owner.grabKeyboardFocus();
    if (e.y<0 || e.x<0) return;
    const int column=(e.x-nameWidth)/columnWidth;
    if (e.y<headerHeight) {
        if (e.x<nameWidth || column>=(int)owner.show.columns.size()) return;
        const int local=e.x-nameWidth-column*columnWidth;
        if (e.y>=11 && e.y<31) {
            if (e.getNumberOfClicks()>1) return;
            if (local>=columnWidth-72 && local<columnWidth-52) owner.reorderColumn(column,-1);
            else if (local>=columnWidth-48 && local<columnWidth-28) owner.reorderColumn(column,1);
            else if (local>=columnWidth-24 && local<columnWidth-4) owner.deleteColumn(column);
        } else owner.select(owner.selectedRow,column);
        return;
    }
    const int row=(e.y-headerHeight)/rowHeight, localY=(e.y-headerHeight)%rowHeight-rowContentInset;
    if (row>=(int)owner.show.rows.size()) return;
    const int tail=nameWidth+(int)owner.show.columns.size()*columnWidth;
    if (e.x>=tail+12 && e.x<tail+96 && localY>=8 && localY<45) {
        if (e.getNumberOfClicks()==1) owner.editRowBpm(row);
        return;
    }
    if (e.x>=tail+105 && e.x<tail+195 && localY>=6 && localY<30) {
        if (e.getNumberOfClicks()==1) owner.setAutoStop(row,!owner.show.rows[row].stopOnNext);
        return;
    }
    if (e.x>=tail+115 && e.x<tail+150 && localY>=31 && localY<53) {
        const int id=owner.show.rows[(size_t)row].id;
        if (autoStopVisibleRows.count(id)) autoStopVisibleRows.erase(id);
        else autoStopVisibleRows.insert(id);
        repaint(); return;
    }
    if (e.x>=nameWidth && column>=0 && column<(int)owner.show.columns.size() && localY>=5 && localY<27) {
        const int localX=(e.x-nameWidth)%columnWidth;
        if (autoStopVisibleRows.count(owner.show.rows[(size_t)row].id)
            && localX>=columnWidth-52 && localX<columnWidth-8) {
            if (e.getNumberOfClicks()==1)
                owner.setCellAutoStop(row,column,!owner.show.rows[(size_t)row].cells[(size_t)column].stopOnNext);
            return;
        }
    }
    if (e.x>=nameWidth && column<(int)owner.show.columns.size() && localY>=28 && localY<46) {
        const int localX=(e.x-nameWidth)%columnWidth;
        if (owner.show.columns[(size_t)column].type!=ColumnType::control &&
            ((localX>=columnWidth-54 && localX<columnWidth-33) ||
             (localX>=columnWidth-29 && localX<columnWidth-8))) {
            if (e.getNumberOfClicks()==1 && owner.hasClip(row,column)) {
                if (localX<columnWidth-33) owner.triggerCellAt(row,column); else owner.stopCellAt(row,column);
            }
            return;
        }
    }
    if (e.x<nameWidth) {
        if (e.x>=nameWidth-36 && e.x<nameWidth-10 && localY>=17 && localY<39) {
            if (e.getNumberOfClicks()==1) owner.stopRow(row); return;
        }
        if (e.x>=nameWidth-70 && e.x<nameWidth-42 && localY>=17 && localY<38) {
            if (e.getNumberOfClicks()==1) showRowMenu(row);
            return;
        }
        if (e.x<137 && e.getNumberOfClicks()>1) return;
        if (localY>=17 && localY<38 && e.x>=6 && e.x<26) owner.reorderRow(row,-1);
        else if (localY>=17 && localY<38 && e.x>=30 && e.x<50) owner.reorderRow(row,1);
        else if (localY>=17 && localY<38 && e.x>=54 && e.x<74) owner.deleteRow(row);
        else if (e.x>=84 && e.x<132 && localY>=12 && localY<45) owner.triggerRow(row);
        else owner.select(row,owner.selectedColumn);
    } else if (column<(int)owner.show.columns.size() && owner.select(row,column)) {
        if (owner.show.columns[(size_t)column].type==ColumnType::control) {
            if (e.getNumberOfClicks()==1) owner.editControlSet(row,column);
        } else if (owner.hasClip(row,column)) {
            dragRow=row; dragColumn=column;
        }
    }
}
void CueGrid::mouseDrag(const juce::MouseEvent& e) {
    if (dragRow<0 || e.getDistanceFromDragStart()<5) return;
    dragging=true; dropRow=dropColumn=-1;
    if (e.x>=nameWidth && e.y>=headerHeight) {
        const int row=(e.y-headerHeight)/rowHeight, col=(e.x-nameWidth)/columnWidth;
        if (row<(int)owner.show.rows.size() && col<(int)owner.show.columns.size()
            && owner.show.columns[col].type==owner.show.columns[dragColumn].type) { dropRow=row; dropColumn=col; }
    }
    setMouseCursor(dropRow>=0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor); repaint();
}
void CueGrid::mouseUp(const juce::MouseEvent&) {
    if (dragging && dropRow>=0 && (dragRow!=dropRow || dragColumn!=dropColumn)) owner.moveClip(dragRow,dragColumn,dropRow,dropColumn);
    dragRow=dragColumn=dropRow=dropColumn=-1; dragging=false; setMouseCursor(juce::MouseCursor::NormalCursor); repaint();
}
void CueGrid::mouseDoubleClick(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu() || e.x<0 || e.y<0) return;
    if (e.y<headerHeight) {
        const int column=(e.x-nameWidth)/columnWidth;
        if (e.x>=nameWidth && column<(int)owner.show.columns.size() && e.y>=34) owner.renameTitle(true,column);
        return;
    }
    const int row=(e.y-headerHeight)/rowHeight;
    if (row>=(int)owner.show.rows.size()) return;
    if (e.x>=137 && e.x<nameWidth-40) owner.renameTitle(false,row);
    else if (e.x>=nameWidth) {
        const int col=(e.x-nameWidth)/columnWidth;
        const int localX=(e.x-nameWidth)%columnWidth, localY=(e.y-headerHeight)%rowHeight;
        if (localY>=28 && localY<46 && localX>=columnWidth-54) return;
        if (col<(int)owner.show.columns.size() && owner.select(row,col)) {
            if (owner.show.columns[col].type==ColumnType::midi) owner.editMidiClip();
            else if (owner.show.columns[col].type==ColumnType::osc) owner.editOscClip();
            else if (owner.show.columns[col].type==ColumnType::control)
                owner.editControlSet(row,col);
            else owner.triggerCell();
        }
    }
}
bool CueGrid::isInterestedInFileDrag(const juce::StringArray& files) {
    return files.size()==1 && (files[0].endsWithIgnoreCase(".wav") || files[0].endsWithIgnoreCase(".mp3") || files[0].endsWithIgnoreCase(".mid") || files[0].endsWithIgnoreCase(".midi"));
}
void CueGrid::filesDropped(const juce::StringArray& files,int x,int y) {
    if (!isInterestedInFileDrag(files) || x<nameWidth || y<headerHeight) return;
    const int row=(y-headerHeight)/rowHeight, column=(x-nameWidth)/columnWidth;
    if (row>=(int)owner.show.rows.size() || column>=(int)owner.show.columns.size()) return;
    if (owner.select(row,column)) owner.importFile(juce::File(files[0]));
}

}
