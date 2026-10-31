#pragma once
#include "ControlMapArchive.h"
#include "ControlLayerBuilder.h"

void controlMapArchiveTests() {
    test("Control Map file exports only the selected independent archive item",[] {
        auto show=makeShow();
        addColumn(show,ColumnType::midi,"MIDI out");
        addColumn(show,ColumnType::control,"Control");
        const int id=show.controls.nextId++;
        ControlFunction function;
        function.id=id;function.name="Resolume Brightness";
        function.destination.target=ControlTarget::midiCc;
        function.destination.targetColumnId=show.columns[1].id;
        function.destination.outputNumber=74;function.destination.maximum=127.0f;
        show.controls.functions.push_back(function);
        ControlBinding fallback=materializeFunctionBinding(ControlBinding{},function);
        fallback.id=show.controls.nextId++;
        fallback.deviceId="controller-A";fallback.number=1;
        fallback.takeover=ControlTakeover::jump;
        show.controls.bindings.push_back(fallback);
        ControlBinding alternate=fallback;
        alternate.id=show.controls.nextId++;
        alternate.deviceId="controller-B";alternate.number=21;
        ControlSetTemplate saved;
        saved.id=show.controls.nextId++;
        saved.name="Night Scene";
        saved.set.bindings.push_back(alternate);
        show.controls.templates.push_back(saved);
        const int row=0,col=2;
        show.rows[(size_t)row].cells[(size_t)col].assignedMapId=saved.id;
        check(validateShow(show).wasOk(),"Archive fixture invalid");

        const auto exported=exportControlMap(show,saved.id);
        check(exported.contains("TheCuelistControlMap") &&
              exported.contains("Night Scene") &&
              exported.contains("Resolume Brightness") &&
              exported.contains("controller-B") &&
              !exported.contains("controller-A") &&
              !exported.contains("cue") &&
              !exported.contains("midiInputId") &&
              !exported.contains("oscInputPort"),
              "Selected map export leaked default inputs, cues or device settings");
        auto imported=show;
        imported.controls.templates.clear();
        imported.rows.front().cells[(size_t)col].assignedMapId=0;
        const auto originalFallback=imported.controls.bindings;
        const auto result=importControlMap(exported,imported);
        check(result.wasOk(),"Reimport of standalone map rejected");
        check(imported.controls.templates.size()==1 &&
              imported.controls.templates[0].name=="Night Scene" &&
              imported.controls.templates[0].id!=saved.id &&
              imported.controls.templates[0].set.bindings.size()==1 &&
              imported.controls.templates[0].set.bindings[0].deviceId=="controller-B" &&
              imported.controls.templates[0].set.bindings[0].functionId==id &&
              imported.controls.templates[0].set.bindings[0].outputNumber==74,
              "Map import changed function identity or destination");
        check(imported.controls.bindings.size()==originalFallback.size() &&
              imported.controls.bindings[0].deviceId=="controller-A",
              "Import modified Default Map");
        check(imported.rows[0].cells[col].assignedMapId==0,
              "Import silently created a cue/Control Column assignment");
        check(validateShow(imported).wasOk(),"Imported map rejected by show schema");
        check(importControlMap(exported,imported).failed() &&
              imported.controls.templates.size()==1,
              "Import silently overwrote an existing map");
        const auto fallbackJson=exportControlMap(show,0);
        check(fallbackJson.contains("Default Map") &&
              fallbackJson.contains("controller-A") &&
              !fallbackJson.contains("controller-B"),
              "Default Map export included unrelated saved map inputs");
    });

    test("Invalid standalone Control Map fails transactionally",[] {
        Show original=makeShow();
        ControlFunction function;
        function.id=original.controls.nextId++;
        function.name="Master Volume";
        function.destination.target=ControlTarget::masterGain;
        original.controls.functions.push_back(function);
        ControlBinding b=materializeFunctionBinding(ControlBinding{},function);
        b.id=original.controls.nextId++;b.deviceId="device";b.number=17;
        original.controls.bindings.push_back(b);
        const auto valid=exportControlMap(original,0);
        check(valid.isNotEmpty(),"Export of populated Default Map failed");
        const auto before=encodeShow(original);
        auto malformed=juce::JSON::parse(valid);
        auto* root=malformed.getDynamicObject();
        auto entries=root->getProperty("bindings").getArray();
        auto* first=entries->getReference(0).getDynamicObject();
        first->setProperty("channel",99);
        check(importControlMap(juce::JSON::toString(malformed),original).failed() &&
              encodeShow(original)==before,
              "Malformed channel changed project despite failed import");
        malformed=juce::JSON::parse(valid);
        entries=malformed.getProperty("bindings",{}).getArray();
        entries->getReference(0).getDynamicObject()->setProperty("function","Unavailable");
        check(importControlMap(juce::JSON::toString(malformed),original).failed() &&
              encodeShow(original)==before,
              "Missing abstract function unexpectedly remapped");
        check(importControlMap("{}",original).failed() &&
              encodeShow(original)==before,
              "Invalid format replaced the show");
    });
}
