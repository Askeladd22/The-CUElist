#pragma once
#include "ControlLayerBuilder.h"
#include "ShowEditOperations.h"

void controlRuntimeTests() {
    test("pure Control Layer builder preserves locked defaults and cue-scoped overrides",[] {
        auto show=makeShow();
        addColumn(show,ColumnType::control,"Locked default");
        addColumn(show,ColumnType::control,"Cue overrides");
        addRow(show,"Scene two");
        const int low=1,high=2;
        auto make=[&](int number,ControlTarget target) {
            ControlBinding b;
            b.id=show.controls.nextId++;
            b.deviceId="desk";
            b.number=number;
            b.target=target;
            b.takeover=ControlTakeover::jump;
            return b;
        };
        const auto inherited=make(20,ControlTarget::masterGain);
        const auto overridden=make(20,ControlTarget::masterPan);
        show.columns[low].controlLocked=true;
        show.columns[low].controlPriority=1;
        show.columns[low].controlDefaultSet.bindings={inherited};
        show.columns[high].controlPriority=3;
        show.rows[1].cells[high].controlSet=ControlSet{{overridden}};
        check(validateShow(show).wasOk(),"Control Layer fixture invalid");

        const auto layers=buildControlLayers(show);
        check(layers.size()==2 && layers[0].columnId==show.columns[low].id &&
              layers[0].locked && layers[0].priority==1 && layers[0].cueId==0 &&
              layers[1].columnId==show.columns[high].id &&
              !layers[1].locked && layers[1].priority==3 &&
              layers[1].cueId==show.rows[1].id,
              "Pure builder lost priority, locked-default or cue associations");
        ControlMappingEngine engine;
        engine.setSettings(show.controls);
        engine.setLayers(layers);
        const ControlEvent event{"desk",ControlKind::cc,1,20,91,false};
        check(engine.matched(event) && engine.matched(event)->id==inherited.id,
              "A non-acknowledged cue overrode the locked default");
        engine.setActiveCue(show.rows[1].id,0);
        check(engine.matched(event) && engine.matched(event)->id==overridden.id,
              "The higher-priority cue-specific override was not applied");
        engine.setActiveCue(0,0);
        check(engine.matched(event) && engine.matched(event)->id==inherited.id,
              "Cue-specific mapping leaked after reset");
    });

    test("same-priority Control Columns retain rightmost-column tie ordering",[] {
        auto show=makeShow();
        addColumn(show,ColumnType::control,"Left");
        addColumn(show,ColumnType::control,"Right");
        auto make=[&](int number,ControlTarget target) {
            ControlBinding b;
            b.id=show.controls.nextId++;
            b.deviceId="controller";b.number=number;
            b.target=target;b.takeover=ControlTakeover::jump;
            return b;
        };
        const auto left=make(10,ControlTarget::masterGain);
        const auto right=make(10,ControlTarget::masterPan);
        show.columns[1].controlPriority=3;
        show.columns[2].controlPriority=3;
        show.rows[0].cells[1].controlSet=ControlSet{{left}};
        show.rows[0].cells[2].controlSet=ControlSet{{right}};
        check(validateShow(show).wasOk(),"Equal-priority Control Set fixture invalid");

        ControlMappingEngine engine;
        const auto event=ControlEvent{"controller",ControlKind::cc,1,10,110,false};
        engine.setLayers(buildControlLayers(show));
        engine.setActiveCue(show.rows[0].id,0);
        check(engine.matched(event) && engine.matched(event)->id==right.id,
              "Rightmost column did not win equal-priority conflict");
        check(swapCueColumns(show,1,2).wasOk(),"Cannot reorder the Control Columns");
        engine.setLayers(buildControlLayers(show));
        check(engine.matched(event) && engine.matched(event)->id==left.id,
              "Layer builder lost visible order after reordering columns");
    });

    test("only the acknowledged cue supplies the active mapping preset",[] {
        auto show=makeShow();
        addRow(show,"Later");
        show.controls.presets.push_back({show.controls.nextId++,"Lighting"});
        const int presetId=show.controls.presets.front().id;
        show.rows[1].mappingPresetId=presetId;
        check(activeControlPresetForCue(show,0)==0 &&
              activeControlPresetForCue(show,123456789)==0 &&
              activeControlPresetForCue(show,show.rows[0].id)==0 &&
              activeControlPresetForCue(show,show.rows[1].id)==presetId,
              "Selected or unknown cue incorrectly activated a preset");
        check(validateShow(show).wasOk(),"Active-preset fixture invalid");
    });

    test("malformed in-memory cue grids cannot crash control layer refresh",[] {
        auto show=makeShow();
        addColumn(show,ColumnType::control,"Control");
        show.columns[1].controlPriority=4;
        show.rows[0].cells.pop_back(); // Deliberately invalid in-memory fixture.
        const auto layers=buildControlLayers(show);
        check(layers.empty(),"Invalid ragged row created a phantom Control Set");
    });

    test("physical GO input debounces held MIDI notes independently of mappings",[] {
        MainComponent ui(false);
        ui.engine.connected.store(true);
        addRow(ui.show,"Second scene");
        ControlBinding go;
        go.deviceId="foot-pedal";
        go.kind=ControlKind::note;
        go.number=64;
        go.target=ControlTarget::go;
        go.takeover=ControlTakeover::jump;
        check(ui.addMappingBinding(go),"Could not install hardware GO binding");
        const auto press=ControlEvent{"foot-pedal",ControlKind::note,1,64,127,false};
        const auto release=ControlEvent{"foot-pedal",ControlKind::note,1,64,0,true};
        ui.dispatchControlForTest(press);
        check(ui.nextCueIndex()==1,"First GO edge did not advance");
        ui.dispatchControlForTest(press);
        check(ui.nextCueIndex()==1,"Held GO button triggered the second cue");
        ui.dispatchControlForTest(release);
        ui.dispatchControlForTest(press);
        check(ui.nextCueIndex()==2,"New GO edge after release did not advance");
    });
}
