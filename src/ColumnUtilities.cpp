#include "MainComponent.h"
#include "GainMeterFader.h"
#include "MasterAudioLayout.h"

namespace cuelist {
struct ColumnUtilities::Strip final : juce::Component {
    MainComponent& owner;
    int id, index;
    GainMeterFader gain;
    juce::Slider pan;
    QuickLearnHotspot gainLearnHotspot{"Column Gain"},panLearnHotspot{"Column Pan"},
        muteLearnHotspot{"Column Mute"},phaseLearnHotspot{"Column Polarity"},
        sendLearnHotspot{"L/R Send"};
    UtilityIcon mute{false},phase{true},dataMute{false};
    MidiActivityLed midiActivity,oscActivity;
    uint64_t lastOscSent=0;
    bool oscActivityInitialized=false;
    double oscActivityUntil=0;
    uint64_t lastMidiSent=0;
    bool midiActivityInitialized=false;
    double activityUntil=0;
    juce::ComboBox output;
    juce::TextButton directMode{"POST"};
    MasterSendButton masterSend;
    juce::TextButton oscDestination{"OSC / destination"};
    juce::TextButton controlPriority{"Medium"},controlEdit{"Edit set"};
    int routingChannels=-1, routingOutput=-1;
    bool routingDirect=false;
    ColumnType routingType=ColumnType::audio;
    juce::String midiSignature;
    juce::StringArray midiIds;
    double nextMidiScan=0;
    juce::String lastMidiId;
    Strip(MainComponent& p,int i) : owner(p),id(p.show.columns[i].id),index(i) {
        addAndMakeVisible(gain);
        addAndMakeVisible(pan);
        addAndMakeVisible(gainLearnHotspot);
        addAndMakeVisible(panLearnHotspot);
        addAndMakeVisible(muteLearnHotspot);
        addAndMakeVisible(phaseLearnHotspot);
        addAndMakeVisible(sendLearnHotspot);
        gainLearnHotspot.setVisible(false);
        panLearnHotspot.setVisible(false);
        muteLearnHotspot.setVisible(false);
        phaseLearnHotspot.setVisible(false);
        sendLearnHotspot.setVisible(false);
        gainLearnHotspot.onPick=[this] {
            owner.chooseQuickLearnGuiTarget(ControlTarget::columnGain,id);
        };
        panLearnHotspot.onPick=[this] {
            owner.chooseQuickLearnGuiTarget(ControlTarget::columnPan,id);
        };
        muteLearnHotspot.onPick=[this] {
            owner.chooseQuickLearnGuiTarget(ControlTarget::columnMute,id);
        };
        phaseLearnHotspot.onPick=[this] {
            owner.chooseQuickLearnGuiTarget(ControlTarget::columnPolarity,id);
        };
        sendLearnHotspot.onPick=[this] {
            owner.chooseQuickLearnGuiTarget(ControlTarget::columnSend,id);
        };
        configureMixerPan(pan);
        gain.setName("columnGainDb"); pan.setName("columnPan"); mute.setName("columnMute"); phase.setName("columnPhase");
        addAndMakeVisible(output); output.setName("columnOutput");
        addAndMakeVisible(directMode); directMode.setName("columnDirectPrePost");
        directMode.setTooltip("Direct Output only: PRE bypasses column fader, POST follows it. Master L/R stays POST.");
        directMode.onClick=[this] {
            owner.setColumnDirectPostFader(index,!owner.show.columns[(size_t)index].directPostFader);
        };
        addAndMakeVisible(dataMute); dataMute.setName("columnDataOutputMute");
        dataMute.setTooltip("Mute MIDI/OSC transmission only. The cue and timeline continue.");
        dataMute.setWantsKeyboardFocus(false);
        dataMute.onClick=[this] {owner.setDataOutputMuted(index,dataMute.getToggleState());};
        addAndMakeVisible(midiActivity); midiActivity.setName("columnMidiOutputLed");
        addAndMakeVisible(oscActivity); oscActivity.setName("columnOscOutputLed");
        addAndMakeVisible(masterSend); masterSend.setName("columnMasterSend");
        masterSend.setTooltip("L/R: post-fader stereo send to Master Bus (independent of Direct Out).");
        masterSend.setWantsKeyboardFocus(false);
        masterSend.onClick=[this] {owner.setColumnMasterSend(index,masterSend.getToggleState());};
        addAndMakeVisible(oscDestination); oscDestination.setName("columnOscDestination");
        addAndMakeVisible(controlPriority);
        addAndMakeVisible(controlEdit);
        controlPriority.setName("controlColumnPriority");
        controlEdit.setName("controlColumnEditSet");
        controlPriority.onClick=[this] {
            juce::PopupMenu menu;
            menu.setLookAndFeel(&getLookAndFeel());
            menu.addItem(2,"Medium"); menu.addItem(3,"High"); menu.addItem(4,"Maximum");
            auto safe=juce::Component::SafePointer<MainComponent>(&owner);
            const int columnId=id;
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&controlPriority),
                [safe,columnId](int value) {
                    if (!safe || value<2 || value>4) return;
                    for (int j=0;j<(int)safe->show.columns.size();++j)
                        if (safe->show.columns[(size_t)j].id==columnId)
                            {safe->setControlColumnPriority(j,value);return;}
                });
        };
        controlEdit.onClick=[this] {
            owner.editControlSet(owner.selectedRow,index);
        };
        oscDestination.setTooltip("OSC receiver IP address or host and UDP port.");
        oscDestination.onClick=[this] {
            const auto columnId=id;
            const auto& col=owner.show.columns[(size_t)index];
            auto* dialog=new juce::AlertWindow("OSC UDP Destination","Enter the OSC receiver host/IP and port.",juce::MessageBoxIconType::NoIcon,&owner);
            dialog->addTextEditor("host",col.oscHost,"Host / IP address");
            dialog->addTextEditor("port",juce::String(col.oscPort),"UDP port (1–65535)");
            dialog->addButton("Save",1); dialog->addButton("Cancel",0);
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
        output.setTooltip("Column audio or MIDI output.");
        output.onChange=[this] {
            if (owner.show.columns[index].type==ColumnType::midi) {
                const int selected=output.getSelectedId()-1;
                if (selected>=0 && selected<midiIds.size()) owner.setMidiOutput(index,midiIds[selected]);
            } else if(output.getSelectedId()==1) owner.setColumnDirectEnabled(index,false);
            else if (output.getSelectedId()>1) owner.setColumnOutput(index,output.getSelectedId()-2);
        };
        gain.setTooltip("Gain for all clips in this column. Double-click: 0 dB.");
        gain.setMaximumDb(owner.audioFaderCeilingDb());
        pan.setTooltip("Stereo pan/balance. Double-click: center.");
        phase.setTooltip("Invert the polarity of both column channels."); mute.setTooltip("Mute the entire column without stopping clips.");
        addAndMakeVisible(mute); addAndMakeVisible(phase); mute.setWantsKeyboardFocus(false); phase.setWantsKeyboardFocus(false);
        auto update=[this](int control) {
            const auto col=owner.show.columns[index];
            owner.setColumnUtilities(index,
                control==0 ? GainMeterFader::toLinear(gain.getGainDb()) : col.gain,
                control==1 ? (float)pan.getValue() : col.pan,
                control==2 ? mute.getToggleState() : col.muted,
                control==3 ? phase.getToggleState() : col.phaseInverted);
        };
        gain.onGainChanged=[update](float) { update(0); }; pan.onValueChange=[update] { update(1); };
        mute.onClick=[update] { update(2); }; phase.onClick=[update] { update(3); };
    }
    void syncAudioControls() {
        const auto& col=owner.show.columns[index];
        if (col.type!=ColumnType::audio) return;
        gain.setGainDb(GainMeterFader::fromLinear(col.gain)); // no callback feedback
        pan.setValue(col.pan,juce::dontSendNotification);
        mute.setToggleState(col.muted,juce::dontSendNotification);
        phase.setToggleState(col.phaseInverted,juce::dontSendNotification);
        masterSend.setToggleState(col.masterSend,juce::dontSendNotification);
        repaint();
    }
    void sync() {
        const auto& col=owner.show.columns[index];
        gain.setMaximumDb(owner.audioFaderCeilingDb());
        syncAudioControls();
        dataMute.setToggleState(col.muted,juce::dontSendNotification);
        directMode.setButtonText(col.directPostFader ? "POST" : "PRE");
        directMode.setEnabled(col.directOutputEnabled);
        gain.setCompact(col.collapsed);
        gain.setVisible(col.type==ColumnType::audio);
        mute.setVisible(col.type==ColumnType::audio);
        pan.setVisible(col.type==ColumnType::audio && !col.collapsed);
        gainLearnHotspot.setLearnActive(owner.isQuickLearnActive() &&
                                      col.type==ColumnType::audio);
        panLearnHotspot.setLearnActive(owner.isQuickLearnActive() &&
                                     col.type==ColumnType::audio && !col.collapsed);
        muteLearnHotspot.setLearnActive(owner.isQuickLearnActive() &&
                                      col.type==ColumnType::audio);
        phaseLearnHotspot.setLearnActive(owner.isQuickLearnActive() &&
                                       col.type==ColumnType::audio && !col.collapsed);
        sendLearnHotspot.setLearnActive(owner.isQuickLearnActive() &&
                                      col.type==ColumnType::audio && !col.collapsed);
        refreshLearnVisuals();
        phase.setVisible(col.type==ColumnType::audio && !col.collapsed);
        masterSend.setVisible(col.type==ColumnType::audio && !col.collapsed);
        dataMute.setVisible(col.type==ColumnType::midi || col.type==ColumnType::osc);
        midiActivity.setVisible(col.type==ColumnType::midi);
        oscActivity.setVisible(col.type==ColumnType::osc);
        directMode.setVisible(col.type==ColumnType::audio && !col.collapsed);
        oscDestination.setVisible(col.type==ColumnType::osc && !col.collapsed);
        output.setVisible(!col.collapsed && (col.type==ColumnType::audio || col.type==ColumnType::midi));
        const bool control=col.type==ColumnType::control;
        controlPriority.setVisible(control && !col.collapsed);
        controlEdit.setVisible(control && !col.collapsed);
        if (control) {
            controlPriority.setButtonText(col.controlPriority==4 ? "Maximum" :
                col.controlPriority==3 ? "High" : "Medium");
            controlPriority.setEnabled(true);
            controlEdit.setButtonText("Edit Q set");
        }
        syncRouting();
    }
    void refreshLearnVisuals() {
        const auto colour=owner.quickLearnAccent();
        const bool bright=owner.quickLearnPulseIsBright();
        gainLearnHotspot.setLearnPresentation(colour,
            owner.isQuickLearnTargetSelected(ControlTarget::columnGain,id),bright);
        panLearnHotspot.setLearnPresentation(colour,
            owner.isQuickLearnTargetSelected(ControlTarget::columnPan,id),bright);
        muteLearnHotspot.setLearnPresentation(colour,
            owner.isQuickLearnTargetSelected(ControlTarget::columnMute,id),bright);
        phaseLearnHotspot.setLearnPresentation(colour,
            owner.isQuickLearnTargetSelected(ControlTarget::columnPolarity,id),bright);
        sendLearnHotspot.setLearnPresentation(colour,
            owner.isQuickLearnTargetSelected(ControlTarget::columnSend,id),bright);
    }
    void syncRouting() {
        const auto& col=owner.show.columns[index]; int channels=2;
        if (col.type==ColumnType::control) {
            routingType=ColumnType::control; return;
        }
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
            output.clear(juce::dontSendNotification); output.setEnabled(true); output.addItem("No MIDI output",1);
            int selected=col.midiOutputId.isEmpty() ? 1 : 0;
            for (const auto& device:available) {
                midiIds.add(device.identifier); output.addItem(device.name,midiIds.size());
                if (device.identifier==col.midiOutputId) selected=midiIds.size();
            }
            if (!selected) { midiIds.add(col.midiOutputId); selected=midiIds.size(); output.addItem("MIDI output unavailable",selected); }
            output.setSelectedId(selected,juce::dontSendNotification); return;
        }
        if (auto* device=owner.engine.devices.getCurrentAudioDevice()) channels=std::max(1,device->getActiveOutputChannels().countNumberOfSetBits());
        if (routingChannels==channels && routingOutput==col.output &&
            routingDirect==col.directOutputEnabled && routingType==col.type) return;
        routingChannels=channels; routingOutput=col.output;
        routingDirect=col.directOutputEnabled; routingType=col.type;
        output.clear(juce::dontSendNotification); output.setEnabled(col.type==ColumnType::audio);
        if (col.type!=ColumnType::audio) {
            output.addItem(col.type==ColumnType::midi ? "MIDI - planned" : "OSC - planned",1);
            output.setSelectedId(1,juce::dontSendNotification); return;
        }
        output.addItem("Direct Off",1);
        for (int i=0;i<channels;i+=2)
            output.addItem("Out "+juce::String(i+1)+(i+1<channels ? " / "+juce::String(i+2) : " (mono)"),i+2);
        if (col.output>=channels || col.output%2!=0)
            output.addItem("Out "+juce::String(col.output+1)+" / "+juce::String(col.output+2)+" (missing)",col.output+2);
        output.setSelectedId(col.directOutputEnabled ? col.output+2 : 1,juce::dontSendNotification);
    }
    void updateMeter() {
        if(owner.show.columns[(size_t)index].type==ColumnType::midi) {
            // Counts are incremented only by the MIDI dispatcher after a
            // successful send to the selected device, never upon enqueue.
            const auto sent=owner.engine.getMidiColumnSent(id);
            const auto now=juce::Time::getMillisecondCounterHiRes();
            if(!midiActivityInitialized) {
                lastMidiSent=sent;
                midiActivityInitialized=true;
            } else if(sent!=lastMidiSent) {
                if(sent>lastMidiSent) activityUntil=now+180.0;
                lastMidiSent=sent;
            }
            midiActivity.setActive(now<activityUntil);
            return;
        }
        if(owner.show.columns[(size_t)index].type==ColumnType::osc) {
            const auto sent=owner.engine.getOscColumnSent(id);
            const auto now=juce::Time::getMillisecondCounterHiRes();
            if(!oscActivityInitialized) {lastOscSent=sent;oscActivityInitialized=true;}
            else if(sent!=lastOscSent) {
                if(sent>lastOscSent) oscActivityUntil=now+180.0;
                lastOscSent=sent;
            }
            oscActivity.setActive(now<oscActivityUntil);
            return;
        }
        if (owner.show.columns[(size_t)index].type!=ColumnType::audio) return;
        const auto stereo=owner.engine.connected.load(std::memory_order_relaxed)
            ? owner.engine.getColumnStereoPeak(id) : std::array<float,2>{{0.0f,0.0f}};
        gain.setStereoPeaks(stereo[0],stereo[1]);
    }
    void resized() override {
        const int w=getWidth(),h=getHeight();
        const bool collapsed=owner.show.columns[(size_t)index].collapsed;
        const auto type=owner.show.columns[(size_t)index].type;
        output.setBounds(6,type==ColumnType::audio ? 8 : 4,
                         type==ColumnType::audio ? w-67 : w-12,25);
        directMode.setBounds(w-56,8,50,25);
        oscDestination.setBounds(6,4,w-12,28);
        gain.setBounds(collapsed ? (w-34)/2 : 7,collapsed ? 23 : 33,
                       collapsed ? 34 : 92,
                       collapsed ? std::max(45,h-103) : std::max(98,h-36));
        pan.setBounds(w-60,34,49,49);
        // Avoid overlapping the mute/phase buttons at the foot of a strip.
        const int reserve=juce::jmin(47,std::max(0,gain.getHeight()-32));
        gainLearnHotspot.setBounds(gain.getBounds().withTrimmedBottom(reserve));
        panLearnHotspot.setBounds(pan.getBounds());
        masterSend.setBounds(w-48,117,26,26);
        mute.setBounds(collapsed ? (w-30)/2 : w-77,h-36,
                       collapsed ? 30 : 32,29);
        phase.setBounds(w-42,h-36,32,29);
        muteLearnHotspot.setBounds(mute.getBounds());
        phaseLearnHotspot.setBounds(phase.getBounds());
        sendLearnHotspot.setBounds(masterSend.getBounds());
        for (auto* hotspot : {&gainLearnHotspot,&panLearnHotspot,&muteLearnHotspot,
                              &phaseLearnHotspot,&sendLearnHotspot})
            if (hotspot->isVisible()) hotspot->toFront(false);
        dataMute.setBounds(collapsed ? (w-30)/2 : (w-32)/2,h-36,30,29);
        midiActivity.setBounds(collapsed ? (w-22)/2 : 12,collapsed ? 40 : 42,
                                collapsed ? 23 : 46,17);
        oscActivity.setBounds(collapsed ? (w-22)/2 : 12,collapsed ? 40 : 42,
                               collapsed ? 23 : 46,17);
        controlPriority.setBounds(6,5,std::max(1,w-12),29);
        controlEdit.setBounds(8,44,w-16,32);
    }
    void paint(juce::Graphics& g) override {
        const auto& col=owner.show.columns[(size_t)index];
        const auto colType=col.type;
        const auto inset=juce::Rectangle<float>(4.0f,4.0f,
            (float)std::max(1,getWidth()-8),(float)std::max(1,getHeight()-8));
        g.setColour(palette::raised.withAlpha(.58f));g.fillRoundedRectangle(inset,8.0f);
        g.setColour(palette::line);g.drawRoundedRectangle(inset,8.0f,1.0f);
        g.setColour(palette::type((int)colType));
        g.fillRoundedRectangle(9.0f,3.0f,(float)std::max(1,getWidth()-18),3.0f,1.5f);
        g.setColour(palette::line); g.drawVerticalLine(0,0,(float)getHeight());
        if (col.collapsed) {
            label(g,colType==ColumnType::audio ? "A" : colType==ColumnType::midi ? "M" :
                  colType==ColumnType::osc ? "O" : "C",
                  {5,4,std::max(1,getWidth()-10),18},11,palette::type((int)colType),
                  true,juce::Justification::centred);
            return;
        }
        if (colType==ColumnType::control) {
            label(g,"CUE-SPECIFIC / DEFAULT MAP OVERRIDES",
                  {7,91,getWidth()-14,22},11,palette::teal,true);
            label(g,col.controlPriority==4 ? "Priority: MAXIMUM" :
                  col.controlPriority==3 ? "Priority: HIGH" : "Priority: MEDIUM",
                  {7,120,getWidth()-14,20},10,palette::muted);
            return;
        }
        if(colType==ColumnType::midi || colType==ColumnType::osc) {
            if(!col.collapsed)
                label(g,"MUTE",{7,getHeight()-63,getWidth()-14,17},10,palette::muted,true,
                      juce::Justification::centred);
            return;
        }
        if(colType!=ColumnType::audio) return;
        const int h=getHeight(),w=getWidth();
        label(g,"MUTE",{w-86,h-53,42,16},10,palette::muted,true,juce::Justification::centred);
        label(g,"PHASE",{w-47,h-53,46,16},10,palette::muted,true,juce::Justification::centred);
        label(g,"PAN",{w-63,86,56,16},11,palette::muted,true,juce::Justification::centred);
        const double v=owner.show.columns[(size_t)index].pan;
        const juce::String panText=std::abs(v)<.005 ? "C" : juce::String(v<0?"L ":"R ")+juce::String((int)std::round(std::abs(v)*100));
        label(g,panText,{w-63,100,58,18},11,palette::text,false,juce::Justification::centred);
    }
};
ColumnUtilities::ColumnUtilities(MainComponent& p) : owner(p) { addAndMakeVisible(scrollableStrips); }
ColumnUtilities::~ColumnUtilities()=default;
void ColumnUtilities::refresh() {
    bool rebuild=strips.size()!=owner.show.columns.size();
    if (!rebuild) for (size_t i=0;i<strips.size();++i) if (strips[i]->id!=owner.show.columns[i].id) rebuild=true;
    if (rebuild) {
        strips.clear();
        for (int i=0;i<(int)owner.show.columns.size();++i) { auto strip=std::make_unique<Strip>(owner,i); scrollableStrips.addAndMakeVisible(*strip); strips.push_back(std::move(strip)); }
    }
    for (auto& strip : strips) strip->sync();
    // A project load or scene refresh must synchronize these non-child Master
    // controls too. Neither setter notifies listeners or starts another GO.
    owner.masterSlider.setMaximumDb(owner.audioFaderCeilingDb());
    owner.masterSlider.setGainDb(GainMeterFader::fromLinear(owner.show.masterGain));
    owner.masterPan.setValue(owner.show.masterPan,juce::dontSendNotification);
    owner.masterMute.setToggleState(owner.show.masterMuted,juce::dontSendNotification);
    owner.safetyLimiter.setToggleState(owner.show.safetyLimiterEnabled,
                                       juce::dontSendNotification);
    // Populate the Master output menu immediately, not only on the next timer.
    refreshRouting();
    // Preserve the historical strip-child order used by the UI and tests.
    // The Master remains an overlay in the left utility area and is last.
    owner.masterSlider.toFront(false);
    owner.masterPan.toFront(false);
    owner.masterMute.toFront(false);
    owner.masterOutput.toFront(false);
    owner.safetyLimiter.toFront(false);
    if (owner.isQuickLearnActive()) {
        owner.masterGainLearnHotspot.toFront(false);
        owner.masterPanLearnHotspot.toFront(false);
        owner.masterLimiterLearnHotspot.toFront(false);
    }

    resized(); repaint();
}
void ColumnUtilities::refreshLearnVisuals() {
    for (auto& strip:strips) strip->refreshLearnVisuals();
}
void ColumnUtilities::syncAudioControls(int column) {
    if (column<0 || column>=(int)owner.show.columns.size()) return;
    if ((size_t)column>=strips.size() ||
        strips[(size_t)column]->id!=owner.show.columns[(size_t)column].id) {
        refresh(); // structural change; rebuild only if this column was replaced
        return;
    }
    strips[(size_t)column]->syncAudioControls();
}
void ColumnUtilities::syncScroll(int x) { if (scrollX!=x) { scrollX=x; resized(); repaint(); } }
void ColumnUtilities::refreshRouting() {
    for (auto& strip : strips) strip->syncRouting();
    int channels=2;
    if(auto* device=owner.engine.devices.getCurrentAudioDevice())
        channels=std::max(1,device->getActiveOutputChannels().countNumberOfSetBits());
    if(masterRoutingChannels!=channels || masterRoutingOutput!=owner.show.masterOutput) {
        masterRoutingChannels=channels; masterRoutingOutput=owner.show.masterOutput;
        owner.masterOutput.clear(juce::dontSendNotification);
        for(int i=0;i<channels;i+=2)
            owner.masterOutput.addItem(juce::String(i+1)+"/"+
                juce::String(i+2<=channels ? i+2 : i+1),i+1);
        if(owner.show.masterOutput>=channels)
            owner.masterOutput.addItem(juce::String(owner.show.masterOutput+1)+"/"+
                juce::String(owner.show.masterOutput+2)+" (missing)",owner.show.masterOutput+1);
        owner.masterOutput.setSelectedId(owner.show.masterOutput+1,juce::dontSendNotification);
    }
}
void ColumnUtilities::refreshMeters() {
    for (auto& strip : strips) strip->updateMeter();
    const auto connected=owner.engine.connected.load(std::memory_order_relaxed);
    owner.masterSlider.setStereoPeaks(
        connected ? owner.engine.masterPeakLeft.load(std::memory_order_relaxed) : 0.0f,
        connected ? owner.engine.masterPeakRight.load(std::memory_order_relaxed) : 0.0f);
    owner.safetyLimiter.setGainReduction(
        connected && owner.show.safetyLimiterEnabled
        ? owner.engine.limiterReduction.load(std::memory_order_relaxed) : 0.0f);
}
void ColumnUtilities::resized() {
    // A dedicated child clips scrolling strips to the centre of the mixer.
    // The Master is pinned at the left; neither area can obscure the other.
    static_assert(MasterAudioLayout::laneWidth==CueGrid::nameWidth,
                  "Pinned Master must match the Cuelist name lane");
    const int masterWidth=MasterAudioLayout::laneWidth;
    const auto layout=MasterAudioLayout::atHeight(getHeight());
    scrollableStrips.setBounds(masterWidth,0,
        std::max(0,getWidth()-masterWidth),getHeight());
    owner.masterSlider.setBounds(layout.fader);
    owner.masterPan.setBounds(layout.panKnob);
    owner.masterGainLearnHotspot.setBounds(owner.masterSlider.getBounds());
    owner.masterPanLearnHotspot.setBounds(owner.masterPan.getBounds());
    owner.masterGainLearnHotspot.setLearnActive(owner.isQuickLearnActive());
    owner.masterPanLearnHotspot.setLearnActive(owner.isQuickLearnActive());
    owner.masterMute.setBounds(layout.muteButton);
    owner.masterOutput.setBounds(layout.output);
    owner.safetyLimiter.setBounds(layout.limiter);
    owner.masterLimiterLearnHotspot.setBounds(owner.safetyLimiter.getBounds());
    owner.masterLimiterLearnHotspot.setLearnActive(owner.isQuickLearnActive());
    if (owner.masterLimiterLearnHotspot.isVisible())
        owner.masterLimiterLearnHotspot.toFront(false);
    for (size_t i=0;i<strips.size();++i)
        strips[i]->setBounds(owner.grid.columnX((int)i)-masterWidth-scrollX,1,
                              owner.grid.columnWidthAt((int)i),getHeight()-2);
}
void ColumnUtilities::paint(juce::Graphics& g) {
    g.fillAll(palette::panel);
    static_assert(MasterAudioLayout::laneWidth==CueGrid::nameWidth,
                  "Master Audio must remain in the Cuelist fixed lane");
    const auto layout=MasterAudioLayout::atHeight(getHeight());

    // Pinned left console: only this 318px lane draws the Master.
    // The other strips are separate children of scrollableStrips and cannot
    // move, mask or clip the Master while the cue grid scrolls.
    g.saveState();
    g.reduceClipRegion(0,0,MasterAudioLayout::laneWidth,getHeight());
    g.setColour(palette::raised.withAlpha(.50f));
    g.fillRoundedRectangle(layout.card.toFloat(),7.f);
    g.setColour(palette::line.withAlpha(.66f));
    g.drawRoundedRectangle(layout.card.toFloat(),7.f,.8f);

    // A restrained header rule provides continuity with the Cue/Transport
    // lane; this card no longer resembles a floating settings dialog.
    g.setColour(palette::teal.withAlpha(.85f));
    g.fillRoundedRectangle((float)layout.card.getX()+11.f,
                           (float)layout.card.getY()+1.f,96.f,2.f,1.f);
    label(g,"MASTER AUDIO",layout.title,14,palette::teal,true);
    label(g,"L/R",{246,14,51,18},10,palette::muted,true,
          juce::Justification::centredRight);
    g.setColour(palette::line.withAlpha(.33f));
    g.fillRect(layout.headerRule);

    // All captions relate to genuine interactive controls, not decorative
    // actions; fader peaks and limiter gain reduction remain runtime driven.
    label(g,"MAIN OUT",layout.outputCaption,11,palette::muted,true);
    label(g,"PAN",layout.panCaption,11,palette::muted,true,
          juce::Justification::centred);
    const auto pan=owner.show.masterPan;
    label(g,std::abs(pan)<.005f ? juce::String("C") :
          juce::String(pan<0?"L ":"R ")+
          juce::String((int)std::round(std::abs(pan)*100)),
          layout.panValue,11,palette::text,false,
          juce::Justification::centred);
    label(g,"LIMITER",layout.limiterCaption,10,palette::muted,true);
    label(g,"MUTE",layout.muteCaption,10,palette::muted,true,
          juce::Justification::centred);
    if (getHeight()>=202)
        label(g,"PCM MASTER BUS",layout.gainCaption,10,palette::muted);
    g.restoreState();
    g.setColour(palette::line.withAlpha(.65f));
    g.drawVerticalLine(MasterAudioLayout::laneWidth,0.f,(float)getHeight());
}

}
