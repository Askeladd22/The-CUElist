#include "MainComponent.h"
#include <cmath>

namespace cuelist {
namespace {
constexpr int timelineStart=140, tracksTop=82, trackHeight=42;
// Invert the monotonic tempo map to place grid lines in musical time,
 // including cues with multiple BPM changes.
double beatAtSeconds(const std::vector<TempoPoint>& tempo,double seconds) {
    if (seconds<=0) return 0;
    double low=0,high=std::max(1.0,seconds*2);
    while (secondsAtBeat(tempo,high)<seconds && high<1.0e7) high*=2;
    for (int i=0;i<48;++i) {
        const double middle=(low+high)*.5;
        if (secondsAtBeat(tempo,middle)<seconds) low=middle;
        else high=middle;
    }
    return (low+high)*.5;
}
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
    addAndMakeVisible(snapDivision);
    addAndMakeVisible(snapToggle);
    snapDivision.setName("timelineSnapDivision"); snapToggle.setName("timelineSnapToggle");
    snapDivision.addItemList({"1/4","1/8","1/16","1/32"},1);
    snapDivision.setSelectedId(3,juce::dontSendNotification);
    snapDivision.setTooltip("Suddivisione musicale per lo snap delle clip.");
    snapToggle.setTooltip("Attiva o disattiva l'aggancio delle clip alla griglia BPM.");
    snapToggle.onClick=[this] {
        snapEnabled=!snapEnabled;
        snapToggle.setButtonText(snapEnabled ? "Magnete ON" : "Magnete OFF");
        repaint();
    };
    snapDivision.onChange=[this] { repaint(); };
}
void Timeline::resized() {
    horizontalScroll.setBounds(timelineStart,std::max(0,getHeight()-43),std::max(0,getWidth()-timelineStart-20),12);
    snapDivision.setBounds(getWidth()-344,10,96,25);
    snapToggle.setBounds(getWidth()-240,10,94,25);
    syncViewport();
}
double Timeline::snapStepBeats() const {
    return std::pow(.5,std::max(0,snapDivision.getSelectedId()-1));
}
double Timeline::snapSeconds(double seconds) const {
    if (!snapEnabled || owner.selectedRow<0 || owner.selectedRow>=(int)owner.show.rows.size()) return seconds;
    const auto& tempo=owner.show.rows[(size_t)owner.selectedRow].tempo;
    const double step=snapStepBeats();
    return secondsAtBeat(tempo,std::round(beatAtSeconds(tempo,seconds)/step)*step);
}
double Timeline::fullLength() const {
    double length=10;
    if (owner.selectedRow<0 || owner.selectedRow>=(int)owner.show.rows.size()) return length;
    const auto& row=owner.show.rows[owner.selectedRow];
    for (int c=0;c<(int)row.cells.size();++c) if (owner.hasClip(owner.selectedRow,c))
        length=std::max(length,(row.cells[c].startSeconds+owner.clipDuration(owner.selectedRow,c))*1.1);
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
    label(g,"SNAP",{getWidth()-391,11,44,23},10,palette::muted);
    g.setColour(palette::line); g.drawHorizontalLine(45,0,(float)getWidth());
    const double length=visibleLength(); const int area=std::max(1,getWidth()-timelineStart-20), bottom=getHeight()-48;
    int audioTracks=0; for (const auto& column : owner.show.columns) if (column.type!=ColumnType::osc) ++audioTracks;
    trackScroll=juce::jlimit(0,std::max(0,audioTracks*trackHeight-std::max(1,bottom-tracksTop)),trackScroll);
    for (int i=0;i<=4;++i) {
        const int x=timelineStart+area*i/4;
        label(g,timeText(viewStart+length*i/4),{x-10,53,65,18},10,palette::muted);
        g.setColour(palette::line.withAlpha(.6f)); g.drawVerticalLine(x,76,(float)bottom);
    }
    // Render the beat grid across the tracks, using secondsAtBeat for tempo changes.
    const double startBeat=beatAtSeconds(row.tempo,viewStart);
    const double endBeat=beatAtSeconds(row.tempo,viewStart+length);
    const double subdivision=snapStepBeats();
    const int skip=std::max(1,(int)std::ceil((endBeat-startBeat)/subdivision/250.0));
    const double renderStep=subdivision*skip;
    const int firstGrid=std::max(0,(int)std::ceil(startBeat/renderStep));
    const int lastGrid=(int)std::floor(endBeat/renderStep);
    g.saveState();
    g.reduceClipRegion(timelineStart,76,area,std::max(0,bottom-76));
    for (int i=firstGrid;i<=lastGrid && i<firstGrid+260;++i) {
        const double beat=i*renderStep;
        const double seconds=secondsAtBeat(row.tempo,beat);
        const int x=timelineStart+(int)std::round(area*(seconds-viewStart)/length);
        const bool bar=std::abs(beat/4-std::round(beat/4))<.0001;
        const bool quarter=std::abs(beat-std::round(beat))<.0001;
        g.setColour(palette::teal.withAlpha(bar ? .37f : quarter ? .22f : .11f));
        g.drawVerticalLine(x,76,(float)bottom);
    }
    g.restoreState();
    int track=0, loaded=0;
    g.saveState(); g.reduceClipRegion(0,tracksTop,getWidth(),std::max(0,bottom-tracksTop));
    for (int c=0;c<(int)row.cells.size();++c) {
        if (owner.show.columns[c].type==ColumnType::osc) continue;
        const int y=tracksTop+track++*trackHeight-trackScroll;
        const bool present=owner.hasClip(owner.selectedRow,c); if (present) ++loaded;
        if (y+32<tracksTop || y>=bottom) continue;
        const auto& name=owner.show.columns[c].name;
        label(g,name.isEmpty() ? (owner.show.columns[c].type==ColumnType::midi ? "MIDI " : "Audio ")+juce::String(c+1) : name,{16,y,115,30},12,c==owner.selectedColumn ? palette::text : palette::muted);
        if (!present) continue;
        const auto& cell=row.cells[c]; const double seconds=owner.clipDuration(owner.selectedRow,c);
        const auto colour=palette::type((int)owner.show.columns[c].type);
        const float x=(float)(timelineStart+area*(cell.startSeconds-viewStart)/length), width=std::max(4.0f,(float)(area*seconds/length));
        g.saveState(); g.reduceClipRegion(timelineStart,tracksTop,area,std::max(0,bottom-tracksTop));
        g.setColour(colour.withAlpha(c==owner.selectedColumn ? .22f : .12f)); g.fillRoundedRectangle(x,(float)y,width,31,4);
        g.setColour(colour.withAlpha(.6f)); g.drawRoundedRectangle(x,(float)y,width,31,4,1);
        g.setColour(colour.withAlpha(.4f));
        if (cell.midi) {
            for (const auto& note:cell.midi->notes) {
                const double begin=secondsAtBeat(row.tempo,note.beat), end=secondsAtBeat(row.tempo,note.beat+note.length);
                g.fillRect(x+(float)(area*begin/length),(float)y+3+(127-note.pitch)*.18f,std::max(1.0f,(float)(area*(end-begin)/length)),2.0f);
            }
        } else {
        const auto& audio=owner.clips.at(cell.id)->samples;
        if (audio.getNumSamples()>0 && audio.getNumChannels()>0) for (int px=std::max(0,(int)(timelineStart-x));px<(int)width;px+=3) {
            if (x+px>getWidth()-20) break;
            const int sample=juce::jlimit(0,audio.getNumSamples()-1,(int)((double)px/width*audio.getNumSamples()));
            const float amp=std::min(1.0f,std::abs(audio.getSample(0,sample)))*24;
            g.fillRect(x+px,(float)y+15-amp/2,1.0f,std::max(1.0f,amp));
        }
        }
        label(g,(cell.name.isEmpty() && cell.midi ? "MIDI" : cell.name)+"  +"+juce::String(cell.startSeconds,2)+"s",{(int)x+8,y+5,std::max(1,(int)width-16),20},11,palette::text);
        g.restoreState();
    }
    g.restoreState();
    if (loaded==0) label(g,"Importa audio o crea una clip MIDI per visualizzarla.",{timelineStart,83,getWidth()-160,32},12,palette::muted);
    if (owner.engine.monitoredRowId.load()==row.id) {
        const auto seconds=owner.engine.playheadSeconds.load(); const auto x=(float)(timelineStart+area*(seconds-viewStart)/length);
        if (x>=timelineStart && x<=getWidth()-20) {
            g.setColour(palette::amber); g.drawLine(x,72,x,(float)bottom,1.5f);
            juce::Path marker; marker.addTriangle(x-5,71,x+5,71,x,78); g.fillPath(marker);
        }
    }
    label(g,juce::String(row.tempo.front().bpm,1)+" BPM  /  4/4  /  doppio clic MIDI: piano roll  /  "+juce::String(zoom,1)+"x",
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
        if (owner.show.columns[c].type==ColumnType::osc || track++!=wanted) continue;
        const auto& cell=row.cells[c]; if (!owner.hasClip(owner.selectedRow,c)) return;
        const double position=dragViewStart+(e.x-timelineStart)*dragLength/area, end=cell.startSeconds+owner.clipDuration(owner.selectedRow,c);
        const auto rowId=row.id; const auto start=cell.startSeconds;
        if (position<start || position>end || !owner.select(owner.selectedRow,c)) return;
        dragColumn=c; dragRowId=rowId; dragStartSeconds=start;
        setMouseCursor(juce::MouseCursor::DraggingHandCursor); return;
    }
}
void Timeline::mouseDoubleClick(const juce::MouseEvent& e) {
    if (!e.mods.isLeftButtonDown()) return;
    syncViewport();
    if (owner.selectedRow<0 || owner.selectedRow>=(int)owner.show.rows.size()
        || e.x<timelineStart || e.x>getWidth()-20 || e.y<tracksTop || e.y>=getHeight()-48) return;
    if ((e.y-tracksTop+trackScroll)%trackHeight>=32) return;
    const int wanted=(e.y-tracksTop+trackScroll)/trackHeight;
    int track=0;
    const auto& row=owner.show.rows[(size_t)owner.selectedRow];
    const int area=std::max(1,getWidth()-timelineStart-20);
    const double position=viewStart+(e.x-timelineStart)*visibleLength()/area;
    for (int c=0;c<(int)row.cells.size();++c) {
        if (owner.show.columns[c].type==ColumnType::osc || track++!=wanted) continue;
        const auto& cell=row.cells[(size_t)c];
        if (owner.show.columns[c].type==ColumnType::midi && cell.midi
            && position>=cell.startSeconds && position<=cell.startSeconds+owner.clipDuration(owner.selectedRow,c)
            && owner.select(owner.selectedRow,c)) owner.editMidiClip();
        return;
    }
}
void Timeline::mouseDrag(const juce::MouseEvent& e) {
    syncViewport();
    if (owner.selectedRow<0 || owner.selectedRow>=(int)owner.show.rows.size() || owner.show.rows[owner.selectedRow].id!=dragRowId) return;
    if (seeking) owner.seekTimeline(juce::jlimit(0.0,86400.0,dragViewStart+juce::jlimit(0.0,dragPixels,(double)e.x-timelineStart)*dragLength/dragPixels));
    else if (dragColumn>=0 && dragColumn<(int)owner.show.rows[owner.selectedRow].cells.size())
        owner.setClipStart(dragColumn,juce::jlimit(0.0,86400.0,snapSeconds(juce::jlimit(0.0,86400.0,dragStartSeconds+e.getDistanceFromDragStartX()*dragLength/dragPixels))));
    repaint();
}
void Timeline::mouseUp(const juce::MouseEvent&) { dragColumn=-1; seeking=false; syncViewport(); setMouseCursor(juce::MouseCursor::NormalCursor); repaint(); }
void Timeline::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    syncViewport();
    if (e.mods.isCtrlDown()) {
        changeZoom(std::pow(2.0,wheel.deltaY*3),(double)(e.x-timelineStart)/std::max(1,getWidth()-timelineStart-20)); return;
    }
    int tracks=0; for (const auto& column : owner.show.columns) if (column.type!=ColumnType::osc) ++tracks;
    const int maxScroll=std::max(0,tracks*trackHeight-std::max(1,getHeight()-48-tracksTop));
    trackScroll=juce::jlimit(0,maxScroll,trackScroll-(int)std::round(wheel.deltaY*trackHeight*3)); repaint();
}
}

