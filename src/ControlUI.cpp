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
int MainComponent::saveControlSetTemplate(int row,int column,const juce::String& name) {
    if (row<0 || row>=(int)show.rows.size() || column<0 ||
        column>=(int)show.columns.size() ||
        show.columns[(size_t)column].type!=ColumnType::control) return 0;
    const auto label=name.trim();
    if (label.isEmpty() || label.length()>120) return 0;
    auto candidate=show;
    auto& col=candidate.columns[(size_t)column];
    const ControlSet* source=col.controlLocked ? &col.controlDefaultSet :
        (candidate.rows[(size_t)row].cells[(size_t)column].controlSet ?
         &*candidate.rows[(size_t)row].cells[(size_t)column].controlSet : nullptr);
    if (!source || source->bindings.empty()) return 0;
    ControlSetTemplate item;
    item.id=candidate.controls.nextId++;
    item.name=label;
    item.set=*source;
    for (auto& binding:item.set.bindings)
        binding.id=candidate.controls.nextId++;
    candidate.controls.templates.push_back(std::move(item));
    if (auto check=validateShow(candidate);check.failed()) {
        status=check.getErrorMessage();return 0;
    }
    const int result=candidate.controls.templates.back().id;
    show=std::move(candidate);refreshControlSettings();
    dirty=true;status="Reusable Control Set saved.";refresh();return result;
}
bool MainComponent::applyControlSetTemplate(int row,int column,int templateId) {
    if (row<0 || row>=(int)show.rows.size() || column<0 ||
        column>=(int)show.columns.size() ||
        show.columns[(size_t)column].type!=ColumnType::control) return false;
    auto candidate=show;
    const auto it=std::find_if(candidate.controls.templates.begin(),
        candidate.controls.templates.end(),
        [templateId](const ControlSetTemplate& item){return item.id==templateId;});
    if (it==candidate.controls.templates.end()) return false;
    ControlSet copy=it->set;
    for (auto& binding:copy.bindings)
        binding.id=candidate.controls.nextId++;
    auto& col=candidate.columns[(size_t)column];
    if (col.controlLocked) col.controlDefaultSet=std::move(copy);
    else candidate.rows[(size_t)row].cells[(size_t)column].controlSet=std::move(copy);
    if (auto valid=validateShow(candidate);valid.failed()) {
        status=valid.getErrorMessage();return false;
    }
    show=std::move(candidate);refreshControlSettings();
    dirty=true;status="Control Set template applied to selected cell.";refresh();return true;
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
        col.controlDefaultSourceRowId=candidate.rows[(size_t)sourceRow].id;
        col.controlLocked=true; col.controlPriority=1;
    } else if (!locked && col.controlLocked) {
        // Always restore the edited default to its original source cue, not
        // the currently selected cue: otherwise unlocking could silently
        // destroy a different Q's partial override.
        int destination=-1;
        for (int i=0;i<(int)candidate.rows.size();++i)
            if (candidate.rows[(size_t)i].id==col.controlDefaultSourceRowId)
                {destination=i;break;}
        if (destination<0) {
            if (!candidate.rows[(size_t)sourceRow].cells[(size_t)column].controlSet)
                destination=sourceRow;
            else for (int i=0;i<(int)candidate.rows.size();++i)
                if (!candidate.rows[(size_t)i].cells[(size_t)column].controlSet)
                    {destination=i;break;}
        }
        if (destination<0 && !col.controlDefaultSet.bindings.empty()) {
            status="Cannot unlock without overwriting another Control Set. Add an empty cue first.";
            repaint(); return false;
        }
        if (destination>=0 && !col.controlDefaultSet.bindings.empty()) {
            auto& target=candidate.rows[(size_t)destination].cells[(size_t)column];
            target.controlSet=col.controlDefaultSet;
            for (auto& binding:target.controlSet->bindings)
                binding.id=candidate.controls.nextId++;
        }
        col.controlDefaultSet.bindings.clear();
        col.controlDefaultSourceRowId=0;
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
            &outputCc,&oscAddress,&invert,&updateSelected,&curve,&minimum,&maximum,
             &effectivePreview,&templateName,&templates,&saveTemplate,&loadTemplate})
            addAndMakeVisible(c);
        device.setName("controlInputDevice");
        device.setTooltip("Select a device explicitly. No automatic fallback on disconnect.");
        connect.setButtonText("Refresh ports");
        learn.setButtonText("MIDI Learn");
        add.setButtonText("Add mapping");
        updateSelected.setButtonText("Update selected");
        updateSelected.setName("mappingUpdateSelected");
        updateSelected.setEnabled(false);
        updateSelected.onClick=[this] { createBinding(true); };
        curve.setName("mappingResponseCurve");
        curve.addItemList({"Linear","Ease in","Ease out"},1);
        curve.setSelectedId(1);
        minimum.setName("mappingMinimum");
        maximum.setName("mappingMaximum");
        minimum.setText("0"); maximum.setText("1");
        effectivePreview.setName("effectiveControlPreview");
        effectivePreview.setMultiLine(true);
        effectivePreview.setReadOnly(true);
        effectivePreview.setScrollbarsShown(true);
        effectivePreview.setVisible(editSet);
        templateName.setVisible(editSet); templates.setVisible(editSet);
        saveTemplate.setVisible(editSet); loadTemplate.setVisible(editSet);
        templateName.setTextToShowWhenEmpty("Reusable Set name",palette::muted);
        templateName.setName("controlTemplateName");
        templates.setName("controlTemplates");
        saveTemplate.setName("saveControlTemplate");
        loadTemplate.setName("loadControlTemplate");
        saveTemplate.setButtonText("Save Set");
        loadTemplate.setButtonText("Apply Set");
        saveTemplate.onClick=[this] {
            if (!owner) return;
            int row=-1,col=-1;
            if (!resolveSource(row,col)) return;
            const int id=owner->saveControlSetTemplate(row,col,templateName.getText());
            if (id) {
                refreshTemplates();templates.setSelectedId(id+1,juce::dontSendNotification);
                state.setText("Control Set saved for reuse.",juce::dontSendNotification);
            } else state.setText(owner->status,juce::dontSendNotification);
        };
        loadTemplate.onClick=[this] {
            if (!owner || templates.getSelectedId()<2) return;
            int row=-1,col=-1;
            if (!resolveSource(row,col)) return;
            if (owner->applyControlSetTemplate(row,col,templates.getSelectedId()-1)) {
                selected=-1;bindingList.deselectAllRows();
                bindingList.updateContent();bindingList.repaint();
                updateSelected.setEnabled(false);
                updateEffectivePreview();
                state.setText("Reusable Set applied.",juce::dontSendNotification);
            } else state.setText(owner->status,juce::dontSendNotification);
        };
        effectivePreview.setColour(juce::TextEditor::outlineColourId,palette::line);
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
        target.onChange=[this] {
            refreshColumns();
            const int t=target.getSelectedId();
            minimum.setText(t==7 || t==8 ? "-1" : "0",false);
            maximum.setText(t==5 ? "127" : "1",false);
        };
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
            state.setText(selected>=0 && currentBindings() && selected<(int)currentBindings()->size() ?
                "MIDI Learn for mapping #"+juce::String((*currentBindings())[(size_t)selected].id)+
                    " — move its hardware control, then click Update selected." :
                "MIDI Learn for NEW mapping — move a controller control.",juce::dontSendNotification);
        };
        add.onClick=[this]{ createBinding(false); };
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
        refreshDevices(); refreshPresets(); refreshColumns();refreshTemplates();
        updateEffectivePreview();
        setSize(780,editSet ? 718 : 610); startTimerHz(10);
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
        if (editSet) label(g,"REUSABLE CONTROL SETS",{17,421,290,18},10,palette::teal,true);
        label(g,editSet ? "Mappings in this Control Set" : "Saved mappings",
              {17,editSet ? 475 : 402,350,20},12,palette::teal,true);
        label(g,"Curve",{440,263,100,16},10,palette::muted,true);
        label(g,"Min",{572,263,75,16},10,palette::muted,true);
        label(g,"Max",{671,263,75,16},10,palette::muted,true);
        label(g,editSet ? "Control Set takes effect on GO; locked sets apply to every cue." :
              "Mapping chooses its active destination after GO, not when selecting a row.",
              {17,editSet ? 686 : 579,getWidth()-32,19},10,palette::muted);
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
        invert.setBounds(345,281,88,29);
        curve.setBounds(440,281,116,29);
        minimum.setBounds(572,281,86,29);
        maximum.setBounds(671,281,86,29);
        effectivePreview.setBounds(17,344,w-34,66);
        templateName.setBounds(17,440,170,27);
        templates.setBounds(193,440,232,27);
        saveTemplate.setBounds(432,440,140,27);
        loadTemplate.setBounds(580,440,w-597,27);
        presets.setBounds(17,344,200,30);
        assignPreset.setBounds(226,344,150,30);
        presetName.setBounds(388,344,w-536,30);
        newPreset.setBounds(w-140,344,123,30);
        bindingList.setBounds(17,editSet ? 501 : 425,w-230,editSet ? 164 : 139);
        add.setBounds(w-202,editSet ? 501 : 426,184,37);
        updateSelected.setBounds(w-202,editSet ? 547 : 472,184,37);
        remove.setBounds(w-202,editSet ? 593 : 518,184,37);
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
    juce::ComboBox device,presets,scope,target,takeover,columns,curve,templates;
    juce::TextButton connect,learn,add,remove,updateSelected,newPreset,assignPreset,saveTemplate,loadTemplate;
    juce::TextEditor presetName,outputCc,oscAddress,minimum,maximum,effectivePreview,templateName;
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
            state.setText("Learned "+event->deviceId+" ch"+juce::String(event->channel)+
                " / "+juce::String(event->number)+" value "+juce::String(event->value),
                juce::dontSendNotification);
        }
        if (editSet) updateEffectivePreview();
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
    void refreshTemplates() {
        if (!owner) return;
        const int old=templates.getSelectedId();
        templates.clear(juce::dontSendNotification);
        templates.addItem("Choose saved set",1);
        for (const auto& item:owner->show.controls.templates)
            templates.addItem(item.name,item.id+1);
        templates.setSelectedId(old>0 && templates.indexOfItemId(old)>=0 ? old : 1,
                                juce::dontSendNotification);
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
    void createBinding(bool update) {
        const auto* existing=currentBindings();
        if (!owner || (update && (!existing || selected<0 || selected>=(int)existing->size())) ||
            (!update && !captured)) {
            state.setText("Use MIDI Learn first or select a mapping to edit.",
                juce::dontSendNotification); return;
        }
        ControlBinding b=update ? (*existing)[(size_t)selected] : ControlBinding{};
        if (captured) {
            b.deviceId=captured->deviceId;
            b.kind=captured->kind; b.channel=captured->channel; b.number=captured->number;
        }
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
        b.curve=(ControlCurve)(curve.getSelectedId()-1);
        b.scopeId=0; b.targetColumnId=0;
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
            b.outputChannel=b.channel;
            b.outputNumber=outputCc.getText().getIntValue();
        }
        if (b.target==ControlTarget::oscFloat)
            b.oscAddress=oscAddress.getText().trim();
        const auto low=minimum.getText().trim(),high=maximum.getText().trim();
        const auto numeric=[](const juce::String& value) {
            return value.isNotEmpty() && value.containsAnyOf("0123456789") &&
                value.retainCharacters("0123456789.-+")==value;
        };
        if (!numeric(low) || !numeric(high)) {
            state.setText("Enter valid minimum and maximum values.",juce::dontSendNotification); return;
        }
        b.minimum=low.getFloatValue(); b.maximum=high.getFloatValue();
        int row=-1,column=-1;
        const bool resolved=!editSet || resolveSource(row,column);
        const bool saved=resolved && (update ?
            owner->updateControlBinding(row,column,b.id,b,editSet) :
            editSet ? owner->addControlSetBinding(row,column,b) :
                      owner->addMappingBinding(b));
        if (saved) {
            bindingList.updateContent(); bindingList.repaint();
            selected=-1;updateSelected.setEnabled(false);bindingList.deselectAllRows();
            captured.reset();updateEffectivePreview();
            state.setText(update ? "Mapping updated." : "Mapping saved to the show.",
                juce::dontSendNotification);
        } else state.setText(owner->status,juce::dontSendNotification);
    }
    void updateEffectivePreview() {
        if (!owner || !editSet) return;
        int row=-1,column=-1;
        if (!resolveSource(row,column)) {
            effectivePreview.setText("Control Column no longer exists.",false);return;
        }
        const int cue=owner->show.rows[(size_t)row].id;
        const bool live=owner->activeCueMappingId()==cue;
        const auto resolved=owner->effectiveControlSet(cue);
        juce::String summary=live ? "LIVE Q"+juce::String(row+1) :
            "PREVIEW Q"+juce::String(row+1)+" — changes take effect after GO";
        if (resolved.empty()) summary+="\nNo active Control Column mappings.";
        for (const auto& item:resolved) {
            juce::String source="unknown";
            for (const auto& col:owner->show.columns)
                if (col.id==item.columnId) {source=col.name;break;}
            const auto& b=item.binding;
            const bool local=item.columnId==sourceColumnId;
            summary+="\n"+juce::String(local ? "LOCAL / " : "INHERITED / ")+
                source+" (P"+juce::String(item.priority)+")  "+
                (b.kind==ControlKind::cc ? "CC " :
                 b.kind==ControlKind::note ? "NOTE " : "BEND ")+
                juce::String(b.channel)+":"+juce::String(b.number)+
                " -> "+(b.target==ControlTarget::masterGain ? "MASTER GAIN" :
                  b.target==ControlTarget::masterPan ? "MASTER PAN" :
                  b.target==ControlTarget::columnGain ? "CHANNEL GAIN" :
                  b.target==ControlTarget::columnPan ? "CHANNEL PAN" :
                  b.target==ControlTarget::midiCc ? "MIDI CC" :
                  b.target==ControlTarget::oscFloat ? "OSC" :
                  b.target==ControlTarget::go ? "GO" : "PANIC");
        }
        if (effectivePreview.getText()!=summary)
            effectivePreview.setText(summary,false);
    }
    int getNumRows() override {
        const auto* entries=currentBindings();
        return entries ? (int)entries->size():0;
    }
    void selectedRowsChanged(int row) override {
        selected=row;
        const auto* entries=currentBindings();
        updateSelected.setEnabled(entries && row>=0 && row<(int)entries->size());
        if (!entries || row<0 || row>=(int)entries->size()) return;
        const auto& b=(*entries)[(size_t)row];
        captured.reset();
        scope.setSelectedId((int)b.scope+1,juce::dontSendNotification);
        target.setSelectedId((int)b.target+1,juce::dontSendNotification);
        refreshColumns();
        for (size_t i=0;i<columnIds.size();++i)
            if (columnIds[i]==b.targetColumnId)
                columns.setSelectedId((int)i+1,juce::dontSendNotification);
        takeover.setSelectedId((int)b.takeover+1,juce::dontSendNotification);
        curve.setSelectedId((int)b.curve+1,juce::dontSendNotification);
        minimum.setText(juce::String(b.minimum,3),false);
        maximum.setText(juce::String(b.maximum,3),false);
        outputCc.setText(juce::String(b.outputNumber),false);
        oscAddress.setText(b.oscAddress,false);
        invert.setToggleState(b.invert,juce::dontSendNotification);
        state.setText("Editing #"+juce::String(b.id)+" — input "+b.deviceId+
            " ch"+juce::String(b.channel)+" / "+juce::String(b.number),
            juce::dontSendNotification);
    }
    void paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool active) override {
        if (!owner || row<0 || row>=getNumRows()) return;
        const auto& b=(*currentBindings())[(size_t)row];
        if (active) g.fillAll(palette::teal.withAlpha(.20f));
        juce::String targetName=b.target==ControlTarget::go ? "GO" :
            b.target==ControlTarget::panic ? "PANIC" :
            b.target==ControlTarget::masterGain ? "MASTER GAIN" :
            b.target==ControlTarget::masterPan ? "MASTER PAN" :
            b.target==ControlTarget::columnPan ? "PAN" :
            b.target==ControlTarget::oscFloat ? "OSC" :
            b.target==ControlTarget::midiCc ? "MIDI CC" : "GAIN";
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
