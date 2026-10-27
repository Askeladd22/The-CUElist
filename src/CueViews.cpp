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


int CueGrid::columnWidthAt(int index) const {
    if (index<0 || index>=(int)owner.show.columns.size()) return 0;
    return owner.show.columns[(size_t)index].collapsed ? compressedWidth : columnWidth;
}
int CueGrid::columnX(int index) const {
    int x=nameWidth;
    for (int i=0;i<index && i<(int)owner.show.columns.size();++i) x+=columnWidthAt(i);
    return x;
}
int CueGrid::totalColumnsWidth() const { return columnX((int)owner.show.columns.size())-nameWidth; }
int CueGrid::columnAt(int x) const {
    if (x<nameWidth) return -1;
    for (int i=0;i<(int)owner.show.columns.size();++i)
        if (x>=columnX(i) && x<columnX(i)+columnWidthAt(i)) return i;
    return -1;
}
juce::Colour CueGrid::cueColour(int id) {
    switch (id) {
        case 1:return palette::teal;
        case 2:return palette::amber;
        case 3:return palette::red;
        case 4:return palette::purple;
        case 5:return juce::Colour(0xff69aeea);
        case 6:return juce::Colour(0xff73d89b);
        case 7:return juce::Colour(0xffffa669);
        default:return juce::Colour();
    }
}
void CueGrid::toggleAutoStopDetails(int row) {
    if (row<0 || row>=(int)owner.show.rows.size()) return;
    const int id=owner.show.rows[(size_t)row].id;
    if (autoStopVisibleRows.count(id)) autoStopVisibleRows.erase(id);
    else autoStopVisibleRows.insert(id);
    repaint();
}
void CueGrid::paint(juce::Graphics& g) {
    g.fillAll(palette::background);
    g.setColour(palette::raised);g.fillRect(0,0,getWidth(),headerHeight);
    g.setColour(palette::line);g.drawHorizontalLine(headerHeight-1,0,(float)getWidth());
    g.setColour(palette::teal);g.fillRoundedRectangle(10.f,8.f,3.f,49.f,1.5f);
    label(g,"CUE LIST",{22,9,160,22},14,palette::teal,true);
    label(g,"SEQUENCE  /  GO  /  NAME",{22,35,nameWidth-40,20},11,palette::muted,true);
    const auto& show=owner.show;
    for (int c=0;c<(int)show.columns.size();++c) {
        const auto& col=show.columns[(size_t)c];
        const int x=columnX(c),w=columnWidthAt(c);
        const auto tone=palette::type((int)col.type);
        g.setColour(tone);g.fillRect(x,0,w-1,4);
        g.setColour(tone.withAlpha(col.collapsed ? .15f : .08f));
        g.fillRect(x,4,w-1,headerHeight-5);
        if (col.collapsed) {
            label(g,"+",{x+7,9,w-14,23},17,tone,true,juce::Justification::centred);
            const auto shortName=col.type==ColumnType::audio ? "A" :
                col.type==ColumnType::midi ? "M" : col.type==ColumnType::osc ? "O" : "C";
            label(g,shortName,{x+7,39,w-14,20},12,palette::muted,true,juce::Justification::centred);
        } else {
            label(g,columnTypeName(col.type),{x+7,9,w-68,20},11,tone,true);
            smallButton(g,"<",{x+w-61,10,18,19},c>0);
            smallButton(g,">",{x+w-41,10,18,19},c+1<(int)show.columns.size());
            smallButton(g,"x",{x+w-21,10,18,19},true,true);
            int ordinal=0;
            for (int i=0;i<=c;++i) if (show.columns[(size_t)i].type==col.type) ++ordinal;
            const auto defaultName=col.type==ColumnType::audio ? "Audio" :
                col.type==ColumnType::midi ? "MIDI" : col.type==ColumnType::osc ? "OSC" : "Control";
            const auto name=col.name.isEmpty()? juce::String(defaultName)+" "+juce::String(ordinal) : col.name;
            label(g,name,{x+8,35,w-38,25},13,palette::text,true);
            smallButton(g,"−",{x+w-26,38,19,20},true);
        }
        g.setColour(palette::line);g.drawVerticalLine(x,0,(float)getHeight());
    }
    const auto clip=g.getClipBounds();
    const int first=std::max(0,(clip.getY()-headerHeight)/rowHeight);
    const int last=std::min((int)show.rows.size(),(clip.getBottom()-headerHeight)/rowHeight+1);
    for (int r=first;r<last;++r) {
        const int y=headerHeight+r*rowHeight;
        const auto& row=show.rows[(size_t)r];
        const bool selected=r==owner.selectedRow,next=r==owner.nextCueIndex();
        bool playing=false,paused=false;
        if (owner.engine.connected.load()) for (const auto& cell:row.cells) {
            const auto state=owner.engine.getCellPlayback(cell.id);
            playing=playing||state.playing;paused=paused||state.paused;
        }
        const auto custom=cueColour(row.colour);
        g.setColour(selected ? palette::raised : playing ? palette::teal.withAlpha(.17f)
                    : next ? palette::amber.withAlpha(.12f) : palette::panel);
        g.fillRect(0,y,getWidth(),rowHeight);
        if (row.colour>0) {
            g.setColour(custom.withAlpha(.11f));
            g.fillRect(0,y,getWidth(),rowHeight);
        }
        if (next||playing||selected||row.colour>0) {
            g.setColour(row.colour>0?custom:playing?palette::teal:next?palette::amber:palette::muted);
            g.fillRect(0,y,5,rowHeight);
        }
        g.saveState();
        g.addTransform(juce::AffineTransform::translation(0.f,(float)rowContentInset));
        smallButton(g,"^",{5,y+13,18,20},r>0);
        smallButton(g,"v",{26,y+13,18,20},r+1<(int)show.rows.size());
        smallButton(g,"x",{47,y+13,18,20},true,true);
        smallButton(g,"...",{nameWidth-65,y+13,25,20},true);
        g.setColour(next ? palette::amber : palette::teal);
        g.fillRoundedRectangle(75.f,(float)y+9,43.f,32.f,5.f);
        label(g,"GO",{75,y+9,43,32},13,palette::background,true,juce::Justification::centred);
        label(g,row.name.isEmpty()?"Untitled cue":row.name,
            {128,y+4,nameWidth-201,24},15,palette::text,true);
        const auto state=playing?"PLAYING":paused?"PAUSED":next?"NEXT CUE":row.muted?"MUTED":"STANDBY";
        label(g,juce::String(r+1).paddedLeft('0',2)+" / "+state,
            {128,y+29,nameWidth-201,18},11,playing?palette::teal:next?palette::amber:palette::muted,true);
        transportButton(g,{nameWidth-34,y+13,25,20},true,true);
        for (int c=0;c<(int)show.columns.size();++c) {
            const auto& column=show.columns[(size_t)c];
            const auto& cell=row.cells[(size_t)c];
            const int x=columnX(c),w=columnWidthAt(c);
            const bool loaded=owner.hasClip(r,c),missing=cell.file.isNotEmpty()&&!loaded;
            const auto tone=missing?palette::red:palette::type((int)column.type);
            if (column.collapsed) {
                g.setColour(loaded?tone.withAlpha(.37f):palette::background.withAlpha(.66f));
                g.fillRoundedRectangle((float)x+5,(float)y+7,(float)w-10,39.f,5.f);
                g.setColour(tone.withAlpha(.85f));
                g.drawRoundedRectangle((float)x+5,(float)y+7,(float)w-10,39.f,5.f,1.f);
                if (loaded) g.fillEllipse((float)x+w/2-4,(float)y+21,8.f,8.f);
                else label(g,"—",{x+8,y+13,w-16,20},12,palette::muted,true,juce::Justification::centred);
                continue;
            }
            const auto box=juce::Rectangle<float>((float)x+4,(float)y+3,(float)w-8,49.f);
            g.setColour(loaded?tone.withAlpha(.20f):palette::background.withAlpha(.55f));
            g.fillRoundedRectangle(box,6.f);
            const bool drop=dragging && dropRow==r && dropColumn==c;
            g.setColour(drop?palette::amber:(selected&&c==owner.selectedColumn?tone:palette::line));
            g.drawRoundedRectangle(box,6.f,drop||selected&&c==owner.selectedColumn?1.8f:1.f);
            if (column.type==ColumnType::control) {
                const ControlSet* set=column.controlLocked?&column.controlDefaultSet:
                    (cell.controlSet?&*cell.controlSet:nullptr);
                const int count=set?(int)set->bindings.size():0;
                label(g,column.controlLocked?"DEFAULT / LOCKED":count?"CONTROL SET":"+ Control Set",
                      {x+8,y+6,w-16,18},11,count?tone:palette::muted,true);
                if (count) label(g,juce::String(count)+" mappings",{x+8,y+26,w-16,16},10,palette::muted);
            } else if (loaded) {
                auto playback=owner.engine.getCellPlayback(cell.id);
                if (!owner.engine.connected.load())
                    playback.playing=playback.paused=playback.waiting=false;
                const auto name=cell.name.isNotEmpty()?cell.name:cell.midi?"MIDI":cell.osc?"OSC":
                    juce::File(cell.file).getFileNameWithoutExtension();
                label(g,name,{x+9,y+4,w-53,21},12,palette::text,true);
                const auto seconds=owner.clipDuration(r,c);
                label(g,playback.waiting?"WAITING":timeText(playback.positionSeconds)+" / "+timeText(seconds),
                     {x+9,y+26,w-59,16},10,tone);
                const auto progress=seconds>0?juce::jlimit(0.,1.,playback.positionSeconds/seconds):0.;
                g.setColour(palette::line);g.fillRect(x+7,y+45,w-14,3);
                g.setColour(playback.waiting||playback.paused?palette::amber:tone);
                g.fillRect((float)x+7,(float)y+45,(float)((w-14)*progress),3.f);
            } else label(g,missing?"! Media missing":column.type==ColumnType::audio?"+ WAV/MP3":
                          column.type==ColumnType::midi?"+ MIDI":"+ OSC",
                          {x+8,y+11,w-16,20},11,missing?palette::red:palette::muted,
                          false,juce::Justification::centred);
            if (autoStopVisibleRows.count(row.id)) {
                const bool active=row.stopOnNext&&cell.stopOnNext;
                g.setColour(active?palette::teal.withAlpha(.27f):palette::amber.withAlpha(.16f));
                g.fillRoundedRectangle((float)x+w-41,(float)y+4,34.f,16.f,3.f);
                label(g,cell.stopOnNext?"AS":"OFF",{x+w-41,y+4,34,16},9,
                      active?palette::teal:palette::amber,true,juce::Justification::centred);
            }
            if (column.type!=ColumnType::control) {
                transportButton(g,{x+w-49,y+25,19,18},false,loaded);
                transportButton(g,{x+w-26,y+25,19,18},true,loaded);
            }
        }
        g.restoreState();
        g.setColour(palette::line.withAlpha(.66f));
        g.drawHorizontalLine(y+rowHeight-1,0,(float)getWidth());
    }
}

