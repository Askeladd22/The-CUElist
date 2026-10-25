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
constexpr int timelineStart=140, tracksTop=82, trackHeight=42;
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
        label(g,row.name,{137,y+7,nameWidth-145,25},13,palette::text,selected);
        label(g,juce::String(r+1).paddedLeft('0',2)+(next ? "  PROSSIMA" : row.muted ? "  MUTE" : ""),{137,y+32,nameWidth-145,14},9,next ? palette::amber : palette::muted);
        for (int c=0;c<(int)show.columns.size();++c) {
            const int x=nameWidth+c*columnWidth; const auto& cell=row.cells[c]; const auto found=owner.clips.find(cell.id);
            const bool loaded=found!=owner.clips.end(), missing=cell.file.isNotEmpty() && !loaded;
            const auto colour=missing ? palette::red : palette::type((int)show.columns[c].type);
            const auto box=juce::Rectangle<float>((float)x+5,(float)y+6,(float)columnWidth-10,(float)rowHeight-12);
            g.setColour(loaded ? colour.withAlpha(.15f) : palette::background.withAlpha(.5f)); g.fillRoundedRectangle(box,5);
            const bool drop=dragging && dropRow==r && dropColumn==c;
            g.setColour(drop ? palette::amber : selected && c==owner.selectedColumn ? colour : palette::line.withAlpha(.65f));
            g.drawRoundedRectangle(box,5,drop || (selected && c==owner.selectedColumn) ? 1.5f : .6f);
            if (loaded) {
                label(g,cell.name.isEmpty() ? juce::File(cell.file).getFileNameWithoutExtension() : cell.name,{x+13,y+10,columnWidth-26,22},12,palette::text);
                const auto seconds=found->second->samples.getNumSamples()/found->second->sampleRate;
                label(g,(cell.loop ? "LOOP  " : "")+timeText(seconds)+(cell.startSeconds>0 ? "  +"+juce::String(cell.startSeconds,1)+"s" : ""),
                      {x+13,y+30,columnWidth-26,14},9,colour);
            } else label(g,missing ? "! File non disponibile" : show.columns[c].type==ColumnType::audio ? "+ WAV / MP3" : "previsto",
                         {x+10,y+15,columnWidth-20,24},11,missing ? palette::red : palette::muted,false,juce::Justification::centred);
        }
        const auto tempo=row.tempo.front();
        label(g,juce::String(tempo.bpm,1),{tail+12,y+14,70,24},12,palette::muted);
        label(g,juce::String(tempo.numerator)+"/"+juce::String(tempo.denominator),{tail+88,y+14,70,24},12,palette::muted);
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
    if (e.x<nameWidth) {
        if (e.x<137 && e.getNumberOfClicks()>1) return;
        if (localY>=17 && localY<38 && e.x>=6 && e.x<26) owner.reorderRow(row,-1);
        else if (localY>=17 && localY<38 && e.x>=30 && e.x<50) owner.reorderRow(row,1);
        else if (localY>=17 && localY<38 && e.x>=54 && e.x<74) owner.deleteRow(row);
        else if (e.x>=84 && e.x<128 && localY>=12 && localY<42) owner.triggerRow(row);
        else owner.select(row,owner.selectedColumn);
    } else if (column<(int)owner.show.columns.size() && owner.select(row,column) && owner.clips.count(owner.show.rows[row].cells[column].id)) {
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
    if (e.x>=137 && e.x<nameWidth) owner.renameTitle(false,row);
    else if (e.x>=nameWidth) {
        const int col=(e.x-nameWidth)/columnWidth;
        if (col<(int)owner.show.columns.size() && owner.select(row,col)) owner.triggerCell();
    }
}
bool CueGrid::isInterestedInFileDrag(const juce::StringArray& files) {
    return files.size()==1 && (files[0].endsWithIgnoreCase(".wav") || files[0].endsWithIgnoreCase(".mp3"));
}
void CueGrid::filesDropped(const juce::StringArray& files,int x,int y) {
    if (!isInterestedInFileDrag(files) || x<nameWidth || y<headerHeight) return;
    const int row=(y-headerHeight)/rowHeight, column=(x-nameWidth)/columnWidth;
    if (row>=(int)owner.show.rows.size() || column>=(int)owner.show.columns.size()) return;
    if (owner.select(row,column)) owner.importFile(juce::File(files[0]));
}

