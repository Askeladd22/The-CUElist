#pragma once

void autoStopV7Tests() {
    test("auto stop 2.0 protects selected MIDI/OSC voices while stopping audio", [] {
        auto show=makeShow();
        addColumn(show,ColumnType::midi,"MIDI");
        addColumn(show,ColumnType::osc,"OSC");
        addRow(show,"Next");
        MidiClip midi; midi.lengthBeats=16; midi.notes.push_back({0,8,60,90,1});
        OscClip osc; osc.length=10; osc.events.push_back({5,"/scene/cue",{}});
        auto& previous=show.rows[0];
        previous.cells[1].midi=midi; previous.cells[2].osc=osc;
        previous.cells[0].stopOnNext=true;
        previous.cells[1].stopOnNext=false;
        previous.cells[2].stopOnNext=false;
        auto audio=std::make_shared<AudioClip>();
        audio->sampleRate=10;
        audio->samples.setSize(1,100);
        audio->samples.clear();
        ClipMap clips; clips[previous.cells[0].id]=audio;
        auto state=makeRenderState(show,clips);
        check(state->voices.size()==3,"Audio/MIDI/OSC render voices missing");
        check(state->voices[0].stopOnNext && !state->voices[1].stopOnNext
              && !state->voices[2].stopOnNext,"Per-cell auto stop flags not propagated");
        state->trigger(previous.id,true);
        float out[10]{}; float* channels[]{out};
        state->render(channels,1,10,10);
        near(state->rows[0].seconds,1);
        state->trigger(show.rows[1].id,true);
        check(!state->voices[0].playing,"Participating audio should stop");
        check(state->voices[1].playing && state->voices[2].playing,"Protected MIDI/OSC should survive next GO");
        check(state->rows[0].playing,"Protected voices should keep earlier cue timeline running");
        state->render(channels,1,10,10);
        near(state->rows[0].seconds,2);
        // An explicit Stop remains authoritative regardless of auto-stop exemptions.
        state->trigger(previous.id,true,true);
        check(!state->voices[1].playing && !state->voices[2].playing,
              "Explicit row Stop failed to stop protected MIDI/OSC");
    });
    test("row Auto Stop is master gate, cell ON cannot override row OFF", [] {
        auto show=makeShow(); addRow(show,"Later");
        auto clip=std::make_shared<AudioClip>(); clip->sampleRate=4; clip->samples.setSize(1,64); clip->samples.clear();
        ClipMap clips; clips[show.rows[0].cells[0].id]=clip;
        show.rows[0].stopOnNext=false; show.rows[0].cells[0].stopOnNext=true;
        auto state=makeRenderState(show,clips);
        check(!state->voices[0].stopOnNext,"Cell ON ignored row Auto Stop OFF");
        state->trigger(show.rows[0].id,true);
        state->trigger(show.rows[1].id,true);
        check(state->voices[0].playing,"Row master OFF should protect every cell");
        state->trigger(show.rows[0].cells[0].id,false,true);
        check(!state->voices[0].playing,"Manual Stop must ignore Auto Stop exemptions");
    });
    test("new cue stops all old voices and resets clock under legacy defaults", [] {
        auto show=makeShow(); addRow(show,"Next");
        auto clip=std::make_shared<AudioClip>(); clip->sampleRate=10; clip->samples.setSize(1,100); clip->samples.clear();
        ClipMap clips; clips[show.rows[0].cells[0].id]=clip;
        auto state=makeRenderState(show,clips);
        state->trigger(show.rows[0].id,true);
        float out[10]{}; float* channels[]{out};
        state->render(channels,1,10,10);
        state->trigger(show.rows[1].id,true);
        check(!state->voices[0].playing && !state->rows[0].playing,"Legacy GO did not stop earlier cue");
        near(state->rows[0].seconds,0);
    });
    test("show v7 persists cell participation and old v6 data defaults to ON", [] {
        auto original=makeShow();
        addColumn(original,ColumnType::midi,"MIDI");
        addColumn(original,ColumnType::osc,"OSC");
        addRow(original,"Second");
        original.rows[0].stopOnNext=false;
        original.rows[0].cells[0].stopOnNext=false;
        original.rows[0].cells[1].stopOnNext=true;
        original.rows[0].cells[2].stopOnNext=false;
        original.rows[1].cells[1].stopOnNext=false;
        const auto encoded=encodeShow(original);
        near((int)juce::JSON::parse(encoded)["version"],10);
        Show restored;
        check(decodeShow(encoded,restored).wasOk(),"v7 file rejected");
        check(!restored.rows[0].cells[0].stopOnNext
              && restored.rows[0].cells[1].stopOnNext
              && !restored.rows[0].cells[2].stopOnNext
              && !restored.rows[1].cells[1].stopOnNext,
              "Per-cell settings not preserved by save/load");
        check(!restored.rows[0].stopOnNext,"Row master flag lost");
        auto legacy=juce::JSON::parse(encoded);
        legacy.getDynamicObject()->setProperty("version",6);
        for (auto& row:*legacy.getProperty("rows",{}).getArray())
            for (auto& cell:*row.getProperty("cells",{}).getArray())
                cell.getDynamicObject()->removeProperty("stopOnNext");
        Show old;
        check(decodeShow(juce::JSON::toString(legacy),old).wasOk(),"v6 project rejected");
        check(!old.rows[0].stopOnNext,"Old row-level Auto Stop lost");
        for (const auto& row:old.rows) for (const auto& cell:row.cells)
            check(cell.stopOnNext,"Legacy project should keep all cells participating by default");
        auto malformed=juce::JSON::parse(encoded);
        malformed["rows"][0]["cells"][0].getDynamicObject()->setProperty("stopOnNext","false");
        Show target; target.name="Keep";
        check(decodeShow(juce::JSON::toString(malformed),target).failed(),"Invalid cell participation accepted");
        check(target.name=="Keep","Failed v7 decode mutated existing project");
    });
    test("cue grid cell Auto Stop badge and inspector toggle update immediately", [] {
        using namespace cueViewFixtures;
        MainComponent ui(false);
        addColumn(ui.show,ColumnType::midi,"M");
        addColumn(ui.show,ColumnType::osc,"O");
        auto* grid=find<CueGrid>(ui);
        ui.setSize(1600,900);
        auto* control=dynamic_cast<juce::ToggleButton*>(controlsV3::named(ui,"cellAutoStop"));
        check(control!=nullptr,"Inspector Auto Stop checkbox missing");
        check(control->getToggleState(),"New cell checkbox should default to ON");
        const int x=CueGrid::nameWidth + CueGrid::columnWidth*2 + CueGrid::columnWidth-22;
        const int y=CueGrid::headerHeight+CueGrid::rowContentInset+12;
        // Badge toggles stay hidden until the row disclosure is expanded.
        click(*grid,(float)x,(float)y);
        check(ui.show.rows[0].cells[2].stopOnNext,"Hidden Auto Stop badge was clickable");
        auto* fixed=find<CueMetricsOverlay>(ui);
        check(fixed!=nullptr,"Fixed Auto Stop strip missing");
        // The old x=96 click was outside the new 108..176 CELLS
        // disclosure. Use the shared painted/hit-tested bounds instead.
        const auto disclosure=CueMetricsOverlay::cellDetailsBounds(CueGrid::headerHeight);
        click(*fixed,(float)disclosure.getCentreX(),(float)disclosure.getCentreY());
        check(grid->autoStopDetailsVisible(ui.show.rows[0].id),
              "CELLS disclosure did not expand when clicked at its centre");
        click(*grid,(float)x,(float)y);
        check(!ui.show.rows[0].cells[2].stopOnNext,"OSC cell badge did not turn OFF");
        check(ui.show.rows[0].cells[0].stopOnNext && ui.show.rows[0].cells[1].stopOnNext,
              "OSC badge incorrectly changed other cells");
        ui.select(0,2);
        check(!control->getToggleState(),"Inspector did not mirror selected cell");
        control->setToggleState(true,juce::dontSendNotification);
        control->onClick(); // mirrors Button's checked state before dispatching callback
        check(ui.show.rows[0].cells[2].stopOnNext,"Inspector toggle did not update OSC cell");
        ui.setAutoStop(0,false);
        ui.setCellAutoStop(0,2,false);
        check(!ui.show.rows[0].stopOnNext && !ui.show.rows[0].cells[2].stopOnNext,
              "Changing row gate modified the cell setting");
        ui.setAutoStop(0,true);
        check(!ui.show.rows[0].cells[2].stopOnNext,"Re-enabling row gate reset exception");
        check(ui.hasUnsavedChanges(),"Cell Auto Stop edit did not mark show dirty");
    });
}