void CueGrid::showRowMenu(int row,juce::Point<int> screenPoint) {
    if (row<0 || row>=(int)owner.show.rows.size()) return;
    const int rowId=owner.show.rows[(size_t)row].id;
    auto& model=owner.show;
    juce::PopupMenu menu;
    menu.addItem(1,"Copy cue");
    menu.addItem(2,"Paste cue below",owner.copiedRow.has_value() &&
        owner.copiedRow->cells.size()==model.columns.size());
    menu.addItem(3,"Duplicate cue");
    menu.addSeparator();
    juce::PopupMenu colours;
    const char* labels[]={"Default","Turquoise","Amber","Red","Purple","Blue","Green","Orange"};
    for (int i=0;i<8;++i)
        colours.addItem(100+i,labels[i],true,model.rows[(size_t)row].colour==i);
    menu.addSubMenu("Cue colour",colours);
    menu.addItem(4,"Delete cue");
    auto safe=juce::Component::SafePointer<MainComponent>(&owner);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(
        {screenPoint.x,screenPoint.y,1,1}),[safe,rowId](int item) {
        if (!safe || item==0) return;
        for (int index=0;index<(int)safe->show.rows.size();++index)
            if (safe->show.rows[(size_t)index].id==rowId) {
                if (item==1) safe->copyRow(index);
                else if (item==2) safe->pasteRowAfter(index);
                else if (item==3) safe->duplicateRow(index);
                else if (item==4) safe->deleteRow(index);
                else if (item>=100 && item<=107) safe->setRowColour(index,item-100);
                return;
            }
    });
}
void CueGrid::mouseDown(const juce::MouseEvent& e) {
    dragRow=dragColumn=dropRow=dropColumn=-1;dragging=false;
    const int row=e.y>=headerHeight ? (e.y-headerHeight)/rowHeight : -1;
    const int column=columnAt(e.x);
    if (e.mods.isPopupMenu()) {
        if (row<0 || row>=(int)owner.show.rows.size()) return;
        // Keep cue actions available at the exact pointer position. Cell
        // copy/paste is retained as a submenu rather than replaced.
        if (column>=0 && !owner.show.columns[(size_t)column].collapsed) {
            const int rowId=owner.show.rows[(size_t)row].id;
            const int columnId=owner.show.columns[(size_t)column].id;
            const auto& col=owner.show.columns[(size_t)column];
            const bool locked=col.type==ColumnType::control&&col.controlLocked;
            juce::PopupMenu menu;
            menu.addItem(1,"Cue actions and colour...");
            menu.addSeparator();
            menu.addItem(2,"Copy cell  Ctrl+C",owner.hasClip(row,column)&&!locked);
            menu.addItem(3,"Paste cell  Ctrl+V",owner.copiedCell.has_value()&&
                owner.copiedType==col.type&&!locked);
            if (locked) menu.addItem(4,"Edit default Control Set");
            auto safe=juce::Component::SafePointer<CueGrid>(this);
            const auto screen=e.getScreenPosition();
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(
                {screen.x,screen.y,1,1}),[safe,rowId,columnId,screen](int item) {
                if (!safe || item==0) return;
                auto& main=safe->owner;
                int r=-1,c=-1;
                for (int i=0;i<(int)main.show.rows.size();++i)
                    if (main.show.rows[(size_t)i].id==rowId) r=i;
                for (int i=0;i<(int)main.show.columns.size();++i)
                    if (main.show.columns[(size_t)i].id==columnId) c=i;
                if (r<0 || c<0) return;
                if (item==1) {safe->showRowMenu(r,screen);return;}
                if (!main.select(r,c)) return;
                if (item==2) main.copySelectedCell();
                else if (item==3) main.pasteSelectedCell();
                else if (item==4) main.editControlSet(r,c);
            });
        } else showRowMenu(row,e.getScreenPosition());
        return;
    }
    if (!e.mods.isLeftButtonDown()) return;
    owner.grabKeyboardFocus();
    if (e.x<0 || e.y<0) return;
    if (e.y<headerHeight) {
        if (column<0) return;
        const auto& col=owner.show.columns[(size_t)column];
        const int w=columnWidthAt(column),local=e.x-columnX(column);
        if (col.collapsed) { if (e.getNumberOfClicks()==1) owner.toggleColumnCollapsed(column); return; }
        if (e.getNumberOfClicks()>1) return;
        if (e.y>=10 && e.y<29) {
            if (local>=w-61 && local<w-43) owner.reorderColumn(column,-1);
            else if (local>=w-41 && local<w-23) owner.reorderColumn(column,1);
            else if (local>=w-21 && local<w-3) owner.deleteColumn(column);
            else owner.select(owner.selectedRow,column);
        } else if (e.y>=38 && e.y<59 && local>=w-26)
            owner.toggleColumnCollapsed(column);
        else owner.select(owner.selectedRow,column);
        return;
    }
    if (row<0 || row>=(int)owner.show.rows.size()) return;
    const int localY=(e.y-headerHeight)%rowHeight-rowContentInset;
    if (e.x<nameWidth) {
        if (e.x<70 && e.getNumberOfClicks()>1) return;
        if (localY>=13 && localY<34 && e.x>=nameWidth-34 && e.x<nameWidth-9) {
            if (e.getNumberOfClicks()==1) owner.stopRow(row);
            return;
        }
        if (localY>=13 && localY<33 && e.x>=nameWidth-65 && e.x<nameWidth-40) {
            if (e.getNumberOfClicks()==1) showRowMenu(row,e.getScreenPosition());
            return;
        }
        if (localY>=13 && localY<33 && e.x>=5 && e.x<23) owner.reorderRow(row,-1);
        else if (localY>=13 && localY<33 && e.x>=26 && e.x<44) owner.reorderRow(row,1);
        else if (localY>=13 && localY<33 && e.x>=47 && e.x<65) owner.deleteRow(row);
        else if (e.x>=75 && e.x<118 && localY>=9 && localY<41) owner.triggerRow(row);
        else owner.select(row,owner.selectedColumn);
        return;
    }
    if (column<0 || owner.show.columns[(size_t)column].collapsed) return;
    const int localX=e.x-columnX(column),w=columnWidthAt(column);
    const auto& col=owner.show.columns[(size_t)column];
    if (autoStopVisibleRows.count(owner.show.rows[(size_t)row].id) &&
        localY>=4 && localY<20 && localX>=w-41 && localX<w-7) {
        if (e.getNumberOfClicks()==1)
            owner.setCellAutoStop(row,column,!owner.show.rows[(size_t)row].cells[(size_t)column].stopOnNext);
        return;
    }
    if (col.type!=ColumnType::control && localY>=25 && localY<43) {
        if (localX>=w-49 && localX<w-30) {
            if (e.getNumberOfClicks()==1 && owner.hasClip(row,column)) owner.triggerCellAt(row,column);
            return;
        }
        if (localX>=w-26 && localX<w-7) {
            if (e.getNumberOfClicks()==1 && owner.hasClip(row,column)) owner.stopCellAt(row,column);
            return;
        }
    }
    if (!owner.select(row,column)) return;
    if (col.type==ColumnType::control) {
        if (e.getNumberOfClicks()==1) owner.editControlSet(row,column);
    } else if (owner.hasClip(row,column)) {
        dragRow=row; dragColumn=column;
    }
}
void CueGrid::mouseDrag(const juce::MouseEvent& e) {
    if (dragRow<0 || e.getDistanceFromDragStart()<5) return;
    dragging=true;dropRow=dropColumn=-1;
    const int row=e.y>=headerHeight ? (e.y-headerHeight)/rowHeight : -1;
    const int column=columnAt(e.x);
    if (row>=0 && row<(int)owner.show.rows.size() &&
        column>=0 && !owner.show.columns[(size_t)column].collapsed &&
        owner.show.columns[(size_t)column].type==owner.show.columns[(size_t)dragColumn].type) {
        dropRow=row;dropColumn=column;
    }
    setMouseCursor(dropRow>=0?juce::MouseCursor::DraggingHandCursor:juce::MouseCursor::NormalCursor);
    repaint();
}
void CueGrid::mouseUp(const juce::MouseEvent&) {
    if (dragging && dropRow>=0 && (dragRow!=dropRow || dragColumn!=dropColumn))
        owner.moveClip(dragRow,dragColumn,dropRow,dropColumn);
    dragRow=dragColumn=dropRow=dropColumn=-1;dragging=false;
    setMouseCursor(juce::MouseCursor::NormalCursor);repaint();
}
void CueGrid::mouseDoubleClick(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu() || e.x<0 || e.y<0) return;
    const int column=columnAt(e.x);
    if (e.y<headerHeight) {
        if (column>=0 && !owner.show.columns[(size_t)column].collapsed &&
            e.y>=34 && e.x-columnX(column)<columnWidthAt(column)-28)
            owner.renameTitle(true,column);
        return;
    }
    const int row=(e.y-headerHeight)/rowHeight;
    if (row<0 || row>=(int)owner.show.rows.size()) return;
    if (e.x>=128 && e.x<nameWidth-40) owner.renameTitle(false,row);
    else if (column>=0 && !owner.show.columns[(size_t)column].collapsed) {
        const int localX=e.x-columnX(column),localY=(e.y-headerHeight)%rowHeight-rowContentInset;
        if (localY>=25 && localY<43 && localX>=columnWidthAt(column)-49) return;
        if (owner.select(row,column)) {
            const auto type=owner.show.columns[(size_t)column].type;
            if (type==ColumnType::midi) owner.editMidiClip();
            else if (type==ColumnType::osc) owner.editOscClip();
            else if (type==ColumnType::control) owner.editControlSet(row,column);
            else owner.triggerCell();
        }
    }
}
bool CueGrid::isInterestedInFileDrag(const juce::StringArray& files) {
    return files.size()==1 && (files[0].endsWithIgnoreCase(".wav") ||
        files[0].endsWithIgnoreCase(".mp3") || files[0].endsWithIgnoreCase(".mid") ||
        files[0].endsWithIgnoreCase(".midi"));
}
void CueGrid::filesDropped(const juce::StringArray& files,int x,int y) {
    if (!isInterestedInFileDrag(files) || y<headerHeight) return;
    const int row=(y-headerHeight)/rowHeight,column=columnAt(x);
    if (row<0 || row>=(int)owner.show.rows.size() || column<0 ||
        owner.show.columns[(size_t)column].collapsed) return;
    if (owner.select(row,column)) owner.importFile(juce::File(files[0]));
}
void CueMetricsOverlay::paint(juce::Graphics& g) {
    g.fillAll(palette::raised);
    g.setColour(palette::teal);g.fillRect(0,0,3,getHeight());
    label(g,"BPM",{7,18,50,23},11,palette::text,true);
    label(g,"AUTO STOP",{72,18,76,23},10,palette::text,true);
    const int scroll=owner.viewport.getViewPositionY();
    const int first=std::max(0,(scroll-CueGrid::headerHeight)/CueGrid::rowHeight);
    const int last=std::min((int)owner.show.rows.size(),
        (scroll+getHeight()-CueGrid::headerHeight)/CueGrid::rowHeight+1);
    for (int r=first;r<last;++r) {
        const auto& row=owner.show.rows[(size_t)r];
        const int y=CueGrid::headerHeight+r*CueGrid::rowHeight-scroll;
        g.setColour(r==owner.selectedRow?palette::panel:palette::background.withAlpha(.55f));
        g.fillRect(3,y,getWidth()-3,CueGrid::rowHeight);
        const auto cueTone=CueGrid::cueColour(row.colour);
        if (row.colour>0) {
            g.setColour(cueTone.withAlpha(.14f));g.fillRect(3,y,getWidth()-3,CueGrid::rowHeight);
        }
        label(g,juce::String(row.tempo.front().bpm,1),{7,y+14,61,24},14,palette::text,true);
        g.setColour(row.stopOnNext?palette::teal.withAlpha(.3f):palette::muted.withAlpha(.2f));
        g.fillRoundedRectangle(77.f,(float)y+12,31.f,19.f,9.f);
        g.setColour(row.stopOnNext?palette::teal:palette::muted);
        g.fillEllipse(row.stopOnNext?93.f:80.f,(float)y+15,13.f,13.f);
        label(g,row.stopOnNext?"ON":"OFF",{111,y+11,34,22},10,
              row.stopOnNext?palette::teal:palette::muted,true);
        const bool details=owner.grid.autoStopDetailsVisible(row.id);
        label(g,details?"−":"+",
              {89,y+36,24,20},15,details?palette::amber:palette::muted,true,
              juce::Justification::centred);
        g.setColour(palette::line);
        g.drawHorizontalLine(y+CueGrid::rowHeight-1,0,(float)getWidth());
    }
}
void CueMetricsOverlay::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu()) {
        const int scroll=owner.viewport.getViewPositionY();
        const int row=(e.y+scroll-CueGrid::headerHeight)/CueGrid::rowHeight;
        owner.grid.showRowMenu(row,e.getScreenPosition());
        return;
    }
    if (!e.mods.isLeftButtonDown()) return;
    const int scroll=owner.viewport.getViewPositionY();
    if (e.y+scroll<CueGrid::headerHeight) return;
    const int row=(e.y+scroll-CueGrid::headerHeight)/CueGrid::rowHeight;
    if (row<0 || row>=(int)owner.show.rows.size()) return;
    const int localY=(e.y+scroll-CueGrid::headerHeight)%CueGrid::rowHeight;
    if (e.x<69) owner.editRowBpm(row);
    else if (localY>=32) owner.grid.toggleAutoStopDetails(row);
    else owner.setAutoStop(row,!owner.show.rows[(size_t)row].stopOnNext);
    repaint();
}
void CueMetricsOverlay::mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel) {
    const int x=owner.viewport.getViewPositionX(),y=owner.viewport.getViewPositionY();
    if (e.mods.isShiftDown())
        owner.viewport.setViewPosition(x-(int)(wheel.deltaY*100),y);
    else owner.viewport.setViewPosition(x,y-(int)(wheel.deltaY*100));
    repaint();
}
}
