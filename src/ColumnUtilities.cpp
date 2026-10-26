#include "MainComponent.h"

namespace cuelist {
namespace {
class UtilityIcon final : public juce::ToggleButton {
public:
    explicit UtilityIcon(bool invert) : phase(invert) { setButtonText(invert ? "Inversione di fase" : "Mute"); }
    void paintButton(juce::Graphics& g,bool hover,bool down) override {
        const auto box=getLocalBounds().toFloat().reduced(2);
        const auto active=phase ? palette::teal : palette::red;
        g.setColour(getToggleState() ? active.withAlpha(.18f) : palette::raised);
        g.fillRoundedRectangle(box,4);
        g.setColour(getToggleState() ? active : hover || down ? palette::text : palette::muted);
        g.drawRoundedRectangle(box,4,getToggleState() ? 1.4f : .7f);
        const auto icon=juce::Rectangle<float>(20,20).withCentre(box.getCentre());
        if (phase) {
            g.drawEllipse(icon.reduced(2),2.3f);
            g.drawLine(icon.getX()+3,icon.getBottom(),icon.getRight()-3,icon.getY(),2.8f);
        } else {
            const float x=icon.getX(),y=icon.getY();
            juce::Path speaker; speaker.startNewSubPath(x+2,y+7); speaker.lineTo(x+7,y+7);
            speaker.lineTo(x+14,y+2); speaker.lineTo(x+14,y+18); speaker.lineTo(x+7,y+13);
            speaker.lineTo(x+2,y+13); speaker.closeSubPath(); g.fillPath(speaker);
            g.drawLine(x+1,y+1,x+19,y+19,3.0f);
        }
    }
private: bool phase;
};
}
struct ColumnUtilities::Strip final : juce::Component {
    MainComponent& owner;
    int id, index;
    juce::Slider gain,pan;
    UtilityIcon mute{false},phase{true};
    juce::ComboBox output;
    juce::TextButton oscDestination{"OSC / destinazione"};
    int routingChannels=-1, routingOutput=-1;
    ColumnType routingType=ColumnType::audio;
    juce::String midiSignature;
    juce::StringArray midiIds;
    double nextMidiScan=0;
    float meterLevel=0;
    juce::String lastMidiId;
    Strip(MainComponent& p,int i) : owner(p),id(p.show.columns[i].id),index(i) {
        for (auto* slider : {&gain,&pan}) {
            addAndMakeVisible(slider);
            slider->setDoubleClickReturnValue(true,0);
        }
        gain.setSliderStyle(juce::Slider::LinearVertical);
        gain.setTextBoxStyle(juce::Slider::TextBoxBelow,false,59,19);
        pan.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        pan.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        gain.setName("columnGainDb"); pan.setName("columnPan"); mute.setName("columnMute"); phase.setName("columnPhase");
        addAndMakeVisible(output); output.setName("columnOutput");
        addAndMakeVisible(oscDestination); oscDestination.setName("columnOscDestination");
        oscDestination.setTooltip("Indirizzo IP o host e porta UDP del ricevitore OSC.");
        oscDestination.onClick=[this] {
            const auto columnId=id;
            const auto& col=owner.show.columns[(size_t)index];
            auto* dialog=new juce::AlertWindow("Destinazione OSC UDP","Inserisci host/IP e porta del ricevitore OSC.",juce::MessageBoxIconType::NoIcon,&owner);
            dialog->addTextEditor("host",col.oscHost,"Host / indirizzo IP");
            dialog->addTextEditor("port",juce::String(col.oscPort),"Porta UDP (1-65535)");
            dialog->addButton("Salva",1); dialog->addButton("Annulla",0);
            auto safe=juce::Component::SafePointer<MainComponent>(&owner);
            dialog->enterModalState(true,juce::ModalCallbackFunction::create([safe,dialog,columnId](int result) {
                if (!safe || result!=1) return;
                const auto host=dialog->getTextEditorContents("host").trim();
                const auto portText=dialog->getTextEditorContents("port").trim();
                if (portText.isEmpty() || portText.retainCharacters("0123456789")!=portText) return;
                for (int i=0;i<(int)safe->show.columns.size();++i)
                    if (safe->show.columns[(size_t)i].id==columnId) { safe->setOscDestination(i,host,portText.getIntValue()); return; }
            }),true);
        };
        output.setTooltip("Uscita audio o MIDI della colonna.");
        output.onChange=[this] {
            if (owner.show.columns[index].type==ColumnType::midi) {
                const int selected=output.getSelectedId()-1;
                if (selected>=0 && selected<midiIds.size()) owner.setMidiOutput(index,midiIds[selected]);
            } else if (output.getSelectedId()>0) owner.setColumnOutput(index,output.getSelectedId()-1);
        };
        gain.setRange(-60,12,0.1); gain.setNumDecimalPlacesToDisplay(1); gain.setTextValueSuffix(" dB");
        pan.setRange(-1,1,0.01);
        pan.textFromValueFunction=[](double value) { return std::abs(value)<0.005 ? juce::String("C") : juce::String(value<0 ? "L " : "R ")+juce::String((int)std::round(std::abs(value)*100)); };
        pan.valueFromTextFunction=[](const juce::String& text) { return text.containsIgnoreCase("L") ? -std::abs(text.retainCharacters("0123456789.").getDoubleValue())/100 : text.containsIgnoreCase("R") ? std::abs(text.retainCharacters("0123456789.").getDoubleValue())/100 : text.getDoubleValue(); };
        pan.updateText();
        gain.setTooltip("Livello di tutte le clip della colonna. Doppio click: 0 dB.");
        pan.setTooltip("Pan / bilanciamento stereo. Doppio click: centro.");
        phase.setTooltip("Inverte la polarita di entrambi i canali della colonna."); mute.setTooltip("Silenzia tutta la colonna senza fermare le clip.");
        addAndMakeVisible(mute); addAndMakeVisible(phase); mute.setWantsKeyboardFocus(false); phase.setWantsKeyboardFocus(false);
        auto update=[this](int control) {
            const auto col=owner.show.columns[index];
            owner.setColumnUtilities(index,
                control==0 ? juce::Decibels::decibelsToGain((float)gain.getValue(),-60.0f) : col.gain,
                control==1 ? (float)pan.getValue() : col.pan,
                control==2 ? mute.getToggleState() : col.muted,
                control==3 ? phase.getToggleState() : col.phaseInverted);
        };
        gain.onValueChange=[update] { update(0); }; pan.onValueChange=[update] { update(1); };
        mute.onClick=[update] { update(2); }; phase.onClick=[update] { update(3); };
    }
    void sync() {
        const auto& col=owner.show.columns[index];
        gain.setValue(juce::Decibels::gainToDecibels(col.gain,-60.0f),juce::dontSendNotification);
        pan.setValue(col.pan,juce::dontSendNotification); mute.setToggleState(col.muted,juce::dontSendNotification);
        phase.setToggleState(col.phaseInverted,juce::dontSendNotification);
        for (juce::Component* control : std::initializer_list<juce::Component*>{&gain,&pan,&mute,&phase}) control->setVisible(col.type==ColumnType::audio);
        oscDestination.setVisible(col.type==ColumnType::osc);
        output.setVisible(col.type!=ColumnType::osc);
        syncRouting();
    }
    void syncRouting() {
        const auto& col=owner.show.columns[index]; int channels=2;
        if (col.type==ColumnType::osc) {
            routingType=ColumnType::osc;
            oscDestination.setButtonText(col.oscHost+":"+juce::String(col.oscPort));
            return;
        }
        if (col.type==ColumnType::midi) {
            const double now=juce::Time::getMillisecondCounterHiRes();
            if (routingType==ColumnType::midi && lastMidiId==col.midiOutputId && now<nextMidiScan) return;
            nextMidiScan=now+1000; lastMidiId=col.midiOutputId;
            const auto available=juce::MidiOutput::getAvailableDevices();
            juce::String signature=col.midiOutputId+"|";
            for (const auto& device:available) signature+=device.identifier+"/"+device.name+"|";
            if (routingType==ColumnType::midi && midiSignature==signature) return;
            routingType=col.type; midiSignature=signature; midiIds.clear(); midiIds.add({});
            output.clear(juce::dontSendNotification); output.setEnabled(true); output.addItem("Nessuna uscita MIDI",1);
            int selected=col.midiOutputId.isEmpty() ? 1 : 0;
            for (const auto& device:available) {
                midiIds.add(device.identifier); output.addItem(device.name,midiIds.size());
                if (device.identifier==col.midiOutputId) selected=midiIds.size();
            }
            if (!selected) { midiIds.add(col.midiOutputId); selected=midiIds.size(); output.addItem("Uscita MIDI assente",selected); }
            output.setSelectedId(selected,juce::dontSendNotification); return;
        }
        if (auto* device=owner.engine.devices.getCurrentAudioDevice()) channels=std::max(1,device->getActiveOutputChannels().countNumberOfSetBits());
        if (routingChannels==channels && routingOutput==col.output && routingType==col.type) return;
        routingChannels=channels; routingOutput=col.output; routingType=col.type;
        output.clear(juce::dontSendNotification); output.setEnabled(col.type==ColumnType::audio);
        if (col.type!=ColumnType::audio) {
            output.addItem(col.type==ColumnType::midi ? "MIDI - previsto" : "OSC - previsto",1);
            output.setSelectedId(1,juce::dontSendNotification); return;
        }
        for (int i=0;i<channels;i+=2) output.addItem("Out "+juce::String(i+1)+(i+1<channels ? " / "+juce::String(i+2) : " (mono)"),i+1);
        if (col.output>=channels || col.output%2!=0) output.addItem("Out "+juce::String(col.output+1)+" / "+juce::String(col.output+2)+" (assente)",col.output+1);
        output.setSelectedId(col.output+1,juce::dontSendNotification);
    }
    void updateMeter() {
        if (owner.show.columns[(size_t)index].type!=ColumnType::audio) return;
        meterLevel=std::max(owner.engine.getColumnPeak(id),meterLevel*.84f);
        repaint();
    }
    void resized() override {
        const int w=getWidth(),h=getHeight();
        output.setBounds(6,4,w-12,25);
        oscDestination.setBounds(6,4,w-12,28);
        gain.setBounds(10,35,73,std::max(95,h-37));
        pan.setBounds(w-49,37,35,35);
        mute.setBounds(w-77,h-36,32,29);
        phase.setBounds(w-42,h-36,32,29);
    }
    void paint(juce::Graphics& g) override {
        g.setColour(palette::line); g.drawVerticalLine(0,0,(float)getHeight());
        if (owner.show.columns[(size_t)index].type!=ColumnType::audio) return;
        const int h=getHeight(),w=getWidth();
        label(g,"PAN",{w-54,73,45,13},9,palette::muted,true,juce::Justification::centred);
        const double v=owner.show.columns[(size_t)index].pan;
        const juce::String panText=std::abs(v)<.005 ? "C" : juce::String(v<0?"L ":"R ")+juce::String((int)std::round(std::abs(v)*100));
        label(g,panText,{w-61,88,60,15},9,palette::text,false,juce::Justification::centred);
        // Meter backdrop shares the fader's vertical axis; the slider is
        // rendered above it by JUCE's child-component paint order.
        const float x=23.0f,top=44.0f,meterWidth=39.0f,meterHeight=(float)std::max(36,h-89);
        g.setColour(palette::raised);
        g.fillRoundedRectangle(x,top,meterWidth,meterHeight,3);
        const float db=20.0f*std::log10(std::max(0.000001f,meterLevel));
        const float fill=juce::jlimit(0.0f,1.0f,(db+60.0f)/66.0f);
        const float fillH=meterHeight*fill;
        if (fillH>0.0f) {
            g.setColour(db>=0 ? palette::red.withAlpha(.50f) : db>=-9 ? palette::amber.withAlpha(.5f) : palette::teal.withAlpha(.5f));
            g.fillRect(x+1,top+meterHeight-fillH,meterWidth-2,fillH);
        }
        for (int tick:{6,0,-6,-12,-24,-36,-48,-60}) {
            const float y=top+meterHeight*(1.0f-(tick+60.0f)/66.0f);
            g.setColour(palette::line.withAlpha(.9f));
            g.drawHorizontalLine((int)std::round(y),x+3,x+meterWidth-3);
            label(g,tick>0 ? "+6" : juce::String(tick),{(int)x+2,(int)y-7,26,13},8,palette::text);
        }
        g.setColour(palette::line); g.drawRoundedRectangle(x,top,meterWidth,meterHeight,3,1);
        label(g,"PEAK dB",{9,h-29,69,13},8,palette::muted,true);
    }
};
ColumnUtilities::ColumnUtilities(MainComponent& p) : owner(p) {}
ColumnUtilities::~ColumnUtilities()=default;
void ColumnUtilities::refresh() {
    bool rebuild=strips.size()!=owner.show.columns.size();
    if (!rebuild) for (size_t i=0;i<strips.size();++i) if (strips[i]->id!=owner.show.columns[i].id) rebuild=true;
    if (rebuild) {
        strips.clear();
        for (int i=0;i<(int)owner.show.columns.size();++i) { auto strip=std::make_unique<Strip>(owner,i); addAndMakeVisible(*strip); strips.push_back(std::move(strip)); }
    }
    for (auto& strip : strips) strip->sync();
    // Preserve the historical strip-child order used by the UI and tests.
    // The Master remains an overlay in the left utility area and is last.
    owner.masterSlider.toFront(false);
    resized(); repaint();
}
void ColumnUtilities::syncScroll(int x) { if (scrollX!=x) { scrollX=x; resized(); repaint(); } }
void ColumnUtilities::refreshRouting() { for (auto& strip : strips) strip->syncRouting(); }
void ColumnUtilities::refreshMeters() { for (auto& strip : strips) strip->updateMeter(); }
void ColumnUtilities::resized() {
    owner.masterSlider.setBounds(21-scrollX,34,76,std::max(92,getHeight()-37));
    for (size_t i=0;i<strips.size();++i)
        strips[i]->setBounds(CueGrid::nameWidth+(int)i*CueGrid::columnWidth-scrollX,1,CueGrid::columnWidth,getHeight()-2);
}
void ColumnUtilities::paint(juce::Graphics& g) {
    g.fillAll(palette::panel); g.setColour(palette::line); g.drawHorizontalLine(0,0,(float)getWidth());
    label(g,"MASTER AUDIO",{14-scrollX,10,CueGrid::nameWidth-28,20},11,palette::teal,true);
    label(g,"Volume generale",{112-scrollX,58,175,20},12,palette::text);
    label(g,"Solo audio PCM",{112-scrollX,84,175,18},10,palette::muted);
    label(g,"MIDI e OSC invariati",{112-scrollX,104,175,18},10,palette::muted);
}
}
