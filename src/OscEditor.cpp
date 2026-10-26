#include "OscEditor.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace cuelist {
OscClipEditor::OscClipEditor(OscClip value,std::vector<TempoPoint> points,
             std::function<void(const OscClip&,const OscClip&,bool)> commit)
    : clip(std::move(value)),tempo(std::move(points)),onEdit(std::move(commit)) {
    setLookAndFeel(&theme); setName("sharedOscEditor"); setWantsKeyboardFocus(true);
    for (juce::Component* component:std::initializer_list<juce::Component*>{
        &unit,&resolution,&grid,&snap,&add,&ramp,&remove,&duplicate,&detach})
        addAndMakeVisible(component);
    unit.setName("oscTimeUnit");
    unit.addItem("Secondi",1); unit.addItem("Beat",2);
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
    for (int i=0;i<4;++i) resolution.addItem(juce::String(1<<i)+"/16",i+1);
    resolution.setSelectedId(2,juce::dontSendNotification);
    resolution.setTooltip("Risoluzione musicale o frazione di secondo dello snap.");
    grid.setName("oscGridVisible"); grid.setToggleState(true,juce::dontSendNotification);
    grid.onClick=[this] { repaint(); };
    snap.setName("oscSnapEnabled"); snap.setToggleState(true,juce::dontSendNotification);
    snap.onClick=[this] { repaint(); };
    add.setName("oscAddMessage"); add.setTooltip("Aggiungi un evento OSC istantaneo.");
    ramp.setName("oscAddAutomation"); ramp.setTooltip("Crea una rampa automatica OSC con due punti.");
    remove.setName("oscRemoveMessage"); duplicate.setName("oscDuplicate");
    add.onClick=[this] { addEvent(); };
    ramp.onClick=[this] { addAutomation(); };
    remove.onClick=[this] { deleteSelection(); };
    duplicate.onClick=[this] { duplicateSelection(); };
    detach.setName("oscDetach");
    detach.onClick=[this] { if (onDetach) onDetach(); };
    for (auto* button:{&add,&ramp,&remove,&duplicate,&detach}) button->setWantsKeyboardFocus(false);
    setSize(940,320);
}
OscClipEditor::~OscClipEditor() { setLookAndFeel(nullptr); }
void OscClipEditor::setFloating(bool floating) { detach.setButtonText(floating?"Riaggancia":"Stacca"); }
void OscClipEditor::loadClip(const OscClip& next) {
    ++revision; clip=next; selectedEvent=selectedAutomation=selectedPoint=-1;
    dragging=false; laneScroll=0;
    unit.setSelectedId(clip.useBeats ? 2 : 1,juce::dontSendNotification);
    repaint();
}
void OscClipEditor::resized() {
    const int w=getWidth();
    detach.setBounds(w-105,7,95,27);
    unit.setBounds(w-228,7,110,26);
    add.setBounds(13,7,35,26); ramp.setBounds(53,7,83,26);
    remove.setBounds(141,7,70,26); duplicate.setBounds(216,7,75,26);
    grid.setBounds(300,7,82,26); snap.setBounds(380,7,72,26);
    resolution.setBounds(452,8,85,24);
    repaint();
}
float OscClipEditor::xForTime(double time) const {
    return (float)axisLeft()+(float)(juce::jlimit(0.0,clip.length,time)/clip.length)*axisWidth();
}
double OscClipEditor::timeForX(float x) const {
    return juce::jlimit(0.0,clip.length-.00001,
        (double)(x-axisLeft())/axisWidth()*clip.length);
}
double OscClipEditor::quantize(double t) const {
    if (!snap.getToggleState()) return t;
    // Beat mode uses musical quarter-note subdivisions; seconds mode uses
    // exact fractions of one second independently of BPM.
    const double step=1.0/(1<<juce::jlimit(0,3,resolution.getSelectedId()-1));
    return juce::jlimit(0.0,std::max(0.0,clip.length-.00001),std::round(t/step)*step);
}
int OscClipEditor::automationAt(float y) const {
    for (int i=0;i<(int)clip.automations.size();++i)
        if (y>=laneTop(i) && y<laneTop(i)+laneHeight-4) return i;
    return -1;
}
float OscClipEditor::yForValue(int index,double value) const {
    const auto& a=clip.automations[(size_t)index];
    double lo=0,hi=1;
    for (auto& p:a.points) { lo=std::min(lo,p.value); hi=std::max(hi,p.value); }
    if (hi-lo<.01) hi=lo+1;
    const auto t=juce::jlimit(0.0,1.0,(value-lo)/(hi-lo));
    return laneTop(index)+laneHeight-13-(float)t*(laneHeight-28);
}
double OscClipEditor::valueForY(int index,float y) const {
    const auto& a=clip.automations[(size_t)index];
    double lo=0,hi=1;
    for (const auto& p:a.points) { lo=std::min(lo,p.value); hi=std::max(hi,p.value); }
    if (hi-lo<.01) hi=lo+1;
    const auto frac=juce::jlimit(0.0,1.0,
        (double)(laneTop(index)+laneHeight-13-y)/(laneHeight-28));
    return lo+(hi-lo)*frac;
}
void OscClipEditor::paint(juce::Graphics& g) {
    g.fillAll(palette::panel);
    label(g,"OSC  /  timeline",{13,38,225,24},13,palette::teal,true);
    label(g,"Clic: evento  ·  Doppio clic: modifica  ·  Trascina: sposta / curva",
          {230,38,getWidth()-245,24},10,palette::muted);
    const int left=axisLeft(),right=left+axisWidth();
    const auto ruler=juce::Rectangle<float>((float)left,rulerTop,(float)axisWidth(),rulerBottom-rulerTop);
    g.setColour(palette::background); g.fillRect(ruler);
    const double step=1.0/(1<<juce::jlimit(0,3,resolution.getSelectedId()-1));
    if (grid.getToggleState()) {
        // Render a bounded number of lines even for clips with very fine snap.
        const double visualStep=std::max(step,clip.length/260.0);
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
    label(g,"EVENTI",{8,eventTop+8,66,19},10,palette::teal,true);
    for (int i=0;i<(int)clip.events.size();++i) {
        const auto& e=clip.events[(size_t)i];
        const float x=xForTime(e.time);
        const float y=eventTop+10+(i%2)*17;
        g.setColour(i==selectedEvent ? palette::amber : palette::teal);
        g.fillRoundedRectangle(x-5,y,10,14,2);
        if (i==selectedEvent || axisWidth()/std::max(1,(int)clip.events.size())>74)
            label(g,e.address,{(int)x+7,(int)y-1,110,17},10,palette::text);
    }
    g.setColour(palette::muted);
    g.drawHorizontalLine(eventTop+eventHeight+4,(float)left,(float)right);
    for (int i=0;i<(int)clip.automations.size();++i) {
        const auto& a=clip.automations[(size_t)i];
        const int y=laneTop(i);
        if (y>=getHeight() || y+laneHeight<rulerBottom) continue;
        g.setColour(i%2 ? palette::raised : palette::background);
        g.fillRect(left,y,axisWidth(),laneHeight-4);
        label(g,"AUTO "+juce::String(i+1),{6,y+4,68,16},9,i==selectedAutomation ? palette::amber : palette::teal,true);
        label(g,a.message.address,{left+9,y+2,std::max(25,axisWidth()-16),16},10,palette::muted);
        const float x0=xForTime(a.time),x1=xForTime(a.time+a.duration);
        g.setColour(palette::teal.withAlpha(.10f));
        g.fillRect(x0,(float)y+20,std::max(1.0f,x1-x0),(float)laneHeight-25);
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
    dragging=false; changedDuringDrag=false; dragBefore=clip;
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
    if (index<0) return;
    chooseAutomation(index);
    const auto& automation=clip.automations[(size_t)index];
    for (int i=0;i<(int)automation.points.size();++i) {
        const auto& p=automation.points[(size_t)i];
        if (std::abs(xForTime(automation.time+p.time)-e.position.x)<8 &&
            std::abs(yForValue(index,p.value)-e.position.y)<9) {
            selectedPoint=dragPoint=i; dragAutomation=index; repaint(); return;
        }
    }
    if (cursor>=automation.time && cursor<=automation.time+automation.duration) {
        // Click a curve to add a controllable intermediate point.
        if (automation.points.size()>=128) return;
        const auto relative=cursor-automation.time;
        if (relative<=.0001 || relative>=automation.duration-.0001) return;
        const auto before=clip;
        auto& au=clip.automations[(size_t)index];
        au.points.push_back({relative,valueForY(index,e.position.y)});
        std::sort(au.points.begin(),au.points.end(),
            [](const auto& a,const auto& b){return a.time<b.time;});
        notify(before);
    }
}
void OscClipEditor::mouseDrag(const juce::MouseEvent& e) {
    if (dragEvent>=0 && dragEvent<(int)clip.events.size()) {
        const auto before=clip;
        clip.events[(size_t)dragEvent].time=quantize(timeForX(e.position.x));
        if (clip.events[(size_t)dragEvent].time!=before.events[(size_t)dragEvent].time) {
            notify(before,false); changedDuringDrag=true;
        }
    } else if (dragAutomation>=0 && dragPoint>=0) {
        const auto before=clip;
        auto& a=clip.automations[(size_t)dragAutomation];
        auto& p=a.points[(size_t)dragPoint];
        if (dragPoint>0 && dragPoint+1<(int)a.points.size()) {
            const auto relative=quantize(timeForX(e.position.x))-a.time;
            p.time=juce::jlimit(a.points[(size_t)dragPoint-1].time+.0001,
                a.points[(size_t)dragPoint+1].time-.0001,relative);
        }
        p.value=valueForY(dragAutomation,e.position.y);
        if (a.message.arguments[(size_t)a.argumentIndex].type==OscValueType::integer)
            p.value=std::round(p.value);
        notify(before,false); changedDuringDrag=true;
    }
}
void OscClipEditor::mouseUp(const juce::MouseEvent&) {
    if (changedDuringDrag && onEdit) onEdit(dragBefore,clip,true);
    dragEvent=dragAutomation=dragPoint=-1;
    changedDuringDrag=false; repaint();
}
void OscClipEditor::mouseDoubleClick(const juce::MouseEvent& e) {
    if (e.position.y>=eventTop && e.position.y<eventTop+eventHeight) {
        for (int i=(int)clip.events.size()-1;i>=0;--i)
            if (std::abs(xForTime(clip.events[(size_t)i].time)-e.position.x)<10) {
                editEvent(i); return;
            }
    }
    const int index=automationAt(e.position.y);
    if (index>=0) editAutomation(index);
}
void OscClipEditor::mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails& wheel) {
    const int total=(int)clip.automations.size()*laneHeight+eventTop+eventHeight;
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
    menu.addItem(1,"Modifica",selectedEvent>=0 || selectedAutomation>=0);
    menu.addItem(2,"Duplica",selectedEvent>=0 || selectedAutomation>=0);
    menu.addItem(3,"Elimina",selectedEvent>=0 || selectedAutomation>=0);
    auto safe=juce::Component::SafePointer<OscClipEditor>(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),[safe](int item) {
        if (!safe) return;
        if (item==1) {
            if (safe->selectedEvent>=0) safe->editEvent(safe->selectedEvent);
            else if (safe->selectedAutomation>=0) safe->editAutomation(safe->selectedAutomation);
        } else if (item==2) safe->duplicateSelection();
        else if (item==3) safe->deleteSelection();
    });
}
void OscClipEditor::addEvent() {
    if (clip.events.size()>=256) return;
    const auto before=clip;
    clip.events.push_back({cursor,"/cue",{}});
    chooseEvent((int)clip.events.size()-1);
    notify(before); editEvent(selectedEvent);
}
void OscClipEditor::addAutomation() {
    if (clip.automations.size()>=16) return;
    const auto before=clip;
    OscAutomation au;
    au.time=juce::jlimit(0.0,std::max(0.0,clip.length-.01),cursor);
    au.duration=std::min(1.0,clip.length-au.time);
    if (au.duration<=0) return;
    au.message.address="/parameter";
    au.message.arguments={{OscValueType::decimal,0,0,juce::String()}};
    au.points={{0,0},{au.duration,1}};
    clip.automations.push_back(au);
    chooseAutomation((int)clip.automations.size()-1);
    notify(before); editAutomation(selectedAutomation);
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
    auto* dialog=new juce::AlertWindow("Messaggio OSC",
        "Tempo, address e argomenti in ordine: i:7; f:0.5; s:scena; b:true",
        juce::MessageBoxIconType::NoIcon,this);
    dialog->addTextEditor("time",juce::String(event.time,3),clip.useBeats?"Beat":"Secondi");
    dialog->addTextEditor("address",event.address,"OSC address");
    dialog->addTextEditor("args",describeArguments(event.arguments),"Argomenti");
    dialog->addButton("Salva",1); dialog->addButton("Annulla",0);
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
    auto* dialog=new juce::AlertWindow("Automazione OSC",
        "Punti relativi all'inizio: tempo:valore; tempo:valore.\n"
        "Semplice = due punti; avanzata = piu punti (interpolazione lineare).",
        juce::MessageBoxIconType::NoIcon,this);
    dialog->addTextEditor("time",juce::String(old.time,3),"Inizio");
    dialog->addTextEditor("duration",juce::String(old.duration,3),"Durata");
    dialog->addTextEditor("address",old.message.address,"Address OSC");
    dialog->addTextEditor("args",describeArguments(old.message.arguments),"Argomenti");
    dialog->addTextEditor("index",juce::String(old.argumentIndex+1),"Argomento numerico (1, 2, ...)");
    dialog->addTextEditor("points",describePoints(old.points),"Punti di controllo");
    dialog->addButton("Salva",1); dialog->addButton("Annulla",0);
    auto safe=juce::Component::SafePointer<OscClipEditor>(this); const auto generation=revision;
    dialog->enterModalState(true,juce::ModalCallbackFunction::create(
        [safe,dialog,index,generation](int answer) {
            if (!safe || answer!=1 || generation!=safe->revision || index>=(int)safe->clip.automations.size()) return;
            const auto time=dialog->getTextEditorContents("time").trim();
            const auto length=dialog->getTextEditorContents("duration").trim();
            if (time.isEmpty() || length.isEmpty() ||
                time.retainCharacters("0123456789.")!=time ||
                length.retainCharacters("0123456789.")!=length) return;
            auto updated=safe->clip;
            auto& a=updated.automations[(size_t)index];
            a.time=time.getDoubleValue(); a.duration=length.getDoubleValue();
            a.message.address=dialog->getTextEditorContents("address").trim();
            a.argumentIndex=dialog->getTextEditorContents("index").getIntValue()-1;
            if (parseArguments(dialog->getTextEditorContents("args"),a.message.arguments).failed() ||
                parsePoints(dialog->getTextEditorContents("points"),a.points).failed() ||
                validateOscClip(updated).failed()) return;
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
        if (part.length()<2 || part[1]!=':') return juce::Result::fail("Argomento senza tipo.");
        const auto v=part.substring(2).trim();
        OscValue a;
        if (part[0]=='i') {
            if (v.isEmpty() || v.retainCharacters("-0123456789")!=v) return juce::Result::fail("Intero OSC non valido.");
            const auto n=v.getLargeIntValue();
            if (n < -2147483648LL || n>2147483647LL) return juce::Result::fail("Intero OSC fuori range.");
            a.type=OscValueType::integer; a.integer=(int)n;
        } else if (part[0]=='f') {
            if (v.isEmpty() || v.retainCharacters("-0123456789.eE+")!=v) return juce::Result::fail("Float OSC non valido.");
            a.type=OscValueType::decimal; a.decimal=(float)v.getDoubleValue();
            if (!std::isfinite(a.decimal)) return juce::Result::fail("Float OSC non valido.");
        } else if (part[0]=='s') { a.type=OscValueType::text; a.text=v; }
        else if (part[0]=='b') {
            if (v!="true" && v!="false") return juce::Result::fail("Boolean OSC: true o false.");
            a.type=OscValueType::boolean; a.boolean=v=="true";
        } else return juce::Result::fail("Tipo OSC sconosciuto.");
        output.push_back(std::move(a));
        if (output.size()>16) return juce::Result::fail("Massimo 16 argomenti.");
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
        if (colon<=0) return juce::Result::fail("Usa tempo:valore; tempo:valore.");
        const auto t=v.substring(0,colon).trim(),x=v.substring(colon+1).trim();
        if (t.isEmpty() || x.isEmpty() ||
            t.retainCharacters("0123456789.")!=t ||
            x.retainCharacters("-0123456789.eE+")!=x) return juce::Result::fail("Punto non valido.");
        output.push_back({t.getDoubleValue(),x.getDoubleValue()});
        if (output.size()>128) return juce::Result::fail("Troppi punti.");
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
