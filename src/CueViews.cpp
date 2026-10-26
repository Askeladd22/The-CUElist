#include "MainComponent.h"
#include <cmath>

namespace cuelist {
namespace {
juce::String columnTypeName(ColumnType type) { return type==ColumnType::audio ? "AUDIO" : type==ColumnType::midi ? "MIDI" : "OSC"; }
juce::String timeText(double seconds) {
    const auto whole=(int)seconds;
    return juce::String(whole/60).paddedLeft('0',2)+":"+juce::String(whole%60).paddedLeft('0',2);
}
void smallButton(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> box, bool enabled, bool danger=false) {
    const auto colour=danger ? palette::red : palette::muted;
    g.setColour(palette::raised.withAlpha(enabled ? 1.0f : .4f)); g.fillRoundedRectangle(box.toFloat(),3);
    label(g,text,box,11,colour.withAlpha(enabled ? 1.0f : .25f),true,juce::Justification::centred);
}
void transportButton(juce::Graphics& g,juce::Rectangle<int> box,bool stop,bool enabled) {
    g.setColour(palette::raised); g.fillRoundedRectangle(box.toFloat(),3);
    g.setColour((stop ? palette::red : palette::teal).withAlpha(enabled ? 1.0f : .25f));
    const float x=(float)box.getCentreX(),y=(float)box.getCentreY();
    if (stop) g.fillRect(x-4,y-4,8.0f,8.0f);
    else { juce::Path triangle; triangle.addTriangle(x-3,y-5,x-3,y+5,x+5,y); g.fillPath(triangle); }
}
}

void CueGrid::paint(juce::Graphics& g) {
    g.fillAll(palette::panel); g.setColour(palette::raised); g.fillRect(0,0,getWidth(),headerHeight);
    label(g,"CUE",{14,10,80,18},10,palette::muted,true);
    label(g,"Ordine / GO / nome",{14,35,nameWidth-25,22},12,palette::text);
    const auto& show=owner.show;
    for (int c=0;c<(int)show.columns.size();++c) {
        const int x=nameWidth+c*columnWidth; const auto colour=palette::type((int)show.columns[c].type);
        g.setColour(colour); g.fillRect(x,0,columnWidth-1,3);
        label(g,columnTypeName(show.columns[c].type),{x+10,11,65,20},10,colour,true);
        smallButton(g,"<",{x+88,11,20,20},c>0);
        smallButton(g,">",{x+112,11,20,20},c+1<(int)show.columns.size());
        smallButton(g,"x",{x+136,11,20,20},true,true);
        label(g,show.columns[c].name.isEmpty() ? "Doppio click: nome" : show.columns[c].name,{x+10,36,columnWidth-20,22},12,
              show.columns[c].name.isEmpty() ? palette::muted : palette::text,true);
        g.setColour(palette::line); g.drawVerticalLine(x,0,(float)getHeight());
    }
    const int tail=nameWidth+(int)show.columns.size()*columnWidth;
    label(g,"BPM",{tail+12,31,70,24},11,palette::muted,true); label(g,"METRICA",{tail+88,31,80,24},11,palette::muted,true);
    label(g,"AUTO STOP",{tail+178,31,96,24},10,palette::muted,true);
    const auto clip=g.getClipBounds();
    const int first=std::max(0,(clip.getY()-headerHeight)/rowHeight), last=std::min((int)show.rows.size(),(clip.getBottom()-headerHeight)/rowHeight+1);
    for (int r=first;r<last;++r) {
        const int y=headerHeight+r*rowHeight; const bool selected=r==owner.selectedRow, next=r==owner.nextCueIndex();
        const auto& row=show.rows[r];
        g.setColour(selected ? palette::teal.withAlpha(.07f) : r%2 ? palette::background.withAlpha(.4f) : palette::panel);
        g.fillRect(0,y,getWidth(),rowHeight);
        if (selected || next) { g.setColour(next ? palette::amber : palette::teal); g.fillRect(0,y,3,rowHeight); }
        smallButton(g,"^",{6,y+17,20,21},r>0); smallButton(g,"v",{30,y+17,20,21},r+1<(int)show.rows.size());
        smallButton(g,"x",{54,y+17,20,21},true,true);
        g.setColour((next ? palette::amber : palette::teal).withAlpha(.14f)); g.fillRoundedRectangle(84.0f,(float)y+12,44,30,4);
        label(g,"GO",{84,y+12,44,30},11,next ? palette::amber : palette::teal,true,juce::Justification::centred);
        label(g,row.name,{137,y+7,nameWidth-181,25},13,palette::text,selected);
        label(g,juce::String(r+1).paddedLeft('0',2)+(next ? "  PROSSIMA" : row.muted ? "  MUTE" : ""),{137,y+32,nameWidth-181,14},9,next ? palette::amber : palette::muted);
        transportButton(g,{nameWidth-36,y+17,26,22},true,true);
        for (int c=0;c<(int)show.columns.size();++c) {
            const int x=nameWidth+c*columnWidth; const auto& cell=row.cells[c];
            const bool loaded=owner.hasClip(r,c), missing=cell.file.isNotEmpty() && !loaded;
            const auto colour=missing ? palette::red : palette::type((int)show.columns[c].type);
            const auto box=juce::Rectangle<float>((float)x+5,(float)y+6,(float)columnWidth-10,(float)rowHeight-12);
            g.setColour(loaded ? colour.withAlpha(.15f) : palette::background.withAlpha(.5f)); g.fillRoundedRectangle(box,5);
            const bool drop=dragging && dropRow==r && dropColumn==c;
            g.setColour(drop ? palette::amber : selected && c==owner.selectedColumn ? colour : palette::line.withAlpha(.65f));
            g.drawRoundedRectangle(box,5,drop || (selected && c==owner.selectedColumn) ? 1.5f : .6f);
            if (loaded) {
                auto playback=owner.engine.getCellPlayback(cell.id);
                if (!owner.engine.connected.load()) playback.playing=playback.waiting=playback.paused=false;
                label(g,cell.name.isEmpty() ? (cell.midi ? "MIDI" : cell.osc ? "OSC" : juce::File(cell.file).getFileNameWithoutExtension()) : cell.name,{x+13,y+10,columnWidth-40,22},12,palette::text);
                const auto seconds=owner.clipDuration(r,c);
                label(g,playback.waiting ? "ATTESA" : timeText(playback.positionSeconds)+" / "+timeText(seconds),
                      {x+13,y+30,columnWidth-72,14},9,colour);
                const auto progress=seconds>0 ? juce::jlimit(0.0,1.0,playback.positionSeconds/seconds) : 0;
                g.setColour(palette::line); g.fillRect(x+8,y+rowHeight-6,columnWidth-16,2);
                g.setColour(playback.paused || playback.waiting ? palette::amber : colour);
                g.fillRect((float)x+8,(float)y+rowHeight-6,(float)((columnWidth-16)*progress),2.0f);
                if (playback.playing || playback.paused) g.fillEllipse((float)x+columnWidth-19,(float)y+13,6,6);
            } else label(g,missing ? "! File non disponibile" : show.columns[c].type==ColumnType::audio ? "+ WAV / MP3" : show.columns[c].type==ColumnType::midi ? "+ MIDI / Piano roll" : "+ OSC / messaggi",
                         {x+10,y+8,columnWidth-20,18},11,missing ? palette::red : palette::muted,false,juce::Justification::centred);
            transportButton(g,{x+columnWidth-54,y+28,21,18},false,loaded);
            transportButton(g,{x+columnWidth-29,y+28,21,18},true,loaded);
        }
        const auto tempo=row.tempo.front();
        label(g,juce::String(tempo.bpm,1),{tail+12,y+14,70,24},12,palette::muted);
        label(g,juce::String(tempo.numerator)+"/"+juce::String(tempo.denominator),{tail+88,y+14,70,24},12,palette::muted);
        g.setColour(row.stopOnNext ? palette::teal.withAlpha(.25f) : palette::raised);
        g.fillRoundedRectangle((float)tail+188,(float)y+18,34,19,9.5f);
        g.setColour(row.stopOnNext ? palette::teal : palette::muted);
        g.fillEllipse((float)tail+(row.stopOnNext ? 207 : 191),(float)y+21,13,13);
        label(g,row.stopOnNext ? "ON" : "OFF",{tail+228,y+17,38,20},10,row.stopOnNext ? palette::teal : palette::muted);
        g.setColour(palette::line.withAlpha(.6f)); g.drawHorizontalLine(y+rowHeight-1,0,(float)getWidth());
    }
}

void CueGrid::mouseDown(const juce::MouseEvent& e) {
    dragRow=dragColumn=dropRow=dropColumn=-1; dragging=false;
    if (!e.mods.isLeftButtonDown()) return;
    owner.grabKeyboardFocus();
    if (e.y<0 || e.x<0) return;
    const int column=(e.x-nameWidth)/columnWidth;
    if (e.y<headerHeight) {
        if (e.x<nameWidth || column>=(int)owner.show.columns.size()) return;
        const int local=e.x-nameWidth-column*columnWidth;
        if (e.y>=11 && e.y<31) {
            if (e.getNumberOfClicks()>1) return;
            if (local>=88 && local<108) owner.reorderColumn(column,-1);
            else if (local>=112 && local<132) owner.reorderColumn(column,1);
            else if (local>=136 && local<156) owner.deleteColumn(column);
        } else owner.select(owner.selectedRow,column);
        return;
    }
    const int row=(e.y-headerHeight)/rowHeight, localY=(e.y-headerHeight)%rowHeight;
    if (row>=(int)owner.show.rows.size()) return;
    const int tail=nameWidth+(int)owner.show.columns.size()*columnWidth;
    if (e.x>=tail+178 && e.x<tail+274 && localY>=10 && localY<44) {
        if (e.getNumberOfClicks()==1) owner.setAutoStop(row,!owner.show.rows[row].stopOnNext);
        return;
    }
    if (e.x>=nameWidth && column<(int)owner.show.columns.size() && localY>=28 && localY<46) {
        const int localX=(e.x-nameWidth)%columnWidth;
        if ((localX>=columnWidth-54 && localX<columnWidth-33) || (localX>=columnWidth-29 && localX<columnWidth-8)) {
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
        if (e.x<137 && e.getNumberOfClicks()>1) return;
        if (localY>=17 && localY<38 && e.x>=6 && e.x<26) owner.reorderRow(row,-1);
        else if (localY>=17 && localY<38 && e.x>=30 && e.x<50) owner.reorderRow(row,1);
        else if (localY>=17 && localY<38 && e.x>=54 && e.x<74) owner.deleteRow(row);
        else if (e.x>=84 && e.x<128 && localY>=12 && localY<42) owner.triggerRow(row);
        else owner.select(row,owner.selectedColumn);
    } else if (column<(int)owner.show.columns.size() && owner.select(row,column) && owner.hasClip(row,column)) {
        dragRow=row; dragColumn=column;
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
