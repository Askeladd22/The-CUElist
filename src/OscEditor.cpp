#include "OscEditor.h"
#include <cmath>
#include <cstdint>

namespace cuelist {
OscClipEditor::OscClipEditor(OscClip value,std::vector<TempoPoint> points,
                           std::function<void(const OscClip&)> commit)
    : clip(std::move(value)),tempo(std::move(points)),onEdit(std::move(commit)) {
    setLookAndFeel(&theme);
    setName("sharedOscEditor");
    for (juce::Component* component:std::initializer_list<juce::Component*>{
        &eventList,&unit,&add,&remove,&detach}) addAndMakeVisible(component);
    eventList.setName("oscEventList");
    unit.setName("oscTimeUnit");
    unit.addItem("Secondi",1); unit.addItem("Beat",2);
    unit.setSelectedId(clip.useBeats ? 2 : 1,juce::dontSendNotification);
    unit.setTooltip("Tempi in secondi oppure beat. Il cambio converte gli eventi con la mappa BPM.");
    unit.onChange=[this] {
        const bool beats=unit.getSelectedId()==2;
        if (clip.useBeats==beats) return;
        for (auto& e:clip.events)
            e.time=beats ? beatFromSeconds(tempo,e.time) : secondsAtBeat(tempo,e.time);
        clip.length=beats ? beatFromSeconds(tempo,clip.length) : secondsAtBeat(tempo,clip.length);
        clip.useBeats=beats;
        notify();
    };
    add.setName("oscAddMessage");
    add.setTooltip("Aggiungi un messaggio OSC. Doppio clic su un messaggio per modificarlo.");
    remove.setName("oscRemoveMessage");
    remove.setTooltip("Elimina il messaggio selezionato.");
    add.onClick=[this] { addEvent(); };
    remove.onClick=[this] {
        if (selected<0 || selected>=(int)clip.events.size()) return;
        clip.events.erase(clip.events.begin()+selected);
        selected=clip.events.empty() ? -1 : std::min(selected,(int)clip.events.size()-1);
        eventList.updateContent();
        if (selected>=0) eventList.selectRow(selected);
        notify();
    };
    detach.setName("oscDetach");
    detach.onClick=[this] { if (onDetach) onDetach(); };
    for (auto* b:{&add,&remove,&detach}) b->setWantsKeyboardFocus(false);
    setSize(940,250);
    if (!clip.events.empty()) eventList.selectRow(0);
}
OscClipEditor::~OscClipEditor() { setLookAndFeel(nullptr); }
void OscClipEditor::setFloating(bool floating) { detach.setButtonText(floating ? "Riaggancia" : "Stacca"); }
void OscClipEditor::loadClip(const OscClip& replacement) {
    ++revision;
    clip=replacement; selected=-1; eventList.updateContent();
    unit.setSelectedId(clip.useBeats ? 2 : 1,juce::dontSendNotification);
    if (!clip.events.empty()) eventList.selectRow(0);
    repaint();
}
void OscClipEditor::paint(juce::Graphics& g) {
    g.fillAll(palette::panel);
    label(g,"OSC  /  messaggi UDP",{12,7,280,28},15,palette::teal,true);
    label(g,"Doppio clic su un messaggio per cambiare tempo, indirizzo e argomenti",
          {14,42,getWidth()-28,24},11,palette::muted);
    label(g,"Unita",{getWidth()-390,8,48,27},11,palette::muted);
}
void OscClipEditor::resized() {
    const int w=getWidth(),h=getHeight();
    detach.setBounds(w-103,7,93,28);
    unit.setBounds(w-336,9,107,25);
    add.setBounds(w-221,8,39,28);
    remove.setBounds(w-175,8,67,28);
    eventList.setBounds(12,72,std::max(20,w-24),std::max(28,h-83));
}
void OscClipEditor::paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool active) {
    if (row<0 || row>=(int)clip.events.size()) return;
    if (active) g.fillAll(palette::teal.withAlpha(.2f));
    const auto& e=clip.events[(size_t)row];
    label(g,juce::String(e.time,3)+(clip.useBeats ? " beat   " : " s   ")+e.address+
          (e.arguments.empty() ? juce::String() : "   ["+describeArguments(e.arguments)+"]"),
          {9,0,width-16,height},12,active ? palette::text : palette::muted);
}
void OscClipEditor::addEvent() {
    if (clip.events.size()>=256) return;
    OscEvent e;
    e.time=clip.events.empty() ? 0.0 : std::min(clip.length-.001,clip.events.back().time+.5);
    e.time=std::max(0.0,e.time);
    e.address="/cue";
    clip.events.push_back(e);
    selected=(int)clip.events.size()-1;
    eventList.updateContent(); eventList.selectRow(selected);
    notify(); editEvent(selected);
}
void OscClipEditor::editEvent(int index) {
    if (index<0 || index>=(int)clip.events.size()) return;
    selected=index;
    const auto event=clip.events[(size_t)index];
    auto* dialog=new juce::AlertWindow("Modifica messaggio OSC",
        "Tempo, indirizzo e argomenti; usa i:7; f:0.5; s:scena per i tipi.",juce::MessageBoxIconType::NoIcon,this);
    dialog->addTextEditor("time",juce::String(event.time,3),clip.useBeats ? "Tempo (beat)" : "Tempo (secondi)");
    dialog->addTextEditor("address",event.address,"Indirizzo OSC");
    dialog->addTextEditor("args",describeArguments(event.arguments),"Argomenti");
    dialog->addButton("Salva",1);
    dialog->addButton("Annulla",0);
    auto safe=juce::Component::SafePointer<OscClipEditor>(this);
    const auto generation=revision;
    dialog->enterModalState(true,juce::ModalCallbackFunction::create(
        [safe,dialog,index,generation](int answer) {
            if (!safe || answer!=1 || safe->revision!=generation || index>=(int)safe->clip.events.size()) return;
            const auto timeText=dialog->getTextEditorContents("time").trim();
            if (timeText.isEmpty() || timeText.retainCharacters("0123456789.")!=timeText) return;
            auto proposed=safe->clip;
            auto& e=proposed.events[(size_t)index];
            e.time=timeText.getDoubleValue();
            e.address=dialog->getTextEditorContents("address").trim();
            if (parseArguments(dialog->getTextEditorContents("args"),e.arguments).failed()) return;
            if (e.time>=proposed.length) proposed.length=std::min(86400.0,e.time+1.0);
            if (validateOscClip(proposed).failed()) return;
            safe->clip=std::move(proposed);
            safe->eventList.updateContent();
            safe->eventList.selectRow(index);
            safe->notify();
        }),true);
}
void OscClipEditor::notify() {
    if (validateOscClip(clip).wasOk() && onEdit) onEdit(clip);
    eventList.repaint();
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
}
