#include "OscEditor.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace cuelist {
namespace {
bool parseFiniteNumber(const juce::String& text,double& value) {
    const auto trimmed=text.trim();
    if (trimmed.isEmpty()) return false;
    const char* start=trimmed.toRawUTF8();
    char* end=nullptr;
    value=std::strtod(start,&end);
    return end!=start && *end=='\0' && std::isfinite(value);
}
}
OscClipEditor::OscClipEditor(OscClip value,std::vector<TempoPoint> points,
             std::function<void(const OscClip&,const OscClip&,bool)> commit)
    : clip(std::move(value)),tempo(std::move(points)),onEdit(std::move(commit)) {
    setLookAndFeel(&theme); setName("sharedOscEditor"); setWantsKeyboardFocus(true);
    for (juce::Component* component:std::initializer_list<juce::Component*>{
        &unit,&resolution,&grid,&snap,&add,&remove,&duplicate,&detach,
        &zoomOut,&zoomIn,&zoomFit})
        addAndMakeVisible(component);
    unit.setName("oscTimeUnit");
    unit.addItem("Seconds",1); unit.addItem("Beat",2);
    unit.setSelectedId(clip.useBeats ? 2 : 1,juce::dontSendNotification);
    unit.onChange=[this] {
        const bool beats=unit.getSelectedId()==2;
        if (clip.useBeats==beats) return;
        const auto before=clip;
        const auto convert=[&](double x) {
            return beats ? beatFromSeconds(tempo,x) : secondsAtBeat(tempo,x);
        };
        for (auto& e:clip.events) e.time=convert(e.time);
        for (auto& a:clip.automations) {
            const double original=a.time,begin=convert(original);
            const double end=convert(original+a.duration);
            for (auto& p:a.points) p.time=convert(original+p.time)-begin;
            a.time=begin; a.duration=end-begin;
            a.points.front().time=0; a.points.back().time=a.duration;
        }
        clip.length=convert(clip.length);
        clip.useBeats=beats;
        if (validateOscClip(clip).failed()) { clip=before; unit.setSelectedId(before.useBeats?2:1,juce::dontSendNotification); return; }
        notify(before);
    };
    resolution.setName("oscGridResolution");
    for (int i=0;i<4;++i) resolution.addItem("1/"+juce::String(4<<i),i+1);
    resolution.setSelectedId(2,juce::dontSendNotification);
    resolution.setTooltip("Musical snap resolution or fraction of a second.");
    grid.setName("oscGridVisible"); grid.setToggleState(true,juce::dontSendNotification);
    grid.onClick=[this] { repaint(); };
    snap.setName("oscSnapEnabled"); snap.setToggleState(true,juce::dontSendNotification);
    snap.onClick=[this] { repaint(); };
    add.setName("oscAddAutomation"); add.setTooltip("Add a new OSC automation track.");
    zoomOut.setName("oscZoomOut"); zoomIn.setName("oscZoomIn"); zoomFit.setName("oscZoomFit");
    remove.setName("oscRemoveMessage"); duplicate.setName("oscDuplicate");
    add.onClick=[this] { createTrack(); };
    zoomOut.onClick=[this] { zoomAround(zoom/1.5f,axisLeft()+axisWidth()*.5f); };
    zoomIn.onClick=[this] { zoomAround(zoom*1.5f,axisLeft()+axisWidth()*.5f); };
    zoomFit.onClick=[this] { zoom=1; viewStart=0; repaint(); };
    remove.onClick=[this] { deleteSelection(); };
    duplicate.onClick=[this] { duplicateSelection(); };
    detach.setName("oscDetach");
    detach.onClick=[this] { if (onDetach) onDetach(); };
    for (auto* button:{&add,&remove,&duplicate,&detach,&zoomOut,&zoomIn,&zoomFit}) button->setWantsKeyboardFocus(false);
    ensureTracks();
    setSize(940,320);
}
OscClipEditor::~OscClipEditor() { setLookAndFeel(nullptr); }
void OscClipEditor::setFloating(bool floating) { detach.setButtonText(floating?"Reattach":"Detach"); }
void OscClipEditor::loadClip(const OscClip& next) {
    ++revision; clip=next; selectedEvent=selectedTrack=selectedAutomation=selectedPoint=-1;
    ensureTracks();
    dragging=false; rampDrag=RampDrag::none; laneScroll=0; zoom=1; viewStart=0;
    unit.setSelectedId(clip.useBeats ? 2 : 1,juce::dontSendNotification);
    repaint();
}
void OscClipEditor::resized() {
    const int w=getWidth();
    detach.setBounds(w-105,7,95,27);
    unit.setBounds(w-228,7,110,26);
    add.setBounds(13,7,35,26);
    remove.setBounds(53,7,70,26); duplicate.setBounds(128,7,83,26);
    grid.setBounds(219,7,69,26); snap.setBounds(291,7,60,26);
    resolution.setBounds(353,8,78,24);
    // In compact docks the window actions and zoom tools occupy row two.
    const bool compact=w<950;
    zoomOut.setBounds(compact ? 13 : 435,compact ? 39 : 7,29,26);
    zoomIn.setBounds(compact ? 46 : 468,compact ? 39 : 7,29,26);
    zoomFit.setBounds(compact ? 79 : 501,compact ? 39 : 7,30,26);
    repaint();
}
float OscClipEditor::xForTime(double time) const {
    return (float)axisLeft()+(float)((time-viewStart)/(clip.length/zoom))*axisWidth();
}
double OscClipEditor::timeForX(float x) const {
    return juce::jlimit(0.0,std::max(0.0,clip.length-.00001),
        viewStart+(double)(x-axisLeft())/axisWidth()*(clip.length/zoom));
}
double OscClipEditor::quantize(double t) const {
    if (!snap.getToggleState()) return t;
    const double step=1.0/(1<<juce::jlimit(0,3,resolution.getSelectedId()-1));
    return juce::jlimit(0.0,std::max(0.0,clip.length-.00001),std::round(t/step)*step);
}
void OscClipEditor::zoomAround(double factor,float anchorX) {
    const double anchor=timeForX(anchorX);
    const double proportion=juce::jlimit(0.0,1.0,(double)(anchorX-axisLeft())/axisWidth());
    zoom=juce::jlimit(1.0,64.0,factor);
    viewStart=juce::jlimit(0.0,std::max(0.0,clip.length-clip.length/zoom),
        anchor-proportion*clip.length/zoom);
    repaint();
}
OscTrack OscClipEditor::defaultTrack() const {
    OscTrack track;
    track.message.address="/parameter/"+juce::String((int)clip.tracks.size()+1);
    if (clip.tracks.empty()) track.message.address="/parameter";
    track.message.arguments={{OscValueType::decimal,0,0,juce::String()}};
    return track;
}
void OscClipEditor::ensureTracks() {
    for (const auto& au:clip.automations) {
        const auto found=std::find_if(clip.tracks.begin(),clip.tracks.end(),
            [&](const OscTrack& t) {
                return t.message.address==au.message.address &&
                    t.argumentIndex==au.argumentIndex &&
                    t.message.arguments.size()==au.message.arguments.size() &&
                    std::equal(t.message.arguments.begin(),t.message.arguments.end(),
                        au.message.arguments.begin(),[](const OscValue& x,const OscValue& y){
                            return x.type==y.type;
                        });
            });
        if (found!=clip.tracks.end() || clip.tracks.size()>=16) continue;
        OscTrack t; t.message=au.message; t.argumentIndex=au.argumentIndex;
        for (const auto& p:au.points) {
            t.rangeMin=std::min(t.rangeMin,p.value);
            t.rangeMax=std::max(t.rangeMax,p.value);
        }
        clip.tracks.push_back(std::move(t));
    }
}
int OscClipEditor::trackForAutomation(int index) const {
    if (index<0 || index>=(int)clip.automations.size()) return -1;
    const auto& au=clip.automations[(size_t)index];
    for (int i=0;i<(int)clip.tracks.size();++i) {
        const auto& t=clip.tracks[(size_t)i];
        if (t.message.address==au.message.address && t.argumentIndex==au.argumentIndex
            && t.message.arguments.size()==au.message.arguments.size()
            && std::equal(t.message.arguments.begin(),t.message.arguments.end(),
                au.message.arguments.begin(),[](const OscValue& x,const OscValue& y){
                    return x.type==y.type;
                })) return i;
    }
    return -1;
}
int OscClipEditor::trackAt(float y) const {
    if (y>=getHeight()-26) return -1; // reserved footer is never a lane
    for (int i=0;i<=(int)clip.tracks.size();++i)
        if (y>=laneTop(i) && y<laneTop(i)+laneHeight-4) return i;
    return -1;
}
int OscClipEditor::automationAt(float x,float y) const {
    const int track=trackAt(y);
    if (track<0 || track>=(int)clip.tracks.size()) return -1;
    for (int i=(int)clip.automations.size()-1;i>=0;--i) {
        const auto& a=clip.automations[(size_t)i];
        if (trackForAutomation(i)==track && x>=xForTime(a.time)-6
            && x<=xForTime(a.time+a.duration)+6 && y>=laneTop(track)+19) return i;
    }
    return -1;
}
float OscClipEditor::yForValue(int index,double value) const {
    const int track=trackForAutomation(index);
    if (track<0) return (float)eventLaneTop();
    const auto& t=clip.tracks[(size_t)track];
    const double frac=juce::jlimit(0.0,1.0,(value-t.rangeMin)/(t.rangeMax-t.rangeMin));
    return laneTop(track)+laneHeight-13-(float)frac*(laneHeight-39);
}
double OscClipEditor::valueForY(int index,float y) const {
    const int track=trackForAutomation(index);
    if (track<0) return 0;
    const auto& t=clip.tracks[(size_t)track];
    const double frac=juce::jlimit(0.0,1.0,
        (double)(laneTop(track)+laneHeight-13-y)/(laneHeight-39));
    return t.rangeMin+(t.rangeMax-t.rangeMin)*frac;
}
void OscClipEditor::createTrack() {
    ensureTracks();
    if (clip.tracks.size()>=16) return;
    const auto before=clip;
    clip.tracks.push_back(defaultTrack());
    selectedTrack=(int)clip.tracks.size()-1;
    selectedAutomation=selectedPoint=selectedEvent=-1;
    notify(before);
}
void OscClipEditor::paint(juce::Graphics& g) {
    g.fillAll(palette::panel);
    const bool compact=getWidth()<950;
    label(g,"OSC  /  TIMELINE",{compact ? 120 : 13,38,
          compact ? 166 : 235,24},13,palette::teal,true);
    if (!compact)
        label(g,"Double-click: ramp / edit | Shift-click: point | Right-click: menu",
              {255,38,std::max(1,getWidth()-275),24},12,palette::muted);
    const int left=axisLeft(),right=left+axisWidth();
    const auto ruler=juce::Rectangle<float>((float)left,rulerTop,(float)axisWidth(),rulerBottom-rulerTop);
    g.setColour(palette::background); g.fillRect(ruler);
    const double step=1.0/(1<<juce::jlimit(0,3,resolution.getSelectedId()-1));
    const double visible=clip.length/zoom;
    const double stride=std::max(1.0,std::ceil(visible/(70*step)));
    if (grid.getToggleState()) {
        g.setColour(palette::line.withAlpha(.7f));
        const double first=std::ceil(viewStart/(step*stride))*(step*stride);
        for (double t=first;t<=std::min(clip.length,viewStart+visible)+1e-7;t+=step*stride)
            g.drawVerticalLine((int)xForTime(t),rulerTop,(float)getHeight());
    }
    for (int i=0;i<=8;++i) {
        const double t=viewStart+visible*i/8.0;
        const int tick=(int)xForTime(t);
        const int labelX=juce::jlimit(left,right-58,tick-29);
        label(g,juce::String(t,2),
              {labelX,rulerTop+3,58,19},11,palette::muted,
              false,juce::Justification::centred);
    }
    // Ruler and toolbar remain fixed. Events, their labels and every track
    // share one clip region and one vertical offset.
    g.saveState();
    // Keep the footer band outside the drawing region for every track.
    const int laneBottom=getHeight()-26;
    g.reduceClipRegion(0,rulerBottom,getWidth(),
                       std::max(0,laneBottom-rulerBottom));
    const int eventsY=eventLaneTop();
    g.setColour(palette::raised);
    g.fillRoundedRectangle((float)left,(float)eventsY,(float)axisWidth(),(float)eventHeight,3);
    label(g,"EVENTS",{8,eventsY+6,68,19},12,palette::teal,true);
    for (int i=0;i<(int)clip.events.size();++i) {
        const auto& event=clip.events[(size_t)i];
        const float x=xForTime(event.time);
        const int y=eventsY+9+(i%2)*17;
        if (x<left-12 || x>right+12) continue;
        g.setColour(i==selectedEvent ? palette::amber : palette::teal);
        g.fillRoundedRectangle(x-5,(float)y,10,14,2);
        const auto arguments=describeArguments(event.arguments);
        const auto summary=event.address+(arguments.isNotEmpty() ? "   "+arguments : juce::String());
        if (i==selectedEvent || clip.events.size()<12)
            label(g,summary,{(int)x+7,y-2,
                std::max(1,std::min(300,right-(int)x-9)),18},10,palette::text);
    }
    for (int track=0;track<=(int)clip.tracks.size();++track) {
        const bool placeholder=track==(int)clip.tracks.size();
        if (placeholder && clip.tracks.size()>=16) break;
        const int y=laneTop(track);
        if (y>=laneBottom || y+laneHeight<rulerBottom ||
            (placeholder && y+25>laneBottom)) continue;
        g.setColour(track%2 ? palette::raised : palette::background);
        g.fillRect(left,y,axisWidth(),laneHeight-4);
        const bool selected=track==selectedTrack;
        label(g,placeholder ? "+ TRACK" : "OSC "+juce::String(track+1),
              {6,y+5,70,20},12,selected?palette::amber:palette::teal,true);
        const auto& address=placeholder ? juce::String("Double-click to add a ramp")
            : clip.tracks[(size_t)track].message.address;
        label(g,address,{left+9,y+3,std::max(1,axisWidth()-18),20},
              12,palette::muted);
        if (placeholder) continue;
        const auto& lane=clip.tracks[(size_t)track];
        label(g,juce::String(lane.rangeMax,2),{4,y+27,68,17},11,palette::muted);
        label(g,juce::String(lane.rangeMin,2),{4,y+laneHeight-29,68,17},11,palette::muted);
        for (int i=0;i<(int)clip.automations.size();++i) {
            if (trackForAutomation(i)!=track) continue;
            const auto& a=clip.automations[(size_t)i];
            const float x0=xForTime(a.time),x1=xForTime(a.time+a.duration);
            if (x1<left || x0>right) continue;
            const bool active=i==selectedAutomation;
            g.setColour(palette::teal.withAlpha(active ? .22f : .12f));
            g.fillRoundedRectangle(x0,(float)y+20,std::max(1.0f,x1-x0),
                                   (float)laneHeight-24,3.0f);
            g.setColour((active ? palette::amber : palette::teal).withAlpha(.85f));
            // Small resize grip; preserve the existing x1 drag hit area.
            g.fillRoundedRectangle(x1-2,(float)y+laneHeight-37,4,18,2.0f);
            juce::Path curve;
            for (int p=0;p<(int)a.points.size();++p) {
                const auto& point=a.points[(size_t)p];
                const float px=xForTime(a.time+point.time),py=yForValue(i,point.value);
                if (p==0) curve.startNewSubPath(px,py);
                else curve.lineTo(px,py);
                g.setColour(active && p==selectedPoint ? palette::amber : palette::text);
                g.fillEllipse(px-4,py-4,8,8);
            }
            g.setColour(active ? palette::amber : palette::teal);
            g.strokePath(curve,juce::PathStrokeType(2.0f));
        }
    }
    g.restoreState();
    g.setColour(palette::panel);
    g.fillRect(0,std::max(0,getHeight()-26),getWidth(),26);
    g.setColour(palette::line.withAlpha(.65f));
    g.drawHorizontalLine(std::max(0,getHeight()-26),0,(float)getWidth());
    g.setColour(palette::amber.withAlpha(.7f));
    g.drawVerticalLine((int)xForTime(cursor),(float)rulerTop,
                       (float)std::max(rulerTop,getHeight()-26));
    if (editHint.isNotEmpty())
        label(g,editHint,{13,getHeight()-24,std::max(1,getWidth()-26),20},
              11,palette::amber);
    else
        label(g,"OSC int / float | Right-click: delete point | Ctrl+wheel: zoom",
              {13,getHeight()-24,std::max(1,getWidth()-26),20},
              11,palette::muted);
}
void OscClipEditor::chooseEvent(int index) {
    selectedEvent=index; selectedTrack=selectedAutomation=selectedPoint=-1; repaint();
}
void OscClipEditor::chooseAutomation(int index) {
    selectedAutomation=index; selectedTrack=trackForAutomation(index);
    selectedEvent=selectedPoint=-1; repaint();
}
void OscClipEditor::notify(const OscClip& before,bool complete) {
    if (validateOscClip(clip).failed()) { clip=before; repaint(); return; }
    if (onEdit) onEdit(before,clip,complete);
    repaint();
}
int OscClipEditor::pointAt(int automationIndex,float x,float y) const {
    if (automationIndex<0 || automationIndex>=(int)clip.automations.size()) return -1;
    const auto& a=clip.automations[(size_t)automationIndex];
    for (int p=(int)a.points.size()-1;p>=0;--p) {
        const auto& pt=a.points[(size_t)p];
        if (std::abs(xForTime(a.time+pt.time)-x)<=8 &&
            std::abs(yForValue(automationIndex,pt.value)-y)<=9) return p;
    }
    return -1;
}
void OscClipEditor::mouseDown(const juce::MouseEvent& e) {
    grabKeyboardFocus();
    editHint.clear();
    if (e.mods.isPopupMenu()) { showContext(e); return; }
    if (!e.mods.isLeftButtonDown()) return;
    const float x=e.position.x,y=e.position.y;
    if (x<axisLeft() || x>axisLeft()+axisWidth() || y<rulerTop) return;
    cursor=quantize(timeForX(x));
    dragEvent=dragAutomation=dragPoint=-1; rampDrag=RampDrag::none;
    dragging=false; changedDuringDrag=false; dragBefore=clip;
    dragStartTime=timeForX(x);
    if (y>=rulerTop && y<rulerBottom) { repaint(); return; }
    if (y>=eventLaneTop() && y<eventLaneTop()+eventHeight) {
        for (int i=(int)clip.events.size()-1;i>=0;--i) {
            if (std::abs(xForTime(clip.events[(size_t)i].time)-x)<9) {
                chooseEvent(i);
                dragEvent=i; dragStartTime=clip.events[(size_t)i].time;
                setTooltip(clip.events[(size_t)i].address+"  "+describeArguments(clip.events[(size_t)i].arguments));
                return;
            }
        }
        if (e.getNumberOfClicks()==1 && clip.events.size()<256) {
            const auto before=clip;
            clip.events.push_back({cursor,"/cue",{{OscValueType::decimal,0,0,juce::String()}}});
            chooseEvent((int)clip.events.size()-1);
            notify(before);
        }
        return;
    }
    const int track=trackAt(y);
    if (track<0 || track>=(int)clip.tracks.size()) return;
    selectedTrack=track; selectedEvent=-1;
    const int index=automationAt(x,y);
    if (index<0) { selectedAutomation=selectedPoint=-1; repaint(); return; }
    chooseAutomation(index);
    const auto& a=clip.automations[(size_t)index];
    const float end=xForTime(a.time+a.duration);
    if (y<laneTop(track)+20) return;
    dragAutomation=index;
    // Shift+click adds a control point without requiring an extra dialog.
    // Check before the edge handle so the established Shift gesture remains valid.
    if (e.mods.isShiftDown() && a.points.size()<128) {
        const double relative=cursor-a.time;
        if (relative>.0001 && relative<a.duration-.0001) {
            auto before=clip;
            auto& points=clip.automations[(size_t)index].points;
            if (std::none_of(points.begin(),points.end(),
                [&](const OscControlPoint& p){return std::abs(p.time-relative)<.0001;})) {
                double value=valueForY(index,y);
                if (a.message.arguments[(size_t)a.argumentIndex].type==OscValueType::integer)
                    value=std::round(value);
                points.push_back({relative,value});
                std::sort(points.begin(),points.end(),[](const auto& x,const auto& z){return x.time<z.time;});
                selectedPoint=(int)(std::find_if(points.begin(),points.end(),
                    [&](const auto& p){return std::abs(p.time-relative)<.0001;})-points.begin());
                notify(before);
            }
        }
        dragAutomation=-1; return;
    }
    const int point=pointAt(index,x,y);
    if (point>=0) {
        selectedPoint=dragPoint=point;
        rampDrag=RampDrag::point; repaint(); return;
    }
    if (std::abs(x-end)<=9) { rampDrag=RampDrag::resize; return; }
    rampDrag=RampDrag::move;
}
void OscClipEditor::mouseDrag(const juce::MouseEvent& e) {
    if (dragEvent>=0 && dragEvent<(int)clip.events.size()) {
        const auto before=clip;
        clip.events[(size_t)dragEvent].time=quantize(timeForX(e.position.x));
        if (clip.events[(size_t)dragEvent].time!=before.events[(size_t)dragEvent].time) {
            notify(before,false);
            if (clip.events[(size_t)dragEvent].time!=before.events[(size_t)dragEvent].time)
                changedDuringDrag=true;
        }
        return;
    }
    if (dragAutomation<0 || dragAutomation>=(int)clip.automations.size()
        || rampDrag==RampDrag::none) return;
    const auto before=clip;
    auto& a=clip.automations[(size_t)dragAutomation];
    const auto& original=dragBefore.automations[(size_t)dragAutomation];
    if (rampDrag==RampDrag::move) {
        const double delta=timeForX(e.position.x)-dragStartTime;
        a.time=juce::jlimit(0.0,std::max(0.0,clip.length-original.duration),
                           quantize(original.time+delta));
    } else if (rampDrag==RampDrag::resize ||
               (rampDrag==RampDrag::point && dragPoint==(int)a.points.size()-1)) {
        const double raw=juce::jlimit(0.0,clip.length,
            viewStart+(double)(e.position.x-axisLeft())/axisWidth()*clip.length/zoom);
        const double step=1.0/(1<<juce::jlimit(0,3,resolution.getSelectedId()-1));
        const double end=snap.getToggleState() ? std::round(raw/step)*step : raw;
        const double next=end-original.time;
        const double latestInterior=original.points.size()>2 ?
            original.points[original.points.size()-2].time : 0;
        if (next<=latestInterior+.0001 || next<.01 ||
            original.time+next>clip.length+1e-7) {
            editHint="Cannot shorten: an intermediate point would be outside the ramp.";
            repaint(); return;
        }
        a.duration=next;
        a.points.back().time=next;
        // Intermediate points remain at their original offsets; no implicit stretch.
        if (rampDrag==RampDrag::point)
            a.points.back().value=valueForY(dragAutomation,e.position.y);
    } else if (rampDrag==RampDrag::point && dragPoint>=0) {
        auto& pt=a.points[(size_t)dragPoint];
        if (dragPoint>0) {
            const double relative=quantize(timeForX(e.position.x))-a.time;
            pt.time=juce::jlimit(a.points[(size_t)dragPoint-1].time+.0001,
                a.points[(size_t)dragPoint+1].time-.0001,relative);
        }
        pt.value=valueForY(dragAutomation,e.position.y);
    }
    if (rampDrag==RampDrag::point && dragPoint>=0 &&
        a.message.arguments[(size_t)a.argumentIndex].type==OscValueType::integer)
        a.points[(size_t)dragPoint].value=std::round(a.points[(size_t)dragPoint].value);
    const auto& prior=before.automations[(size_t)dragAutomation];
    bool changed=a.time!=prior.time || a.duration!=prior.duration || a.points.size()!=prior.points.size();
    if (rampDrag==RampDrag::point && dragPoint>=0)
        changed=changed || a.points[(size_t)dragPoint].time!=prior.points[(size_t)dragPoint].time ||
            a.points[(size_t)dragPoint].value!=prior.points[(size_t)dragPoint].value;
    if (changed) {
        if (validateOscClip(clip).failed()) { clip=before; repaint(); return; }
        editHint.clear(); notify(before,false); changedDuringDrag=true;
    }
}
void OscClipEditor::mouseUp(const juce::MouseEvent&) {
    if (changedDuringDrag && onEdit) onEdit(dragBefore,clip,true);
    dragEvent=dragAutomation=dragPoint=-1;
    rampDrag=RampDrag::none; changedDuringDrag=false; repaint();
}
void OscClipEditor::mouseDoubleClick(const juce::MouseEvent& e) {
    if (e.position.y>=eventLaneTop() && e.position.y<eventLaneTop()+eventHeight) {
        for (int i=(int)clip.events.size()-1;i>=0;--i)
            if (std::abs(xForTime(clip.events[(size_t)i].time)-e.position.x)<10) {
                editEvent(i); return;
            }
    }
    const int track=trackAt(e.position.y);
    if (track<0) return;
    if (track<(int)clip.tracks.size() &&
        (e.position.x<axisLeft() || e.position.y<laneTop(track)+20)) {
        editTrackAddress(track); return;
    }
    if (e.position.x<axisLeft() || e.position.x>axisLeft()+axisWidth()) return;
    const int index=automationAt(e.position.x,e.position.y);
    if (index>=0) editAutomation(index);
    else createAutomationAt(quantize(timeForX(e.position.x)),track);
}
void OscClipEditor::mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel) {
    if (e.mods.isCommandDown() || e.mods.isCtrlDown()) {
        zoomAround(zoom*std::pow(1.25,(double)wheel.deltaY*4),e.position.x);
    } else if (e.mods.isShiftDown()) {
        viewStart=juce::jlimit(0.0,std::max(0.0,clip.length-clip.length/zoom),
            viewStart-wheel.deltaY*clip.length/zoom*.2);
        repaint();
    } else {
        const int bottom=laneTop((int)clip.tracks.size()+1)+laneHeight;
        laneScroll=juce::jlimit(0,std::max(0,bottom+laneScroll-getHeight()+26),
            laneScroll-(int)(wheel.deltaY*100));
        repaint();
    }
}
bool OscClipEditor::keyPressed(const juce::KeyPress& k) {
    if (k.getKeyCode()==juce::KeyPress::deleteKey || k.getKeyCode()==juce::KeyPress::backspaceKey) {
        deleteSelection(); return true;
    }
    if (k.getModifiers().isCommandDown() && (k.getTextCharacter()=='d'||k.getTextCharacter()=='D')) {
        duplicateSelection(); return true;
    }
    if (k.getModifiers().isCommandDown() && (k.getTextCharacter()=='z'||k.getTextCharacter()=='Z')) {
        if (k.getModifiers().isShiftDown()) { if (onRedo) onRedo(); }
        else if (onUndo) onUndo();
        return true;
    }
    return false;
}
void OscClipEditor::removeSelectedPoint() {
    if (selectedAutomation<0 || selectedAutomation>=(int)clip.automations.size()) return;
    auto& points=clip.automations[(size_t)selectedAutomation].points;
    if (selectedPoint<=0 || selectedPoint>=(int)points.size()-1) {
        editHint="The first and last control points cannot be deleted.";
        repaint(); return;
    }
    const auto before=clip;
    points.erase(points.begin()+selectedPoint);
    selectedPoint=-1;
    notify(before);
}
void OscClipEditor::showContext(const juce::MouseEvent& e) {
    if (e.position.y>=eventLaneTop() && e.position.y<eventLaneTop()+eventHeight) {
        for (int i=(int)clip.events.size()-1;i>=0;--i)
            if (std::abs(xForTime(clip.events[(size_t)i].time)-e.position.x)<10)
                {chooseEvent(i); break;}
    } else {
        const int hit=automationAt(e.position.x,e.position.y);
        if (hit>=0) {
            chooseAutomation(hit);
            const int p=pointAt(hit,e.position.x,e.position.y);
            if (p>=0) {
                selectedPoint=p;
                juce::PopupMenu pointMenu;
                pointMenu.setLookAndFeel(&getLookAndFeel());
                pointMenu.addItem(10,"Delete point",p>0 && p+1<(int)clip.automations[(size_t)hit].points.size());
                auto safe=juce::Component::SafePointer<OscClipEditor>(this);
                pointMenu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                    [safe](int action) {if (safe && action==10) safe->removeSelectedPoint();});
                repaint(); return;
            }
        }
        const int track=trackAt(e.position.y);
        if (track>=0 && track<(int)clip.tracks.size()) selectedTrack=track;
    }
    juce::PopupMenu menu;
    menu.setLookAndFeel(&getLookAndFeel());
    menu.addItem(1,"Edit",selectedEvent>=0 || selectedAutomation>=0);
    menu.addItem(2,"Duplicate",selectedEvent>=0 || selectedAutomation>=0);
    menu.addItem(3,"Delete",selectedEvent>=0 || selectedAutomation>=0);
    menu.addItem(4,"OSC track address",selectedTrack>=0);
    auto safe=juce::Component::SafePointer<OscClipEditor>(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),[safe](int item) {
        if (!safe) return;
        if (item==1) {
            if (safe->selectedEvent>=0) safe->editEvent(safe->selectedEvent);
            else if (safe->selectedAutomation>=0) safe->editAutomation(safe->selectedAutomation);
        } else if (item==2) safe->duplicateSelection();
        else if (item==3) safe->deleteSelection();
        else if (item==4 && safe->selectedTrack>=0)
            safe->editTrackAddress(safe->selectedTrack);
    });
}
void OscClipEditor::addEvent() {
    if (clip.events.size()>=256) return;
    const auto before=clip;
    // The message must have a typed argument. Historically this button
    // created an empty typetag list: receivers saw only its OSC address.
    clip.events.push_back({cursor,"/cue",{{OscValueType::decimal,0,0.0f,juce::String()}}});
    chooseEvent((int)clip.events.size()-1);
    notify(before); editEvent(selectedEvent);
}
void OscClipEditor::addAutomation() { createTrack(); }
void OscClipEditor::createAutomationAt(double start,int trackIndex) {
    if (clip.automations.size()>=16 || !std::isfinite(start) ||
        trackIndex<0 || trackIndex>(int)clip.tracks.size()) return;
    const auto before=clip;
    if (trackIndex==(int)clip.tracks.size()) clip.tracks.push_back(defaultTrack());
    const auto& track=clip.tracks[(size_t)trackIndex];
    OscAutomation au;
    au.time=juce::jlimit(0.0,clip.length,start);
    au.duration=clip.useBeats
        ? beatFromSeconds(tempo,secondsAtBeat(tempo,au.time)+1.0)-au.time
        : 1.0;
    if (!(au.duration>0) || au.time+au.duration>86400) {clip=before;return;}
    au.message=track.message;
    au.argumentIndex=track.argumentIndex;
    au.points={{0,0},{au.duration,1}};
    clip.length=std::max(clip.length,au.time+au.duration);
    clip.automations.push_back(au);
    if (validateOscClip(clip).failed()) {
        clip=before; editHint="Cannot add an overlapping OSC segment."; repaint(); return;
    }
    chooseAutomation((int)clip.automations.size()-1);
    notify(before);
}
void OscClipEditor::editTrackAddress(int index) {
    if (index<0 || index>=(int)clip.tracks.size()) return;
    const auto address=clip.tracks[(size_t)index].message.address;
    auto* dialog=new juce::AlertWindow("OSC track address",
        "Shared by all segments in this track.",
        juce::MessageBoxIconType::NoIcon,this);
    dialog->addTextEditor("address",address,"OSC address");
    dialog->addButton("Save",1); dialog->addButton("Cancel",0);
    auto safe=juce::Component::SafePointer<OscClipEditor>(this);
    const auto generation=revision;
    dialog->enterModalState(true,juce::ModalCallbackFunction::create(
        [safe,dialog,index,generation](int response) {
            if (!safe || response!=1 || generation!=safe->revision ||
                index>=(int)safe->clip.tracks.size()) return;
            const auto address=dialog->getTextEditorContents("address").trim();
            const auto before=safe->clip;
            for (int i=0;i<(int)safe->clip.automations.size();++i)
                if (safe->trackForAutomation(i)==index)
                    safe->clip.automations[(size_t)i].message.address=address;
            safe->clip.tracks[(size_t)index].message.address=address;
            safe->notify(before);
        }),true);
}
void OscClipEditor::deleteSelection() {
    if (selectedPoint>=0 && selectedAutomation>=0) {
        removeSelectedPoint(); return;
    }
    const auto before=clip;
    if (selectedEvent>=0 && selectedEvent<(int)clip.events.size()) {
        clip.events.erase(clip.events.begin()+selectedEvent);
        selectedEvent=-1;
    } else if (selectedAutomation>=0 && selectedAutomation<(int)clip.automations.size()) {
        clip.automations.erase(clip.automations.begin()+selectedAutomation);
        selectedAutomation=selectedPoint=-1;
    } else if (selectedTrack>=0 && selectedTrack<(int)clip.tracks.size()) {
        for (int i=(int)clip.automations.size()-1;i>=0;--i)
            if (trackForAutomation(i)==selectedTrack)
                clip.automations.erase(clip.automations.begin()+i);
        clip.tracks.erase(clip.tracks.begin()+selectedTrack);
        selectedTrack=-1;
    } else return;
    notify(before);
}
void OscClipEditor::duplicateSelection() {
    const auto before=clip;
    if (selectedEvent>=0 && selectedEvent<(int)clip.events.size() && clip.events.size()<256) {
        auto copy=clip.events[(size_t)selectedEvent];
        copy.time=juce::jlimit(0.0,clip.length-.0001,copy.time+.25);
        clip.events.push_back(copy); chooseEvent((int)clip.events.size()-1);
    } else if (selectedAutomation>=0 && selectedAutomation<(int)clip.automations.size()
               && clip.automations.size()<16) {
        auto copy=clip.automations[(size_t)selectedAutomation];
        copy.time+=copy.duration+.25; // avoid overlap on the same track
        clip.length=std::max(clip.length,copy.time+copy.duration);
        if (clip.length>86400) { clip=before; return; }
        clip.automations.push_back(copy); chooseAutomation((int)clip.automations.size()-1);
    } else return;
    notify(before);
}
void OscClipEditor::editEvent(int index) {
    if (index<0 || index>=(int)clip.events.size()) return;
    chooseEvent(index);
    const auto event=clip.events[(size_t)index];
    auto* dialog=new juce::AlertWindow("OSC message",
        "Time, address and ordered arguments: i:7; f:0.5; s:scene; b:true",
        juce::MessageBoxIconType::NoIcon,this);
    dialog->addTextEditor("time",juce::String(event.time,3),clip.useBeats?"Beat":"Seconds");
    dialog->addTextEditor("address",event.address,"OSC address");
    dialog->addTextEditor("args",describeArguments(event.arguments),"Arguments");
    dialog->addButton("Save",1); dialog->addButton("Cancel",0);
    auto safe=juce::Component::SafePointer<OscClipEditor>(this); const auto generation=revision;
    dialog->enterModalState(true,juce::ModalCallbackFunction::create(
        [safe,dialog,index,generation](int answer) {
            if (!safe || answer!=1 || generation!=safe->revision || index>=(int)safe->clip.events.size()) return;
            const auto t=dialog->getTextEditorContents("time").trim();
            if (t.isEmpty() || t.retainCharacters("0123456789.")!=t) return;
            auto modified=safe->clip;
            auto& e=modified.events[(size_t)index];
            e.time=t.getDoubleValue(); e.address=dialog->getTextEditorContents("address").trim();
            if (parseArguments(dialog->getTextEditorContents("args"),e.arguments).failed()) return;
            if (e.time>=modified.length) return;
            if (validateOscClip(modified).failed()) return;
            const auto before=safe->clip;
            safe->clip=std::move(modified);
            safe->notify(before);
        }),true);
}
void OscClipEditor::editAutomation(int index) {
    if (index<0 || index>=(int)clip.automations.size()) return;
    chooseAutomation(index);
    const auto original=clip.automations[(size_t)index];
    const int trackIndex=trackForAutomation(index);
    if (trackIndex<0) return;
    const auto track=clip.tracks[(size_t)trackIndex];
    const auto type=original.message.arguments[(size_t)original.argumentIndex].type;
    auto* dialog=new juce::AlertWindow("OSC ramp",
        "Start and end values are independent of the parameter range.\n"
        "Intermediate points retain their values. The argument index is 1-based.",
        juce::MessageBoxIconType::NoIcon,this);
    dialog->addTextEditor("start",juce::String(original.points.front().value,3),"Start value");
    dialog->addTextEditor("end",juce::String(original.points.back().value,3),"End value");
    dialog->addTextEditor("rangeMin",juce::String(track.rangeMin,3),"Parameter minimum");
    dialog->addTextEditor("rangeMax",juce::String(track.rangeMax,3),"Parameter maximum");
    dialog->addTextEditor("args",describeArguments(original.message.arguments),"OSC arguments (i:7; f:0.5)");
    dialog->addTextEditor("argumentIndex",juce::String(original.argumentIndex+1),"Automated argument (1-based)");
    dialog->addTextEditor("type",type==OscValueType::integer?"i":"f","Automated type: f or i");
    dialog->addButton("Save",1); dialog->addButton("Cancel",0);
    auto safe=juce::Component::SafePointer<OscClipEditor>(this);
    const auto generation=revision;
    dialog->enterModalState(true,juce::ModalCallbackFunction::create(
        [safe,dialog,index,trackIndex,generation](int answer) {
            if (!safe || answer!=1 || generation!=safe->revision ||
                index>=(int)safe->clip.automations.size() ||
                trackIndex>=(int)safe->clip.tracks.size()) return;
            double start=0,end=0,min=0,max=0;
            if (!parseFiniteNumber(dialog->getTextEditorContents("start"),start) ||
                !parseFiniteNumber(dialog->getTextEditorContents("end"),end) ||
                !parseFiniteNumber(dialog->getTextEditorContents("rangeMin"),min) ||
                !parseFiniteNumber(dialog->getTextEditorContents("rangeMax"),max) ||
                min>=max || start<min || start>max || end<min || end>max) return;
            const auto type=dialog->getTextEditorContents("type").trim().toLowerCase();
            if (type!="f" && type!="i") return;
            const auto indexText=dialog->getTextEditorContents("argumentIndex").trim();
            if (indexText.isEmpty() || indexText.retainCharacters("0123456789")!=indexText) return;
            const int argumentIndex=indexText.getIntValue()-1;
            std::vector<OscValue> arguments;
            if (parseArguments(dialog->getTextEditorContents("args"),arguments).failed() ||
                argumentIndex<0 || argumentIndex>=(int)arguments.size()) return;
            arguments[(size_t)argumentIndex].type=
                type=="i" ? OscValueType::integer : OscValueType::decimal;
            if (type=="i" && (std::floor(start)!=start || std::floor(end)!=end)) return;
            auto updated=safe->clip;
            // All segments on a track share their OSC format and argument index.
            for (int i=0;i<(int)safe->clip.automations.size();++i) {
                if (safe->trackForAutomation(i)!=trackIndex) continue;
                auto& a=updated.automations[(size_t)i];
                a.message.arguments=arguments;
                a.argumentIndex=argumentIndex;
                if (i==index) {
                    a.points.front().value=start;
                    a.points.back().value=end;
                }
                for (const auto& point:a.points)
                    if (point.value<min || point.value>max ||
                        (type=="i" && std::floor(point.value)!=point.value))
                        return;
            }
            auto& target=updated.tracks[(size_t)trackIndex];
            target.message.arguments=arguments;
            target.argumentIndex=argumentIndex;
            target.rangeMin=min; target.rangeMax=max;
            if (validateOscClip(updated).failed()) return;
            const auto before=safe->clip;
            safe->clip=std::move(updated);
            safe->selectedTrack=trackIndex;
            safe->notify(before);
        }),true);
}
juce::Result OscClipEditor::parseArguments(const juce::String& text,std::vector<OscValue>& output) {
    output.clear();
    if (text.trim().isEmpty()) return juce::Result::ok();
    for (auto item:juce::StringArray::fromTokens(text,";","")) {
        const auto part=item.trim();
        if (part.length()<2 || part[1]!=':') return juce::Result::fail("Argument has no type.");
        const auto v=part.substring(2).trim();
        OscValue a;
        if (part[0]=='i') {
            if (v.isEmpty() || v.retainCharacters("-0123456789")!=v) return juce::Result::fail("Invalid OSC integer.");
            const auto n=v.getLargeIntValue();
            if (n < -2147483648LL || n>2147483647LL) return juce::Result::fail("OSC integer out of range.");
            a.type=OscValueType::integer; a.integer=(int)n;
        } else if (part[0]=='f') {
            if (v.isEmpty() || v.retainCharacters("-0123456789.eE+")!=v) return juce::Result::fail("Invalid OSC float.");
            a.type=OscValueType::decimal; a.decimal=(float)v.getDoubleValue();
            if (!std::isfinite(a.decimal)) return juce::Result::fail("Invalid OSC float.");
        } else if (part[0]=='s') { a.type=OscValueType::text; a.text=v; }
        else if (part[0]=='b') {
            if (v!="true" && v!="false") return juce::Result::fail("OSC boolean must be true or false.");
            a.type=OscValueType::boolean; a.boolean=v=="true";
        } else return juce::Result::fail("Unknown OSC argument type.");
        output.push_back(std::move(a));
        if (output.size()>16) return juce::Result::fail("Maximum 16 arguments.");
    }
    return juce::Result::ok();
}
juce::String OscClipEditor::describeArguments(const std::vector<OscValue>& values) {
    juce::StringArray items;
    for (const auto& a:values)
        items.add(a.type==OscValueType::integer ? "i:"+juce::String(a.integer)
            : a.type==OscValueType::decimal ? "f:"+juce::String(a.decimal)
            : a.type==OscValueType::boolean ? "b:"+(a.boolean?juce::String("true"):juce::String("false"))
            : "s:"+a.text);
    return items.joinIntoString("; ");
}
juce::String OscClipEditor::describePoints(const std::vector<OscControlPoint>& points) {
    juce::StringArray tokens;
    for (const auto& p:points) tokens.add(juce::String(p.time,3)+":"+juce::String(p.value,3));
    return tokens.joinIntoString("; ");
}
juce::Result OscClipEditor::parsePoints(const juce::String& text,std::vector<OscControlPoint>& output) {
    output.clear();
    for (auto token:juce::StringArray::fromTokens(text,";","")) {
        const auto v=token.trim();
        const int colon=v.indexOfChar(':');
        if (colon<=0) return juce::Result::fail("Use time:value; time:value.");
        const auto t=v.substring(0,colon).trim(),x=v.substring(colon+1).trim();
        if (t.isEmpty() || x.isEmpty() ||
            t.retainCharacters("0123456789.")!=t ||
            x.retainCharacters("-0123456789.eE+")!=x) return juce::Result::fail("Invalid point.");
        output.push_back({t.getDoubleValue(),x.getDoubleValue()});
        if (output.size()>128) return juce::Result::fail("Too many points.");
    }
    return juce::Result::ok();
}
double OscClipEditor::beatFromSeconds(const std::vector<TempoPoint>& points,double seconds) {
    double lo=0,hi=std::max(1.0,seconds*4);
    while (secondsAtBeat(points,hi)<seconds && hi<1e7) hi*=2;
    for (int i=0;i<48;++i) {
        const auto mid=(lo+hi)*.5;
        if (secondsAtBeat(points,mid)<seconds) lo=mid; else hi=mid;
    }
    return (lo+hi)*.5;
}
}