double Timeline::visibleLength() const {
    if (dragColumn>=0 || seeking) return dragLength;
    double length=10;
    for (const auto& cell : owner.show.rows[owner.selectedRow].cells) {
        const auto clip=owner.clips.find(cell.id);
        if (clip!=owner.clips.end()) length=std::max(length,(cell.startSeconds+clip->second->samples.getNumSamples()/clip->second->sampleRate)*1.1);
    }
    return length;
}
void Timeline::paint(juce::Graphics& g) {
    g.fillAll(palette::panel); const auto& row=owner.show.rows[owner.selectedRow];
    label(g,"TIMELINE",{16,11,100,23},11,palette::teal,true);
    label(g,row.name+"  /  trascina clip o cursore",{123,11,getWidth()-140,23},12,palette::muted);
    g.setColour(palette::line); g.drawHorizontalLine(45,0,(float)getWidth());
    const double length=visibleLength(); const int area=std::max(1,getWidth()-timelineStart-20), bottom=getHeight()-34;
    int audioTracks=0; for (const auto& column : owner.show.columns) if (column.type==ColumnType::audio) ++audioTracks;
    trackScroll=juce::jlimit(0,std::max(0,audioTracks*trackHeight-std::max(1,bottom-tracksTop)),trackScroll);
    for (int i=0;i<=4;++i) {
        const int x=timelineStart+area*i/4;
        label(g,timeText(length*i/4),{x-10,53,55,18},10,palette::muted);
        g.setColour(palette::line.withAlpha(.6f)); g.drawVerticalLine(x,76,(float)bottom);
    }
    int track=0, loaded=0;
    g.saveState(); g.reduceClipRegion(0,tracksTop,getWidth(),std::max(0,bottom-tracksTop));
    for (int c=0;c<(int)row.cells.size();++c) {
        if (owner.show.columns[c].type!=ColumnType::audio) continue;
        const int y=tracksTop+track++*trackHeight-trackScroll;
        const auto found=owner.clips.find(row.cells[c].id); if (found!=owner.clips.end()) ++loaded;
        if (y+32<tracksTop || y>=bottom) continue;
        const auto& name=owner.show.columns[c].name;
        label(g,name.isEmpty() ? "Audio "+juce::String(c+1) : name,{16,y,115,30},12,c==owner.selectedColumn ? palette::text : palette::muted);
        if (found==owner.clips.end()) continue;
        const auto& cell=row.cells[c]; const auto& audio=found->second->samples;
        const double seconds=audio.getNumSamples()/found->second->sampleRate;
        const float x=(float)(timelineStart+area*cell.startSeconds/length), width=std::max(4.0f,(float)(area*seconds/length));
        g.setColour(palette::teal.withAlpha(c==owner.selectedColumn ? .22f : .12f)); g.fillRoundedRectangle(x,(float)y,width,31,4);
        g.setColour(palette::teal.withAlpha(.6f)); g.drawRoundedRectangle(x,(float)y,width,31,4,1);
        g.setColour(palette::teal.withAlpha(.4f));
        if (audio.getNumSamples()>0 && audio.getNumChannels()>0) for (int px=0;px<(int)width;px+=3) {
            if (x+px>getWidth()) break;
            const int sample=juce::jlimit(0,audio.getNumSamples()-1,(int)((double)px/width*audio.getNumSamples()));
            const float amp=std::min(1.0f,std::abs(audio.getSample(0,sample)))*24;
            g.fillRect(x+px,(float)y+15-amp/2,1.0f,std::max(1.0f,amp));
        }
        label(g,cell.name+"  +"+juce::String(cell.startSeconds,2)+"s",{(int)x+8,y+5,std::max(1,(int)width-16),20},11,palette::text);
    }
    g.restoreState();
    if (loaded==0) label(g,"Importa WAV o MP3 per visualizzare le clip.",{timelineStart,83,getWidth()-160,32},12,palette::muted);
    if (owner.engine.monitoredRowId.load()==row.id) {
        const auto seconds=owner.engine.playheadSeconds.load(); const auto x=(float)(timelineStart+area*seconds/length);
        if (x>=timelineStart && x<=getWidth()-20) {
            g.setColour(palette::amber); g.drawLine(x,72,x,(float)bottom,1.5f);
            juce::Path marker; marker.addTriangle(x-5,71,x+5,71,x,78); g.fillPath(marker);
        }
    }
    label(g,juce::String(row.tempo.front().bpm,1)+" BPM  /  "+juce::String((int)row.tempo.size())+" punti tempo  /  rotella: scorri tracce",
          {16,getHeight()-27,getWidth()-30,20},10,palette::muted);
    const int available=std::max(1,bottom-tracksTop), total=track*trackHeight;
    if (total>available) {
        const float thumb=std::max(12.0f,(float)available*available/total);
        const float y=tracksTop+(available-thumb)*trackScroll/std::max(1,total-available);
        g.setColour(palette::muted.withAlpha(.45f)); g.fillRoundedRectangle((float)getWidth()-7,y,3,thumb,1.5f);
    }
}
void Timeline::mouseDown(const juce::MouseEvent& e) {
    if (!e.mods.isLeftButtonDown()) return;
    owner.grabKeyboardFocus(); dragColumn=-1; seeking=false; dragLength=visibleLength();
    if (e.x<timelineStart) return;
    const int area=std::max(1,getWidth()-timelineStart-20);
    if (e.y>=46 && e.y<tracksTop) {
        seeking=true; owner.seekTimeline(juce::jlimit(0.0,86400.0,(e.x-timelineStart)*dragLength/area)); return;
    }
    if (e.y<tracksTop || e.y>=getHeight()-34) return;
    const int wanted=(e.y-tracksTop+trackScroll)/trackHeight; int track=0;
    const auto& row=owner.show.rows[owner.selectedRow];
    for (int c=0;c<(int)row.cells.size();++c) {
        if (owner.show.columns[c].type!=ColumnType::audio || track++!=wanted) continue;
        const auto& cell=row.cells[c]; const auto found=owner.clips.find(cell.id); if (found==owner.clips.end()) return;
        const double position=(e.x-timelineStart)*dragLength/area, end=cell.startSeconds+found->second->samples.getNumSamples()/found->second->sampleRate;
        const auto rowId=row.id; const auto start=cell.startSeconds;
        if (position<start || position>end || !owner.select(owner.selectedRow,c)) return;
        dragColumn=c; dragRowId=rowId; dragStartSeconds=start;
        setMouseCursor(juce::MouseCursor::DraggingHandCursor); return;
    }
}
void Timeline::mouseDrag(const juce::MouseEvent& e) {
    const int area=std::max(1,getWidth()-timelineStart-20);
    if (seeking) owner.seekTimeline(juce::jlimit(0.0,86400.0,(e.x-timelineStart)*dragLength/area));
    else if (dragColumn>=0 && owner.show.rows[owner.selectedRow].id==dragRowId)
        owner.setClipStart(dragColumn,juce::jlimit(0.0,86400.0,dragStartSeconds+e.getDistanceFromDragStartX()*dragLength/area));
    repaint();
}
void Timeline::mouseUp(const juce::MouseEvent&) { dragColumn=-1; seeking=false; setMouseCursor(juce::MouseCursor::NormalCursor); repaint(); }
void Timeline::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) {
    int tracks=0; for (const auto& column : owner.show.columns) if (column.type==ColumnType::audio) ++tracks;
    const int maxScroll=std::max(0,tracks*trackHeight-std::max(1,getHeight()-34-tracksTop));
    trackScroll=juce::jlimit(0,maxScroll,trackScroll-(int)std::round(wheel.deltaY*trackHeight*3)); repaint();
}
}

