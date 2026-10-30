#pragma once
#include "QuickLearnHotspot.h"

// Test the real application workflow, not an independent routing mock.
void quickLearnPersistenceTests() {
    test("MIDI Quick Learn blinks the chosen function without a connected input",[] {
        MainComponent ui(false);
        auto* go=dynamic_cast<QuickLearnHotspot*>(
            controlsV3::named(ui,"quickLearnGo"));
        auto* panic=dynamic_cast<QuickLearnHotspot*>(
            controlsV3::named(ui,"quickLearnPanic"));
        auto* targets=dynamic_cast<QuickControlTargets*>(
            controlsV3::named(ui,"quickControlTargets"));
        check(go && panic && targets,"MIDI Learn GUI targets are missing.");
        check(ui.show.controls.midiInputId.isEmpty(),
              "Offline MIDI regression requires no selected hardware input.");
        ui.beginQuickLearn(false);
        check(ui.isQuickLearnActive() && go->isVisible() &&
              go->getLearnAccent()==palette::amber,
              "MIDI Learn must show yellow available targets without hardware.");
        cueViewFixtures::click(*go,5.0f,5.0f);
        check(ui.isQuickLearnAwaitingInput() && go->isArmedTarget() &&
              go->isPulseBright() &&
              targets->getArmedFunctionId()>0 &&
              targets->getLearnAccent()==palette::amber,
              "Selecting MIDI GO must activate yellow blink despite no device.");
        // The warning is painted by MainComponent, but its internal status
        // string is deliberately private. Assert the supported public
        // behavior here: armed target, protocol colour and no fake capture.
        cueViewFixtures::click(*panic,5.0f,5.0f);
        check(ui.isQuickLearnAwaitingInput() && panic->isArmedTarget() &&
              !go->isArmedTarget() &&
              panic->getLearnAccent()==palette::amber,
              "Changing the selected MIDI target must transfer the blink.");
        check(ui.show.controls.bindings.empty(),
              "Selecting an offline MIDI target must not invent a hardware binding.");
        ui.finishQuickLearn();
        check(!ui.isQuickLearnActive() && !go->isVisible() &&
              !panic->isVisible(),"MIDI Cancel must restore the normal GUI.");
    });
    test("MIDI Learn highlights amber and OSC Learn highlights violet",[] {
        MainComponent ui(false);
        auto* go=dynamic_cast<QuickLearnHotspot*>(
            controlsV3::named(ui,"quickLearnGo"));
        auto* list=dynamic_cast<QuickControlTargets*>(
            controlsV3::named(ui,"quickControlTargets"));
        check(go && list,"Quick Learn colour test requires GUI and Inspector.");
        ui.beginQuickLearn(false);
        check(ui.quickLearnAccent()==palette::amber &&
              go->isVisible() && go->getLearnAccent()==palette::amber &&
              list->getLearnAccent()==palette::amber,
              "MIDI targets must use the yellow/amber palette.");
        ui.beginQuickLearn(true);
        check(ui.quickLearnAccent()==palette::purple &&
              go->getLearnAccent()==palette::purple &&
              list->getLearnAccent()==palette::purple,
              "OSC targets must turn violet, not amber.");
        check(!go->isArmedTarget(),"An unselected function must not blink.");
        cueViewFixtures::click(*go,5.0f,5.0f);
        check(ui.isQuickLearnAwaitingInput() && go->isArmedTarget() &&
              go->isPulseBright() && list->getArmedFunctionId()>0,
              "Selecting GO must arm and highlight the chosen function.");
        const auto id=list->getArmedFunctionId();
        go->setLearnPresentation(palette::purple,true,false);
        list->setLearnPresentation(palette::purple,id,-3,false);
        check(!go->isPulseBright() && !list->isPulseBright(),
              "The selected target must support dim and bright blink frames.");
        ui.updateQuickLearnVisuals();
        ui.completeQuickLearn({"/test/violet",ControlKind::osc,1,0,60,false});
        check(ui.isQuickLearnActive() && !ui.isQuickLearnAwaitingInput() &&
              !go->isArmedTarget() && list->getArmedFunctionId()==0 &&
              go->getLearnAccent()==palette::purple,
              "Learn should remain violet but stop blinking after capture.");
        ui.finishQuickLearn();
        check(!go->isVisible(),"Stopping Learn must hide all colour overlays.");
    });
    test("GUI Learn overlays appear only during Learn, including audio strips",[] {
        MainComponent ui(false);
        const std::function<int(juce::Component&)> count=[&](juce::Component& c)->int {
            int n=(dynamic_cast<QuickLearnHotspot*>(&c) && c.isVisible()) ? 1 : 0;
            for (auto* child:c.getChildren()) n+=count(*child);
            return n;
        };
        check(count(ui)==0,"The normal GUI must never intercept control clicks.");
        ui.beginQuickLearn(false);
        check(ui.isQuickLearnActive(),"MIDI Learn did not remain active.");
        check(count(ui)==10,
            "All ten GUI Learn targets must appear, including Mute, Polarity, Send and Limiter.");
        ui.finishQuickLearn();
        check(!ui.isQuickLearnActive() && count(ui)==0,
            "Leaving Learn must restore the original GUI click behavior.");
    });
    test("GUI Learn maps audio switches to their correct independent functions",[] {
        MainComponent ui(false);
        const int audioId=ui.show.columns[0].id;
        ui.beginQuickLearn(false);
        const std::array<ControlTarget,5> targets{{
            ControlTarget::columnGain,ControlTarget::columnMute,
            ControlTarget::columnPolarity,ControlTarget::columnSend,
            ControlTarget::limiterToggle
        }};
        int inputNumber=20;
        for (auto kind:targets) {
            ui.chooseQuickLearnGuiTarget(kind,kind==ControlTarget::limiterToggle?0:audioId);
            check(ui.isQuickLearnAwaitingInput(),"GUI target did not arm Learn");
            ui.completeQuickLearn({"virtual-device",ControlKind::cc,1,inputNumber++,127,false});
            check(ui.isQuickLearnActive() && !ui.isQuickLearnAwaitingInput(),
                  "Learn must remain persistent after an audio parameter assignment");
            const auto found=std::find_if(ui.show.controls.bindings.begin(),
                ui.show.controls.bindings.end(),
                [inputNumber](const ControlBinding& b){return b.number==inputNumber-1;});
            check(found!=ui.show.controls.bindings.end() &&
                  found->target==kind &&
                  (kind==ControlTarget::limiterToggle ||
                   found->targetColumnId==audioId),
                  "GUI Learn created an incorrect audio function or column ID");
        }
        check(validateShow(ui.show).wasOk(),"Direct GUI Learn generated invalid mappings");
        ui.finishQuickLearn();
    });
    test("saved Control Maps are selectable as persistent Learn destinations",[] {
        MainComponent ui(false);
        ControlBinding route;route.target=ControlTarget::go;
        const int functionId=ui.createAbstractControlFunction("Scene Go",route);
        check(functionId>0,"Could not create logical function");
        const int mapId=ui.createReusableControlMap("Alt Controller",{});
        check(mapId>0,"Could not create reusable map");
        ui.beginQuickLearn(false);
        ui.selectQuickLearnMap(-mapId,0);
        ui.chooseQuickLearnTarget(functionId);
        check(ui.isQuickLearnAwaitingInput(),"Saved map did not arm for MIDI Learn");
        ui.completeQuickLearn({"controller-alt",ControlKind::cc,1,54,127,false});
        check(ui.isQuickLearnActive() && !ui.isQuickLearnAwaitingInput(),
              "Saved-map Learn must stay active between assignments");
        check(ui.show.controls.bindings.empty(),
              "Editing a saved map mutated Default Map");
        check(ui.show.controls.templates.size()==1 &&
              ui.show.controls.templates.front().set.bindings.size()==1 &&
              ui.show.controls.templates.front().set.bindings.front().functionId==functionId,
              "Learn did not persist to selected reusable map");
        ui.finishQuickLearn();
        check(validateShow(ui.show).wasOk(),"Saved Learn output failed project validation");
    });
    test("OSC Learn persists across two GUI assignments and preserves global scope",[] {
        MainComponent ui(false);
        ui.show.controls.oscInputPort=49153; // isolated, explicit test listener
        ui.beginQuickLearn(true);
        check(ui.isQuickLearnActive(),"OSC Learn did not activate.");
        auto* goTarget=controlsV3::named(ui,"quickLearnGo");
        check(goTarget && goTarget->isVisible() && goTarget->getWidth()>0,
              "GO must have a real highlighted GUI selection target.");
        cueViewFixtures::click(*goTarget,5.0f,5.0f);
        check(ui.isQuickLearnAwaitingInput(),"Clicking highlighted GO did not arm OSC input.");
        const ControlEvent go{"/test/quick-go",ControlKind::osc,1,0,127,false};
        ui.completeQuickLearn(go);
        check(ui.isQuickLearnActive() && !ui.isQuickLearnAwaitingInput(),
              "Learn should remain active and await another target after binding.");
        check(ui.show.controls.bindings.size()==1 &&
              ui.show.controls.bindings.front().scope==ControlScope::global &&
              ui.show.controls.bindings.front().deviceId==go.deviceId,
              "First quick binding must enter only Default Map.");
        auto* masterPanTarget=controlsV3::named(ui,"quickLearnMasterPan");
        check(masterPanTarget && masterPanTarget->isVisible(),
              "Master Pan must be selectable through the live GUI.");
        cueViewFixtures::click(*masterPanTarget,5.0f,5.0f);
        check(ui.isQuickLearnAwaitingInput(),
              "Next GUI target must arm capture without toggling Learn.");
        const ControlEvent pan{"/test/quick-pan",ControlKind::osc,1,0,32,false};
        ui.completeQuickLearn(pan);
        check(ui.isQuickLearnActive() && !ui.isQuickLearnAwaitingInput() &&
              ui.show.controls.bindings.size()==2,
              "Second binding must be created while mode stays active.");
        check(validateShow(ui.show).wasOk(),"Quick Learn created an invalid project.");
        check(ui.show.controls.bindings[0].functionId!=
              ui.show.controls.bindings[1].functionId,
              "GUI targets must use distinct stable function identities.");
        // Renaming a function must not create another logical function
        // when the operator clicks the same physical GUI parameter.
        const int panId=ui.show.controls.bindings.back().functionId;
        const auto found=std::find_if(ui.show.controls.functions.begin(),
            ui.show.controls.functions.end(),
            [panId](const ControlFunction& fn){return fn.id==panId;});
        check(found!=ui.show.controls.functions.end(),
              "Learned Pan function was not found.");
        const auto route=found->destination;
        const auto functionCount=ui.show.controls.functions.size();
        check(ui.updateAbstractControlFunction(panId,"Renamed Master Balance",route),
              "Unable to rename abstract Pan function.");
        cueViewFixtures::click(*masterPanTarget,5.0f,5.0f);
        check(ui.isQuickLearnAwaitingInput() &&
              ui.show.controls.functions.size()==functionCount,
              "Renamed GUI target must reuse the existing function ID.");
        ui.completeQuickLearn({"/test/quick-pan-new",ControlKind::osc,1,0,90,false});
        check(ui.show.controls.bindings.size()==2 &&
              ui.show.controls.bindings.back().deviceId=="/test/quick-pan-new",
              "Rebinding a renamed function should replace its physical input.");
        ui.finishQuickLearn();
        check(!ui.isQuickLearnActive(),"Explicit Cancel must stop OSC Learn.");
    });
}
