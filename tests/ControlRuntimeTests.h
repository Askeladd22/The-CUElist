#pragma once
#include "ControlLayerBuilder.h"
#include "ShowEditOperations.h"

void controlRuntimeTests() {
    test("reusable maps duplicate, replace and restore Default Map fallback",[] {
        MainComponent ui(false);
        addColumn(ui.show,ColumnType::control,"Control");
        addRow(ui.show,"Second cue");
        const int controlColumn=1;
        ControlBinding destination;destination.target=ControlTarget::go;
        const int functionId=ui.createAbstractControlFunction("Scene Trigger",destination);
        check(functionId>0,"Function creation failed");
        ControlBinding defaultInput;defaultInput.deviceId="desk";
        defaultInput.kind=ControlKind::cc;defaultInput.channel=1;
        defaultInput.number=10;defaultInput.functionId=functionId;
        defaultInput.target=ControlTarget::go;
        check(ui.addMappingBinding(defaultInput),"Default Map assignment failed");
        const int templateId=ui.createReusableControlMap("Stage B",{});
        check(templateId>0,"Empty saved map creation failed");
        ControlBinding overrideInput=defaultInput;overrideInput.number=11;
        check(ui.modifyReusableControlBinding(templateId,0,overrideInput,0),
              "Saved map binding creation failed");
        check(ui.applyControlSetTemplate(0,controlColumn,templateId),
              "Could not assign saved map to Control Column");
        check(validateShow(ui.show).wasOk(),"Saved map did not validate after assignment");
        ControlMappingEngine engine;
        engine.setSettings(ui.show.controls);
        engine.setLayers(buildControlLayers(ui.show));
        const ControlEvent baseline{"desk",ControlKind::cc,1,10,127,false};
        const ControlEvent overridden{"desk",ControlKind::cc,1,11,127,false};
        engine.setActiveCue(ui.show.rows[0].id,0);
        check(engine.matched(overridden) && !engine.matched(baseline),
              "Cue-local map failed to replace the same function's Default Map input");
        engine.setActiveCue(ui.show.rows[1].id,0);
        check(engine.matched(baseline) && !engine.matched(overridden),
              "Default Map did not return when cue had no override");
        overrideInput.number=12;
        const int savedId=ui.show.controls.templates[0].set.bindings.front().id;
        check(ui.modifyReusableControlBinding(templateId,savedId,overrideInput,1),
              "Could not edit reusable saved map");
        check(ui.show.rows[0].cells[controlColumn].controlSet->bindings.front().number==11,
              "Changing a saved template must not silently rewrite an existing cue");
        Show restored;
        check(decodeShow(encodeShow(ui.show),restored).wasOk() &&
              restored.controls.templates.size()==1,
              "Saved map was not serialized in the show");
        check(ui.clearControlCellMap(0,controlColumn),
              "Could not clear a cue-local mapping");
        check(!ui.show.rows[0].cells[controlColumn].controlSet &&
              validateShow(ui.show).wasOk(),
              "Removing an override did not restore a valid fallback");
    });
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
