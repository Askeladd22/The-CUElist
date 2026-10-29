#include "MainComponent.h"
#include "ControlLayerBuilder.h"
#include "ControlPresetArchive.h"
#include <algorithm>
#include <cmath>

namespace cuelist {
// Device and mapping application remains on JUCE's message thread.
// This module does not run in the audio callback.

void MainComponent::resetActiveMapping() {
    activeCueId=0;
    controlMapping.setActiveCue(0,0);
}

bool MainComponent::startOscControlInput(int port) {
    if (port<1 || port>65535) return false;
    if (!oscInput.startListening(port)) {
        status="OSC input port unavailable. Choose another UDP port.";
        return false;
    }
    show.controls.oscInputPort=port;
    dirty=true;
    status="OSC control listener enabled on UDP "+juce::String(port)+".";
    repaint();
    return true;
}

void MainComponent::refreshControlSettings() {
    if (show.controls.oscInputPort==0) oscInput.stopListening();
    else if (oscInput.port()!=show.controls.oscInputPort &&
             !oscInput.startListening(show.controls.oscInputPort))
        status="Saved OSC input port is unavailable.";
    controlMapping.setSettings(show.controls);
    // Resolution order and preset selection are pure, tested show-model
    // operations; device ownership/reconnection stays on the UI thread.
    controlMapping.setLayers(buildControlLayers(show));
    controlMapping.setActiveCue(activeCueId,
                                activeControlPresetForCue(show,activeCueId));
    if (midiInput.identifier()!=show.controls.midiInputId) {
        const auto result=midiInput.connect(show.controls.midiInputId);
        if (result.failed()) status=result.getErrorMessage();
    }
}

juce::Result MainComponent::selectControlInput(const juce::String& id) {
    auto result=midiInput.connect(id);
    physicalEdges.clear();
    // Preserve an explicitly selected disconnected identifier, never silently
    // switch to the first available device.
    show.controls.midiInputId=id;
    dirty=true;
    status=result.wasOk() ? (id.isEmpty() ? "MIDI input disabled." :
        "MIDI input connected.") : result.getErrorMessage();
    repaint(); return result;
}

juce::String MainComponent::exportControlPresetText() const {
    return exportControllerPreset(show);
}
juce::Result MainComponent::importControlPresetText(const juce::String& text) {
    auto candidate=show;
    const auto result=importControllerPreset(text,candidate);
    if(result.failed()) {status=result.getErrorMessage();return result;}
    show=std::move(candidate);
    refreshControlSettings();dirty=true;
    status="Default Map preset imported; project function names and cue overrides preserved.";
    refresh();return juce::Result::ok();
}

int MainComponent::createAbstractControlFunction(
        const juce::String& name,ControlBinding destination) {
    const auto title=name.trim();
    if (title.isEmpty() || title.length()>120) return 0;
    for (const auto& column:show.columns)
        if (column.controlLocked) {
            status="Unlock legacy Control Columns before adding named functions.";
            return 0;
        }
    auto candidate=show;
    // The source fields are *never* part of the output function identity.
    destination.id=0;destination.functionId=0;
    destination.deviceId={};destination.scope=ControlScope::global;
    destination.scopeId=0;
    const int id=candidate.controls.nextId++;
    candidate.controls.functions.push_back({id,title,std::move(destination)});
    if (const auto valid=validateShow(candidate);valid.failed()) {
        status=valid.getErrorMessage();return 0;
    }
    show=std::move(candidate);refreshControlSettings();dirty=true;
    status="Abstract function created: "+title;refresh();return id;
}
bool MainComponent::updateAbstractControlFunction(
        int id,const juce::String& name,ControlBinding destination) {
    const auto title=name.trim();
    if (title.isEmpty() || title.length()>120) return false;
    auto candidate=show;
    const auto it=std::find_if(candidate.controls.functions.begin(),
        candidate.controls.functions.end(),
        [id](const ControlFunction& f){return f.id==id;});
    if (it==candidate.controls.functions.end()) return false;
    destination.id=0;destination.functionId=0;
    destination.deviceId={};destination.scope=ControlScope::global;
    destination.scopeId=0;
    it->name=title;it->destination=std::move(destination);
    if (const auto valid=validateShow(candidate);valid.failed()) {
        status=valid.getErrorMessage();return false;
    }
    show=std::move(candidate);refreshControlSettings();dirty=true;
    status="Function route updated without changing its hardware inputs.";
    refresh();return true;
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
        sourceRow<0 || sourceRow>=(int)show.rows.size() ||
        (locked && !show.controls.functions.empty())) return false;
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

void MainComponent::dispatchControl(const ControlEvent& input) {
    // Record even unmatched notes and CC releases, including across a GO
    // that switches the effective Control Set.
    const bool wasDown=physicalEdges.recordAndWasDown(input);
    // Hardware MIDI events have already been copied to the UI thread.
    // Pickup compares against the physical value expected for the current
    // actual target. No mutation of show happens in a MIDI callback.
    // Compare against the actual output associated with the winning
    // binding, never an Inspector-selected or lower-priority destination.
    std::optional<float> current;
    if (const auto* winner=controlMapping.matched(input))
        current=mappedPickupTarget(show,*winner,audioFaderCeilingDb());
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
        const float bounded=mappedFaderGain(*action,audioFaderCeilingDb());
        if (shouldApplyMappedValue(show.masterGain,bounded))
            setMasterAudio(bounded);
    } else if (binding.target==ControlTarget::masterPan) {
        if (shouldApplyMappedValue(show.masterPan,action->scaled))
            setMasterPan(action->scaled);
    } else if (binding.target==ControlTarget::columnGain ||
               binding.target==ControlTarget::columnPan) {
        const int i=mappedTargetColumn(show,binding.targetColumnId,ColumnType::audio);
        if (i>=0) {
                const auto& c=show.columns[(size_t)i];
                const float currentValue=binding.target==ControlTarget::columnPan ? c.pan : c.gain;
                const float bounded=binding.target==ControlTarget::columnGain ?
                    mappedFaderGain(*action,audioFaderCeilingDb()) : action->scaled;
                if (shouldApplyMappedValue(currentValue,bounded)) {
                    setColumnUtilities(i,
                        binding.target==ControlTarget::columnGain ? bounded : c.gain,
                        binding.target==ControlTarget::columnPan ? action->scaled : c.pan,
                        c.muted,c.phaseInverted);
                    utilities.refresh();
                }
        }
    }
    if (binding.target==ControlTarget::midiCc) {
        const int i=mappedTargetColumn(show,binding.targetColumnId,ColumnType::midi);
        if (i>=0) {
            const auto& col=show.columns[(size_t)i];
            if (!engine.sendMappedMidiCc(col.midiOutputId,binding.outputChannel,
                    binding.outputNumber,mappedMidiCcValue(action->scaled)))
                status="MIDI control: output unavailable or queue full.";
        }
    } else if (binding.target==ControlTarget::oscFloat) {
        const int i=mappedTargetColumn(show,binding.targetColumnId,ColumnType::osc);
        if (i>=0) {
            const auto& col=show.columns[(size_t)i];
            if (!controlOutput.sendOscFloat(col.oscHost,col.oscPort,
                binding.oscAddress,action->scaled))
                status="OSC control: invalid destination or queue full.";
        }
    }
}



