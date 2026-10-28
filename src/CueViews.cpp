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
// Draw the loop glyph as vector geometry: never rely on a missing Unicode glyph.
void drawLoopIcon(juce::Graphics& g,juce::Rectangle<int> bounds,juce::Colour colour) {
    // JUCE arc angles are clockwise from 12 o'clock, not the conventional
    // cos(x)/sin(y) coordinate system. The arrow must use the same convention
    // and follow the tangent at the arc endpoint to remain aligned at 13-15 px.
    const auto centre=bounds.toFloat().getCentre();
    const float side=(float)std::min(bounds.getWidth(),bounds.getHeight());
    const float radius=side*0.33f;
    constexpr float from=1.92f;
    const float to=juce::MathConstants<float>::twoPi+0.64f;
    juce::Path circle;
    circle.addCentredArc(centre.x,centre.y,radius,radius,0.0f,from,to,true);
    g.setColour(colour);
    g.strokePath(circle,juce::PathStrokeType(1.55f,juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));

    const juce::Point<float> end{centre.x+radius*std::sin(to),
                                 centre.y-radius*std::cos(to)};
    const juce::Point<float> tangent{std::cos(to),std::sin(to)};
    const juce::Point<float> normal{-tangent.y,tangent.x};
    const float headLength=side*0.26f;
    const float halfWidth=side*0.14f;
    const auto tip=end+tangent*(headLength*0.48f);
    const auto base=end-tangent*(headLength*0.52f);
    juce::Path arrow;
    const auto left=base+normal*halfWidth;
    const auto right=base-normal*halfWidth;
    arrow.addTriangle(tip.x,tip.y,left.x,left.y,right.x,right.y);
    g.fillPath(arrow);
}
void playPauseIcon(juce::Graphics& g,juce::Rectangle<int> box,bool pause) {
    g.setColour(palette::raised); g.fillRoundedRectangle(box.toFloat(),5);
    g.setColour(palette::line); g.drawRoundedRectangle(box.toFloat().reduced(.5f),5,1);
    g.setColour(palette::teal);
    const float cx=(float)box.getCentreX(),cy=(float)box.getCentreY();
    if (pause) {
        g.fillRoundedRectangle(cx-5.5f,cy-5.f,3.5f,10.f,0.7f);
        g.fillRoundedRectangle(cx+2.f,cy-5.f,3.5f,10.f,0.7f);
    } else {
        juce::Path p; p.addTriangle(cx-3.5f,cy-5.5f,cx-3.5f,cy+5.5f,cx+5.f,cy);
        g.fillPath(p);
    }
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
    const auto& show=owner.show;
    const auto activeName=show.cuelists.empty()
        ? juce::String("Cuelist 1")
        : show.cuelists[(size_t)show.activeCuelist].name;
    label(g,activeName,{22,8,nameWidth-114,23},
          13,palette::teal,true);
    label(g,juce::String((int)show.rows.size())+" cues  /  "+
          juce::String(owner.visibleClipCount)+" clips",
          {22,35,nameWidth-40,20},11,palette::muted,true);
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
            smallButton(g,"-",{x+w-26,38,19,20},true);
        }
        g.setColour(palette::line);g.drawVerticalLine(x,0,(float)getHeight());
    }
    // Contextual add-column control follows the rightmost column, including
    // after reordering or when all preceding columns are compressed.
    smallButton(g,"+",{columnX((int)show.columns.size())+6,16,31,31},true);
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
            g.setColour(custom.withAlpha(.28f));
            g.fillRect(0,y,getWidth(),rowHeight);
        }
        if (next||playing||selected||row.colour>0) {
            g.setColour(row.colour>0?custom:playing?palette::teal:next?palette::amber:palette::muted);
            g.fillRect(0,y,5,rowHeight);
        }
        g.saveState();
        g.addTransform(juce::AffineTransform::translation(0.f,(float)rowContentInset));
        smallButton(g,"^",{5,y+6,18,18},r>0);
        smallButton(g,"v",{5,y+27,18,18},r+1<(int)show.rows.size());
        smallButton(g,"x",{27,y+13,18,20},true,true);
        g.setColour(next ? palette::amber : palette::teal);
        g.fillRoundedRectangle(56.f,(float)y+9,42.f,32.f,5.f);
        label(g,"GO",{56,y+9,42,32},13,palette::background,true,juce::Justification::centred);
        label(g,row.name.isEmpty()?"Untitled cue":row.name,
            {107,y+4,nameWidth-213,24},14,palette::text,true);
        const auto state=playing?"PLAYING":paused?"PAUSED":next?"NEXT CUE":row.muted?"MUTED":"STANDBY";
        label(g,juce::String(r+1).paddedLeft('0',2)+" / "+state,
            {107,y+29,nameWidth-213,18},10,playing?palette::teal:next?palette::amber:palette::muted,true);
        playPauseIcon(g,{nameWidth-95,y+13,25,20},playing);
        transportButton(g,{nameWidth-65,y+13,25,20},true,true);
        smallButton(g,"...",{nameWidth-34,y+13,25,20},true);
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
                else label(g,"-",{x+8,y+13,w-16,20},12,palette::muted,true,juce::Justification::centred);
                if(!cell.followsRow) {g.setColour(palette::red);g.fillEllipse((float)x+6,(float)y+8,3.f,3.f);}
                if(cell.loop) drawLoopIcon(g,{x+w-19,y+8,13,15},palette::amber);
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
            if(!cell.followsRow) {
                g.setColour(palette::red);g.fillEllipse((float)x+7,(float)y+6,3.f,3.f);
            }
            if(cell.loop)
                drawLoopIcon(g,{autoStopVisibleRows.count(row.id) ? x+w-59 : x+w-19,
                    y+5,15,15},palette::amber);
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
    // The add-cue button is part of the scrolled grid and is always last.
    smallButton(g,"+",{nameWidth/2-18,headerHeight+(int)show.rows.size()*rowHeight+7,36,29},true);
}

