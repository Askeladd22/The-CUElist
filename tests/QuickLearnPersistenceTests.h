#pragma once
#include "QuickLearnHotspot.h"

// Test the real application workflow, not an independent routing mock.
void quickLearnPersistenceTests() {
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
        check(count(ui)==6,
            "All six supported GUI targets (GO, PANIC, master and strip gain/pan) must highlight.");
        ui.finishQuickLearn();
        check(!ui.isQuickLearnActive() && count(ui)==0,
            "Leaving Learn must restore the original GUI click behavior.");
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
        ui.finishQuickLearn();
        check(!ui.isQuickLearnActive(),"Explicit Cancel must stop OSC Learn.");
    });
}
