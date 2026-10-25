#include "MainComponent.h"

namespace cuelist {
struct ColumnUtilities::Strip final : juce::Component {
    MainComponent& owner;
    int id, index;
    juce::Slider gain,pan;
    juce::ToggleButton mute{"Mute"},phase{"Fase"};
    Strip(MainComponent& p,int i) : owner(p),id(p.show.columns[i].id),index(i) {
        for (auto* slider : {&gain,&pan}) {
            addAndMakeVisible(slider); slider->setSliderStyle(juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,62,22); slider->setDoubleClickReturnValue(true,0);
        }
        gain.setName("columnGainDb"); pan.setName("columnPan"); mute.setName("columnMute"); phase.setName("columnPhase");
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
        phase.setToggleState(col.phaseInverted,juce::dontSendNotification); setEnabled(col.type==ColumnType::audio);
    }
    void resized() override {
        gain.setBounds(5,4,getWidth()-10,24); pan.setBounds(5,30,getWidth()-10,24);
        mute.setBounds(4,58,73,24); phase.setBounds(80,58,76,24);
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
void ColumnUtilities::resized() {
    for (size_t i=0;i<strips.size();++i) strips[i]->setBounds(CueGrid::nameWidth+(int)i*CueGrid::columnWidth-scrollX,1,CueGrid::columnWidth,getHeight()-2);
}
void ColumnUtilities::paint(juce::Graphics& g) {
    g.fillAll(palette::panel); g.setColour(palette::line); g.drawHorizontalLine(0,0,(float)getWidth());
    label(g,"UTILITY COLONNE",{14-scrollX,10,CueGrid::nameWidth-28,20},11,palette::teal,true);
    label(g,"Livello / Pan / Mute / Fase",{14-scrollX,37,CueGrid::nameWidth-28,22},11,palette::muted);
}
}