void CueGrid::showCellMenu(int row,int column,juce::Point<int> screenPoint) {
    if(row<0 || row>=(int)owner.show.rows.size() ||
       column<0 || column>=(int)owner.show.columns.size() ||
       owner.show.columns[(size_t)column].collapsed) return;
    const auto& col=owner.show.columns[(size_t)column];
    const bool locked=col.type==ColumnType::control && col.controlLocked;
    const auto& cell=owner.show.rows[(size_t)row].cells[(size_t)column];
    const int rowId=owner.show.rows[(size_t)row].id;
    const int columnId=col.id;
    juce::PopupMenu menu;
    menu.setLookAndFeel(&getLookAndFeel());
    menu.addItem(1,"Delete cell  Backspace",!locked);
    menu.addItem(2,"Cut  Ctrl+X",!locked);
    menu.addItem(3,"Copy  Ctrl+C",!locked);
    menu.addItem(4,"Paste  Ctrl+V",!locked && owner.copiedCell.has_value() &&
                 owner.copiedType==col.type);
    menu.addSeparator();
    menu.addItem(5,"Loop Playback",!locked,cell.loop);
    menu.addItem(6,"Include in Row GO",!locked,cell.followsRow);
    auto safe=juce::Component::SafePointer<MainComponent>(&owner);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(
        {screenPoint.x,screenPoint.y,1,1}),[safe,rowId,columnId](int choice) {
        if(!safe || choice==0) return;
        int r=-1,c=-1;
        for(int i=0;i<(int)safe->show.rows.size();++i)
            if(safe->show.rows[(size_t)i].id==rowId) r=i;
        for(int i=0;i<(int)safe->show.columns.size();++i)
            if(safe->show.columns[(size_t)i].id==columnId) c=i;
        if(r<0 || c<0 || !safe->select(r,c)) return;
        switch(choice) {
            case 1:safe->deleteSelectedCell();break;
            case 2:safe->cutSelectedCell();break;
            case 3:safe->copySelectedCell();break;
            case 4:safe->pasteSelectedCell();break;
            case 5:safe->setCellLoop(r,c,!safe->show.rows[(size_t)r].cells[(size_t)c].loop);break;
            case 6:safe->setCellFollowsRow(r,c,!safe->show.rows[(size_t)r].cells[(size_t)c].followsRow);break;
        }
    });
}
void CueGrid::showRowMenu(int row,juce::Point<int> screenPoint,int /*cellColumn*/) {
    if (row<0 || row>=(int)owner.show.rows.size()) return;
    const int rowId=owner.show.rows[(size_t)row].id;
    const auto& model=owner.show;

    juce::PopupMenu menu;
    menu.setLookAndFeel(&getLookAndFeel());
    menu.addItem(1,"Copy cue");
    menu.addItem(2,"Paste cue below",owner.copiedRow.has_value() &&
        owner.copiedRow->cells.size()==model.columns.size());
    menu.addItem(3,"Duplicate cue");
    menu.addSeparator();
    juce::PopupMenu colours;
    colours.setLookAndFeel(&getLookAndFeel());
    const char* labels[]={"Default","Turquoise","Amber","Red","Purple","Blue","Green","Orange"};
    for (int i=0;i<8;++i)
        colours.addItem(100+i,labels[i],true,model.rows[(size_t)row].colour==i);
    menu.addSubMenu("Cue colour",colours);
    menu.addSeparator();
    menu.addItem(4,"Delete cue");
    auto safe=juce::Component::SafePointer<MainComponent>(&owner);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(
        {screenPoint.x,screenPoint.y,1,1}),[safe,rowId](int item) {
        if (!safe || item==0) return;
        int r=-1;
        for (int i=0;i<(int)safe->show.rows.size();++i)
            if (safe->show.rows[(size_t)i].id==rowId) r=i;
        if (r<0) return;
        if (item==1) safe->copyRow(r);
        else if (item==2) safe->pasteRowAfter(r);
        else if (item==3) safe->duplicateRow(r);
        else if (item==4) safe->deleteRow(r);
        else if (item>=100 && item<=107) safe->setRowColour(r,item-100);
    });
}
void CueGrid::clickRowHeader(int row,int x,int localY,int clicks,juce::Point<int> screen) {
    if (row<0 || row>=(int)owner.show.rows.size()) return;
        if (x<50 && clicks>1) return;
        if (localY>=13 && localY<34 && x>=nameWidth-34 && x<nameWidth-9) {
            if (clicks==1) showRowMenu(row,screen);
            return;
        }
        if (localY>=13 && localY<33 && x>=nameWidth-65 && x<nameWidth-40) {
            if (clicks==1) owner.stopRow(row);
            return;
        }
        if (localY>=13 && localY<33 && x>=nameWidth-95 && x<nameWidth-70) {
            if (clicks==1) owner.toggleRowPlayPause(row);
            return;
        }
        if (localY>=6 && localY<24 && x>=5 && x<23) owner.reorderRow(row,-1);
        else if (localY>=27 && localY<45 && x>=5 && x<23) owner.reorderRow(row,1);
        else if (localY>=13 && localY<33 && x>=27 && x<45) owner.deleteRow(row);
        else if (x>=56 && x<98 && localY>=9 && localY<41) owner.triggerRow(row);
        else owner.select(row,owner.selectedColumn);
        return;
}
void CueGrid::mouseDown(const juce::MouseEvent& e) {
    dragRow=dragColumn=dropRow=dropColumn=-1;dragging=false;
    const int row=e.y>=headerHeight ? (e.y-headerHeight)/rowHeight : -1;
    const int column=columnAt(e.x);
    if (e.mods.isPopupMenu()) {
        if (row>=0 && row<(int)owner.show.rows.size()) {
            if(column>=0) showCellMenu(row,column,e.getScreenPosition());
            else showRowMenu(row,e.getScreenPosition());
        }
        return;
    }
    if (!e.mods.isLeftButtonDown()) return;
    owner.grabKeyboardFocus();
    if (e.x<0 || e.y<0) return;
    const int addColumnX=columnX((int)owner.show.columns.size())+6;
    if (e.y>=16 && e.y<47 && e.x>=addColumnX && e.x<addColumnX+31) {
        owner.showColumnMenu(e.getScreenPosition());
        return;
    }
    const int addCueY=headerHeight+(int)owner.show.rows.size()*rowHeight+7;
    if (e.y>=addCueY && e.y<addCueY+29 &&
        e.x>=nameWidth/2-18 && e.x<nameWidth/2+18) {
        owner.addCue();
        return;
    }
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
        clickRowHeader(row,e.x,localY,e.getNumberOfClicks(),e.getScreenPosition());
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
        if (column<0 && e.x>=18 && e.x<nameWidth-114 && e.y<33)
            owner.renameActiveCuelist();
        else if (column>=0 && !owner.show.columns[(size_t)column].collapsed &&
            e.y>=34 && e.x-columnX(column)<columnWidthAt(column)-28)
            owner.renameTitle(true,column);
        return;
    }
    const int row=(e.y-headerHeight)/rowHeight;
    if (row<0 || row>=(int)owner.show.rows.size()) return;
    if (e.x>=107 && e.x<nameWidth-100) owner.renameTitle(false,row);
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
void PinnedCueHeaders::paint(juce::Graphics& g) {
    // Use exactly the same vertical scroll offset as the cells. The row
    // identities, colour bands, GO/Pause/Stop and menus remain pixel-aligned.
    g.saveState();
    g.reduceClipRegion(getLocalBounds());
    g.addTransform(juce::AffineTransform::translation(
        0.0f,-(float)owner.viewport.getViewPositionY()));
    owner.grid.paint(g);
    g.restoreState();
    g.setColour(palette::line);g.drawVerticalLine(getWidth()-1,0,(float)getHeight());
}
void PinnedCueHeaders::mouseDown(const juce::MouseEvent& e) {
    const int y=e.y+owner.viewport.getViewPositionY();
    const int row=y>=CueGrid::headerHeight ?
        (y-CueGrid::headerHeight)/CueGrid::rowHeight : -1;
    if (e.mods.isPopupMenu()) {
        if (row>=0 && row<(int)owner.show.rows.size())
            owner.grid.showRowMenu(row,e.getScreenPosition());
        return;
    }
    if (!e.mods.isLeftButtonDown()) return;
    const int addCueY=CueGrid::headerHeight+
        (int)owner.show.rows.size()*CueGrid::rowHeight+7;
    if (y>=addCueY && y<addCueY+29 &&
        e.x>=CueGrid::nameWidth/2-18 && e.x<CueGrid::nameWidth/2+18) {
        owner.addCue(); return;
    }
    if (row<0 || row>=(int)owner.show.rows.size()) return;
    owner.grabKeyboardFocus();
    owner.grid.clickRowHeader(row,e.x,
        (y-CueGrid::headerHeight)%CueGrid::rowHeight-CueGrid::rowContentInset,
        e.getNumberOfClicks(),e.getScreenPosition());
}
void PinnedCueHeaders::mouseDoubleClick(const juce::MouseEvent& e) {
    const int y=e.y+owner.viewport.getViewPositionY();
    if (e.mods.isLeftButtonDown() && y>=0 && y<33 &&
        e.x>=18 && e.x<CueGrid::nameWidth-114) {
        owner.renameActiveCuelist();
        return;
    }
    const int row=y>=CueGrid::headerHeight ?
        (y-CueGrid::headerHeight)/CueGrid::rowHeight : -1;
    if (row>=0 && row<(int)owner.show.rows.size() &&
        e.x>=107 && e.x<CueGrid::nameWidth-100)
        owner.renameTitle(false,row);
}
void PinnedCueHeaders::mouseWheelMove(const juce::MouseEvent& e,
                                      const juce::MouseWheelDetails& wheel) {
    const int x=owner.viewport.getViewPositionX();
    const int y=owner.viewport.getViewPositionY();
    if (e.mods.isShiftDown())
        owner.viewport.setViewPosition(x-(int)(wheel.deltaY*100),y);
    else owner.viewport.setViewPosition(x,y-(int)(wheel.deltaY*100));
    repaint();
}
void CueMetricsOverlay::paint(juce::Graphics& g) {
    g.fillAll(palette::raised);
    g.setColour(palette::teal);g.fillRect(0,0,3,getHeight());
    label(g,"BPM",{22,18,61,23},11,palette::text,true);
    label(g,"AUTO STOP",{100,18,78,23},10,palette::text,true);
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
            g.setColour(cueTone.withAlpha(.28f));g.fillRect(3,y,getWidth()-3,CueGrid::rowHeight);
        }
        label(g,juce::String(row.tempo.front().bpm,1),{22,y+14,65,24},14,palette::text,true);
        g.setColour(row.stopOnNext?palette::teal.withAlpha(.3f):palette::muted.withAlpha(.2f));
        g.fillRoundedRectangle(105.f,(float)y+12,31.f,19.f,9.f);
        g.setColour(row.stopOnNext?palette::teal:palette::muted);
        g.fillEllipse(row.stopOnNext?121.f:108.f,(float)y+15,13.f,13.f);
        label(g,row.stopOnNext?"ON":"OFF",{139,y+11,39,22},10,
              row.stopOnNext?palette::teal:palette::muted,true);
        const bool details=owner.grid.autoStopDetailsVisible(row.id);
        label(g,details?"-":"+",
              {117,y+36,24,20},15,details?palette::amber:palette::muted,true,
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
    if (e.x<91) owner.editRowBpm(row);
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