bool MainComponent::deleteAbstractControlFunction(int id) {
    auto candidate=show;
    auto& functions=candidate.controls.functions;
    const auto found=std::find_if(functions.begin(),functions.end(),
        [id](const ControlFunction& f){return f.id==id;});
    if (found==functions.end()) return false;
    functions.erase(found);
    auto discard=[id](std::vector<ControlBinding>& bindings) {
        bindings.erase(std::remove_if(bindings.begin(),bindings.end(),
            [id](const ControlBinding& b){return b.functionId==id;}),
            bindings.end());
    };
    discard(candidate.controls.bindings);
    for (auto& item:candidate.controls.templates) discard(item.set.bindings);
    for (auto& col:candidate.columns) discard(col.controlDefaultSet.bindings);
    for (auto& row:candidate.rows)
        for (auto& cell:row.cells) if (cell.controlSet)
            discard(cell.controlSet->bindings);
    for (auto& list:candidate.cuelists)
        for (auto& row:list.rows)
            for (auto& cell:row.cells) if (cell.controlSet)
                discard(cell.controlSet->bindings);
    if (auto valid=validateShow(candidate);valid.failed()) {
        status=valid.getErrorMessage();return false;
    }
    show=std::move(candidate);refreshControlSettings();dirty=true;refresh();
    status="Function and associated assignments removed.";return true;
}
bool MainComponent::moveAbstractControlFunction(int id,int direction) {
    auto candidate=show;
    auto& functions=candidate.controls.functions;
    const auto it=std::find_if(functions.begin(),functions.end(),
        [id](const ControlFunction& f){return f.id==id;});
    if (it==functions.end() || (direction!=-1 && direction!=1)) return false;
    const int index=(int)std::distance(functions.begin(),it);
    const int next=index+direction;
    if (next<0 || next>=(int)functions.size()) return false;
    std::iter_swap(functions.begin()+index,functions.begin()+next);
    if (auto valid=validateShow(candidate);valid.failed()) {
        status=valid.getErrorMessage();return false;
    }
    show=std::move(candidate);refreshControlSettings();dirty=true;
    status="Function order updated.";refresh();return true;
}

