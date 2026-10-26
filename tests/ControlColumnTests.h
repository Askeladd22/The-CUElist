#pragma once
#include "MainComponent.h"
#include "ControlMapping.h"

void controlColumnTests() {
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

}
