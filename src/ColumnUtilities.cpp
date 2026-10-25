#include "MainComponent.h"

namespace cuelist {
namespace {
class UtilityIcon final : public juce::ToggleButton {
public:
    explicit UtilityIcon(bool invert) : phase(invert) { setButtonText(invert ? "Inversione di fase" : "Mute"); }
    void paintButton(juce::Graphics& g,bool hover,bool down) override {
        const auto box=getLocalBounds().toFloat().reduced(2);
        g.setColour(getToggleState() ? palette::teal.withAlpha(.18f) : palette::raised);
        g.fillRoundedRectangle(box,4);
        g.setColour(getToggleState() ? palette::teal : hover || down ? palette::text : palette::muted);
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
    int routingChannels=-1, routingOutput=-1;
    ColumnType routingType=ColumnType::audio;
    Strip(MainComponent& p,int i) : owner(p),id(p.show.columns[i].id),index(i) {
        for (auto* slider : {&gain,&pan}) {
            addAndMakeVisible(slider); slider->setSliderStyle(juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,62,22); slider->setDoubleClickReturnValue(true,0);
        }
        gain.setName("columnGainDb"); pan.setName("columnPan"); mute.setName("columnMute"); phase.setName("columnPhase");
        addAndMakeVisible(output); output.setName("columnOutput");
        output.setTooltip("Uscita della colonna. Audio: coppie delle uscite attive del dispositivo. MIDI e OSC: previsti.");
        output.onChange=[this] { if (output.getSelectedId()>0) owner.setColumnOutput(index,output.getSelectedId()-1); };
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
        syncRouting();
    }
    void syncRouting() {
        const auto& col=owner.show.columns[index]; int channels=2;
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
    void resized() override {
        output.setBounds(6,4,getWidth()-12,24);
        gain.setBounds(5,32,getWidth()-10,24); pan.setBounds(5,58,getWidth()-10,24);
        mute.setBounds(27,84,44,28); phase.setBounds(88,84,44,28);
    }
    void paint(juce::Graphics& g) override {
        g.setColour(palette::line); g.drawVerticalLine(0,0,(float)getHeight());
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
    resized(); repaint();
}
void ColumnUtilities::syncScroll(int x) { if (scrollX!=x) { scrollX=x; resized(); repaint(); } }
void ColumnUtilities::refreshRouting() { for (auto& strip : strips) strip->syncRouting(); }
void ColumnUtilities::resized() {
    for (size_t i=0;i<strips.size();++i) strips[i]->setBounds(CueGrid::nameWidth+(int)i*CueGrid::columnWidth-scrollX,1,CueGrid::columnWidth,getHeight()-2);
}
void ColumnUtilities::paint(juce::Graphics& g) {
    g.fillAll(palette::panel); g.setColour(palette::line); g.drawHorizontalLine(0,0,(float)getWidth());
    label(g,"UTILITY COLONNE",{14-scrollX,10,CueGrid::nameWidth-28,20},11,palette::teal,true);
    label(g,"Uscite / Livello / Pan",{14-scrollX,37,CueGrid::nameWidth-28,22},11,palette::muted);
    label(g,"Mute / Inversione di fase",{14-scrollX,66,CueGrid::nameWidth-28,22},11,palette::muted);
}
}
