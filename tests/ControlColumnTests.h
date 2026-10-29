#pragma once
#include "MainComponent.h"
#include "ControlMapping.h"
#include "ControlPresetArchive.h"

void controlColumnTests() {
    test("Legacy Control Column lock migrates without implicitly activating dormant Q sets",[] {
        Show original=makeShow();
        addColumn(original,ColumnType::control,"Legacy control");
        addColumn(original,ColumnType::midi,"External MIDI");
        addRow(original,"Next Q");
        const int controlIndex=1;
        auto& col=original.columns[(size_t)controlIndex];
        col.controlLocked=true;
        col.controlPriority=1;
        col.controlDefaultSourceRowId=original.rows.front().id;
        ControlBinding old;
        old.id=original.controls.nextId++;
        old.deviceId="fader-A";old.kind=ControlKind::cc;
        old.channel=1;old.number=20;
        old.target=ControlTarget::midiCc;
        old.targetColumnId=original.columns[2].id;
        old.outputNumber=74;old.outputChannel=9;
        old.minimum=0;old.maximum=127;
        old.takeover=ControlTakeover::jump;
        col.controlDefaultSet.bindings.push_back(old);
        ControlBinding dormant=old;
        dormant.id=original.controls.nextId++;
        dormant.deviceId="fader-B";dormant.number=25;
        original.rows.front().cells[controlIndex].controlSet=
            ControlSet{{dormant}};
        check(validateShow(original).wasOk(),
              "Legacy locked show fixture is invalid");
        Show migrated=original;
        bool changed=false;
        check(migrateLegacyControlColumns(migrated,changed).wasOk() && changed,
              "Legacy migration failed");
        check(!migrated.columns[controlIndex].controlLocked &&
              migrated.columns[controlIndex].controlPriority==2 &&
              migrated.columns[controlIndex].controlDefaultSet.bindings.empty(),
              "Per-column lock was not retired");
        check(migrated.controls.functions.size()==1 &&
              migrated.controls.bindings.size()==1 &&
              migrated.controls.templates.size()==1 &&
              !migrated.rows.front().cells[controlIndex].controlSet,
              "Legacy global input or dormant Q set was lost/activated");
        const auto& function=migrated.controls.functions.front();
        check(function.destination.target==ControlTarget::midiCc &&
              function.destination.outputNumber==74 &&
              function.destination.outputChannel==9,
              "Migration altered the destination's MIDI CC 74");
        ControlMappingEngine mapping;
        mapping.setSettings(migrated.controls);
        mapping.setLayers(buildControlLayers(migrated));
        mapping.setActiveCue(migrated.rows.front().id,0);
        const auto* effective=mapping.matched(
            {"fader-A",ControlKind::cc,1,20,85,false});
        check(effective && effective->functionId==function.id &&
              effective->outputNumber==74,
              "Default Map did not inherit the migrated input");
        check(mapping.matched({"fader-B",ControlKind::cc,1,25,85,false})==nullptr,
              "Dormant Q set unexpectedly fired after migration");
        check(validateShow(migrated).wasOk(),
              "Migrated Control Map failed complete Show validation");
        const auto snapshot=encodeShow(migrated);
        Show loaded;
        check(decodeShow(snapshot,loaded).wasOk() &&
              loaded.controls.functions.size()==1 &&
              loaded.controls.templates.size()==1,
              "Migrated map or archived template did not persist");
        const auto before=encodeShow(migrated);
        changed=true;
        check(migrateLegacyControlColumns(migrated,changed).wasOk() &&
              !changed && encodeShow(migrated)==before,
              "Legacy migration was not idempotent");
        check(original.columns[controlIndex].controlLocked &&
              original.rows.front().cells[controlIndex].controlSet.has_value(),
              "Migration mutated the input model");
    });

    test("Portable controller preset excludes names and preserves cue overrides",[] {
        auto source=makeShow();
        addColumn(source,ColumnType::midi,"MIDI Out");
        ControlFunction f;f.id=source.controls.nextId++;
        f.name="Resolume Brightness";
        f.destination.target=ControlTarget::midiCc;
        f.destination.targetColumnId=source.columns[1].id;
        f.destination.outputChannel=9;
        f.destination.outputNumber=74;
        f.destination.maximum=127;
        source.controls.functions.push_back(f);
        ControlBinding input;input.id=source.controls.nextId++;
        input.functionId=f.id;input.deviceId="physical-A";
        input.number=20;input.target=f.destination.target;
        input.targetColumnId=f.destination.targetColumnId;
        source.controls.bindings.push_back(input);
        const auto archive=exportControllerPreset(source);
        check(!archive.contains("Resolume Brightness") &&
              archive.contains("TheCuelistControllerPreset"),
              "Portable preset leaked project-specific function names");
        auto project=makeShow();
        addColumn(project,ColumnType::midi,"Output");
        addColumn(project,ColumnType::control,"Control");
        ControlFunction existing=f;
        existing.name="Project-specific name";
        existing.destination.targetColumnId=project.columns[1].id;
        project.controls.functions.push_back(existing);
        project.controls.nextId=3;
        ControlBinding local=input;local.id=project.controls.nextId++;
        local.deviceId="physical-B";local.number=30;
        local.targetColumnId=project.columns[1].id;
        project.rows[0].cells[2].controlSet=ControlSet{{local}};
        check(validateShow(project).wasOk(),"Controller preset fixture invalid");
        check(importControllerPreset(archive,project).wasOk(),
              "Unable to import a preset with a compatible MIDI column");
        check(project.controls.functions[0].name=="Project-specific name" &&
              project.controls.functions[0].destination.outputNumber==74 &&
              project.controls.functions[0].destination.outputChannel==9 &&
              project.controls.bindings.size()==1 &&
              project.controls.bindings[0].deviceId=="physical-A" &&
              project.rows[0].cells[2].controlSet->bindings.front().deviceId=="physical-B",
              "Import changed project labels, unrelated cue override or MIDI routing");
        auto invalid=makeShow(); // no compatible MIDI destination
        const auto oldName=invalid.name;
        check(importControllerPreset(archive,invalid).failed() &&
              invalid.controls.functions.empty() && invalid.name==oldName,
              "Failed preset import mutated an incompatible project");
    });
    test("OSC input decodes numeric and boolean messages into learned addresses",[] {
        OscEvent event;event.address="/show/brightness";
        OscValue value;value.type=OscValueType::decimal;value.decimal=.75f;
        event.arguments.push_back(value);
        const auto bytes=encodeOscMessage(event);
        OscControlInput::Packet packet;
        check(OscControlInput::decode(bytes.data(),(int)bytes.size(),packet) &&
              juce::String(packet.address.data())=="/show/brightness" &&
              packet.value>=94 && packet.value<=97,
              "OSC numeric control input decoded an incorrect address/value");
        OscControlInput input;
        input.ingestForTest(bytes.data(),(int)bytes.size());
        ControlEvent incoming;
        check(input.poll(incoming) && incoming.kind==ControlKind::osc &&
              incoming.channel==1 && incoming.number==0 &&
              incoming.deviceId=="/show/brightness" &&
              incoming.value==packet.value,
              "OSC Learn did not receive the parsed control event");
        event.arguments[0].type=OscValueType::boolean;
        event.arguments[0].boolean=true;
        const auto on=encodeOscMessage(event);
        check(OscControlInput::decode(on.data(),(int)on.size(),packet) &&
              packet.value==127,"OSC true must trigger a full-scale action");
        event.arguments[0].boolean=false;
        const auto off=encodeOscMessage(event);
        check(OscControlInput::decode(off.data(),(int)off.size(),packet) &&
              packet.value==0,"OSC false must release a momentary input");
        const uint8_t malformed[]={'/',0,0,0,',','f',0,0,0,0,0,0};
        check(!OscControlInput::decode(malformed,sizeof(malformed),packet) ||
              packet.address[0]=='/',
              "Malformed OSC packet caused invalid memory access");
    });
    test("Abstract function preserves MIDI destination when local fader changes",[] {
        ControlSettings settings;
        ControlFunction function;
        function.id=1;function.name="Resolume Brightness";
        function.destination.target=ControlTarget::midiCc;
        function.destination.outputNumber=74;
        function.destination.outputChannel=9;
        function.destination.minimum=0;
        function.destination.maximum=127;
        settings.functions.push_back(function);
        settings.nextId=5;
        ControlBinding global;global.id=2;global.deviceId="hardware-A";
        global.number=11;global.takeover=ControlTakeover::jump;
        global.functionId=1;settings.bindings.push_back(global);
        ControlBinding override=global;
        override.id=3;override.deviceId="hardware-B";override.number=25;
        override.target=ControlTarget::oscFloat;override.outputNumber=12;
        ControlLayer local;local.columnId=100;local.cueId=202;
        local.priority=2;local.set.bindings.push_back(override);
        ControlMappingEngine mapping;
        mapping.setSettings(settings);mapping.setLayers({local});
        const ControlEvent a{"hardware-A",ControlKind::cc,1,11,64,false};
        const ControlEvent b{"hardware-B",ControlKind::cc,1,25,64,false};
        mapping.setActiveCue(201,0);
        check(mapping.matched(a)!=nullptr && mapping.matched(b)==nullptr,
              "Default Map not active on first cue");
        mapping.setActiveCue(202,0);
        check(mapping.matched(a)==nullptr && mapping.matched(b)!=nullptr,
              "Cue override did not replace the function input");
        const auto action=mapping.resolve(b);
        check(action && action->binding.functionId==1 &&
              action->binding.outputNumber==74 &&
              action->binding.outputChannel==9 &&
              action->binding.target==ControlTarget::midiCc,
              "Local fader changed the abstract function destination");
        mapping.setActiveCue(203,0);
        check(mapping.matched(a)!=nullptr && mapping.matched(b)==nullptr,
              "Cue transition retained a stale Control Set override");
        const auto preview=mapping.effectiveForCue(202);
        const auto found=std::find_if(preview.begin(),preview.end(),
            [](const EffectiveControl& c){return c.binding.functionId==1;});
        check(found!=preview.end() && found->overrides && found->columnId==100,
              "Control Set preview lost provenance of the function override");
    });
    test("Abstract function and cue override persist in optional v10 fields",[] {
        auto show=makeShow();
        addColumn(show,ColumnType::control,"Lighting");
        ControlFunction f;f.id=show.controls.nextId++;
        f.name="Resolume Brightness";
        f.destination.target=ControlTarget::midiCc;
        addColumn(show,ColumnType::midi,"Resolume");
        f.destination.targetColumnId=show.columns.back().id;
        f.destination.outputNumber=74;
        f.destination.outputChannel=7;
        f.destination.maximum=127;
        show.controls.functions.push_back(f);
        ControlBinding input;input.id=show.controls.nextId++;
        input.deviceId="fader-A";input.number=10;input.functionId=f.id;
        input.target=f.destination.target;input.targetColumnId=f.destination.targetColumnId;
        show.controls.bindings.push_back(input);
        ControlBinding replacement=input;replacement.id=show.controls.nextId++;
        replacement.deviceId="fader-B";replacement.number=30;
        show.rows[0].cells[1].controlSet=ControlSet{{replacement}};
        check(validateShow(show).wasOk(),"Abstract mapping invalidates v10 show");
        Show loaded;
        check(decodeShow(encodeShow(show),loaded).wasOk(),
              "Abstract mapping cannot survive a project roundtrip");
        check(loaded.controls.functions.size()==1 &&
              loaded.controls.functions[0].destination.outputNumber==74 &&
              loaded.rows[0].cells[1].controlSet &&
              loaded.rows[0].cells[1].controlSet->bindings.front().functionId==f.id,
              "Function identity or cue override was lost on reload");
    });
    test("Control Column Q mappings inherit and override only matching hardware inputs",[] {
        ControlBinding first; first.id=11; first.deviceId="controller";first.number=1;
        first.target=ControlTarget::columnGain;first.takeover=ControlTakeover::jump;
        ControlBinding second=first;second.id=12;second.number=2;
        ControlBinding override=first;override.id=13;
        override.target=ControlTarget::oscFloat;override.oscAddress="/composition/opacity";
        ControlMappingEngine mapping;
        ControlLayer low;low.columnId=100;low.priority=1;low.locked=true;
        low.set.bindings={first,second};
        ControlLayer high;high.columnId=200;high.cueId=22;high.priority=3;
        high.set.bindings={override};
        mapping.setLayers({low,high});
        ControlEvent f1{"controller",ControlKind::cc,1,1,64,false};
        ControlEvent f2{"controller",ControlKind::cc,1,2,64,false};
        mapping.setActiveCue(21,0);
        check(mapping.matched(f1)->id==11 && mapping.matched(f2)->id==12,
              "Unrelated Q inherited wrong controls");
        mapping.setActiveCue(22,0);
        check(mapping.matched(f1)->id==13 && mapping.matched(f2)->id==12,
              "High priority override replaced an unrelated input");
        const auto effective=mapping.effectiveForCue(22);
        check(effective.size()==2 && effective[0].columnId==200 &&
              effective[0].overrides && !effective[1].overrides &&
              effective[1].columnId==100,
              "Merged preview lost inherited mapping or override provenance");
    });
    test("Equal-priority conflicts deterministically choose the rightmost Control Column",[] {
        ControlBinding left; left.id=1;left.deviceId="dev";left.number=16;
        left.target=ControlTarget::masterGain;
        auto right=left;right.id=2;right.target=ControlTarget::oscFloat;
        ControlLayer a;a.columnId=10;a.priority=3;a.locked=true;a.set.bindings={left};
        ControlLayer b;b.columnId=11;b.priority=3;b.locked=true;b.set.bindings={right};
        ControlMappingEngine mapping;mapping.setLayers({a,b});
        ControlEvent input{"dev",ControlKind::cc,1,16,64,false};
        check(mapping.matched(input)->id==2,"Equal-priority rightmost tie rule failed");
        check(mapping.effectiveForCue(0)[0].columnId==11,"Preview disagrees with live tie rule");
        mapping.setLayers({b,a});
        check(mapping.matched(input)->id==1,"Column reordering did not change the tie winner");
    });
    test("Multiple mappings persist within one cue Control Set and through lock/unlock",[] {
        MainComponent ui(false);
        addColumn(ui.show,ColumnType::control,{});
        auto make=[&](int number) {
            ControlBinding b;b.deviceId="dev";b.number=number;b.takeover=ControlTakeover::jump;
            b.target=ControlTarget::masterGain;return b;
        };
        check(ui.addControlSetBinding(0,1,make(10)),"First set mapping failed");
        check(ui.addControlSetBinding(0,1,make(11)),"Second set mapping failed");
        check(ui.show.rows[0].cells[1].controlSet->bindings.size()==2,
              "Adding a mapping cleared previous mappings");
        check(ui.setControlColumnLocked(1,true,0),"Locking failed");
        check(ui.show.columns[1].controlPriority==1 &&
              ui.show.columns[1].controlDefaultSet.bindings.size()==2,
              "Locked default must be low priority with both mappings");
        addRow(ui.show,"Q2");
        const auto q2=ui.show.rows[1].id;
        check(ui.effectiveControlSet(q2).size()==2,"Locked set not inherited by new Q");
        check(ui.setControlColumnLocked(1,false,0),"Unlocking failed");
        check(ui.show.columns[1].controlPriority==2 &&
              ui.show.rows[0].cells[1].controlSet->bindings.size()==2,
              "Unlock lost cue mappings");
        check(validateShow(ui.show).wasOk(),"Lock/unlock reused binding identifiers");
    });
    test("Reusable Control Set templates roundtrip without sharing mapping IDs",[] {
        MainComponent ui(false);
        addColumn(ui.show,ColumnType::control,"Control A");
        addColumn(ui.show,ColumnType::control,"Control B");
        ControlBinding b;b.deviceId="ctrl";b.number=4;b.target=ControlTarget::masterGain;
        check(ui.addControlSetBinding(0,1,b),"Set creation failed");
        const auto id=ui.saveControlSetTemplate(0,1,"Default show controls");
        check(id>0,"Cannot save reusable Control Set template");
        check(ui.applyControlSetTemplate(0,2,id),"Cannot apply saved Control Set to another column");
        check(ui.show.rows[0].cells[1].controlSet->bindings[0].id!=
              ui.show.rows[0].cells[2].controlSet->bindings[0].id,
              "Applied template reused live binding ID");
        Show loaded;
        check(decodeShow(encodeShow(ui.show),loaded).wasOk(),
              "Project with template refused after serialization");
        check(loaded.controls.templates.size()==1 &&
              loaded.rows[0].cells[2].controlSet->bindings.size()==1,
              "Control Set template or applied mapping lost after save");
    });
    test("Mapping edit, response curves and pan targets remain valid",[] {
        MainComponent ui(false);
        addColumn(ui.show,ColumnType::control,{});
        ControlBinding b;b.deviceId="knob";b.number=27;
        b.target=ControlTarget::masterPan;b.minimum=-1;b.maximum=1;
        b.curve=ControlCurve::easeIn;b.takeover=ControlTakeover::jump;
        check(ui.addControlSetBinding(0,1,b),"Master pan mapping rejected");
        const int id=ui.show.rows[0].cells[1].controlSet->bindings[0].id;
        b.target=ControlTarget::columnPan;
        b.targetColumnId=ui.show.columns[0].id;
        b.curve=ControlCurve::easeOut;
        check(ui.updateControlBinding(0,1,id,b,true),"Editing existing pan mapping failed");
        check(ui.show.rows[0].cells[1].controlSet->bindings[0].id==id,
              "Mapping update changed its stable identity");
        near(applyControlCurve(.5f,ControlCurve::easeIn),.25);
        near(applyControlCurve(.5f,ControlCurve::easeOut),.75);
        near(inverseControlCurve(.25f,ControlCurve::easeIn),.5);
        Show loaded;
        check(decodeShow(encodeShow(ui.show),loaded).wasOk(),"Pan/curve mapping lost");
        const auto& saved=loaded.rows[0].cells[1].controlSet->bindings[0];
        check(saved.target==ControlTarget::columnPan &&
              saved.curve==ControlCurve::easeOut &&
              saved.targetColumnId==loaded.columns[0].id,
              "Updated pan destination or curve failed to roundtrip");
    });
    test("Legacy mapping remains an inherited fallback in Control Column preview",[] {
        ControlSettings settings; settings.nextId=3;
        ControlBinding legacy; legacy.id=1;legacy.deviceId="dev";legacy.number=7;
        legacy.target=ControlTarget::masterGain;
        settings.bindings.push_back(legacy);
        ControlBinding override=legacy;override.id=2;override.number=8;
        override.target=ControlTarget::oscFloat;
        ControlLayer layer;layer.columnId=45;layer.cueId=100;layer.priority=3;
        layer.set.bindings={override};
        ControlMappingEngine mapping;mapping.setSettings(settings);
        mapping.setLayers({layer});mapping.setActiveCue(100,0);
        const auto effective=mapping.effectiveForCue(100);
        check(effective.size()==2 && effective[0].columnId==0 &&
              effective[1].columnId==45,
              "Effective view dropped the legacy fallback or local override");
        ControlEvent f1{"dev",ControlKind::cc,1,7,64,false};
        ControlEvent f2{"dev",ControlKind::cc,1,8,64,false};
        check(mapping.matched(f1)->id==1 && mapping.matched(f2)->id==2,
              "Legacy fallback differed from displayed effective mapping");
    });

    test("Deleting a target column cleans reusable Control Set templates",[] {
        MainComponent ui(false);
        addColumn(ui.show,ColumnType::control,"Lighting");
        ControlBinding mapped; mapped.deviceId="fader";mapped.number=4;
        mapped.target=ControlTarget::columnGain;
        mapped.targetColumnId=ui.show.columns[0].id;
        check(ui.addControlSetBinding(0,1,mapped),"Unable to create Control Set with audio target");
        check(ui.saveControlSetTemplate(0,1,"Audio controls")>0,
              "Unable to save Control Set template before deleting the target");
        ui.deleteColumn(0);
        check(validateShow(ui.show).wasOk(),"Deleting audio column left dangling Control Set reference");
        check(ui.show.controls.templates.size()==1 &&
              ui.show.controls.templates[0].set.bindings.empty(),
              "Deleted target survived inside a reusable Control Set template");
    });

}