void MainComponent::ensureStarterControlFunctions() {
    if (!show.controls.functions.empty()) return;
    // Legacy locked columns are converted only when their document is loaded.
    // Do not silently change a still-open legacy control configuration.
    for (const auto& column:show.columns)
        if (column.controlLocked) return;
    auto candidate=show;
    // New projects start with a compact 8 + 8 palette. A missing output
    // column is represented by ID 0 and never produces output until configured.
    auto add=[&](ControlTarget type,const juce::String& prefix,int ordinal) {
        ControlBinding dest;
        dest.target=type;
        dest.minimum=0.0f;
        dest.maximum=type==ControlTarget::midiCc ? 127.0f : 1.0f;
        dest.oscAddress="/control/"+juce::String(ordinal);
        for (const auto& col:candidate.columns)
            if ((type==ControlTarget::midiCc && col.type==ColumnType::midi) ||
                (type==ControlTarget::oscFloat && col.type==ColumnType::osc)) {
                dest.targetColumnId=col.id;break;
            }
        candidate.controls.functions.push_back({
            candidate.controls.nextId++,prefix+" "+juce::String(ordinal),dest});
    };
    for (int i=1;i<=8;++i) add(ControlTarget::midiCc,"MIDI Function",i);
    for (int i=1;i<=8;++i) add(ControlTarget::oscFloat,"OSC Function",i);
    if (auto valid=validateShow(candidate);valid.failed()) {
        status=valid.getErrorMessage();return;
    }
    show=std::move(candidate);
    refreshControlSettings();dirty=true;
}
void MainComponent::finishQuickLearn() {
    quickLearnMode=QuickLearnMode::off;quickLearnFunctionId=0;
    controlLearnArmed=false;oscLearnArmed=false;
    quickControlTargets.setVisible(false);cancelQuickLearn.setVisible(false);
    refresh();
}
void MainComponent::beginQuickLearn(bool osc) {
    const auto mode=osc ? QuickLearnMode::osc : QuickLearnMode::midi;
    if (quickLearnMode==mode) {finishQuickLearn();return;}
    controlLearnArmed=false;oscLearnArmed=false;learnedInput.reset();
    if (osc && oscInput.port()==0) {
        // Normal Learn never requires navigating an advanced configuration
        // dialog. A configured port wins; otherwise 9011 is the default.
        if (!startOscControlInput(show.controls.oscInputPort>0 ?
                                  show.controls.oscInputPort : 9011)) {
            repaint();return;
        }
    }
    ensureStarterControlFunctions();
    quickLearnMode=mode;quickLearnFunctionId=0;
    std::vector<QuickControlTargets::Entry> entries;
    for (const auto& function:show.controls.functions) {
        juce::String detail;
        for (const auto& binding:show.controls.bindings)
            if (binding.functionId==function.id) {
                detail=binding.deviceId+" / "+juce::String(binding.channel)+
                    ":"+juce::String(binding.number);
                break;
            }
        if (detail.isEmpty()) detail="No Default Map assignment";
        if ((function.destination.target==ControlTarget::midiCc ||
             function.destination.target==ControlTarget::oscFloat) &&
            function.destination.targetColumnId==0)
            detail+="  · Configure output in Control Mappings";
        entries.push_back({function.id,function.name,detail});
    }
    entries.push_back({-1,"Master Gain","Internal audio master fader"});
    entries.push_back({-2,"Master Pan","Internal master balance"});
    entries.push_back({-3,"GO","Transport: next cue"});
    entries.push_back({-4,"PANIC","Emergency stop"});
    quickControlTargets.setEntries(std::move(entries));
    inspectorVisible=true;
    status=osc ? "OSC Learn: select a function, then send OSC." :
                 "MIDI Learn: select a function, then move a fader or press a button.";
    resized();repaint();
}
void MainComponent::chooseQuickLearnTarget(int functionId) {
    if (quickLearnMode==QuickLearnMode::off) return;
    if (functionId<0) {
        const ControlTarget target=functionId==-1 ? ControlTarget::masterGain :
            functionId==-2 ? ControlTarget::masterPan :
            functionId==-3 ? ControlTarget::go : ControlTarget::panic;
        const juce::String title=functionId==-1 ? "Master Gain" :
            functionId==-2 ? "Master Pan" : functionId==-3 ? "GO" : "PANIC";
        functionId=0;
        for (const auto& fn:show.controls.functions)
            if (fn.name==title && fn.destination.target==target) {
                functionId=fn.id;break;
            }
        if (functionId==0) {
            ControlBinding destination;
            destination.target=target;
            if (target==ControlTarget::masterPan) destination.minimum=-1.0f;
            functionId=createAbstractControlFunction(title,destination);
        }
    }
    const bool exists=std::any_of(show.controls.functions.begin(),
        show.controls.functions.end(),
        [functionId](const ControlFunction& f){return f.id==functionId;});
    if (!exists || functionId<=0) {
        status="Could not select a mappable function.";repaint();return;
    }
    if (quickLearnMode==QuickLearnMode::midi && !midiInput.connected()) {
        status="Connect a MIDI input through Control Mappings first.";
        repaint();return;
    }
    quickLearnFunctionId=functionId;
    if (quickLearnMode==QuickLearnMode::midi) armControlLearn();
    else armOscControlLearn();
    status="Listening for "+juce::String(quickLearnMode==QuickLearnMode::midi ?
        "MIDI" : "OSC")+" input. The next received message creates the binding.";
    repaint();
}
void MainComponent::completeQuickLearn(const ControlEvent& event) {
    if (quickLearnFunctionId<=0) return;
    const auto fn=std::find_if(show.controls.functions.begin(),
        show.controls.functions.end(),
        [this](const ControlFunction& f){return f.id==quickLearnFunctionId;});
    if (fn==show.controls.functions.end()) return;
    ControlBinding input=materializeFunctionBinding(ControlBinding{},*fn);
    input.functionId=quickLearnFunctionId;
    input.deviceId=event.deviceId;input.kind=event.kind;
    input.channel=event.channel;input.number=event.number;
    input.scope=ControlScope::global;input.scopeId=0;
    input.takeover=(event.kind==ControlKind::note ||
        event.kind==ControlKind::osc || input.target==ControlTarget::go ||
        input.target==ControlTarget::panic) ? ControlTakeover::jump :
        ControlTakeover::pickup;
    // Stable function identity: Learn changes only input, never destination.
    const auto found=std::find_if(show.controls.bindings.begin(),
        show.controls.bindings.end(),[this](const ControlBinding& b){
            return b.functionId==quickLearnFunctionId &&
                b.scope==ControlScope::global;
        });
    const auto name=fn->name;
    const bool ok=found!=show.controls.bindings.end() ?
        updateControlBinding(-1,-1,found->id,input,false) :
        addMappingBinding(input);
    if (!ok) {
        status="Learn failed: "+status+". Check for conflicting hardware bindings.";
        quickLearnFunctionId=0;controlLearnArmed=false;oscLearnArmed=false;
        repaint();return;
    }
    finishQuickLearn();
    status="Learn saved to Default Map: "+name+" ← "+event.deviceId;
    repaint();
}

} // namespace cuelist
