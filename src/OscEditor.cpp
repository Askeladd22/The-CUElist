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
    zoomOut.setBounds(441,7,31,26); zoomIn.setBounds(474,7,31,26);
    zoomFit.setBounds(508,7,42,26);
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
    label(g,"OSC  /  timeline",{13,38,225,24},13,palette::teal,true);
    label(g,"Double-click: create/edit ramp  ·  Drag: move/resize  ·  Shift+click: point",
          {230,38,getWidth()-245,24},10,palette::muted);
    const int left=axisLeft(),right=left+axisWidth();
    const auto ruler=juce::Rectangle<float>((float)left,rulerTop,(float)axisWidth(),rulerBottom-rulerTop);
    g.setColour(palette::background); g.fillRect(ruler);
    const double step=1.0/(1<<juce::jlimit(0,3,resolution.getSelectedId()-1));
    if (grid.getToggleState()) {
        // Render a bounded number of lines even for clips with very fine snap.
        const int stride=std::max(1,(int)std::ceil(clip.length/(260.0*step)));
        const double visualStep=step*stride; // every painted line is a true snap-grid multiple
        for (int i=0;i<=260 && i*visualStep<=clip.length+.00001;++i) {
            const float x=xForTime(i*visualStep);
            g.setColour(palette::line.withAlpha(.75f));
            g.drawVerticalLine((int)x,rulerTop,(float)getHeight());
        }
    }
    for (int i=0;i<=8;++i) {
        const auto time=clip.length*i/8.0;
        const auto x=xForTime(time);
        label(g,juce::String(time,clip.useBeats?2:1),{(int)x-8,rulerTop+4,58,18},9,palette::muted);
    }
    g.setColour(palette::raised);
    g.fillRoundedRectangle((float)left,eventTop,(float)axisWidth(),eventHeight,3);
    label(g,"EVENTS",{8,eventTop+8,66,19},10,palette::teal,true);
    for (int i=0;i<(int)clip.events.size();++i) {
        const auto& e=clip.events[(size_t)i];
        const float x=xForTime(e.time);
        const float y=eventTop+10+(i%2)*17;
        g.setColour(i==selectedEvent ? palette::amber : palette::teal);
        g.fillRoundedRectangle(x-5,y,10,14,2);
        if (i==selectedEvent || axisWidth()/std::max(1,(int)clip.events.size())>74) {
            const auto arguments=describeArguments(e.arguments);
            const auto summary=e.address+(arguments.isNotEmpty() ? "   "+arguments : juce::String());
            label(g,summary,{(int)x+7,(int)y-1,
                std::max(0,std::min(245,right-(int)x-11)),17},10,palette::text);
        }
    }
    g.setColour(palette::muted);
    g.drawHorizontalLine(eventTop+eventHeight+4,(float)left,(float)right);
    for (int i=0;i<(int)clip.automations.size();++i) {
        const auto& a=clip.automations[(size_t)i];
        const int y=laneTop(i);
        if (y>=getHeight() || y+laneHeight<rulerBottom) continue;
        g.setColour(i%2 ? palette::raised : palette::background);
        g.fillRect(left,y,axisWidth(),laneHeight-4);
        label(g,"AUTO "+juce::String(i+1),{6,y+4,68,16},9,
              i==selectedAutomation ? palette::amber : palette::teal,true);
        label(g,a.message.address+"  (double-click: address)",
              {left+9,y+2,std::max(25,axisWidth()-16),16},10,palette::muted);
        const float x0=xForTime(a.time),x1=xForTime(a.time+a.duration);
        g.setColour(palette::teal.withAlpha(i==selectedAutomation ? .23f : .12f));
        g.fillRoundedRectangle(x0,(float)y+20,std::max(1.0f,x1-x0),
                               (float)laneHeight-25,3.0f);
        g.setColour(i==selectedAutomation ? palette::amber : palette::teal);
        g.fillRoundedRectangle(x1-4,(float)y+23,8,(float)laneHeight-31,2.0f);
        juce::Path curve;
        bool first=true;
        for (int p=0;p<(int)a.points.size();++p) {
            const auto& point=a.points[(size_t)p];
            const auto x=xForTime(a.time+point.time),py=yForValue(i,point.value);
            if (first) { curve.startNewSubPath(x,py); first=false; } else curve.lineTo(x,py);
            g.setColour(i==selectedAutomation && p==selectedPoint ? palette::amber : palette::text);
            g.fillEllipse(x-4,py-4,8,8);
        }
        g.setColour(palette::teal); g.strokePath(curve,juce::PathStrokeType(2.0f));
    }
    // Ready-to-use empty track; no change to the serialized automation model.
    if (clip.automations.size()<16) {
        const int y=laneTop((int)clip.automations.size());
        if (y<getHeight() && y+laneHeight>rulerBottom) {
            g.setColour(palette::raised.withAlpha(.65f));
            g.fillRect(left,y,axisWidth(),laneHeight-4);
            label(g,"+ AUTO",{6,y+4,68,17},9,palette::teal,true);
            label(g,newTrackAddress+"  (next track address)",
                  {left+9,y+3,std::max(25,axisWidth()-16),17},10,palette::muted);
            label(g,"Double-click here: ramp 0 → 1 / 1 second",
                  {left+12,y+32,std::max(25,axisWidth()-20),22},11,palette::muted);
        }
    }
    g.setColour(palette::amber.withAlpha(.7f));
    g.drawVerticalLine((int)xForTime(cursor),(float)rulerTop,(float)getHeight());
    label(g,"GAIN: i / f  •  BOOLEAN: T / F",{13,getHeight()-17,getWidth()-20,16},9,palette::muted);
}
void OscClipEditor::chooseEvent(int index) {
    selectedEvent=index; selectedAutomation=selectedPoint=-1; repaint();
}
void OscClipEditor::chooseAutomation(int index) {
    selectedAutomation=index; selectedEvent=selectedPoint=-1; repaint();
}
void OscClipEditor::notify(const OscClip& before,bool complete) {
    if (validateOscClip(clip).failed()) { clip=before; repaint(); return; }
    if (onEdit) onEdit(before,clip,complete);
    repaint();
}
void OscClipEditor::mouseDown(const juce::MouseEvent& e) {
    grabKeyboardFocus();
    if (e.mods.isPopupMenu()) { showContext(e); return; }
    if (!e.mods.isLeftButtonDown() || e.position.x<axisLeft() ||
        e.position.x>axisLeft()+axisWidth() || e.position.y<rulerTop) return;
    cursor=quantize(timeForX(e.position.x));
    dragEvent=dragAutomation=dragPoint=-1;
    rampDrag=RampDrag::none;
    dragging=false; changedDuringDrag=false; dragBefore=clip;
    dragStartTime=timeForX(e.position.x);
    if (e.position.y>=rulerTop && e.position.y<rulerBottom) { repaint(); return; }
    if (e.position.y>=eventTop && e.position.y<eventTop+eventHeight) {
        for (int i=(int)clip.events.size()-1;i>=0;--i)
            if (std::abs(xForTime(clip.events[(size_t)i].time)-e.position.x)<9) {
                chooseEvent(i); dragEvent=i; dragStartTime=clip.events[(size_t)i].time; return;
            }
        if (e.getNumberOfClicks()==1) {
            const auto before=clip;
            if (clip.events.size()>=256) return;
            clip.events.push_back({cursor,"/cue",{}});
            chooseEvent((int)clip.events.size()-1);
            notify(before);
        }
        return;
    }
    const int index=automationAt(e.position.y);
    if (index<0) return; // The empty lane only responds to double-click.
    chooseAutomation(index);
    const auto& automation=clip.automations[(size_t)index];
    const float x0=xForTime(automation.time),x1=xForTime(automation.time+automation.duration);
    if (e.position.y<laneTop(index)+20 || e.position.x<x0-5 || e.position.x>x1+6) return;
    dragAutomation=index;
    if (std::abs(e.position.x-x1)<=9) { rampDrag=RampDrag::resize; return; }
    for (int i=0;i<(int)automation.points.size();++i) {
        const auto& p=automation.points[(size_t)i];
        if (std::abs(xForTime(automation.time+p.time)-e.position.x)<7 &&
            std::abs(yForValue(index,p.value)-e.position.y)<9) {
            selectedPoint=dragPoint=i; rampDrag=RampDrag::point; repaint(); return;
        }
    }
    // Shift-click creates intermediate points; ordinary drag moves the ramp.
    if (e.mods.isShiftDown() && automation.points.size()<128) {
        const double relative=cursor-automation.time;
        if (relative>.0001 && relative<automation.duration-.0001) {
            const auto before=clip;
            auto& au=clip.automations[(size_t)index];
            const double value=valueForY(index,e.position.y);
            au.points.push_back({relative,
                au.message.arguments[(size_t)au.argumentIndex].type==OscValueType::integer
                    ? std::round(value) : value});
            std::sort(au.points.begin(),au.points.end(),
                [](const auto& a,const auto& b){return a.time<b.time;});
            notify(before);
        }
        return;
    }
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
    } else if (dragAutomation>=0 && dragAutomation<(int)clip.automations.size() &&
               rampDrag!=RampDrag::none) {
        const auto before=clip;
        auto& a=clip.automations[(size_t)dragAutomation];
        const auto& original=dragBefore.automations[(size_t)dragAutomation];
        if (rampDrag==RampDrag::move) {
            const double offset=timeForX(e.position.x)-dragStartTime;
            a.time=juce::jlimit(0.0,std::max(0.0,clip.length-original.duration),
                                quantize(original.time+offset));
        } else if (rampDrag==RampDrag::resize) {
            // Unlike instant events, a ramp's right edge may reach clip.length.
            // Preserve exact endpoint snapping instead of clipping to length-epsilon.
            const double raw=juce::jlimit(0.0,clip.length,
                (double)(e.position.x-axisLeft())/axisWidth()*clip.length);
            const double step=1.0/(1<<juce::jlimit(0,3,resolution.getSelectedId()-1));
            const double end=snap.getToggleState()
                ? juce::jlimit(0.0,clip.length,std::round(raw/step)*step) : raw;
            const double next=end-original.time;
            if (next<.01 || original.time+next>clip.length) return;
            a.duration=next;
            for (size_t i=0;i<a.points.size();++i)
                a.points[i].time=i+1==a.points.size() ? next
                    : original.points[i].time*next/original.duration;
        } else if (rampDrag==RampDrag::point && dragPoint>=0) {
            auto& p=a.points[(size_t)dragPoint];
            if (dragPoint>0 && dragPoint+1<(int)a.points.size()) {
                const double relative=quantize(timeForX(e.position.x))-a.time;
                p.time=juce::jlimit(a.points[(size_t)dragPoint-1].time+.0001,
                    a.points[(size_t)dragPoint+1].time-.0001,relative);
            }
            p.value=valueForY(dragAutomation,e.position.y);
            if (a.message.arguments[(size_t)a.argumentIndex].type==OscValueType::integer)
                p.value=std::round(p.value);
        }
        bool changed=a.time!=before.automations[(size_t)dragAutomation].time ||
            a.duration!=before.automations[(size_t)dragAutomation].duration;
        if (rampDrag==RampDrag::point && dragPoint>=0) {
            const auto& prior=before.automations[(size_t)dragAutomation].points[(size_t)dragPoint];
            changed=changed || a.points[(size_t)dragPoint].time!=prior.time ||
                a.points[(size_t)dragPoint].value!=prior.value;
        }
        if (changed) {
            if (validateOscClip(clip).failed()) { clip=before; repaint(); return; }
            notify(before,false); changedDuringDrag=true;
        }
    }
}
void OscClipEditor::mouseUp(const juce::MouseEvent&) {
    if (changedDuringDrag && onEdit) onEdit(dragBefore,clip,true);
    dragEvent=dragAutomation=dragPoint=-1;
    rampDrag=RampDrag::none; changedDuringDrag=false; repaint();
}
void OscClipEditor::mouseDoubleClick(const juce::MouseEvent& e) {
    if (e.position.y>=eventTop && e.position.y<eventTop+eventHeight) {
        for (int i=(int)clip.events.size()-1;i>=0;--i)
            if (std::abs(xForTime(clip.events[(size_t)i].time)-e.position.x)<10) {
                editEvent(i); return;
            }
    }
    const int index=automationAt(e.position.y);
    if (index>=0) {
        if (e.position.x<axisLeft() || e.position.y<laneTop(index)+20)
            editTrackAddress(index);
        else editAutomation(index);
        return;
    }
    if (clip.automations.size()<16 &&
        e.position.y>=laneTop((int)clip.automations.size()) &&
        e.position.y<laneTop((int)clip.automations.size())+laneHeight-4) {
        if (e.position.x<axisLeft()) editTrackAddress(-1);
        else if (e.position.x<=axisLeft()+axisWidth())
            createAutomationAt(quantize(timeForX(e.position.x)));
    }
}
void OscClipEditor::mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails& wheel) {
    const int total=((int)clip.automations.size()+1)*laneHeight+eventTop+eventHeight;
    laneScroll=juce::jlimit(0,std::max(0,total-getHeight()+42),
        laneScroll-(int)(wheel.deltaY*100));
    repaint();
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
void OscClipEditor::showContext(const juce::MouseEvent& e) {
    if (e.position.y>=eventTop && e.position.y<eventTop+eventHeight) {
        for (int i=(int)clip.events.size()-1;i>=0;--i)
            if (std::abs(xForTime(clip.events[(size_t)i].time)-e.position.x)<10) {
                chooseEvent(i); break;
            }
    } else if (const auto i=automationAt(e.position.y);i>=0) chooseAutomation(i);
    juce::PopupMenu menu;
    menu.addItem(1,"Edit",selectedEvent>=0 || selectedAutomation>=0);
    menu.addItem(2,"Duplicate",selectedEvent>=0 || selectedAutomation>=0);
    menu.addItem(3,"Delete",selectedEvent>=0 || selectedAutomation>=0);
    menu.addItem(4,"OSC track address",selectedAutomation>=0);
    auto safe=juce::Component::SafePointer<OscClipEditor>(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),[safe](int item) {
        if (!safe) return;
        if (item==1) {
            if (safe->selectedEvent>=0) safe->editEvent(safe->selectedEvent);
            else if (safe->selectedAutomation>=0) safe->editAutomation(safe->selectedAutomation);
        } else if (item==2) safe->duplicateSelection();
        else if (item==3) safe->deleteSelection();
        else if (item==4 && safe->selectedAutomation>=0)
            safe->editTrackAddress(safe->selectedAutomation);
    });
}
void OscClipEditor::addEvent() {
    if (clip.events.size()>=256) return;
    const auto before=clip;
    clip.events.push_back({cursor,"/cue",{}});
    chooseEvent((int)clip.events.size()-1);
    notify(before); editEvent(selectedEvent);
}
void OscClipEditor::addAutomation() { createAutomationAt(cursor); }

