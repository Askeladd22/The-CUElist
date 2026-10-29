#pragma once
#include "ControlMapping.h"
#include "ControlPresetArchive.h"
#include "MidiInputManager.h"
#include "ControlOutputRouter.h"
#include <limits>

void controlMappingV10Tests() {
    test("unconfigured abstract outputs remain inert and preserve stable IDs",[] {
        auto show=makeShow();
        ControlBinding midi,osc;
        midi.target=ControlTarget::midiCc;midi.targetColumnId=0;
        midi.maximum=127.0f;
        osc.target=ControlTarget::oscFloat;osc.targetColumnId=0;
        show.controls.functions={{1,"MIDI Function 1",midi},
                                 {2,"OSC Function 1",osc}};
        show.controls.nextId=3;
        check(validateShow(show).wasOk(),"Unconfigured output placeholders rejected");
        const auto preset=exportControllerPreset(show);
        Show controllerImport=makeShow();
        check(importControllerPreset(preset,controllerImport).wasOk() &&
              controllerImport.controls.functions.size()==2 &&
              controllerImport.controls.functions[0].destination.targetColumnId==0,
              "Unconfigured controller preset import must remain inert");
        Show loaded;
        check(decodeShow(encodeShow(show),loaded).wasOk() &&
              loaded.controls.functions.size()==2,"Function placeholders lost on reload");
        loaded.controls.functions[0].destination.targetColumnId=99999;
        check(validateShow(loaded).failed(),"Invalid nonzero output accepted");
    });
    test("Control Mapping v10 roundtrip, saved preset and v9 migration",[] {
        auto show=makeShow();
        show.controls.midiInputId="controller-id";
        show.controls.presets.push_back({1,"Scene lights"});
        show.controls.nextId=3;
        ControlBinding b; b.id=2; b.deviceId="controller-id";
        b.kind=ControlKind::cc; b.channel=2; b.number=14;
        b.scope=ControlScope::preset; b.scopeId=1; b.target=ControlTarget::masterGain;
        b.minimum=0; b.maximum=1;
        show.controls.bindings.push_back(b);
        show.rows[0].mappingPresetId=1;
        check(validateShow(show).wasOk(),"Mapping show failed validation");
        auto json=encodeShow(show);
        near((int)juce::JSON::parse(json)["version"],10);
        Show recovered;
        check(decodeShow(json,recovered).wasOk(),"v10 mapping show refused");
        check(recovered.controls.midiInputId=="controller-id" &&
              recovered.controls.presets.size()==1 && recovered.controls.bindings.size()==1 &&
              recovered.rows[0].mappingPresetId==1,
              "v10 lost input, preset or binding");
        check(recovered.controls.bindings[0].number==14 &&
              recovered.controls.bindings[0].channel==2,"Binding trigger changed");
        auto legacy=juce::JSON::parse(json);
        legacy.getDynamicObject()->setProperty("version",9);
        legacy.getDynamicObject()->removeProperty("controls");
        for (auto& row:*legacy.getProperty("rows",{}).getArray())
            row.getDynamicObject()->removeProperty("mappingPresetId");
        Show restored;
        check(decodeShow(juce::JSON::toString(legacy),restored).wasOk(),
              "v9 project refused after mapping update");
        check(restored.controls.bindings.empty() && restored.rows[0].mappingPresetId==0,
              "Legacy project unexpectedly contains mappings");
    });
    test("invalid mapping data fails without mutating loaded show",[] {
        auto show=makeShow();
        ControlBinding b;
        b.id=1; b.deviceId="device"; b.target=ControlTarget::columnGain;
        b.targetColumnId=show.columns[0].id;
        show.controls.bindings={b}; show.controls.nextId=2;
        check(validateShow(show).wasOk(),"Valid binding rejected");
        auto invalid=show;
        invalid.controls.bindings[0].targetColumnId=999999;
        check(validateShow(invalid).failed(),"Dangling column mapping accepted");
        invalid=show; invalid.controls.bindings.push_back(b);
        check(validateShow(invalid).failed(),"Duplicate binding ID accepted");
        invalid=show; invalid.controls.bindings[0].minimum=-3;
        check(validateShow(invalid).failed(),"Gain range outside [0,4] accepted");
        invalid=show;
        invalid.rows[0].mappingPresetId=99;
        check(validateShow(invalid).failed(),"Nonexistent cue preset accepted");
        auto json=juce::JSON::parse(encodeShow(show));
        auto* controls=json.getProperty("controls",{}).getDynamicObject();
        (*controls->getProperty("bindings").getArray())[0].getDynamicObject()
            ->setProperty("number",9999);
        Show untouched; untouched.name="Keep";
        check(decodeShow(juce::JSON::toString(json),untouched).failed(),
              "Malformed v10 binding accepted");
        check(untouched.name=="Keep","Decode failure replaced working show");
    });
    test("cue mappings override preset, column and global without following selected row",[] {
        ControlSettings s; s.nextId=5; s.presets={{1,"Main"}};
        auto binding=[&](int id,ControlScope scope,int scopeId,ControlTarget target){
            ControlBinding b; b.id=id; b.deviceId="ctrl"; b.kind=ControlKind::cc;
            b.channel=1; b.number=20; b.scope=scope; b.scopeId=scopeId;
            b.target=target; b.takeover=ControlTakeover::jump; return b;
        };
        s.bindings={binding(1,ControlScope::global,0,ControlTarget::go),
                    binding(2,ControlScope::column,0,ControlTarget::columnGain),
                    binding(3,ControlScope::preset,1,ControlTarget::masterGain),
                    binding(4,ControlScope::cue,99,ControlTarget::panic)};
        ControlMappingEngine mapping; mapping.setSettings(s);
        ControlEvent event{"ctrl",ControlKind::cc,1,20,100,false};
        check(mapping.resolve(event)->binding.id==2,"Column should override global");
        mapping.setActiveCue(123,1);
        check(mapping.resolve(event)->binding.id==3,"Preset failed to override column");
        mapping.setActiveCue(99,1);
        check(mapping.resolve(event)->binding.id==4,"Cue override failed");
        mapping.setActiveCue(0,0);
        check(mapping.resolve(event)->binding.id==2,"Cue controls leaked after Panic");
    });
    test("pickup only captures when physical control crosses current audio level",[] {
        ControlSettings s; ControlBinding b;
        b.id=1; b.deviceId="dev"; b.number=10; b.target=ControlTarget::masterGain;
        b.takeover=ControlTakeover::pickup; s.nextId=2; s.bindings={b};
        ControlMappingEngine mapping;mapping.setSettings(s);
        const auto make=[&](int value){return ControlEvent{"dev",ControlKind::cc,1,10,value,false};};
        check(!mapping.resolve(make(12),.75f).has_value(),"Pickup jumped from low value");
        check(!mapping.resolve(make(50),.75f).has_value(),"Pickup triggered early");
        check(mapping.resolve(make(98),.75f).has_value(),"Crossing failed to acquire pickup");
        check(mapping.resolve(make(80),.75f).has_value(),"Captured fader dropped later message");
        mapping.setActiveCue(99,0);
        check(!mapping.resolve(make(10),.75f).has_value(),"Cue switch failed to reset pickup");
        check(mapping.resolve(make(110),.75f).has_value(),"New cue pickup never captured");
    });
    test("conflicting equal-priority triggers and wrong protocol columns are rejected",[] {
        auto show=makeShow();
        ControlBinding one; one.id=1; one.deviceId="dev"; one.number=19;
        ControlBinding two=one; two.id=2; two.target=ControlTarget::go;
        show.controls.nextId=3; show.controls.bindings={one,two};
        check(validateShow(show).failed(),"Conflicting global CC mappings allowed");
        show.controls.bindings.resize(1);
        show.controls.bindings[0].target=ControlTarget::midiCc;
        show.controls.bindings[0].targetColumnId=show.columns[0].id;
        check(validateShow(show).failed(),"MIDI CC routed to an audio column");
        show.controls.bindings[0].target=ControlTarget::oscFloat;
        check(validateShow(show).failed(),"OSC routed to an audio column");
        addColumn(show,ColumnType::midi,"MIDI 1");
        show.controls.bindings[0].target=ControlTarget::midiCc;
        show.controls.bindings[0].targetColumnId=show.columns[1].id;
        check(validateShow(show).wasOk(),"MIDI CC binding rejected on MIDI column");
    });
    test("independent OSC controller worker rejects invalid destinations",[] {
        ControlOutputRouter output;
        check(!output.sendOscFloat("",9000,"/value",.5f),"Empty OSC host accepted");
        check(!output.sendOscFloat("127.0.0.1",0,"/value",.5f),"Invalid UDP port accepted");
        check(!output.sendOscFloat("127.0.0.1",9000,"bad",.5f),"Invalid OSC path accepted");
        check(!output.sendOscFloat("127.0.0.1",9000,"/value",
                std::numeric_limits<float>::infinity()),"Infinite OSC value accepted");
        check(output.droppedCount()==0,"Invalid sends incorrectly filled output queue");
    });
    test("hardware CC, notes and bend are accepted and bounded off the UI thread",[] {
        MidiInputManager input;
        input.ingestForTest(juce::MidiMessage::controllerEvent(3,21,127));
        input.ingestForTest(juce::MidiMessage::noteOn(1,60,(juce::uint8)100));
        input.ingestForTest(juce::MidiMessage::noteOff(1,60));
        input.ingestForTest(juce::MidiMessage::pitchWheel(5,4096));
        ControlEvent e;
        check(input.poll(e) && e.kind==ControlKind::cc && e.channel==3 &&
              e.number==21 && e.value==127,"CC input not normalized");
        check(input.poll(e) && e.kind==ControlKind::note && !e.isNoteOff &&
              e.number==60,"Note On missed");
        check(input.poll(e) && e.isNoteOff && e.value==0,"Note Off missed");
        check(input.poll(e) && e.kind==ControlKind::pitchBend && e.value==4096,
              "Pitch Bend input missed");
        check(!input.poll(e),"MIDI input queue failed to drain");
    });
    test("UI dispatch changes mapped Master and column but rejects unrelated device",[] {
        MainComponent ui(false);
        ControlBinding master;
        master.deviceId="dev";master.number=7; master.target=ControlTarget::masterGain;
        master.takeover=ControlTakeover::jump;
        check(ui.addMappingBinding(master),"Unable to add master mapping");
        ui.dispatchControlForTest({"notdev",ControlKind::cc,1,7,20,false});
        near(ui.show.masterGain,1);
        ui.dispatchControlForTest({"dev",ControlKind::cc,1,7,64,false});
        near(ui.show.masterGain,64.0/127);
        ControlBinding col=master;
        col.id=0; col.number=8;col.target=ControlTarget::columnGain;
        col.targetColumnId=ui.show.columns[0].id;
        check(ui.addMappingBinding(col),"Unable to add audio column mapping");
        ui.dispatchControlForTest({"dev",ControlKind::cc,1,8,32,false});
        near(ui.show.columns[0].gain,32.0/127);
        check(ui.hasUnsavedChanges(),"Mapped faders did not mark project dirty");
    });
    test("cue control mapping changes only after the audio callback confirms GO",[] {
        MainComponent ui(false);
        const int id=ui.show.rows[0].id;
        ui.engine.connected.store(true);
        check(ui.activeCueMappingId()==0,"New show should have global controls only");
        ui.triggerRow(0);
        check(ui.activeCueMappingId()==0,"UI activated mapping before GO was executed");
        check(ui.engine.acknowledgedCueId.load()==0,"Queued GO was prematurely acknowledged");
        juce::AudioBuffer<float> output(2,64);
        ui.engine.processBlock(output.getArrayOfWritePointers(),2,64,48000.0);
        check(ui.engine.acknowledgedCueId.load()==id,"Transport did not confirm executed GO");
        ui.engine.panic();
        check(ui.engine.acknowledgedCueId.load()==0,"Panic did not disarm cue mapping");
    });
    test("unknown GO rows cannot become active control mappings",[] {
        auto show=makeShow();
        AudioEngine audio;
        audio.publish(makeRenderState(show,{}));
        check(audio.send(987654,true),"Unable to queue test command");
        juce::AudioBuffer<float> out(2,64);
        audio.processBlock(out.getArrayOfWritePointers(),2,64,48000.0);
        check(audio.acknowledgedCueId.load()==0,"Invalid cue ID activated mapping");
    });
    test("preset creation and selection keep IDs stable on cue and show",[] {
        MainComponent ui(false);
        auto first=ui.createMappingPreset("Main");
        check(first>0,"Preset creation failed");
        const int id=ui.show.rows[0].id;
        check(ui.assignMappingPreset(id,first),"Could not assign preset to cue");
        check(ui.show.rows[0].mappingPresetId==first,"Cue lost preset reference");
        check(!ui.assignMappingPreset(id,50000),"Nonexistent preset accepted");
        check(validateShow(ui.show).wasOk(),"Assigned mapping preset invalidated show");
    });

    test("Control Column priority produces partial physical-input overrides",[] {
        auto show=makeShow();
        addColumn(show,ColumnType::audio,"Audio 2");
        addColumn(show,ColumnType::control,"Default");
        addColumn(show,ColumnType::control,"Scene overrides");
        const int low=2,high=3;
        auto make=[&](int number,ControlTarget target,int destination=0) {
            ControlBinding b; b.id=show.controls.nextId++;
            b.deviceId="physical-faders";b.kind=ControlKind::cc;
            b.number=number;b.channel=1;b.target=target;
            b.targetColumnId=destination;b.takeover=ControlTakeover::jump;
            return b;
        };
        auto first=make(1,ControlTarget::columnGain,show.columns[0].id);
        auto second=make(2,ControlTarget::columnGain,show.columns[1].id);
        auto fourth=make(4,ControlTarget::masterGain);
        auto override=make(2,ControlTarget::masterGain);
        show.columns[low].controlLocked=true;
        show.columns[low].controlPriority=1;
        show.columns[low].controlDefaultSet.bindings={first,second,fourth};
        show.columns[high].controlPriority=3;
        show.rows[0].cells[high].controlSet=ControlSet{{override}};
        check(validateShow(show).wasOk(),"Valid locked/override Control Columns rejected");
        std::vector<ControlLayer> layers={
            {show.columns[low].id,0,1,true,show.columns[low].controlDefaultSet},
            {show.columns[high].id,show.rows[0].id,3,false,
                *show.rows[0].cells[high].controlSet}};
        ControlMappingEngine mapping;mapping.setLayers(layers);
        auto event=[&](int number) {
            return ControlEvent{"physical-faders",ControlKind::cc,1,number,70,false};
        };
        check(mapping.matched(event(2))->id==second.id,
              "Cue-specific mapping was applied before GO");
        mapping.setActiveCue(show.rows[0].id,0);
        check(mapping.matched(event(1))->id==first.id &&
              mapping.matched(event(2))->id==override.id &&
              mapping.matched(event(4))->id==fourth.id,
              "Higher priority must override only its own physical input");
        mapping.setActiveCue(0,0);
        check(mapping.matched(event(2))->id==second.id,
              "Cue override leaked after active cue was reset");
        auto json=encodeShow(show);Show loaded;
        check(decodeShow(json,loaded).wasOk(),"Control Column save/load rejected");
        check(loaded.columns[low].controlLocked &&
              loaded.columns[low].controlPriority==1 &&
              loaded.columns[low].controlDefaultSet.bindings.size()==3 &&
              loaded.rows[0].cells[high].controlSet->bindings.size()==1,
              "Control default/override set lost after save");
    
    });
    test("Control Column lock, priority and MIDI Learn bindings update live mapping",[] {
        MainComponent ui(false);
        addColumn(ui.show,ColumnType::control,"Control 1");
        addColumn(ui.show,ColumnType::control,"Control 2");
        ui.select(0,1);
        ControlBinding b;b.deviceId="desk";b.number=7;
        b.target=ControlTarget::masterGain;b.takeover=ControlTakeover::jump;
        check(ui.addControlSetBinding(0,1,b),"Could not add cue Control Set mapping");
        check(ui.show.rows[0].cells[1].controlSet->bindings.size()==1,
              "Control Set did not store mapping");
        check(ui.setControlColumnLocked(1,true,0),"Could not lock Control Column");
        check(ui.show.columns[1].controlPriority==1 &&
              ui.show.columns[1].controlDefaultSet.bindings.size()==1,
              "Lock did not promote cue set to low-priority column default");
        check(!ui.setControlColumnPriority(1,4),
              "Locked Control Column must reject higher priority");
        ui.dispatchControlForTest({"desk",ControlKind::cc,1,7,64,false});
        near(ui.show.masterGain,64.0/127);
        check(ui.setControlColumnLocked(1,false,0),"Unable to unlock Control Column");
        check(ui.show.columns[1].controlPriority==2 &&
              ui.show.rows[0].cells[1].controlSet->bindings.size()==1,
              "Unlock did not restore the edited default as cue set");
        check(ui.setControlColumnPriority(1,4),"Maximum priority rejected");
        check(ui.show.columns[1].controlPriority==4,
              "Control Column priority update missing");
        check(validateShow(ui.show).wasOk(),"Control UI created invalid show");
    });
    test("Control Set binding output validation and backward-compatible old show",[] {
        auto show=makeShow();
        addColumn(show,ColumnType::control,"Control 1");
        ControlBinding b;b.id=show.controls.nextId++;
        b.deviceId="dev";b.number=12;b.target=ControlTarget::midiCc;
        b.targetColumnId=show.columns[0].id;
        show.rows[0].cells[1].controlSet=ControlSet{{b}};
        check(validateShow(show).failed(),
              "Control Set routed MIDI to an Audio column");
        b.target=ControlTarget::masterGain; b.targetColumnId=0;
        show.rows[0].cells[1].controlSet=ControlSet{{b}};
        check(validateShow(show).wasOk(),"Valid Control Set mapping rejected");
        auto legacy=juce::JSON::parse(encodeShow(show));
        for (auto& col:*legacy.getProperty("columns",{}).getArray()) {
            col.getDynamicObject()->removeProperty("controlLocked");
            col.getDynamicObject()->removeProperty("controlPriority");
            col.getDynamicObject()->removeProperty("controlDefaultSet");
        }
        for (auto& row:*legacy.getProperty("rows",{}).getArray())
            for (auto& cell:*row.getProperty("cells",{}).getArray())
                cell.getDynamicObject()->removeProperty("controlSet");
        Show old;
        check(decodeShow(juce::JSON::toString(legacy),old).wasOk(),
              "Original v10 show rejected when optional Control Sets are absent");
        check(!old.rows[0].cells[1].controlSet.has_value() &&
              old.columns[1].controlPriority==2 && !old.columns[1].controlLocked,
              "Optional Control Column defaults incorrect");
    });
    test("Control Column copy/paste and row duplication preserve independent sets",[] {
        MainComponent ui(false);
        addColumn(ui.show,ColumnType::control,"Control 1");
        ControlBinding b; b.deviceId="desk";b.number=22;
        b.target=ControlTarget::masterGain;b.takeover=ControlTakeover::jump;
        check(ui.addControlSetBinding(0,1,b),"Control Set creation failed");
        check(ui.copyRow(0) && ui.duplicateRow(0),"Cannot duplicate control cue");
        check(ui.show.rows[1].cells[1].controlSet.has_value(),
              "Duplicated cue lost Control Set");
        check(ui.show.rows[1].cells[1].controlSet->bindings[0].id!=
              ui.show.rows[0].cells[1].controlSet->bindings[0].id,
              "Duplicated binding IDs not regenerated");
        check(validateShow(ui.show).wasOk(),"Duplicate Control cue invalid");
        check(ui.select(0,1) && ui.copySelectedCell() &&
              ui.select(1,1) && ui.pasteSelectedCell(),"Control cell copy failed");
        check(validateShow(ui.show).wasOk(),"Pasted Control cell invalid");
    });

    test("unlock restores the source cue without overwriting an unrelated Q override",[] {
        MainComponent ui(false);
        addColumn(ui.show,ColumnType::control,"Control A");
        addRow(ui.show,"Q2");
        ControlBinding first;first.deviceId="desk";first.number=10;
        first.target=ControlTarget::masterGain;first.takeover=ControlTakeover::jump;
        ControlBinding second=first;second.number=20;
        check(ui.addControlSetBinding(0,1,first),"Q1 source mapping was not saved");
        check(ui.addControlSetBinding(1,1,second),"Q2 override mapping was not saved");
        check(ui.setControlColumnLocked(1,true,0),"Could not lock Q1 mapping");
        check(ui.show.columns[1].controlDefaultSourceRowId==ui.show.rows[0].id,
              "Lock did not remember its source cue");
        auto third=first;third.number=11;
        check(ui.addControlSetBinding(1,1,third),"Cannot update the locked default");
        check(ui.setControlColumnLocked(1,false,1),"Unlocking from another selected cue failed");
        const auto& original=ui.show.rows[0].cells[1].controlSet;
        const auto& unrelated=ui.show.rows[1].cells[1].controlSet;
        check(original && original->bindings.size()==2,
              "Edited default was not restored to its original Q1");
        check(unrelated && unrelated->bindings.size()==1 &&
              unrelated->bindings.front().number==20,
              "Unlock erased Q2's unrelated override");
        check(validateShow(ui.show).wasOk(),"Lossless unlock invalidated the show");
        Show recovered;
        check(decodeShow(encodeShow(ui.show),recovered).wasOk(),
              "Lock/unlock edits did not survive a project roundtrip");
    });

}
