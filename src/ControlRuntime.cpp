#include "MainComponent.h"
#include "ControlLayerBuilder.h"
#include "ControlPresetArchive.h"
#include <algorithm>
#include <cmath>

namespace cuelist {
// Device and mapping application remains on JUCE's message thread.
// This module does not run in the audio callback.

void MainComponent::resetActiveMapping() {
    quickLearnMode=QuickLearnMode::off;quickLearnFunctionId=0;
    quickLearnArmedEntryId=0;quickLearnPulseTicks=0;quickLearnPulseBright=true;
    quickLearnFeedback.clear();
    controlLearnArmed=false;oscLearnArmed=false;
    quickControlTargets.setVisible(false);quickLearnMapSelector.setVisible(false);
    cancelQuickLearn.setVisible(false);
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
    if (appPreferences) appPreferences->setValue("oscInputPorts",juce::String(port));
    dirty=true;
    status="OSC control listener enabled on UDP "+juce::String(port)+".";
    repaint();
    return true;
}

juce::String MainComponent::configuredOscPorts() const {
    return appPreferences ? appPreferences->getValue("oscInputPorts","") : juce::String();
}
bool MainComponent::configureOscInputPorts(const juce::String& value) {
    juce::StringArray parts;parts.addTokens(value,",; ","");
    std::vector<int> ports;
    for (const auto& part:parts) {
        const auto token=part.trim();
        if (token.isEmpty()) continue;
        if (token.retainCharacters("0123456789")!=token ||
            token.getIntValue()<1 || token.getIntValue()>65535 ||
            ports.size()>=16 ||
            std::find(ports.begin(),ports.end(),token.getIntValue())!=ports.end()) {
            status="Specify 1–16 unique UDP ports between 1 and 65535.";
            repaint();return false;
        }
        ports.push_back(token.getIntValue());
    }
    if (ports.empty()) {
        oscInput.stopListening();
        if (appPreferences) appPreferences->setValue("oscInputPorts","");
        show.controls.oscInputPort=0;dirty=true;
        status="OSC input disabled.";repaint();return true;
    }
    if (!oscInput.startListeningPorts(ports)) {
        status="One or more OSC ports are unavailable; previous listeners retained.";
        repaint();return false;
    }
    juce::StringArray strings;
    for (int port:ports) strings.add(juce::String(port));
    if (appPreferences) appPreferences->setValue("oscInputPorts",strings.joinIntoString(","));
    show.controls.oscInputPort=ports.front();dirty=true;
    status="OSC input listening on: "+strings.joinIntoString(", ");
    repaint();return true;
}
void MainComponent::refreshControlSettings() {
    const auto configured=configuredOscPorts();
    if (appPreferences && appPreferences->containsKey("oscInputPorts")) {
        if (configured.isEmpty()) oscInput.stopListening();
        else {
            juce::StringArray parts;parts.addTokens(configured,",; ","");
            std::vector<int> requested;
            for (auto part:parts)
                if (part.trim().isNotEmpty()) requested.push_back(part.getIntValue());
            if (!requested.empty() && !oscInput.startListeningPorts(requested))
                status="Saved OSC UDP port configuration is unavailable.";
        }
    } else if (show.controls.oscInputPort==0) oscInput.stopListening();
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

bool MainComponent::isAdditionalMidiInputEnabled(const juce::String& id) const {
    return std::any_of(additionalMidiInputs.begin(),additionalMidiInputs.end(),
        [&id](const auto& e) {return e.first==id;});
}
void MainComponent::restoreAdditionalMidiInputs() {
    additionalMidiInputs.clear();
    if (!appPreferences) return;
    juce::StringArray saved;
    saved.addTokens(appPreferences->getValue("additionalMidiInputIds"),"|","");
    for (const auto& identifier:saved) {
        if (identifier.isEmpty() || identifier==show.controls.midiInputId ||
            isAdditionalMidiInputEnabled(identifier)) continue;
        auto input=std::make_unique<MidiInputManager>();
        input->connect(identifier);
        additionalMidiInputs.emplace_back(identifier,std::move(input));
    }
}
void MainComponent::toggleAdditionalMidiInput(const juce::String& id) {
    if (id.isEmpty() || id==show.controls.midiInputId) return;
    auto it=std::find_if(additionalMidiInputs.begin(),additionalMidiInputs.end(),
        [&id](const auto& e){return e.first==id;});
    if (it!=additionalMidiInputs.end()) {
        it->second->disconnect();
        additionalMidiInputs.erase(it);
        status="MIDI input disabled: "+id;
    } else {
        auto input=std::make_unique<MidiInputManager>();
        const auto result=input->connect(id);
        if (result.failed()) {status=result.getErrorMessage();repaint();return;}
        additionalMidiInputs.emplace_back(id,std::move(input));
        status="Additional MIDI input enabled: "+id;
    }
    juce::StringArray saved;
    for (const auto& entry:additionalMidiInputs) saved.add(entry.first);
    if (appPreferences)
        appPreferences->setValue("additionalMidiInputIds",saved.joinIntoString("|"));
    physicalEdges.clear();repaint();
}
juce::Result MainComponent::selectControlInput(const juce::String& id) {
    auto extra=std::find_if(additionalMidiInputs.begin(),additionalMidiInputs.end(),
        [&id](const auto& item){return item.first==id;});
    if (extra!=additionalMidiInputs.end()) {
        extra->second->disconnect();
        additionalMidiInputs.erase(extra);
        juce::StringArray saved;
        for (const auto& entry:additionalMidiInputs) saved.add(entry.first);
        if (appPreferences)
            appPreferences->setValue("additionalMidiInputIds",saved.joinIntoString("|"));
    }
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

int MainComponent::createReusableControlMap(const juce::String& name,
                                            const ControlSet& original) {
    const auto title=name.trim();
    if (title.isEmpty() || title.length()>120) {
        status="Map name must contain 1–120 characters.";return 0;
    }
    auto candidate=show;
    if (candidate.controls.templates.size()>=128 || original.bindings.size()>256 ||
        candidate.controls.nextId>=999999000) {
        status="Map capacity or identifier limit reached.";return 0;
    }
    for (const auto& item:candidate.controls.templates)
        if (item.name.equalsIgnoreCase(title)) {
            status="A reusable map with this name already exists.";return 0;
        }
    ControlSetTemplate map;
    map.id=candidate.controls.nextId++;
    map.name=title;
    map.set=original;
    for (auto& b:map.set.bindings) {
        b.id=candidate.controls.nextId++;
        b.scope=ControlScope::global;b.scopeId=0;
    }
    const int result=map.id;
    candidate.controls.templates.push_back(std::move(map));
    if (auto valid=validateShow(candidate);valid.failed()) {
        status=valid.getErrorMessage();return 0;
    }
    show=std::move(candidate);
    dirty=true;refreshControlSettings();refresh();
    status="Reusable map created: "+title;
    return result;
}
int MainComponent::reusableControlMapUsage(int mapId) const {
    const auto used=[mapId](const std::vector<Row>& rows) {
        int result=0;
        for (const auto& row:rows)
            for (const auto& cell:row.cells)
                if (cell.assignedMapId==mapId) ++result;
        return result;
    };
    int count=used(show.rows);
    for (int i=0;i<(int)show.cuelists.size();++i)
        if (i!=show.activeCuelist) count+=used(show.cuelists[(size_t)i].rows);
    return count;
}
bool MainComponent::renameReusableControlMap(int mapId,const juce::String& name) {
    const auto title=name.trim();
    if (title.isEmpty() || title.length()>120) {
        status="Map name must contain 1–120 characters.";return false;
    }
    auto candidate=show;
    const auto found=std::find_if(candidate.controls.templates.begin(),
        candidate.controls.templates.end(),[mapId](const ControlSetTemplate& m){return m.id==mapId;});
    if (found==candidate.controls.templates.end()) return false;
    for (const auto& item:candidate.controls.templates)
        if (item.id!=mapId && item.name.equalsIgnoreCase(title)) {
            status="A map with this name already exists.";return false;
        }
    found->name=title;
    if (auto valid=validateShow(candidate);valid.failed()) {
        status=valid.getErrorMessage();return false;
    }
    show=std::move(candidate);dirty=true;refresh();status="Control Map renamed.";return true;
}
bool MainComponent::deleteReusableControlMap(int mapId) {
    if (reusableControlMapUsage(mapId)>0) {
        status="This Control Map is assigned to cues. Reassign or remove those references first.";
        repaint();return false;
    }
    auto candidate=show;
    const auto found=std::find_if(candidate.controls.templates.begin(),
        candidate.controls.templates.end(),[mapId](const ControlSetTemplate& m){return m.id==mapId;});
    if (found==candidate.controls.templates.end()) return false;
    candidate.controls.templates.erase(found);
    if (auto valid=validateShow(candidate);valid.failed()) {
        status=valid.getErrorMessage();return false;
    }
    show=std::move(candidate);dirty=true;refreshControlSettings();refresh();
    status="Unused saved Control Map deleted.";return true;
}

bool MainComponent::modifyReusableControlBinding(int mapId,int oldBindingId,
                                                  ControlBinding binding,int operation) {
    if (operation<0 || operation>2) return false; // 0 add, 1 edit, 2 delete
    auto candidate=show;
    auto map=std::find_if(candidate.controls.templates.begin(),
        candidate.controls.templates.end(),
        [mapId](const ControlSetTemplate& item){return item.id==mapId;});
    if (map==candidate.controls.templates.end()) {
        status="Saved map no longer exists.";return false;
    }
    auto& entries=map->set.bindings;
    if (operation==0) {
        binding.id=candidate.controls.nextId++;
        binding.scope=ControlScope::global;binding.scopeId=0;
        entries.push_back(std::move(binding));
    } else {
        auto found=std::find_if(entries.begin(),entries.end(),
            [oldBindingId](const ControlBinding& item){return item.id==oldBindingId;});
        if (found==entries.end()) {
            status="Saved mapping no longer exists.";return false;
        }
        if (operation==2) entries.erase(found);
        else {
            binding.id=oldBindingId;
            binding.scope=ControlScope::global;binding.scopeId=0;
            *found=std::move(binding);
        }
    }
    if (auto valid=validateShow(candidate);valid.failed()) {
        status=valid.getErrorMessage();return false;
    }
    show=std::move(candidate);dirty=true;
    refreshControlSettings();refresh();status="Saved map updated.";return true;
}
bool MainComponent::clearControlCellMap(int row,int column) {
    if (row<0 || row>=(int)show.rows.size() || column<0 ||
        column>=(int)show.columns.size() ||
        show.columns[(size_t)column].type!=ColumnType::control ||
        show.columns[(size_t)column].controlLocked ||
        !hasControlMapAssignment(show.rows[(size_t)row].cells[(size_t)column])) return false;
    auto candidate=show;
    candidate.rows[(size_t)row].cells[(size_t)column].controlSet.reset();
    candidate.rows[(size_t)row].cells[(size_t)column].assignedMapId=0;
    if (auto valid=validateShow(candidate);valid.failed()) {
        status=valid.getErrorMessage();return false;
    }
    show=std::move(candidate);dirty=true;
    refreshControlSettings();refresh();
    status="Override removed; Default Map restored.";return true;
}

int MainComponent::saveControlSetTemplate(int row,int column,const juce::String& name) {
    if (row<0 || row>=(int)show.rows.size() || column<0 ||
        column>=(int)show.columns.size() ||
        show.columns[(size_t)column].type!=ColumnType::control) return 0;
    const auto label=name.trim();
    if (label.isEmpty() || label.length()>120) return 0;
    auto candidate=show;
    auto& col=candidate.columns[(size_t)column];
    const auto& cell=candidate.rows[(size_t)row].cells[(size_t)column];
    const ControlSet* source=col.controlLocked ? &col.controlDefaultSet :
        (cell.controlSet ? &*cell.controlSet : nullptr);
    if (!col.controlLocked && cell.assignedMapId>0)
        for (const auto& saved:candidate.controls.templates)
            if (saved.id==cell.assignedMapId) {source=&saved.set;break;}
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
    else {
        auto& cell=candidate.rows[(size_t)row].cells[(size_t)column];
        cell.controlSet=std::move(copy); // copy retained as independent legacy/export snapshot
        cell.assignedMapId=templateId;  // live reusable map; share across cues
    }
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
    } else if (binding.target==ControlTarget::limiterToggle) {
        if (show.safetyLimiterEnabled!=action->pressed) setSafetyLimiter(action->pressed);
    } else if (binding.target==ControlTarget::columnMute ||
               binding.target==ControlTarget::columnPolarity ||
               binding.target==ControlTarget::columnSend) {
        const int i=mappedTargetColumn(show,binding.targetColumnId,ColumnType::audio);
        if (i>=0) {
            const auto c=show.columns[(size_t)i];
            if (binding.target==ControlTarget::columnSend) {
                if (c.masterSend!=action->pressed) setColumnMasterSend(i,action->pressed);
            } else if (binding.target==ControlTarget::columnMute) {
                if (c.muted!=action->pressed) setColumnUtilities(i,c.gain,c.pan,action->pressed,c.phaseInverted);
            } else if (c.phaseInverted!=action->pressed)
                setColumnUtilities(i,c.gain,c.pan,c.muted,action->pressed);
        }
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
    quickLearnArmedEntryId=0;quickLearnPulseTicks=0;quickLearnPulseBright=true;
    quickLearnFeedback.clear();
    controlLearnArmed=false;oscLearnArmed=false;
    quickControlTargets.setVisible(false);quickLearnMapSelector.setVisible(false);
    cancelQuickLearn.setVisible(false);
    refresh();
    updateQuickLearnVisuals();
}
void MainComponent::beginQuickLearn(bool osc) {
    const auto mode=osc ? QuickLearnMode::osc : QuickLearnMode::midi;
    if (quickLearnMode==mode) {
        finishQuickLearn();status="Quick Learn cancelled.";repaint();return;
    }
    if (controlDialog) {
        auto* previous=controlDialog.getComponent();
        controlDialog=nullptr;
        delete previous;
    }
    controlLearnArmed=false;oscLearnArmed=false;learnedInput.reset();
    // OSC listeners are configured only in Settings, never enabled by Learn.
    if (osc && oscInput.port()==0)
        status="Enable OSC input in Settings before attempting capture.";
    ensureStarterControlFunctions();
    quickLearnMode=mode;quickLearnFunctionId=0;
    quickLearnMapRowId=quickLearnMapColumnId=0;
    quickLearnArmedEntryId=0;quickLearnPulseTicks=0;quickLearnPulseBright=true;
    quickLearnFeedback.clear();
    refreshQuickLearnTargets();refreshQuickLearnMaps();
    inspectorVisible=true;
    status=osc ? "OSC Learn: choose a GUI control or function, then send OSC." :
                 "MIDI Learn: choose a GUI control or function, then move a controller.";
    utilities.refresh();
    resized();updateQuickLearnVisuals();repaint();
}
void MainComponent::refreshQuickLearnMaps() {
    quickLearnMapLocations.clear();
    quickLearnMapSelector.clear(juce::dontSendNotification);
    quickLearnMapLocations.push_back({0,0});
    quickLearnMapSelector.addItem("Default Map (fallback)",1);
    int choice=1;
    for (int r=0;r<(int)show.rows.size();++r)
        for (int c=0;c<(int)show.columns.size();++c) {
            const auto& column=show.columns[(size_t)c];
            if (column.type!=ColumnType::control || column.controlLocked) continue;
            const auto& cell=show.rows[(size_t)r].cells[(size_t)c];
            // A named map must only appear once, as an independent archive
            // entry. Learning into a cue-local copy would otherwise have no
            // effect on that cue's live shared-map reference.
            if (!cell.controlSet || cell.assignedMapId>0) continue;
            quickLearnMapLocations.push_back({show.rows[(size_t)r].id,column.id});
            const int id=(int)quickLearnMapLocations.size();
            quickLearnMapSelector.addItem("Q"+juce::String(r+1)+" / "+column.name,id);
            if (quickLearnMapRowId==show.rows[(size_t)r].id &&
                quickLearnMapColumnId==column.id) choice=id;
        }
    for (const auto& saved:show.controls.templates) {
        quickLearnMapLocations.push_back({-saved.id,0});
        const int id=(int)quickLearnMapLocations.size();
        quickLearnMapSelector.addItem("Saved: "+saved.name,id);
        if (quickLearnMapRowId==-saved.id) choice=id;
    }
    quickLearnMapSelector.setSelectedId(choice,juce::dontSendNotification);
}
void MainComponent::selectQuickLearnMap(int rowId,int columnId) {
    quickLearnMapRowId=rowId;quickLearnMapColumnId=columnId;
    quickLearnFunctionId=quickLearnArmedEntryId=0;
    controlLearnArmed=oscLearnArmed=false;
    refreshQuickLearnTargets();updateQuickLearnVisuals();
    quickLearnFeedback=rowId==0 ? "Editing Default Map" :
        rowId<0 ? "Editing reusable saved map" : "Editing contextual override";
}
void MainComponent::renameQuickLearnFunction(int functionId) {
    if (quickLearnMode==QuickLearnMode::off || functionId<=0) return;
    const auto it=std::find_if(show.controls.functions.begin(),show.controls.functions.end(),
        [functionId](const ControlFunction& f){return f.id==functionId;});
    if (it==show.controls.functions.end()) return;
    quickControlTargets.beginInlineRename(functionId,it->name,
        [this,functionId](const juce::String& title) {
            const auto fn=std::find_if(show.controls.functions.begin(),show.controls.functions.end(),
                [functionId](const ControlFunction& f){return f.id==functionId;});
            if (fn==show.controls.functions.end()) return;
            if (updateAbstractControlFunction(functionId,title,fn->destination))
                refreshQuickLearnTargets();
        });
}
void MainComponent::refreshQuickLearnTargets() {
    if (quickLearnMode==QuickLearnMode::off) return;
    std::vector<QuickControlTargets::Entry> entries;
    for (const auto& function:show.controls.functions) {
        juce::String detail;
        const std::vector<ControlBinding>* mapping=&show.controls.bindings;
        if (quickLearnMapRowId<0)
            for (const auto& map:show.controls.templates)
                if (map.id==-quickLearnMapRowId) mapping=&map.set.bindings;
        if (quickLearnMapRowId>0)
            for (int r=0;r<(int)show.rows.size();++r)
                if (show.rows[(size_t)r].id==quickLearnMapRowId)
                    for (int c=0;c<(int)show.columns.size();++c)
                        if (show.columns[(size_t)c].id==quickLearnMapColumnId &&
                            show.rows[(size_t)r].cells[(size_t)c].controlSet)
                            mapping=&show.rows[(size_t)r].cells[(size_t)c].controlSet->bindings;
        for (const auto& binding:*mapping)
            if (binding.functionId==function.id) {
                detail=binding.deviceId+" / "+juce::String(binding.channel)+
                    ":"+juce::String(binding.number);
                break;
            }
        if (detail.isEmpty()) detail=quickLearnMapRowId==0 ?
            "No Default Map assignment" : "Inherits Default Map";
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
    entries.push_back({-5,"Limiter On/Off","Master safety limiter"});
    // Every audio column has two direct Learn targets. Stable column IDs
    // are resolved at selection time; their position in this list is UI-only.
    for (int index=0;index<(int)show.columns.size();++index) {
        const auto& col=show.columns[(size_t)index];
        if (col.type!=ColumnType::audio) continue;
        entries.push_back({-100-index*5,col.name+" Gain","Audio column fader"});
        entries.push_back({-101-index*5,col.name+" Pan","Audio column balance"});
        entries.push_back({-102-index*5,col.name+" Mute","Audio column mute"});
        entries.push_back({-103-index*5,col.name+" Phase / Polarity","Audio polarity"});
        entries.push_back({-104-index*5,col.name+" L/R Send","Master bus send"});
    }
    quickControlTargets.setEntries(std::move(entries));
    quickControlTargets.setLearnPresentation(quickLearnAccent(),
        quickLearnFunctionId,quickLearnArmedEntryId,quickLearnPulseBright);
}
bool MainComponent::isQuickLearnTargetSelected(ControlTarget target,int columnId) const {
    if (quickLearnMode==QuickLearnMode::off || quickLearnFunctionId<=0)
        return false;
    const auto found=std::find_if(show.controls.functions.begin(),
        show.controls.functions.end(),[this](const ControlFunction& fn) {
            return fn.id==quickLearnFunctionId;
        });
    if (found==show.controls.functions.end() ||
        found->destination.target!=target) return false;
    return (target==ControlTarget::columnGain ||
            target==ControlTarget::columnPan ||
            target==ControlTarget::columnMute ||
            target==ControlTarget::columnPolarity ||
            target==ControlTarget::columnSend) ?
           found->destination.targetColumnId==columnId : true;
}
void MainComponent::updateQuickLearnVisuals() {
    const auto colour=quickLearnAccent();
    const bool bright=quickLearnPulseBright;
    goLearnHotspot.setLearnPresentation(colour,
        isQuickLearnTargetSelected(ControlTarget::go),bright);
    panicLearnHotspot.setLearnPresentation(colour,
        isQuickLearnTargetSelected(ControlTarget::panic),bright);
    masterGainLearnHotspot.setLearnPresentation(colour,
        isQuickLearnTargetSelected(ControlTarget::masterGain),bright);
    masterPanLearnHotspot.setLearnPresentation(colour,
        isQuickLearnTargetSelected(ControlTarget::masterPan),bright);
    masterLimiterLearnHotspot.setLearnPresentation(colour,
        isQuickLearnTargetSelected(ControlTarget::limiterToggle),bright);
    utilities.refreshLearnVisuals();
    quickControlTargets.setLearnPresentation(colour,quickLearnFunctionId,
        quickLearnArmedEntryId,bright);
    // Protocol buttons remain coloured for the whole session. They do
    // not blink: only the chosen destination should draw attention.
    globalMidiLearn.setColour(juce::TextButton::buttonColourId,
        quickLearnMode==QuickLearnMode::midi ?
        palette::amber.withAlpha(.55f) : palette::raised);
    globalOscLearn.setColour(juce::TextButton::buttonColourId,
        quickLearnMode==QuickLearnMode::osc ?
        palette::purple.withAlpha(.55f) : palette::raised);
    if (quickLearnMode!=QuickLearnMode::off) repaint();
}
void MainComponent::chooseQuickLearnGuiTarget(ControlTarget target,int columnId) {
    if (quickLearnMode==QuickLearnMode::off) return;
    switch (target) {
        case ControlTarget::masterGain: chooseQuickLearnTarget(-1);return;
        case ControlTarget::masterPan: chooseQuickLearnTarget(-2);return;
        case ControlTarget::go: chooseQuickLearnTarget(-3);return;
        case ControlTarget::panic: chooseQuickLearnTarget(-4);return;
        case ControlTarget::limiterToggle: chooseQuickLearnTarget(-5);return;
        case ControlTarget::columnGain:
        case ControlTarget::columnPan:
        case ControlTarget::columnMute:
        case ControlTarget::columnPolarity:
        case ControlTarget::columnSend: {
            for (int index=0;index<(int)show.columns.size();++index) {
                const auto& col=show.columns[(size_t)index];
                if (col.id==columnId && col.type==ColumnType::audio) {
                    const int parameter=target==ControlTarget::columnPan ? 1 :
                        target==ControlTarget::columnMute ? 2 :
                        target==ControlTarget::columnPolarity ? 3 :
                        target==ControlTarget::columnSend ? 4 : 0;
                    chooseQuickLearnTarget(-100-index*5-parameter);
                    return;
                }
            }
            return;
        }
        default: return; // Unsupported widgets are never intercepted.
    }
}
void MainComponent::chooseQuickLearnTarget(int functionId) {
    if (quickLearnMode==QuickLearnMode::off) return;
    const int requestedEntryId=functionId;
    quickLearnFeedback.clear();
    if (functionId<=-100) {
        const int encoded=-100-functionId;
        const int index=encoded/5;
        const int parameter=encoded%5;
        if (index<0 || index>=(int)show.columns.size() ||
            show.columns[(size_t)index].type!=ColumnType::audio) return;
        const int columnId=show.columns[(size_t)index].id;
        const ControlTarget target=parameter==1 ? ControlTarget::columnPan :
            parameter==2 ? ControlTarget::columnMute :
            parameter==3 ? ControlTarget::columnPolarity :
            parameter==4 ? ControlTarget::columnSend : ControlTarget::columnGain;
        functionId=0;
        for (const auto& fn:show.controls.functions)
            if (fn.destination.target==target &&
                fn.destination.targetColumnId==columnId) {
                functionId=fn.id;break;
            }
        if (functionId==0) {
            ControlBinding destination;
            destination.target=target;destination.targetColumnId=columnId;
            if (parameter==1) destination.minimum=-1.0f;
            const auto name=show.columns[(size_t)index].name+
                (parameter==1 ? " Pan" : parameter==2 ? " Mute" :
                 parameter==3 ? " Phase / Polarity" : parameter==4 ? " L/R Send" : " Gain");
            functionId=createAbstractControlFunction(name,destination);
        }
    } else if (functionId<0) {
        const ControlTarget target=functionId==-1 ? ControlTarget::masterGain :
            functionId==-2 ? ControlTarget::masterPan :
            functionId==-3 ? ControlTarget::go :
            functionId==-5 ? ControlTarget::limiterToggle : ControlTarget::panic;
        const juce::String title=functionId==-1 ? "Master Gain" :
            functionId==-2 ? "Master Pan" : functionId==-3 ? "GO" :
            functionId==-5 ? "Limiter On/Off" : "PANIC";
        functionId=0;
        // A renamed logical function must keep its stable destination
        // identity: GUI selection never creates a second GO/Master function.
        for (const auto& fn:show.controls.functions)
            if (fn.destination.target==target) {
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
    // Physical MIDI availability must not gate the visual selection.
    // The UI needs to show WHICH function is armed even when no input
    // device has been selected or while the selected device reconnects.
    // No MIDI data is captured until a device is actually connected.
    const bool midiInputMissing=quickLearnMode==QuickLearnMode::midi &&
        !midiInput.connected() &&
        std::none_of(additionalMidiInputs.begin(),additionalMidiInputs.end(),
            [](const auto& item){return item.second->connected();});
    quickLearnFunctionId=functionId;
    quickLearnArmedEntryId=requestedEntryId;
    quickLearnPulseTicks=0;quickLearnPulseBright=true;
    quickLearnFeedback=midiInputMissing ? "MIDI input not connected" :
                       juce::String();
    if (quickLearnMode==QuickLearnMode::midi) armControlLearn();
    else armOscControlLearn();
    status=midiInputMissing ?
        "MIDI target selected. Enable a MIDI input in Settings to capture." :
        "Listening for "+juce::String(quickLearnMode==QuickLearnMode::midi ?
        "MIDI" : "OSC")+" input. The next received message creates the binding.";
    updateQuickLearnVisuals();
}
void MainComponent::completeQuickLearn(const ControlEvent& event) {
    if (quickLearnFunctionId<=0 ||
        (event.kind==ControlKind::note && event.isNoteOff)) return;
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
        input.target==ControlTarget::panic ||
        input.target==ControlTarget::limiterToggle ||
        input.target==ControlTarget::columnMute ||
        input.target==ControlTarget::columnPolarity ||
        input.target==ControlTarget::columnSend) ? ControlTakeover::jump :
        ControlTakeover::pickup;
    int mapRow=-1,mapColumn=-1;
    if (quickLearnMapRowId>0) {
        for (int r=0;r<(int)show.rows.size();++r)
            if (show.rows[(size_t)r].id==quickLearnMapRowId) mapRow=r;
        for (int c=0;c<(int)show.columns.size();++c)
            if (show.columns[(size_t)c].id==quickLearnMapColumnId) mapColumn=c;
        if (mapRow<0 || mapColumn<0 ||
            !show.rows[(size_t)mapRow].cells[(size_t)mapColumn].controlSet) {
            quickLearnFeedback="Selected map no longer exists.";return;
        }
    }
    const std::vector<ControlBinding>* assignments=&show.controls.bindings;
    if (quickLearnMapRowId<0) {
        const auto foundMap=std::find_if(show.controls.templates.begin(),
            show.controls.templates.end(),[this](const ControlSetTemplate& map) {
                return map.id==-quickLearnMapRowId;
            });
        if (foundMap==show.controls.templates.end()) {
            quickLearnFeedback="Selected saved map no longer exists.";return;
        }
        assignments=&foundMap->set.bindings;
    } else if (quickLearnMapRowId>0)
        assignments=&show.rows[(size_t)mapRow].cells[(size_t)mapColumn].controlSet->bindings;
    const auto found=std::find_if(assignments->begin(),assignments->end(),
        [this](const ControlBinding& b){return b.functionId==quickLearnFunctionId;});
    const auto name=fn->name;
    const bool isOverride=quickLearnMapRowId>0;
    const bool isSaved=quickLearnMapRowId<0;
    const bool ok=isSaved ?
        modifyReusableControlBinding(-quickLearnMapRowId,
            found==assignments->end()?0:found->id,input,found==assignments->end()?0:1) :
        found!=assignments->end() ?
            updateControlBinding(mapRow,mapColumn,found->id,input,isOverride) :
            isOverride ? addControlSetBinding(mapRow,mapColumn,input) : addMappingBinding(input);
    if (!ok) {
        status="Learn failed: "+status+". Check for conflicting hardware bindings.";
        quickLearnFeedback="Binding failed. Select another target.";
        quickLearnFunctionId=0;quickLearnArmedEntryId=0;
        controlLearnArmed=false;oscLearnArmed=false;
        quickControlTargets.deselectAllRows();
        updateQuickLearnVisuals();return;
    }
    // Preserve held-button state so repeating a learned note cannot fire
    // GO/PANIC until the physical control is released.
    physicalEdges.recordAndWasDown(event);
    // Remain in the same Learn protocol until Cancel or its top-right
    // toggle is pressed. This is a one-shot target selection, NOT a
    // one-shot Learn session: the operator can immediately map the next
    // GUI control or function without reopening MIDI/OSC Learn.
    quickLearnFunctionId=0;quickLearnArmedEntryId=0;
    quickLearnPulseTicks=0;quickLearnPulseBright=true;
    controlLearnArmed=false;oscLearnArmed=false;
    quickLearnFeedback="Mapped: "+name;
    refreshQuickLearnTargets(); // clear selection, refresh assignment labels
    updateQuickLearnVisuals();
    status=(isSaved ? "Learn saved to reusable map: " :
            isOverride ? "Learn saved to override: " : "Learn saved to Default Map: ")+name+
        " ← "+event.deviceId+". Select the next target or Cancel Learn.";
    repaint();
}

} // namespace cuelist