void OscClipEditor::createAutomationAt(double start) {
    if (clip.automations.size()>=16 || !std::isfinite(start)) return;
    const auto before=clip;
    OscAutomation au;
    au.time=juce::jlimit(0.0,clip.length,start);
    // One true second even when the visible axis is in musical beats.
    au.duration=clip.useBeats
        ? beatFromSeconds(tempo,secondsAtBeat(tempo,au.time)+1.0)-au.time
        : 1.0;
    if (!(au.duration>0) || au.time+au.duration>86400) return;
    au.message.address=newTrackAddress;
    au.message.arguments={{OscValueType::decimal,0,0,juce::String()}};
    au.points={{0,0},{au.duration,1}};
    clip.length=std::max(clip.length,au.time+au.duration);
    clip.automations.push_back(au);
    if (validateOscClip(clip).failed()) { clip=before; return; }
    chooseAutomation((int)clip.automations.size()-1);
    newTrackAddress="/parameter/"+juce::String((int)clip.automations.size()+1);
    notify(before);
}

void OscClipEditor::editTrackAddress(int index) {
    if (index>=(int)clip.automations.size()) return;
    const juce::String address=index<0 ? newTrackAddress :
        clip.automations[(size_t)index].message.address;
    auto* dialog=new juce::AlertWindow("OSC track address",
        "Address shared by the ramp's OSC messages.",
        juce::MessageBoxIconType::NoIcon,this);
    dialog->addTextEditor("address",address,"OSC address");
    dialog->addButton("Save",1); dialog->addButton("Cancel",0);
    auto safe=juce::Component::SafePointer<OscClipEditor>(this);
    const auto generation=revision;
    dialog->enterModalState(true,juce::ModalCallbackFunction::create(
        [safe,dialog,index,generation](int response) {
            if (!safe || response!=1 || generation!=safe->revision) return;
            const auto address=dialog->getTextEditorContents("address").trim();
            if (index<0) {
                OscClip test; test.events={{0,address,{}}};
                if (validateOscClip(test).failed()) return;
                safe->newTrackAddress=address; safe->repaint(); return;
            }
            if (index>=(int)safe->clip.automations.size()) return;
            const auto before=safe->clip;
            safe->clip.automations[(size_t)index].message.address=address;
            safe->notify(before);
        }),true);
}
void OscClipEditor::deleteSelection() {
    const auto before=clip;
    if (selectedEvent>=0 && selectedEvent<(int)clip.events.size()) {
        clip.events.erase(clip.events.begin()+selectedEvent); selectedEvent=-1;
    } else if (selectedAutomation>=0 && selectedAutomation<(int)clip.automations.size()) {
        clip.automations.erase(clip.automations.begin()+selectedAutomation);
        selectedAutomation=selectedPoint=-1;
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
        copy.time=juce::jlimit(0.0,clip.length-copy.duration,copy.time+.25);
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
    const auto old=clip.automations[(size_t)index];
    const auto type=old.message.arguments[(size_t)old.argumentIndex].type;
    auto* dialog=new juce::AlertWindow("OSC ramp",
        "Edit values and type. Drag the ramp to move it; use the right handle to resize.\n"
        "Double-click the track header to edit its address.",
        juce::MessageBoxIconType::NoIcon,this);
    dialog->addTextEditor("minimum",juce::String(old.points.front().value,3),"Start / Min");
    dialog->addTextEditor("maximum",juce::String(old.points.back().value,3),"End / Max");
    dialog->addTextEditor("type",type==OscValueType::integer?"i":"f","Type: f (float) or i (integer)");
    dialog->addButton("Save",1); dialog->addButton("Cancel",0);
    auto safe=juce::Component::SafePointer<OscClipEditor>(this);
    const auto generation=revision;
    dialog->enterModalState(true,juce::ModalCallbackFunction::create(
        [safe,dialog,index,generation](int answer) {
            if (!safe || answer!=1 || generation!=safe->revision ||
                index>=(int)safe->clip.automations.size()) return;
            double minimum=0,maximum=0;
            if (!parseFiniteNumber(dialog->getTextEditorContents("minimum"),minimum) ||
                !parseFiniteNumber(dialog->getTextEditorContents("maximum"),maximum)) return;
            const auto type=dialog->getTextEditorContents("type").trim().toLowerCase();
            if (type!="f" && type!="i") return;
            auto updated=safe->clip;
            auto& a=updated.automations[(size_t)index];
            const double previousStart=a.points.front().value;
            const double previousEnd=a.points.back().value;
            for (auto& p:a.points) {
                const double fraction=std::abs(previousEnd-previousStart)>1.e-12
                    ? (p.value-previousStart)/(previousEnd-previousStart)
                    : p.time/a.duration;
                p.value=minimum+(maximum-minimum)*fraction;
                if (type=="i") p.value=std::round(p.value);
            }
            a.points.front().value=type=="i" ? std::round(minimum) : minimum;
            a.points.back().value=type=="i" ? std::round(maximum) : maximum;
            a.message.arguments[(size_t)a.argumentIndex].type=
                type=="i" ? OscValueType::integer : OscValueType::decimal;
            if (validateOscClip(updated).failed()) return;
            const auto before=safe->clip;
            safe->clip=std::move(updated);
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
