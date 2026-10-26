#include "MainComponent.h"
#include <cmath>

namespace cuelist {
void MainComponent::resetActiveMapping() {
    activeCueId=0; pressedControls.clear();
    controlMapping.setActiveCue(0,0);
}
void MainComponent::refreshControlSettings() {
    controlMapping.setSettings(show.controls);
    int preset=0;
    for (const auto& row:show.rows)
        if (row.id==activeCueId) { preset=row.mappingPresetId; break; }
    controlMapping.setActiveCue(activeCueId,preset);
    pressedControls.clear();
    if (midiInput.identifier()!=show.controls.midiInputId) {
        const auto result=midiInput.connect(show.controls.midiInputId);
        if (result.failed()) status=result.getErrorMessage();
    }
}
juce::Result MainComponent::selectControlInput(const juce::String& id) {
    auto result=midiInput.connect(id);
    // Preserve an explicitly selected disconnected identifier, never silently
    // switch to the first available device.
    show.controls.midiInputId=id;
    dirty=true;
    status=result.wasOk() ? (id.isEmpty() ? "Ingresso MIDI disattivato." :
        "Ingresso MIDI collegato.") : result.getErrorMessage();
    repaint(); return result;
}
bool MainComponent::addMappingBinding(ControlBinding b) {
    auto candidate=show;
    b.id=candidate.controls.nextId++;
    candidate.controls.bindings.push_back(std::move(b));
    const auto valid=validateShow(candidate);
    if (valid.failed()) { status=valid.getErrorMessage(); return false; }
    show=std::move(candidate); refreshControlSettings();
    dirty=true; status="Mapping aggiunto."; repaint(); return true;
}
bool MainComponent::deleteMappingBinding(int id) {
    auto& bindings=show.controls.bindings;
    const auto n=bindings.size();
    bindings.erase(std::remove_if(bindings.begin(),bindings.end(),
        [id](const auto& b){return b.id==id;}),bindings.end());
    if (bindings.size()==n) return false;
    refreshControlSettings(); dirty=true; status="Mapping eliminato."; repaint();
    return true;
}
int MainComponent::createMappingPreset(const juce::String& name) {
    auto candidate=show; const auto label=name.trim();
    if (label.isEmpty() || label.length()>120) return 0;
    const int id=candidate.controls.nextId++;
    candidate.controls.presets.push_back({id,label});
    if (validateShow(candidate).failed()) return 0;
    show=std::move(candidate); refreshControlSettings(); dirty=true;
    status="Preset "+label+" creato."; repaint(); return id;
}
bool MainComponent::assignMappingPreset(int rowId,int presetId) {
    auto candidate=show;
    bool exists=presetId==0;
    for (const auto& preset:candidate.controls.presets) if (preset.id==presetId) exists=true;
    if (!exists) return false;
    bool found=false;
    for (auto& row:candidate.rows) if (row.id==rowId) {row.mappingPresetId=presetId;found=true;break;}
    if (!found || validateShow(candidate).failed()) return false;
    show=std::move(candidate); refreshControlSettings(); dirty=true;
    status="Preset Control Mapping assegnato."; repaint(); return true;
}
void MainComponent::dispatchControl(const ControlEvent& input) {
    // Hardware MIDI events have already been copied to the UI thread.
    // Pickup compares against the physical value expected for the current
    // actual target. No mutation of show happens in a MIDI callback.
    std::optional<float> current;
    const auto& bindings=show.controls.bindings;
    // Resolve a key once first, then determine the selected target's value.
    // The pure mapper applies cue/preset/column/global precedence.
    for (const auto& b:bindings) if (samePhysicalInput(b,input) &&
        scopeApplies(b,activeCueId, [&]{
            for (const auto& row:show.rows) if (row.id==activeCueId) return row.mappingPresetId;
            return 0;
        }())) {
        if (b.target==ControlTarget::masterGain) {
            const float d=b.maximum-b.minimum;
            if (d>0) current=juce::jlimit(0.0f,1.0f,(show.masterGain-b.minimum)/d);
        } else if (b.target==ControlTarget::columnGain) {
            for (const auto& col:show.columns) if (col.id==b.targetColumnId) {
                const float d=b.maximum-b.minimum;
                if (d>0) current=juce::jlimit(0.0f,1.0f,(col.gain-b.minimum)/d);
                break;
            }
        }
        if (current.has_value() && b.invert) current=1.0f-*current;
        break;
    }
    const auto action=controlMapping.resolve(input,current);
    if (!action) return;
    const auto& binding=action->binding;
    if (binding.target==ControlTarget::go || binding.target==ControlTarget::panic) {
        // Momentary controls fire only on the first press, not repeated CCs.
        const bool old=pressedControls[binding.id];
        pressedControls[binding.id]=action->pressed;
        if (!action->pressed || old) return;
        if (binding.target==ControlTarget::go) goNext();
        else stopAll();
        return;
    }
    if (binding.target==ControlTarget::masterGain) {
        if (std::abs(show.masterGain-action->scaled)>0.005f)
            setMasterAudio(action->scaled);
    } else if (binding.target==ControlTarget::columnGain) {
        for (int i=0;i<(int)show.columns.size();++i)
            if (show.columns[(size_t)i].id==binding.targetColumnId &&
                show.columns[(size_t)i].type==ColumnType::audio) {
                const auto& c=show.columns[(size_t)i];
                if (std::abs(c.gain-action->scaled)>0.005f) {
                    setColumnUtilities(i,action->scaled,c.pan,c.muted,c.phaseInverted);
                    utilities.refresh();
                }
                break;
            }
    }
    // MIDI CC and OSC outbound mappings are represented by the model but not
    // dispatched until independent output paths are introduced.
}
class ControlPanel final : public juce::Component, private juce::Timer,
                           private juce::ListBoxModel {
public:
    explicit ControlPanel(MainComponent& main):owner(&main) {
        setName("controlMappingPanel");
        for (juce::Component* c : std::initializer_list<juce::Component*>{
            &device,&connect,&learn,&add,&remove,&newPreset,&assignPreset,
            &presetName,&presets,&scope,&target,&takeover,&columns,&bindingList,&state})
            addAndMakeVisible(c);
        device.setName("controlInputDevice");
        device.setTooltip("Selezione esplicita. Nessuna sostituzione automatica in caso di disconnessione.");
        connect.setButtonText("Connetti");
        learn.setButtonText("MIDI Learn");
        add.setButtonText("Aggiungi mapping");
        remove.setButtonText("Elimina selezionato");
        newPreset.setButtonText("Nuovo preset");
        assignPreset.setButtonText("Assegna alla cue");
        presetName.setTextToShowWhenEmpty("Nome nuovo preset",palette::muted);
        state.setName("mappingStatus");
        state.setJustificationType(juce::Justification::centredLeft);
        scope.addItemList({"Globale","Colonna","Preset","Singola cue"},1);
        scope.setSelectedId(1);
        target.addItemList({"Master Audio","Gain colonna Audio","GO cue successiva","PANIC"},1);
        target.setSelectedId(1);
        takeover.addItemList({"Pickup","Jump"},1);
        takeover.setSelectedId(1);
        bindingList.setName("mappingBindings");
        bindingList.setModel(this); bindingList.setRowHeight(28);
        device.onChange=[this] {
            if (!owner) return;
            auto id=juce::String();
            int index=device.getSelectedId()-2;
            if (index>=0 && index<(int)deviceIds.size()) id=deviceIds[(size_t)index];
            auto result=owner->selectControlInput(id);
            state.setText(result.wasOk() ? "Dispositivo selezionato." : result.getErrorMessage(),
                juce::dontSendNotification);
        };
        connect.onClick=[this]{ refreshDevices(); };
        learn.onClick=[this]{
            if (!owner || !owner->midiInput.connected()) {
                state.setText("Connetti un ingresso MIDI.",juce::dontSendNotification); return;
            }
            captured.reset(); owner->armControlLearn();
            state.setText("Muovi un fader o premi un tasto sul controller...",juce::dontSendNotification);
        };
        add.onClick=[this]{ createBinding(); };
        remove.onClick=[this]{
            if (!owner || selected<0 || selected>=(int)owner->show.controls.bindings.size()) return;
            const int id=owner->show.controls.bindings[(size_t)selected].id;
            if (owner->deleteMappingBinding(id)) {
                selected=-1; bindingList.updateContent(); bindingList.repaint();
            }
        };
        newPreset.onClick=[this]{
            if (!owner) return;
            const int id=owner->createMappingPreset(presetName.getText());
            if (id==0) {state.setText("Nome preset non valido.",juce::dontSendNotification);return;}
            refreshPresets(); presets.setSelectedId(id+1);
            presetName.clear();
        };
        assignPreset.onClick=[this]{
            if (!owner) return;
            if (owner->assignMappingPreset(owner->show.rows[(size_t)owner->selectedRow].id,
                                           std::max(0,presets.getSelectedId()-1)))
                state.setText("Preset assegnato alla cue selezionata.",juce::dontSendNotification);
        };
        refreshDevices(); refreshPresets(); refreshColumns();
        setSize(760,520); startTimerHz(10);
    }
    ~ControlPanel() override {stopTimer();bindingList.setModel(nullptr);}
    void paint(juce::Graphics& g) override {
        g.fillAll(palette::panel);
        label(g,"CONTROL & MAPPING",{17,12,330,26},20,palette::teal,true);
        label(g,"Ingresso MIDI",{17,55,250,19},11,palette::muted,true);
        label(g,"MIDI Learn",{17,120,150,19},11,palette::muted,true);
        label(g,"Regola",{17,205,120,17},11,palette::muted,true);
        label(g,"Destinazione",{213,205,120,17},11,palette::muted,true);
        label(g,"Takeover",{410,205,120,17},11,palette::muted,true);
        label(g,"Preset cue",{17,286,200,17},11,palette::muted,true);
        label(g,"Mapping salvati",{17,364,250,20},12,palette::teal,true);
        label(g,"Il mapping seleziona la destinazione attiva dopo GO, non al clic sulla riga.",
              {17,483,getWidth()-32,19},10,palette::muted);
    }
    void resized() override {
        const int w=getWidth();
        device.setBounds(17,78,w-157,30);
        connect.setBounds(w-132,78,115,30);
        learn.setBounds(17,143,120,33);
        state.setBounds(147,142,w-164,34);
        scope.setBounds(17,227,178,29);
        target.setBounds(213,227,180,29);
        takeover.setBounds(410,227,133,29);
        columns.setBounds(555,227,w-571,29);
        presets.setBounds(17,308,200,30);
        assignPreset.setBounds(226,308,150,30);
        presetName.setBounds(388,308,w-536,30);
        newPreset.setBounds(w-140,308,123,30);
        bindingList.setBounds(17,388,w-230,89);
        add.setBounds(w-202,389,184,37);
        remove.setBounds(w-202,436,184,37);
    }
private:
    juce::Component::SafePointer<MainComponent> owner;
    juce::ComboBox device,presets,scope,target,takeover,columns;
    juce::TextButton connect,learn,add,remove,newPreset,assignPreset;
    juce::TextEditor presetName;
    juce::Label state;
    juce::ListBox bindingList;
    std::vector<juce::String> deviceIds;
    std::vector<int> columnIds;
    std::optional<ControlEvent> captured;
    int selected=-1;
    void timerCallback() override {
        if (!owner) return;
        if (auto event=owner->consumeLearnedInput()) {
            captured=event;
            state.setText("Ricevuto "+event->deviceId+"  ch"+juce::String(event->channel)+
                " / "+juce::String(event->number)+" val "+juce::String(event->value),
                juce::dontSendNotification);
        }
    }
    void refreshDevices() {
        if (!owner) return;
        device.clear(juce::dontSendNotification);
        deviceIds.clear(); device.addItem("Disconnesso",1);
        int selectedId=1;
        for (const auto& d:juce::MidiInput::getAvailableDevices()) {
            deviceIds.push_back(d.identifier);
            const int id=(int)deviceIds.size()+1;
            device.addItem(d.name,id);
            if (owner->show.controls.midiInputId==d.identifier) selectedId=id;
        }
        device.setSelectedId(selectedId,juce::dontSendNotification);
    }
    void refreshPresets() {
        if (!owner) return;
        const int old=presets.getSelectedId();
        presets.clear(juce::dontSendNotification);
        presets.addItem("Nessun preset",1);
        for (const auto& p:owner->show.controls.presets) presets.addItem(p.name,p.id+1);
        int current=owner->show.rows[(size_t)owner->selectedRow].mappingPresetId+1;
        presets.setSelectedId(old>0 && presets.indexOfItemId(old)>=0 ? old : current,
                              juce::dontSendNotification);
    }
    void refreshColumns() {
        if (!owner) return;
        columns.clear(juce::dontSendNotification);
        columnIds.clear();
        for (const auto& c:owner->show.columns) if (c.type==ColumnType::audio) {
            columnIds.push_back(c.id);
            columns.addItem(c.name,(int)columnIds.size());
        }
        if (!columnIds.empty()) columns.setSelectedId(1);
    }
    void createBinding() {
        if (!owner || !captured) {
            state.setText("Usa prima MIDI Learn.",juce::dontSendNotification); return;
        }
        ControlBinding b;
        b.deviceId=captured->deviceId;
        b.kind=captured->kind; b.channel=captured->channel; b.number=captured->number;
        b.scope=(ControlScope)(scope.getSelectedId()-1);
        b.target=(ControlTarget)(target.getSelectedId()-1);
        b.takeover=(ControlTakeover)(takeover.getSelectedId()-1);
        if (b.scope==ControlScope::cue)
            b.scopeId=owner->show.rows[(size_t)owner->selectedRow].id;
        else if (b.scope==ControlScope::preset)
            b.scopeId=std::max(0,presets.getSelectedId()-1);
        if (b.scope==ControlScope::column || b.target==ControlTarget::columnGain) {
            const int i=columns.getSelectedId()-1;
            if (i<0 || i>=(int)columnIds.size()) {
                state.setText("Seleziona una colonna Audio.",juce::dontSendNotification);return;
            }
            b.targetColumnId=columnIds[(size_t)i];
            if (b.scope==ControlScope::column) b.scopeId=b.targetColumnId;
        }
        if (b.kind==ControlKind::note || b.target==ControlTarget::go ||
            b.target==ControlTarget::panic) b.takeover=ControlTakeover::jump;
        if (b.target==ControlTarget::masterGain || b.target==ControlTarget::columnGain) {
            b.minimum=0; b.maximum=1;
        }
        if (owner->addMappingBinding(b)) {
            bindingList.updateContent(); bindingList.repaint();
            state.setText("Mapping registrato nello show.",juce::dontSendNotification);
        } else state.setText(owner->status,juce::dontSendNotification);
    }
    int getNumRows() override {
        return owner ? (int)owner->show.controls.bindings.size():0;
    }
    void selectedRowsChanged(int row) override {selected=row;}
    void paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool active) override {
        if (!owner || row<0 || row>=getNumRows()) return;
        const auto& b=owner->show.controls.bindings[(size_t)row];
        if (active) g.fillAll(palette::teal.withAlpha(.20f));
        juce::String targetName=b.target==ControlTarget::go ? "GO" :
            b.target==ControlTarget::panic ? "PANIC" :
            b.target==ControlTarget::masterGain ? "MASTER" : "GAIN";
        label(g,"#"+juce::String(b.id)+"   ch"+juce::String(b.channel)+
            " "+(b.kind==ControlKind::cc?"CC ":b.kind==ControlKind::note?"NOTE ":"BEND ")+
            juce::String(b.number)+" → "+targetName+
            "  ["+juce::String((int)b.scope)+"]",
            {7,0,width-14,height},12,active?palette::text:palette::muted);
    }
};
void MainComponent::openControlPanel() {
    if (controlDialog) {controlDialog->toFront(true);return;}
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(new ControlPanel(*this));
    options.dialogTitle="The Cuelist — Control & Mapping";
    options.dialogBackgroundColour=palette::panel;
    options.escapeKeyTriggersCloseButton=true;
    options.useNativeTitleBar=true;
    options.resizable=true;
    controlDialog=options.launchAsync();
}
}
