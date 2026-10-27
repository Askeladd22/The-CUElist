#pragma once
#include "MidiSessionPolicy.h"
#include "MidiInputManager.h"
#include "ControlDispatchPolicy.h"
#include <vector>

void midiSessionPolicyTests() {
    struct FakePort {
        juce::String identifier;
        juce::String name;
    };

    test("MIDI reconnect requires the exact saved input identifier",[] {
        const std::vector<FakePort> ports{
            {"exact-serial-2","Keyboard"},
            {"different-serial","Main Controller"},
            {"other-device","Control Surface"}};
        using Action=MidiSessionAction;
        check(planMidiSession("exact-serial-2",juce::String{},false,ports)==Action::reconnectSelected,
              "Selected MIDI device cannot reconnect when available");
        check(planMidiSession("missing-serial",juce::String{},false,ports)==Action::none,
              "Controller with another identifier was chosen automatically");
        check(planMidiSession("",juce::String{},false,ports)==Action::none,
              "Disabled MIDI input must never auto-connect to the first port");
        check(planMidiSession("different-serial",juce::String{},false,ports)==Action::reconnectSelected,
              "Selected controller was ignored in favor of the prior identity");
        check(planMidiSession("different-serial",juce::String{"exact-serial-2"},true,ports)==Action::none,
              "Connected and available input was spuriously disconnected");
    });

    test("unplug, reconnect and device substitution have explicit actions",[] {
        using Action=MidiSessionAction;
        const std::vector<FakePort> empty;
        const std::vector<FakePort> lookalikes{{"spare","Stage Controller"}};
        const std::vector<FakePort> restored{
            {"spare","Stage Controller"},{"selected-serial","Stage Controller"}};
        check(planMidiSession("selected-serial","selected-serial",true,empty)==Action::disconnectMissing,
              "Unplugging the running controller was not detected");
        check(planMidiSession("selected-serial","selected-serial",true,lookalikes)==Action::disconnectMissing,
              "Same product name falsely substituted a different device");
        check(planMidiSession("selected-serial","",false,lookalikes)==Action::none,
              "Missing controller was silently replaced by a similar one");
        check(planMidiSession("selected-serial","",false,restored)==Action::reconnectSelected,
              "Original controller was not recognized upon returning");
        check(planMidiSession("selected-serial","selected-serial",true,restored)==Action::none,
              "Restored controller was unnecessarily reopened");
    });

    test("MIDI Learn captures one event without sending it to GO or outputs",[] {
        bool armed=true;
        std::optional<ControlEvent> learned;
        const ControlEvent first{"foot-controller",ControlKind::note,2,62,127,false};
        const ControlEvent later{"foot-controller",ControlKind::cc,3,5,90,false};
        check(consumeMidiLearnEvent(armed,learned,first) && !armed && learned &&
              learned->deviceId=="foot-controller" && learned->kind==ControlKind::note &&
              learned->channel==2 && learned->number==62,
              "MIDI Learn did not consume the first controller event");
        check(!consumeMidiLearnEvent(armed,learned,later) && learned->number==62,
              "MIDI Learn consumed a subsequent event without re-arming");
        armed=true;
        check(consumeMidiLearnEvent(armed,learned,later) && !armed &&
              learned->kind==ControlKind::cc && learned->number==5,
              "Re-armed Learn did not capture a different input");
    });

    test("disconnect empties stale MIDI packets and physical held-button edges",[] {
        MidiInputManager input;
        PhysicalControlEdges edges;
        const auto press=ControlEvent{"selected",ControlKind::note,1,64,127,false};
        const auto release=ControlEvent{"selected",ControlKind::note,1,64,0,true};
        check(!edges.recordAndWasDown(press) && edges.recordAndWasDown(press),
              "Pre-disconnect held note fixture failed");
        input.ingestForTest(juce::MidiMessage::noteOn(1,64,(juce::uint8)127));
        input.ingestForTest(juce::MidiMessage::controllerEvent(1,8,127));
        input.disconnect(); // Matches actual unplug/change-of-input cleanup.
        edges.clear();
        ControlEvent received;
        check(!input.poll(received),"Stale pre-disconnect messages remained in the MIDI queue");
        check(!edges.recordAndWasDown(press),
              "A previously pressed physical button stayed down after disconnect");
        check(edges.recordAndWasDown(release) &&
              !edges.recordAndWasDown(press),
              "A post-reconnect release was not able to re-arm the next GO press");
    });

    test("MIDI capture remains separate from transport even when the captured input is mapped to GO",[] {
        ControlSettings settings;
        settings.nextId=2;
        ControlBinding binding;
        binding.id=1; binding.deviceId="pedal"; binding.kind=ControlKind::note;
        binding.number=64; binding.target=ControlTarget::go;
        binding.takeover=ControlTakeover::jump;
        settings.bindings={binding};
        ControlMappingEngine mappings;
        mappings.setSettings(settings);
        bool armed=true;
        std::optional<ControlEvent> learned;
        const ControlEvent press{"pedal",ControlKind::note,1,64,127,false};
        check(consumeMidiLearnEvent(armed,learned,press),
              "Armed Learn failed to claim the mapped GO event");
        check(learned && learned->number==64 && !armed,
              "MIDI Learn did not store the mapping source");
        // The caller dispatches only when consumeMidiLearnEvent returns false.
        check(!consumeMidiLearnEvent(armed,learned,press) &&
              mappings.resolve(press)->binding.target==ControlTarget::go,
              "Ordinary hardware GO should resume after the one-shot Learn");
    });
}
