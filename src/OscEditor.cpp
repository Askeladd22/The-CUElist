#include "OscEditor.h"
#include <cmath>
namespace cuelist {
OscClipEditor::OscClipEditor(OscClip value,std::vector<TempoPoint> points,
                           std::function<void(const OscClip&)> commit)
    : clip(std::move(value)),tempo(std::move(points)),onEdit(std::move(commit)) {
    setLookAndFeel(&theme);
    setName("sharedOscEditor");
    for (juce::Component* component:std::initializer_list<juce::Component*>{
         &eventList,&unit,&time,&address,&args,&length,&add,&remove,&detach,&help})
        addAndMakeVisible(component);
    eventList.setName("oscEventList");
    unit.setName("oscTimeUnit");
    unit.addItem("Secondi",1); unit.addItem("Beat",2);
    unit.setSelectedId(clip.useBeats ? 2 : 1,juce::dontSendNotification);
    unit.setTooltip("I tempi sono misurati dall'inizio della clip; cambiando unita le posizioni vengono convertite con la mappa BPM.");
    time.setName("oscEventTime"); address.setName("oscEventAddress");
    args.setName("oscEventArguments"); length.setName("oscClipLength");
    time.setInputRestrictions(0,"0123456789."); length.setInputRestrictions(0,"0123456789.");
    args.setTooltip("Argomenti separati da punto e virgola: i:12; f:0.5; s:testo. I tipi sono int32, float32 e string.");
    help.setText("Argomenti: i:7; f:0.5; s:ciao. Un messaggio per riga della lista.",juce::dontSendNotification);
    help.setColour(juce::Label::textColourId,palette::muted);
    help.setJustificationType(juce::Justification::centredLeft);
    length.setText(juce::String(clip.length,3),false);
    unit.onChange=[this] {
        if (updating) return;
        const bool nextBeats=unit.getSelectedId()==2;
        if (clip.useBeats==nextBeats) return;
        for (auto& event:clip.events)
            event.time=nextBeats ? beatFromSeconds(tempo,event.time) : secondsAtBeat(tempo,event.time);
        clip.length=nextBeats ? beatFromSeconds(tempo,clip.length) : secondsAtBeat(tempo,clip.length);
        clip.useBeats=nextBeats;
        length.setText(juce::String(clip.length,3),false);
        showEvent(); notify();
    };
    for (auto* editor:{&time,&address,&args}) editor->onTextChange=[this] { if (!updating) commitEvent(); };
    length.onTextChange=[this] {
        if (updating) return;
        const double n=length.getText().getDoubleValue();
        if (n<=0 || !std::isfinite(n)) return;
        const double previous=clip.length; clip.length=n;
        if (validateOscClip(clip).wasOk()) notify();
        else clip.length=previous;
    };
    add.onClick=[this] {
        if (clip.events.size()>=256) return;
        const OscEvent e{0,"/cue",{}};
        clip.events.push_back(e);
        selected=(int)clip.events.size()-1;
        eventList.updateContent(); eventList.selectRow(selected);
        showEvent(); notify();
    };
    remove.onClick=[this] {
        if (selected<0 || selected>=(int)clip.events.size()) return;
        clip.events.erase(clip.events.begin()+selected);
        selected=clip.events.empty() ? -1 : std::min(selected,(int)clip.events.size()-1);
        eventList.updateContent();
        if (selected>=0) eventList.selectRow(selected);
        showEvent(); notify();
    };
    detach.onClick=[this] { if (onDetach) onDetach(); };
    for (auto* b:{&add,&remove,&detach}) b->setWantsKeyboardFocus(false);
    setSize(940,250);
    if (!clip.events.empty()) eventList.selectRow(0);
    showEvent();
}
OscClipEditor::~OscClipEditor() { setLookAndFeel(nullptr); }
void OscClipEditor::setFloating(bool isFloating) { detach.setButtonText(isFloating ? "Riaggancia" : "Stacca"); }
void OscClipEditor::loadClip(const OscClip& replacement) {
    clip=replacement; selected=-1; eventList.updateContent();
    updating=true;
    unit.setSelectedId(clip.useBeats ? 2 : 1,juce::dontSendNotification);
    length.setText(juce::String(clip.length,3),false);
    updating=false;
    if (!clip.events.empty()) eventList.selectRow(0);
    showEvent();
}
void OscClipEditor::paint(juce::Graphics& g) {
    g.fillAll(palette::panel);
    label(g,"OSC  /  messaggi UDP",{12,8,280,23},15,palette::teal,true);
    label(g,"Unita",{13,42,60,20},11,palette::muted);
    label(g,"Durata clip",{207,42,90,20},11,palette::muted);
    label(g,"Tempo",{328,42,65,20},11,palette::muted);
    label(g,"Indirizzo OSC",{438,42,120,20},11,palette::muted);
    label(g,"Argomenti tipizzati",{12,getHeight()-72,200,17},11,palette::muted);
}
void OscClipEditor::resized() {
    const int w=getWidth(),h=getHeight();
    detach.setBounds(w-100,7,90,28);
    unit.setBounds(69,39,115,25); length.setBounds(280,39,72,25);
    time.setBounds(382,39,60,25); address.setBounds(555,39,std::max(140,w-567),25);
    eventList.setBounds(12,76,std::max(20,w-24),std::max(30,h-159));
    args.setBounds(12,h-52,std::max(80,w-200),27);
    add.setBounds(w-178,h-52,79,27); remove.setBounds(w-91,h-52,79,27);
}
void OscClipEditor::paintListBoxItem(int row,juce::Graphics& g,int w,int h,bool active) {
    if (row<0 || row>=(int)clip.events.size()) return;
    if (active) g.fillAll(palette::teal.withAlpha(.2f));
    const auto& e=clip.events[(size_t)row];
    label(g,juce::String(e.time,3)+(clip.useBeats ? " beat   " : " s   ")+e.address+
        "   ["+describeArguments(e.arguments)+"]",{9,0,w-16,h},12,active ? palette::text : palette::muted);
}
void OscClipEditor::selectedRowsChanged(int row) { selected=row; showEvent(); }
void OscClipEditor::showEvent() {
    updating=true;
    if (selected>=0 && selected<(int)clip.events.size()) {
        const auto& e=clip.events[(size_t)selected];
        time.setText(juce::String(e.time,3),false);
        address.setText(e.address,false);
        args.setText(describeArguments(e.arguments),false);
    } else {
        time.clear(); address.clear(); args.clear();
    }
    time.setEnabled(selected>=0); address.setEnabled(selected>=0);
    args.setEnabled(selected>=0); remove.setEnabled(selected>=0);
    updating=false; repaint();
}
juce::Result OscClipEditor::parseArguments(const juce::String& text,std::vector<OscValue>& output) {
    output.clear();
    if (text.trim().isEmpty()) return juce::Result::ok();
    for (auto& item:juce::StringArray::fromTokens(text,";","")) {
        const auto part=item.trim();
        if (part.length()<2 || part[1]!=':') return juce::Result::fail("Usa i:, f:, s:");
        OscValue a; const auto value=part.substring(2).trim();
        if (part[0]=='i') {
            if (value.isEmpty() || value.retainCharacters("-0123456789")!=value) return juce::Result::fail("Intero OSC non valido.");
            const auto parsed=value.getLargeIntValue();
            if (parsed < -2147483648LL || parsed > 2147483647LL) return juce::Result::fail("Intero OSC fuori range.");
            a.type=OscValueType::integer; a.integer=(int)parsed;
        } else if (part[0]=='f') {
            if (value.isEmpty() || value.retainCharacters("-0123456789.eE+")!=value) return juce::Result::fail("Float OSC non valido.");
            a.type=OscValueType::decimal; a.decimal=(float)value.getDoubleValue();
            if (!std::isfinite(a.decimal)) return juce::Result::fail("Float OSC non finito.");
        } else if (part[0]=='s') {
            a.type=OscValueType::text; a.text=value;
        } else return juce::Result::fail("Tipo OSC sconosciuto.");
        output.push_back(std::move(a));
        if (output.size()>16) return juce::Result::fail("Massimo 16 argomenti per messaggio.");
    }
    return juce::Result::ok();
}
juce::String OscClipEditor::describeArguments(const std::vector<OscValue>& values) {
    juce::StringArray items;
    for (const auto& a:values)
        items.add(a.type==OscValueType::integer ? "i:"+juce::String(a.integer)
            : a.type==OscValueType::decimal ? "f:"+juce::String(a.decimal)
            : "s:"+a.text);
    return items.joinIntoString("; ");
}
double OscClipEditor::beatFromSeconds(const std::vector<TempoPoint>& points,double seconds) {
    double low=0,high=std::max(1.0,seconds*2);
    while (secondsAtBeat(points,high)<seconds && high<1e7) high*=2;
    for (int i=0;i<48;++i) {
        const double mid=(low+high)*.5;
        if (secondsAtBeat(points,mid)<seconds) low=mid; else high=mid;
    }
    return (low+high)*.5;
}
void OscClipEditor::commitEvent() {
    if (selected<0 || selected>=(int)clip.events.size()) return;
    auto next=clip;
    auto& e=next.events[(size_t)selected];
    const auto t=time.getText().trim();
    if (t.isEmpty() || t.retainCharacters("0123456789.")!=t) return;
    e.time=t.getDoubleValue(); e.address=address.getText().trim();
    if (parseArguments(args.getText(),e.arguments).failed() || validateOscClip(next).failed()) return;
    clip=std::move(next); notify(); eventList.repaint();
}
void OscClipEditor::notify() {
    if (validateOscClip(clip).wasOk() && onEdit) onEdit(clip);
    eventList.repaint();
}
}
