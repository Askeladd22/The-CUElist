#pragma once
#include "ControlLayerBuilder.h"
#include "ShowEditOperations.h"

void controlRuntimeTests() {
    test("project refresh synchronizes master controls without external GO",[] {
        MainComponent ui(false);
        ui.show.masterGain=0.4f;
        ui.show.masterPan=0.25f;
        ui.show.safetyLimiterEnabled=false;
        ui.refresh();
        auto findNamed=[](juce::Component& root,const juce::String& name)->juce::Component* {
            std::vector<juce::Component*> stack{&root};
            while (!stack.empty()) {
                auto* component=stack.back();stack.pop_back();
                if (component->getName()==name) return component;
                for (auto* child:component->getChildren()) stack.push_back(child);
            }
            return nullptr;
        };
        auto* gain=dynamic_cast<GainMeterFader*>(findNamed(ui,"masterAudioDb"));
        auto* pan=dynamic_cast<juce::Slider*>(findNamed(ui,"masterPan"));
        auto* limiter=dynamic_cast<juce::Button*>(findNamed(ui,"masterSafetyLimiter"));
        check(gain && pan && limiter,"Project refresh lost Master controls");
        check(std::abs(gain->getGainDb()-GainMeterFader::fromLinear(.4f))<.15f,
              "Master fader did not reflect restored Show gain");
        check(std::abs((float)pan->getValue()-.25f)<.0051f,
              "Master pan did not reflect restored Show pan");
        check(!limiter->getToggleState(),
                           "Safety limiter GUI did not reflect restored setting");
    });
    test("MIDI and OSC update real-time master and column controls without GO",[] {
        MainComponent ui(false);
        // Locate actual JUCE controls instead of merely asserting that the Show
        // model changed. This fails if audio is updated but the fader stays stale.
        auto findNamed=[](juce::Component& root,const juce::String& name)->juce::Component* {
            std::vector<juce::Component*> pending{&root};
            while (!pending.empty()) {
                auto* candidate=pending.back();pending.pop_back();
                if (candidate->getName()==name) return candidate;
                for (auto* child:candidate->getChildren()) pending.push_back(child);
            }
            return nullptr;
        };
        auto* master=dynamic_cast<GainMeterFader*>(findNamed(ui,"masterAudioDb"));
        auto* masterPan=dynamic_cast<juce::Slider*>(findNamed(ui,"masterPan"));
        check(master && masterPan,"Master controls missing from the GUI");
        const auto add=[&](juce::String device,ControlKind kind,int number,
                           ControlTarget target,float minimum,float maximum,int column=0) {
            ControlBinding binding;
            binding.deviceId=device;binding.kind=kind;binding.channel=1;
            binding.number=number;binding.target=target;
            binding.minimum=minimum;binding.maximum=maximum;
            binding.targetColumnId=column;binding.takeover=ControlTakeover::jump;
            check(ui.addMappingBinding(binding),"Could not configure real-time control binding");
        };
        add("controller",ControlKind::cc,20,ControlTarget::masterGain,0.0f,1.0f);
        add("controller",ControlKind::cc,21,ControlTarget::masterPan,-1.0f,1.0f);
        add("/master/gain",ControlKind::osc,0,ControlTarget::masterGain,0.0f,1.0f);
        const int audioId=ui.show.columns.front().id;
        add("controller",ControlKind::cc,22,ControlTarget::columnGain,0.0f,1.0f,audioId);
        auto* column=dynamic_cast<GainMeterFader*>(findNamed(ui,"columnGainDb"));
        check(column!=nullptr,"Audio column fader missing");
        const auto send=[&](juce::String device,ControlKind kind,int number,int value) {
            ui.dispatchControlForTest({device,kind,1,number,value,false});
        };
        check(ui.activeCueMappingId()==0,"A cue unexpectedly became active");
        send("controller",ControlKind::cc,20,64);
        check(std::abs(ui.show.masterGain-64.0f/127.0f)<0.00001f,
              "Incoming MIDI CC did not update the real master gain");
        check(std::abs(master->getGainDb()-GainMeterFader::fromLinear(ui.show.masterGain))<0.15f,
              "MIDI master gain changed internally but the visible fader stayed stale");
        send("controller",ControlKind::cc,21,32);
        check(std::abs(ui.show.masterPan-(-1.0f+64.0f/127.0f))<0.00001f,
              "MIDI CC did not update master pan");
        check(std::abs((float)masterPan->getValue()-ui.show.masterPan)<=0.0051f,
              "MIDI pan updated internally without moving its GUI control");
        send("controller",ControlKind::cc,22,28);
        check(std::abs(ui.show.columns.front().gain-28.0f/127.0f)<0.00001f,
              "Incoming MIDI CC did not update the column gain");
        check(std::abs(column->getGainDb()-
                       GainMeterFader::fromLinear(ui.show.columns.front().gain))<0.15f,
              "The column fader did not follow external MIDI input");
        send("/master/gain",ControlKind::osc,0,100);
        check(std::abs(ui.show.masterGain-100.0f/127.0f)<0.00001f &&
              std::abs(master->getGainDb()-GainMeterFader::fromLinear(ui.show.masterGain))<0.15f,
              "OSC changed neither the master value nor its live fader");
        send("controller",ControlKind::cc,20,96);
        check(std::abs(master->getGainDb()-GainMeterFader::fromLinear(ui.show.masterGain))<0.15f,
              "Repeated MIDI movement was not reflected immediately");
        check(ui.activeCueMappingId()==0,"An external fader triggered an unintended GO");
    });
    test("saved maps are independent from columns, shared by reference, and safe to delete",[] {
        MainComponent ui(false);
        const int mapA=ui.createReusableControlMap("Lights A",{});
        check(mapA>0 && ui.show.columns.size()==1,
              "A new saved Control Map must not create a Control Column");
        check(!ui.renameReusableControlMap(mapA,""),
              "Blank saved-map name must be rejected");
        check(ui.renameReusableControlMap(mapA,"Lights Front"),
              "Could not rename saved Control Map");
        check(ui.show.controls.templates[0].id==mapA &&
              ui.show.controls.templates[0].name=="Lights Front",
              "Renaming a map must preserve its stable identity");
        ControlBinding function;function.target=ControlTarget::go;
        const int fn=ui.createAbstractControlFunction("Light GO",function);
        check(fn>0,"Failed to create function");
        ControlBinding input;input.functionId=fn;input.target=ControlTarget::go;
        input.deviceId="desk";input.kind=ControlKind::cc;
        input.channel=1;input.number=31;
        check(ui.modifyReusableControlBinding(mapA,0,input,0),
              "Could not populate saved map");
        addColumn(ui.show,ColumnType::control,"Map assignment");
        addRow(ui.show,"Next");
        check(ui.applyControlSetTemplate(0,1,mapA) &&
              ui.applyControlSetTemplate(1,1,mapA),
              "Same saved map cannot be assigned to different cues in one column");
        check(ui.show.rows[0].cells[1].assignedMapId==mapA &&
              ui.show.rows[1].cells[1].assignedMapId==mapA &&
              ui.reusableControlMapUsage(mapA)==2,
              "Assigned map references were not persisted in cells");
        check(!ui.deleteReusableControlMap(mapA),
              "Deletion of an in-use reusable map must be blocked");
        input.number=32;
        const int bindingId=ui.show.controls.templates[0].set.bindings[0].id;
        check(ui.modifyReusableControlBinding(mapA,bindingId,input,1),
              "Could not change shared map");
        auto layers=buildControlLayers(ui.show);
        check(layers.size()==2 &&
              layers[0].set.bindings[0].number==32 &&
              layers[1].set.bindings[0].number==32,
              "Shared map edit did not reach all referencing cues");
        ControlMappingEngine engine;
        engine.setSettings(ui.show.controls);
        engine.setLayers(layers);
        const ControlEvent changed{"desk",ControlKind::cc,1,32,127,false};
        const ControlEvent previous{"desk",ControlKind::cc,1,31,127,false};
        for (const auto& cue:ui.show.rows) {
            engine.setActiveCue(cue.id,0);
            check(engine.matched(changed)!=nullptr && engine.matched(previous)==nullptr,
                  "Live GO mapping did not switch to the edited shared map for this cue");
        }
        const int mapB=ui.createReusableControlMap("Lights B",{});
        check(mapB>0 && ui.applyControlSetTemplate(1,1,mapB),
              "Reassigning one cue to another saved map failed");
        check(ui.show.rows[0].cells[1].assignedMapId==mapA &&
              ui.show.rows[1].cells[1].assignedMapId==mapB &&
              ui.reusableControlMapUsage(mapA)==1,
              "Reassigning a cue unexpectedly altered another cue");
        Show restored;
        check(decodeShow(encodeShow(ui.show),restored).wasOk() &&
              restored.rows[0].cells[1].assignedMapId==mapA &&
              restored.rows[1].cells[1].assignedMapId==mapB,
              "Named map references did not survive project roundtrip");
        check(ui.clearControlCellMap(0,1) && ui.deleteReusableControlMap(mapA),
              "Could not delete saved map after removing all references");
        check(ui.clearControlCellMap(1,1) && ui.deleteReusableControlMap(mapB),
              "Could not delete second unused saved map");
        check(ui.createEmptyControlSet(0,1) &&
              hasControlMapAssignment(ui.show.rows[0].cells[1]) &&
              ui.show.rows[0].cells[1].controlSet->bindings.empty(),
              "A valid empty assigned map must remain distinct from an unassigned cell");
        check(!hasControlMapAssignment(ui.show.rows[1].cells[1]),
              "An unassigned Control Column cell must remain empty");
        check(validateShow(ui.show).wasOk(),"Final independent-map project invalid");
    });
    test("missing Control Map reference is rejected at load",[] {
        auto project=makeShow();
        addColumn(project,ColumnType::control,"Map");
        project.rows[0].cells[1].assignedMapId=9999;
        check(validateShow(project).failed(),
              "Dangling saved-map reference must never silently fall back");
    });

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
