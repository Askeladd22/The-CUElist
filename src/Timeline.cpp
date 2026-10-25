#include "MainComponent.h"
#include <cmath>

namespace cuelist {
namespace {
constexpr int timelineStart=140, tracksTop=82, trackHeight=42;
juce::String timeText(double seconds) {
    const int whole=(int)seconds;
    return juce::String(whole/60).paddedLeft('0',2)+":"+juce::String(whole%60).paddedLeft('0',2)+"."+juce::String((int)(seconds*10)%10);
}
juce::Rectangle<int> zoomButton(int width,int index) {
    return {width-140+34*index,10,index==2 ? 52 : 28,25};
}
}
Timeline::Timeline(MainComponent& p) : owner(p) {
    addAndMakeVisible(horizontalScroll);
    horizontalScroll.setAutoHide(false);
    horizontalScroll.addListener(this);
}
void Timeline::resized() {
    horizontalScroll.setBounds(timelineStart,std::max(0,getHeight()-43),std::max(0,getWidth()-timelineStart-20),12);
    syncViewport();
}
double Timeline::fullLength() const {
    double length=10;
    if (owner.selectedRow<0 || owner.selectedRow>=(int)owner.show.rows.size()) return length;
    for (const auto& cell : owner.show.rows[owner.selectedRow].cells) {
        const auto clip=owner.clips.find(cell.id);
        if (clip!=owner.clips.end()) length=std::max(length,(cell.startSeconds+clip->second->samples.getNumSamples()/clip->second->sampleRate)*1.1);
    }
    return length;
}
double Timeline::visibleLength() const { return dragColumn>=0 || seeking ? dragLength : fullLength()/zoom; }
void Timeline::syncViewport() {
    const int id=owner.selectedRow>=0 && owner.selectedRow<(int)owner.show.rows.size() ? owner.show.rows[owner.selectedRow].id : -1;
    if (id!=viewportRowId) {
        viewportRowId=id; zoom=1; viewStart=0; trackScroll=0; dragColumn=-1; seeking=false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
    const double length=visibleLength();
    if (dragColumn<0 && !seeking) viewStart=juce::jlimit(0.0,std::max(0.0,fullLength()-length),viewStart);
    horizontalScroll.setRangeLimits(0,std::max(fullLength(),viewStart+length),juce::dontSendNotification);
    horizontalScroll.setCurrentRange(viewStart,length,juce::dontSendNotification);
}
void Timeline::scrollBarMoved(juce::ScrollBar*,double start) {
    syncViewport();
    if (dragColumn>=0 || seeking) return;
    viewStart=juce::jlimit(0.0,std::max(0.0,fullLength()-visibleLength()),start); syncViewport(); repaint();
}
void Timeline::changeZoom(double factor,double anchor) {
    syncViewport();
    if (dragColumn>=0 || seeking) return;
    anchor=juce::jlimit(0.0,1.0,anchor);
    const double point=viewStart+anchor*visibleLength();
    zoom=juce::jlimit(1.0,32.0,zoom*factor);
    viewStart=point-anchor*visibleLength(); syncViewport(); repaint();
}
void Timeline::paint(juce::Graphics& g) {
    syncViewport(); g.fillAll(palette::panel);
    if (owner.selectedRow<0 || owner.selectedRow>=(int)owner.show.rows.size()) return;
    const auto& row=owner.show.rows[owner.selectedRow];
    label(g,"TIMELINE",{16,11,100,23},11,palette::teal,true);
    label(g,row.name+"  /  trascina clip o cursore",{123,11,std::max(0,getWidth()-273),23},12,palette::muted);
    for (int i=0;i<3;++i) {
        const auto box=zoomButton(getWidth(),i);
        g.setColour(palette::raised); g.fillRoundedRectangle(box.toFloat(),3);
        label(g,i==0 ? "-" : i==1 ? "+" : "Fit",box,12,palette::text,true,juce::Justification::centred);
    }
    g.setColour(palette::line); g.drawHorizontalLine(45,0,(float)getWidth());
    const double length=visibleLength(); const int area=std::max(1,getWidth()-timelineStart-20), bottom=getHeight()-48;
    int audioTracks=0; for (const auto& column : owner.show.columns) if (column.type==ColumnType::audio) ++audioTracks;
    trackScroll=juce::jlimit(0,std::max(0,audioTracks*trackHeight-std::max(1,bottom-tracksTop)),trackScroll);
    for (int i=0;i<=4;++i) {
        const int x=timelineStart+area*i/4;
        label(g,timeText(viewStart+length*i/4),{x-10,53,65,18},10,palette::muted);
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
        const float x=(float)(timelineStart+area*(cell.startSeconds-viewStart)/length), width=std::max(4.0f,(float)(area*seconds/length));
        g.saveState(); g.reduceClipRegion(timelineStart,tracksTop,area,std::max(0,bottom-tracksTop));
        g.setColour(palette::teal.withAlpha(c==owner.selectedColumn ? .22f : .12f)); g.fillRoundedRectangle(x,(float)y,width,31,4);
        g.setColour(palette::teal.withAlpha(.6f)); g.drawRoundedRectangle(x,(float)y,width,31,4,1);
        g.setColour(palette::teal.withAlpha(.4f));
        if (audio.getNumSamples()>0 && audio.getNumChannels()>0) for (int px=std::max(0,(int)(timelineStart-x));px<(int)width;px+=3) {
            if (x+px>getWidth()-20) break;
            const int sample=juce::jlimit(0,audio.getNumSamples()-1,(int)((double)px/width*audio.getNumSamples()));
            const float amp=std::min(1.0f,std::abs(audio.getSample(0,sample)))*24;
            g.fillRect(x+px,(float)y+15-amp/2,1.0f,std::max(1.0f,amp));
        }
        label(g,cell.name+"  +"+juce::String(cell.startSeconds,2)+"s",{(int)x+8,y+5,std::max(1,(int)width-16),20},11,palette::text);
        g.restoreState();
    }
    g.restoreState();
    if (loaded==0) label(g,"Importa WAV o MP3 per visualizzare le clip.",{timelineStart,83,getWidth()-160,32},12,palette::muted);
    if (owner.engine.monitoredRowId.load()==row.id) {
        const auto seconds=owner.engine.playheadSeconds.load(); const auto x=(float)(timelineStart+area*(seconds-viewStart)/length);
        if (x>=timelineStart && x<=getWidth()-20) {
            g.setColour(palette::amber); g.drawLine(x,72,x,(float)bottom,1.5f);
            juce::Path marker; marker.addTriangle(x-5,71,x+5,71,x,78); g.fillPath(marker);
        }
    }
    label(g,juce::String(row.tempo.front().bpm,1)+" BPM  /  rotella: tracce  /  Ctrl+rotella: zoom  /  "+juce::String(zoom,1)+"x",
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
    syncViewport();
    owner.grabKeyboardFocus(); dragColumn=-1; seeking=false;
    for (int i=0;i<3;++i) if (zoomButton(getWidth(),i).contains(e.getPosition())) {
        if (i==2) { zoom=1; viewStart=0; syncViewport(); repaint(); }
        else changeZoom(i==0 ? .5 : 2,.5);
        return;
    }
    if (owner.selectedRow<0 || owner.selectedRow>=(int)owner.show.rows.size() || e.x<timelineStart || e.x>getWidth()-20) return;
    const int area=std::max(1,getWidth()-timelineStart-20);
    dragLength=visibleLength(); dragViewStart=viewStart; dragPixels=area; dragRowId=owner.show.rows[owner.selectedRow].id;
    if (e.y>=46 && e.y<tracksTop) {
        seeking=true; owner.seekTimeline(juce::jlimit(0.0,86400.0,dragViewStart+(e.x-timelineStart)*dragLength/area)); return;
    }
    if (e.y<tracksTop || e.y>=getHeight()-48 || (e.y-tracksTop+trackScroll)%trackHeight>=32) return;
    const int wanted=(e.y-tracksTop+trackScroll)/trackHeight; int track=0;
    const auto& row=owner.show.rows[owner.selectedRow];
    for (int c=0;c<(int)row.cells.size();++c) {
        if (owner.show.columns[c].type!=ColumnType::audio || track++!=wanted) continue;
        const auto& cell=row.cells[c]; const auto found=owner.clips.find(cell.id); if (found==owner.clips.end()) return;
        const double position=dragViewStart+(e.x-timelineStart)*dragLength/area, end=cell.startSeconds+found->second->samples.getNumSamples()/found->second->sampleRate;
        const auto rowId=row.id; const auto start=cell.startSeconds;
        if (position<start || position>end || !owner.select(owner.selectedRow,c)) return;
        dragColumn=c; dragRowId=rowId; dragStartSeconds=start;
        setMouseCursor(juce::MouseCursor::DraggingHandCursor); return;
    }
}
void Timeline::mouseDrag(const juce::MouseEvent& e) {
    syncViewport();
    if (owner.selectedRow<0 || owner.selectedRow>=(int)owner.show.rows.size() || owner.show.rows[owner.selectedRow].id!=dragRowId) return;
    if (seeking) owner.seekTimeline(juce::jlimit(0.0,86400.0,dragViewStart+juce::jlimit(0.0,dragPixels,(double)e.x-timelineStart)*dragLength/dragPixels));
    else if (dragColumn>=0 && dragColumn<(int)owner.show.rows[owner.selectedRow].cells.size())
        owner.setClipStart(dragColumn,juce::jlimit(0.0,86400.0,dragStartSeconds+e.getDistanceFromDragStartX()*dragLength/dragPixels));
    repaint();
}
void Timeline::mouseUp(const juce::MouseEvent&) { dragColumn=-1; seeking=false; syncViewport(); setMouseCursor(juce::MouseCursor::NormalCursor); repaint(); }
void Timeline::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    syncViewport();
    if (e.mods.isCtrlDown()) {
        changeZoom(std::pow(2.0,wheel.deltaY*3),(double)(e.x-timelineStart)/std::max(1,getWidth()-timelineStart-20)); return;
    }
    int tracks=0; for (const auto& column : owner.show.columns) if (column.type==ColumnType::audio) ++tracks;
    const int maxScroll=std::max(0,tracks*trackHeight-std::max(1,getHeight()-48-tracksTop));
    trackScroll=juce::jlimit(0,maxScroll,trackScroll-(int)std::round(wheel.deltaY*trackHeight*3)); repaint();
}
}

