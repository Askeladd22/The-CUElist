#include "MainComponent.h"
#include <cmath>

namespace cuelist {
void MainComponent::resetActiveMapping() {
    activeCueId=0;
    controlMapping.setActiveCue(0,0);
}
void MainComponent::refreshControlSettings() {
    controlMapping.setSettings(show.controls);
    std::vector<ControlLayer> layers;
    for (size_t column=0;column<show.columns.size();++column) {
        const auto& col=show.columns[column];
        if (col.type!=ColumnType::control) continue;
        if (col.controlLocked) {
            layers.push_back({col.id,0,1,true,col.controlDefaultSet});
        } else {
            for (const auto& row:show.rows)
                if (row.cells[column].controlSet)
                    layers.push_back({col.id,row.id,col.controlPriority,false,
                                      *row.cells[column].controlSet});
        }
    }
    controlMapping.setLayers(std::move(layers));
    int preset=0;
    for (const auto& row:show.rows)
        if (row.id==activeCueId) { preset=row.mappingPresetId; break; }
    controlMapping.setActiveCue(activeCueId,preset);
    if (midiInput.identifier()!=show.controls.midiInputId) {
        const auto result=midiInput.connect(show.controls.midiInputId);
        if (result.failed()) status=result.getErrorMessage();
    }
}
juce::Result MainComponent::selectControlInput(const juce::String& id) {
    auto result=midiInput.connect(id);
    pressedInputs.clear();
    // Preserve an explicitly selected disconnected identifier, never silently
    // switch to the first available device.
    show.controls.midiInputId=id;
    dirty=true;
    status=result.wasOk() ? (id.isEmpty() ? "MIDI input disabled." :
        "MIDI input connected.") : result.getErrorMessage();
    repaint(); return result;
}
bool MainComponent::addMappingBinding(ControlBinding b) {
    auto candidate=show;
    b.id=candidate.controls.nextId++;
    candidate.controls.bindings.push_back(std::move(b));
    const auto valid=validateShow(candidate);
    if (valid.failed()) { status=valid.getErrorMessage(); return false; }
    show=std::move(candidate); refreshControlSettings();
    dirty=true; status="Mapping added."; repaint(); return true;
}
bool MainComponent::deleteMappingBinding(int id) {
    auto& bindings=show.controls.bindings;
    const auto n=bindings.size();
    bindings.erase(std::remove_if(bindings.begin(),bindings.end(),
        [id](const auto& b){return b.id==id;}),bindings.end());
    if (bindings.size()==n) return false;
    refreshControlSettings(); dirty=true; status="Mapping removed."; repaint();
    return true;
}
int MainComponent::createMappingPreset(const juce::String& name) {
    auto candidate=show; const auto label=name.trim();
    if (label.isEmpty() || label.length()>120) return 0;
    const int id=candidate.controls.nextId++;
    candidate.controls.presets.push_back({id,label});
    if (validateShow(candidate).failed()) return 0;
    show=std::move(candidate); refreshControlSettings(); dirty=true;
    status="Preset "+label+" created."; repaint(); return id;
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
    status="Control Mapping preset assigned."; repaint(); return true;
}
bool MainComponent::addControlSetBinding(int row,int column,ControlBinding b) {
    if (row<0 || row>=(int)show.rows.size() || column<0 ||
        column>=(int)show.columns.size() || show.columns[column].type!=ColumnType::control)
        return false;
    auto candidate=show;
    auto& col=candidate.columns[(size_t)column];
    auto& cellSet=candidate.rows[(size_t)row].cells[(size_t)column].controlSet;
    if (!col.controlLocked && !cellSet) cellSet.emplace();
    ControlSet* set=col.controlLocked ? &col.controlDefaultSet : &*cellSet;
    b.id=candidate.controls.nextId++;
    b.scope=ControlScope::global; b.scopeId=0;
    set->bindings.push_back(std::move(b));
    if (auto valid=validateShow(candidate);valid.failed()) {
        status=valid.getErrorMessage(); repaint(); return false;
    }
    show=std::move(candidate); refreshControlSettings(); dirty=true;
    status="Control Set mapping added."; refresh(); return true;
}
bool MainComponent::deleteControlSetBinding(int row,int column,int id) {
    if (row<0 || row>=(int)show.rows.size() || column<0 ||
        column>=(int)show.columns.size() || show.columns[column].type!=ColumnType::control)
        return false;
    auto& col=show.columns[(size_t)column];
    ControlSet* set=col.controlLocked ? &col.controlDefaultSet :
        (show.rows[(size_t)row].cells[(size_t)column].controlSet ?
         &*show.rows[(size_t)row].cells[(size_t)column].controlSet : nullptr);
    if (!set) return false;
    const auto size=set->bindings.size();
    set->bindings.erase(std::remove_if(set->bindings.begin(),set->bindings.end(),
        [id](const ControlBinding& b){return b.id==id;}),set->bindings.end());
    if (set->bindings.size()==size) return false;
    refreshControlSettings();dirty=true;status="Control Set mapping removed.";refresh();return true;
}
bool MainComponent::updateControlBinding(int row,int column,int id,ControlBinding b,bool inControlSet) {
    auto candidate=show;
    std::vector<ControlBinding>* bindings=&candidate.controls.bindings;
    if (inControlSet) {
        if (row<0 || row>=(int)candidate.rows.size() ||
            column<0 || column>=(int)candidate.columns.size() ||
            candidate.columns[(size_t)column].type!=ColumnType::control) return false;
        auto& col=candidate.columns[(size_t)column];
        auto& cell=candidate.rows[(size_t)row].cells[(size_t)column];
        if (!col.controlLocked && !cell.controlSet) return false;
        bindings=col.controlLocked ? &col.controlDefaultSet.bindings :
            &cell.controlSet->bindings;
        b.scope=ControlScope::global; b.scopeId=0;
    }
    const auto it=std::find_if(bindings->begin(),bindings->end(),
        [id](const ControlBinding& old){return old.id==id;});
    if (it==bindings->end()) return false;
    b.id=id; *it=std::move(b);
    if (auto valid=validateShow(candidate);valid.failed()) {
        status=valid.getErrorMessage();repaint();return false;
    }
    show=std::move(candidate); refreshControlSettings();
    dirty=true;status="Mapping updated.";refresh();return true;
}
bool MainComponent::setControlColumnLocked(int column,bool locked,int sourceRow) {
    if (column<0 || column>=(int)show.columns.size() ||
        show.columns[(size_t)column].type!=ColumnType::control ||
        sourceRow<0 || sourceRow>=(int)show.rows.size()) return false;
    auto candidate=show;
    auto& col=candidate.columns[(size_t)column];
    if (locked && !col.controlLocked) {
        // Preserve source cue and any other cue-specific Control Sets as
        // dormant metadata. Unlocking makes those sets editable again.
        const auto& cell=candidate.rows[(size_t)sourceRow].cells[(size_t)column];
        if (cell.controlSet) {
            col.controlDefaultSet=*cell.controlSet;
            for (auto& binding:col.controlDefaultSet.bindings)
                binding.id=candidate.controls.nextId++;
        }
        col.controlLocked=true; col.controlPriority=1;
    } else if (!locked && col.controlLocked) {
        // Retain edits made to the locked default when returning to cue scope.
        // Other cue-specific sets, held dormant during lock, reappear untouched.
        auto& source=candidate.rows[(size_t)sourceRow].cells[(size_t)column];
        if (!col.controlDefaultSet.bindings.empty()) {
            source.controlSet=col.controlDefaultSet;
            for (auto& binding:source.controlSet->bindings)
                binding.id=candidate.controls.nextId++;
        }
        col.controlDefaultSet.bindings.clear();
        col.controlLocked=false; col.controlPriority=2;
    } else return true;
    if (auto valid=validateShow(candidate);valid.failed()) {
        status=valid.getErrorMessage(); return false;
    }
    show=std::move(candidate);refreshControlSettings();dirty=true;refresh();
    status=locked ? "Control Column locked: default set applies to every cue." :
        "Control Column unlocked: cue-specific sets restored.";
    return true;
}
bool MainComponent::setControlColumnPriority(int column,int priority) {
    if (column<0 || column>=(int)show.columns.size() ||
        show.columns[(size_t)column].type!=ColumnType::control ||
        show.columns[(size_t)column].controlLocked || priority<2 || priority>4)
        return false;
    show.columns[(size_t)column].controlPriority=priority;
    refreshControlSettings();dirty=true;refresh();
    status="Control Column priority updated.";return true;
}
void MainComponent::editControlSet(int row,int column) {
    if (!select(row,column) || show.columns[(size_t)column].type!=ColumnType::control) return;
    openControlPanel(true);
}
void MainComponent::dispatchControl(const ControlEvent& input) {
    // Track physical button edges even when a cue has no matching binding.
    // Otherwise an off/release arriving during a different preset is lost.
    const auto physical=input.deviceId+"|"+juce::String((int)input.kind)+"|"+
        juce::String(input.channel)+"|"+juce::String(input.number);
    const bool wasDown=pressedInputs[physical];
    const bool isDown=!input.isNoteOff && input.normalized()>=0.5f;
    pressedInputs[physical]=isDown;
    // Hardware MIDI events have already been copied to the UI thread.
    // Pickup compares against the physical value expected for the current
    // actual target. No mutation of show happens in a MIDI callback.
    std::optional<float> current;
    // Determine pickup from the same winner the pure mapping engine selected,
    // rather than an arbitrary earlier global binding.
    if (const auto* winner=controlMapping.matched(input)) {
        const auto& b=*winner;
        if (b.target==ControlTarget::masterGain || b.target==ControlTarget::masterPan) {
            const float d=b.maximum-b.minimum;
            if (d>0) current=juce::jlimit(0.0f,1.0f,
                ((b.target==ControlTarget::masterPan ? show.masterPan : show.masterGain)-b.minimum)/d);
        } else if (b.target==ControlTarget::columnGain || b.target==ControlTarget::columnPan) {
            for (const auto& col:show.columns) if (col.id==b.targetColumnId) {
                const float d=b.maximum-b.minimum;
                if (d>0) current=juce::jlimit(0.0f,1.0f,
                    ((b.target==ControlTarget::columnPan ? col.pan : col.gain)-b.minimum)/d);
                break;
            }
        }
        if (current.has_value()) {
            current=inverseControlCurve(*current,b.curve);
            if (b.invert) current=1.0f-*current;
        }
    }
    const auto action=controlMapping.resolve(input,current);
    if (!action) return;
    const auto& binding=action->binding;
    if (binding.target==ControlTarget::go || binding.target==ControlTarget::panic) {
        // Debounce by physical event identity, not mapping ID. GO changes the
        // active cue, so per-binding debounce would re-trigger a held button.
        if (!action->pressed || wasDown) return;
        if (binding.target==ControlTarget::go) goNext();
        else stopAll();
        return;
    }
    if (binding.target==ControlTarget::masterGain) {
        if (std::abs(show.masterGain-action->scaled)>0.005f)
            setMasterAudio(action->scaled);
    } else if (binding.target==ControlTarget::masterPan) {
        if (std::abs(show.masterPan-action->scaled)>0.005f)
            setMasterPan(action->scaled);
    } else if (binding.target==ControlTarget::columnGain ||
               binding.target==ControlTarget::columnPan) {
        for (int i=0;i<(int)show.columns.size();++i)
            if (show.columns[(size_t)i].id==binding.targetColumnId &&
                show.columns[(size_t)i].type==ColumnType::audio) {
                const auto& c=show.columns[(size_t)i];
                const float currentValue=binding.target==ControlTarget::columnPan ? c.pan : c.gain;
                if (std::abs(currentValue-action->scaled)>0.005f) {
                    setColumnUtilities(i,
                        binding.target==ControlTarget::columnGain ? action->scaled : c.gain,
                        binding.target==ControlTarget::columnPan ? action->scaled : c.pan,
                        c.muted,c.phaseInverted);
                    utilities.refresh();
                }
                break;
            }
    }
    if (binding.target==ControlTarget::midiCc) {
        for (const auto& col:show.columns) if (col.id==binding.targetColumnId &&
            col.type==ColumnType::midi) {
            if (!engine.sendMappedMidiCc(col.midiOutputId,binding.outputChannel,
                    binding.outputNumber,juce::jlimit(0,127,(int)std::lround(action->scaled))))
                status="MIDI control: output unavailable or queue full.";
            break;
        }
    } else if (binding.target==ControlTarget::oscFloat) {
        for (const auto& col:show.columns) if (col.id==binding.targetColumnId &&
            col.type==ColumnType::osc) {
            if (!controlOutput.sendOscFloat(col.oscHost,col.oscPort,
                binding.oscAddress,action->scaled))
                status="OSC control: invalid destination or queue full.";
            break;
        }
    }
}
class ControlPanel final : public juce::Component, private juce::Timer,
                           private juce::ListBoxModel {
public:
    explicit ControlPanel(MainComponent& main,bool useSet):owner(&main),
        editSet(useSet),sourceRow(main.selectedRow),sourceColumn(main.selectedColumn),
        sourceRowId(main.show.rows[(size_t)main.selectedRow].id),
        sourceColumnId(main.show.columns[(size_t)main.selectedColumn].id) {
        setName("controlMappingPanel");
        for (juce::Component* c : std::initializer_list<juce::Component*>{
            &device,&connect,&learn,&add,&remove,&newPreset,&assignPreset,
            &presetName,&presets,&scope,&target,&takeover,&columns,&bindingList,&state,
            &outputCc,&oscAddress,&invert})
            addAndMakeVisible(c);
        device.setName("controlInputDevice");
        device.setTooltip("Select a device explicitly. No automatic fallback on disconnect.");
        connect.setButtonText("Refresh ports");
        learn.setButtonText("MIDI Learn");
        add.setButtonText("Add mapping");
        remove.setButtonText("Remove selected");
        newPreset.setButtonText("New preset");
        assignPreset.setButtonText("Assign to cue");
        presetName.setTextToShowWhenEmpty("New preset name",palette::muted);
        state.setName("mappingStatus");
        state.setJustificationType(juce::Justification::centredLeft);
        scope.addItemList({"Global","Column","Preset","Single cue"},1);
        scope.setSelectedId(1);
        scope.setVisible(!editSet);
        presets.setVisible(!editSet); newPreset.setVisible(!editSet);
        assignPreset.setVisible(!editSet); presetName.setVisible(!editSet);
        target.addItemList({"Master Audio","Audio column gain","GO next cue","PANIC",
                            "MIDI CC","OSC Float","Audio column pan","Master pan"},1);
        target.setSelectedId(1);
        target.onChange=[this]{ refreshColumns(); };
        outputCc.setName("mappingOutputCc");
        outputCc.setText("20"); outputCc.setInputRestrictions(3,"0123456789");
        oscAddress.setName("mappingOscAddress");
        oscAddress.setText("/control/value");
        invert.setName("mappingInvert");
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
            state.setText(result.wasOk() ? "Device selected." : result.getErrorMessage(),
                juce::dontSendNotification);
        };
        connect.onClick=[this]{
            refreshDevices();
            if (owner && owner->show.controls.midiInputId.isNotEmpty())
                owner->selectControlInput(owner->show.controls.midiInputId);
        };
        learn.onClick=[this]{
            if (!owner || !owner->midiInput.connected()) {
                state.setText("Connect a MIDI input.",juce::dontSendNotification); return;
            }
            captured.reset(); owner->armControlLearn();
            state.setText("Move a fader or press a button on the controller...",juce::dontSendNotification);
        };
        add.onClick=[this]{ createBinding(); };
        remove.onClick=[this]{
            const auto* bindings=currentBindings();
            if (!owner || !bindings || selected<0 || selected>=(int)bindings->size()) return;
            const int id=(*bindings)[(size_t)selected].id;
            int row=-1,column=-1;
            const bool removed=editSet ?
                (resolveSource(row,column) && owner->deleteControlSetBinding(row,column,id)) :
                owner->deleteMappingBinding(id);
            if (removed) {
                selected=-1; bindingList.updateContent(); bindingList.repaint();
            }
        };
        newPreset.onClick=[this]{
            if (!owner) return;
            const int id=owner->createMappingPreset(presetName.getText());
            if (id==0) {state.setText("Invalid preset name.",juce::dontSendNotification);return;}
            refreshPresets(); presets.setSelectedId(id+1);
            presetName.clear();
        };
        assignPreset.onClick=[this]{
            if (!owner) return;
            if (owner->assignMappingPreset(owner->show.rows[(size_t)owner->selectedRow].id,
                                           std::max(0,presets.getSelectedId()-1)))
                state.setText("Preset assigned to selected cue.",juce::dontSendNotification);
        };
        refreshDevices(); refreshPresets(); refreshColumns();
        setSize(780,610); startTimerHz(10);
    }
    ~ControlPanel() override {stopTimer();bindingList.setModel(nullptr);}
    void paint(juce::Graphics& g) override {
        g.fillAll(palette::panel);
        label(g,editSet ? "CONTROL SET / Q"+juce::String(sourceRow+1) :
              "CONTROL & MAPPING",{17,12,350,26},20,palette::teal,true);
        label(g,"MIDI input",{17,55,250,19},11,palette::muted,true);
        label(g,"MIDI Learn",{17,120,150,19},11,palette::muted,true);
        label(g,editSet ? "Column-level priority (set in utilities)" : "Scope",
              {17,205,195,17},11,palette::muted,true);
        label(g,"Destination",{213,205,120,17},11,palette::muted,true);
        label(g,"Takeover",{410,205,120,17},11,palette::muted,true);
        label(g,"Output CC",{17,263,95,16},10,palette::muted,true);
        label(g,"OSC address",{112,263,140,16},10,palette::muted,true);
        const auto currentTitle=editSet ? juce::String("Control Column / ") +
              juce::String(sourceColumnId) : juce::String("Cue preset");
        label(g,currentTitle,{17,324,300,17},11,palette::muted,true);
        label(g,"Saved mappings",{17,402,250,20},12,palette::teal,true);
        label(g,editSet ? "Control Set takes effect on GO; locked sets apply to every cue." :
              "Mapping chooses its active destination after GO, not when selecting a row.",
              {17,579,getWidth()-32,19},10,palette::muted);
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
        outputCc.setBounds(17,281,83,29);
        oscAddress.setBounds(112,281,210,29);
        invert.setBounds(345,281,90,29);
        presets.setBounds(17,344,200,30);
        assignPreset.setBounds(226,344,150,30);
        presetName.setBounds(388,344,w-536,30);
        newPreset.setBounds(w-140,344,123,30);
        bindingList.setBounds(17,425,w-230,139);
        add.setBounds(w-202,426,184,37);
        remove.setBounds(w-202,473,184,37);
    }
private:
    juce::Component::SafePointer<MainComponent> owner;
    bool editSet=false;
    int sourceRow=0,sourceColumn=0;
    int sourceRowId=0,sourceColumnId=0;
    bool resolveSource(int& row,int& column) const {
        if (!owner) return false;
        for (int i=0;i<(int)owner->show.rows.size();++i)
            if (owner->show.rows[(size_t)i].id==sourceRowId) {row=i;break;}
        for (int i=0;i<(int)owner->show.columns.size();++i)
            if (owner->show.columns[(size_t)i].id==sourceColumnId) {column=i;break;}
        return row>=0 && row<(int)owner->show.rows.size() &&
            column>=0 && column<(int)owner->show.columns.size() &&
            owner->show.rows[(size_t)row].id==sourceRowId &&
            owner->show.columns[(size_t)column].id==sourceColumnId;
    }
    const std::vector<ControlBinding>* currentBindings() const {
        if (!owner) return nullptr;
        if (!editSet) return &owner->show.controls.bindings;
        int row=-1,columnIndex=-1;
        if (!resolveSource(row,columnIndex)) return nullptr;
        const auto& column=owner->show.columns[(size_t)columnIndex];
        if (column.type!=ColumnType::control) return nullptr;
        if (column.controlLocked) return &column.controlDefaultSet.bindings;
        const auto& cell=owner->show.rows[(size_t)row].cells[(size_t)columnIndex];
        return cell.controlSet ? &cell.controlSet->bindings : nullptr;
    }
    juce::ComboBox device,presets,scope,target,takeover,columns;
    juce::TextButton connect,learn,add,remove,newPreset,assignPreset;
    juce::TextEditor presetName,outputCc,oscAddress;
    juce::ToggleButton invert{"Invert"};
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
        deviceIds.clear(); device.addItem("Disconnected",1);
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
        presets.addItem("No preset",1);
        for (const auto& p:owner->show.controls.presets) presets.addItem(p.name,p.id+1);
        int current=owner->show.rows[(size_t)owner->selectedRow].mappingPresetId+1;
        presets.setSelectedId(old>0 && presets.indexOfItemId(old)>=0 ? old : current,
                              juce::dontSendNotification);
    }
    void refreshColumns() {
        if (!owner) return;
        columns.clear(juce::dontSendNotification);
        columnIds.clear();
        const int targetId=target.getSelectedId();
        for (const auto& c:owner->show.columns) {
            const bool eligible=(targetId==2 || targetId==7) ? c.type==ColumnType::audio :
                targetId==5 ? c.type==ColumnType::midi :
                targetId==6 ? c.type==ColumnType::osc : true;
            if (!eligible) continue;
            columnIds.push_back(c.id);
            columns.addItem(c.name,(int)columnIds.size());
        }
        if (!columnIds.empty()) columns.setSelectedId(1);
    }
    void createBinding() {
        if (!owner || !captured) {
            state.setText("Use MIDI Learn first.",juce::dontSendNotification); return;
        }
        ControlBinding b;
        b.deviceId=captured->deviceId;
        b.kind=captured->kind; b.channel=captured->channel; b.number=captured->number;
        b.scope=(ControlScope)(scope.getSelectedId()-1);
        switch (target.getSelectedId()) {
            case 1: b.target=ControlTarget::masterGain; break;
            case 2: b.target=ControlTarget::columnGain; break;
            case 3: b.target=ControlTarget::go; break;
            case 4: b.target=ControlTarget::panic; break;
            case 5: b.target=ControlTarget::midiCc; break;
            case 6: b.target=ControlTarget::oscFloat; break;
            case 7: b.target=ControlTarget::columnPan; break;
            case 8: b.target=ControlTarget::masterPan; break;
            default: return;
        }
        b.takeover=(ControlTakeover)(takeover.getSelectedId()-1);
        if (b.scope==ControlScope::cue)
            b.scopeId=owner->show.rows[(size_t)owner->selectedRow].id;
        else if (b.scope==ControlScope::preset)
            b.scopeId=std::max(0,presets.getSelectedId()-1);
        if (b.scope==ControlScope::column || b.target==ControlTarget::columnGain ||
            b.target==ControlTarget::columnPan ||
            b.target==ControlTarget::midiCc || b.target==ControlTarget::oscFloat) {
            const int i=columns.getSelectedId()-1;
            if (i<0 || i>=(int)columnIds.size()) {
                state.setText("Select a destination column.",juce::dontSendNotification);return;
            }
            b.targetColumnId=columnIds[(size_t)i];
            if (b.scope==ControlScope::column) b.scopeId=b.targetColumnId;
        }
        if (b.kind==ControlKind::note || b.target==ControlTarget::go ||
            b.target==ControlTarget::panic) b.takeover=ControlTakeover::jump;
        b.invert=invert.getToggleState();
        if (b.target==ControlTarget::midiCc) {
            b.outputChannel=captured->channel;
            b.outputNumber=outputCc.getText().getIntValue();
            b.minimum=0; b.maximum=127;
        } else if (b.target==ControlTarget::oscFloat) {
            b.oscAddress=oscAddress.getText().trim();
            b.minimum=0; b.maximum=1;
        } else if (b.target==ControlTarget::masterPan ||
                   b.target==ControlTarget::columnPan) {
            b.minimum=-1; b.maximum=1;
        } else if (b.target==ControlTarget::masterGain ||
                   b.target==ControlTarget::columnGain) {
            b.minimum=0; b.maximum=1;
        }
        int row=-1,column=-1;
        if ((editSet ? (resolveSource(row,column) &&
                         owner->addControlSetBinding(row,column,b)) :
                       owner->addMappingBinding(b))) {
            bindingList.updateContent(); bindingList.repaint();
            state.setText("Mapping saved to the show.",juce::dontSendNotification);
        } else state.setText(owner->status,juce::dontSendNotification);
    }
    int getNumRows() override {
        const auto* entries=currentBindings();
        return entries ? (int)entries->size():0;
    }
    void selectedRowsChanged(int row) override {selected=row;}
    void paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool active) override {
        if (!owner || row<0 || row>=getNumRows()) return;
        const auto& b=(*currentBindings())[(size_t)row];
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
void MainComponent::openControlPanel(bool editSet) {
    if (controlDialog) {
        auto* previous=controlDialog.getComponent();
        controlDialog=nullptr;
        delete previous;
    }
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(new ControlPanel(*this,editSet));
    options.dialogTitle=editSet ? "The Cuelist — Control Set" :
        "The Cuelist — Control & Mapping";
    options.dialogBackgroundColour=palette::panel;
    options.escapeKeyTriggersCloseButton=true;
    options.useNativeTitleBar=true;
    options.resizable=true;
    controlDialog=options.launchAsync();
}
}
