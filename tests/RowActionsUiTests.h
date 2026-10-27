#pragma once

void rowActionsUiTests() {
    test("new unnamed columns use type-specific numbering, custom names remain unchanged",[] {
        auto show=makeShow();
        check(show.columns[0].name=="Audio 1","First audio column is not Audio 1");
        addColumn(show,ColumnType::audio,{});
        addColumn(show,ColumnType::midi,{});
        addColumn(show,ColumnType::osc,{});
        addColumn(show,ColumnType::midi,{});
        addColumn(show,ColumnType::audio,"Dialoghi");
        addColumn(show,ColumnType::audio,{});
        addColumn(show,ColumnType::osc,{});
        check(show.columns[1].name=="Audio 2","Second audio column not named Audio 2");
        check(show.columns[2].name=="MIDI 1" && show.columns[3].name=="OSC 1",
              "MIDI/OSC counters not independent of audio");
        check(show.columns[4].name=="MIDI 2" && show.columns[5].name=="Dialoghi",
              "Custom name changed or MIDI numbering incorrect");
        check(show.columns[6].name=="Audio 4" && show.columns[7].name=="OSC 2",
              "Ordinal numbering does not include existing type columns");
        show.columns[0].name="Audio 5"; addColumn(show,ColumnType::audio,{});
        check(show.columns.back().name=="Audio 6","Automatic names collided with existing names");
        Show restored;
        check(decodeShow(encodeShow(show),restored).wasOk(),"Named columns failed v8 roundtrip");
        check(restored.columns[2].name=="MIDI 1","Names lost after save");
    });
    test("row and column reordering preserves typed cell IDs and cached audio",[] {
        MainComponent ui(false);
        addColumn(ui.show,ColumnType::midi,"MIDI");
        addRow(ui.show,"Later");
        const int originalRow=ui.show.rows[0].id;
        const int audioColumn=ui.show.columns[0].id;
        const int midiColumn=ui.show.columns[1].id;
        const int originalAudio=ui.show.rows[0].cells[0].id;
        const int originalMidi=ui.show.rows[0].cells[1].id;
        const int secondAudio=ui.show.rows[1].cells[0].id;
        auto pcm=std::make_shared<AudioClip>();
        pcm->sampleRate=48000; pcm->samples.setSize(1,128); pcm->samples.clear();
        ui.clips[originalAudio]=pcm;

        ui.reorderColumn(0,1);
        check(ui.show.columns[0].id==midiColumn && ui.show.columns[1].id==audioColumn,
              "Column reorder changed stable column IDs");
        check(ui.show.rows[0].cells[0].id==originalMidi &&
              ui.show.rows[0].cells[1].id==originalAudio &&
              ui.show.rows[1].cells[1].id==secondAudio,
              "Cells lost their association with the reordered column types");
        check(ui.clips.at(originalAudio)==pcm,"Column reorder discarded cached audio");

        ui.reorderRow(0,1);
        check(ui.show.rows[1].id==originalRow &&
              ui.show.rows[1].cells[1].id==originalAudio &&
              ui.clips.at(originalAudio)==pcm,
              "Cue reorder damaged identity or audio ownership");
        check(validateShow(ui.show).wasOk(),
              "Reordering rows and columns produced invalid show structure");
    });
    test("row duplicate keeps all media and cue properties while regenerating stable identities",[] {
        MainComponent ui(false);
        addColumn(ui.show,ColumnType::midi,"MIDI 1");
        addColumn(ui.show,ColumnType::osc,"OSC 1");
        ui.show.rows[0].name="Scena 1";
        ui.show.rows[0].tempo={{0,92,4,4},{8,110,4,4}};
        ui.show.rows[0].stopOnNext=false;
        ui.show.rows[0].gain=.4f;
        ui.show.rows[0].cells[0].file="C:/scena.wav";
        ui.show.rows[0].cells[0].loop=true;
        ui.show.rows[0].cells[0].stopOnNext=false;
        ui.show.rows[0].cells[1].midi=MidiClip{8,{{0,2,60,100,1}}};
        OscClip osc; osc.length=3; osc.events={{1,"/luci",{{OscValueType::integer,7,0,{}}}}};
        ui.show.rows[0].cells[2].osc=osc;
        auto pcm=std::make_shared<AudioClip>();
        pcm->sampleRate=48000; pcm->samples.setSize(1,480);
        pcm->samples.clear();
        const auto original=ui.show.rows[0];
        ui.clips[original.cells[0].id]=pcm;
        // Match the visible inspector with the programmatically populated
        // source cue; otherwise an unapplied UI draft would correctly win.
        auto* name=dynamic_cast<juce::TextEditor*>(controlsV3::named(ui,"cueName"));
        auto* bpm=dynamic_cast<juce::TextEditor*>(controlsV3::named(ui,"cueBpm"));
        auto* level=dynamic_cast<juce::Slider*>(controlsV3::named(ui,"rowGainDb"));
        name->setText("Scena 1",false);
        bpm->setText("92.0",false);
        level->setValue(juce::Decibels::gainToDecibels(.4f),juce::dontSendNotification);
        for (auto* child:ui.getChildren()) if (auto* toggle=dynamic_cast<juce::ToggleButton*>(child))
            if (toggle->getButtonText()=="Loop playback")
                toggle->setToggleState(true,juce::dontSendNotification);
        check(ui.duplicateRow(0),"Duplicate cue failed");
        check(ui.show.rows.size()==2 && ui.selectedRow==1,"Duplicate cue not inserted after source");
        const auto& duplicate=ui.show.rows[1];
        check(duplicate.name=="Scena 1 (copy)" && duplicate.tempo.size()==2,
              "Duplicate cue name or BPM tempo map lost");
        near(duplicate.gain,.4);
        check(!duplicate.stopOnNext && duplicate.cells[0].loop && !duplicate.cells[0].stopOnNext,
              "Duplicate row/cell behaviour lost");
        check(duplicate.id!=original.id && duplicate.cells[0].id!=original.cells[0].id
            && duplicate.cells[1].id!=original.cells[1].id && duplicate.cells[2].id!=original.cells[2].id,
            "Duplicated identities collided with original cue");
        check(ui.clips[duplicate.cells[0].id]==pcm && ui.clips[original.cells[0].id]==pcm,
              "Copied audio should share cached PCM without duplicating RAM");
        check(duplicate.cells[1].midi && duplicate.cells[1].midi->notes.size()==1
            && duplicate.cells[2].osc && duplicate.cells[2].osc->events[0].address=="/luci",
              "MIDI and OSC clips not cloned");
        ui.show.rows[0].cells[2].osc->events[0].address="/changed";
        check(ui.show.rows[1].cells[2].osc->events[0].address=="/luci",
              "Duplicated OSC clip shares mutable data with original");
        check(validateShow(ui.show).wasOk(),"Duplicated show failed validation");
        Show restored;
        check(decodeShow(encodeShow(ui.show),restored).wasOk(),"Duplicate invalid after serialization");
        check(ui.hasUnsavedChanges(),"Duplicating cue did not mark project dirty");
    });
    test("copy and paste entire row remain possible after source is removed",[] {
        MainComponent ui(false); addRow(ui.show,"Altro");
        ui.show.rows[0].name="Partenza"; ui.show.rows[0].cells[0].name="Intro";
        auto pcm=std::make_shared<AudioClip>(); pcm->sampleRate=48000;
        pcm->samples.setSize(1,64); pcm->samples.clear();
        const int oldId=ui.show.rows[0].cells[0].id;
        ui.clips[oldId]=pcm;
        dynamic_cast<juce::TextEditor*>(controlsV3::named(ui,"cueName"))->setText("Partenza",false);
        dynamic_cast<juce::TextEditor*>(controlsV3::named(ui,"cellName"))->setText("Intro",false);
        check(ui.copyRow(0),"Copy entire row failed");
        ui.deleteRow(0);
        check(ui.show.rows.size()==1 && ui.show.rows[0].name=="Altro",
              "Deleting copied source removed wrong row");
        check(ui.pasteRowAfter(0),"Paste after deleting original row failed");
        check(ui.show.rows.size()==2 && ui.show.rows[1].name=="Partenza"
            && ui.show.rows[1].cells[0].name=="Intro","Pasted row metadata lost");
        check(ui.show.rows[1].cells[0].id!=oldId && ui.show.rows[1].id!=ui.show.rows[0].id,
              "Pasted row reused deleted IDs");
        check(ui.clips[ui.show.rows[1].cells[0].id]==pcm,"Pasted row lost audio after deletion");
        check(validateShow(ui.show).wasOk(),"Paste created invalid identities");
    });
    test("row paste rejects stale clipboard column layout without changing the show",[] {
        MainComponent ui(false);
        check(ui.copyRow(0),"Could not copy source row");
        addColumn(ui.show,ColumnType::midi,{});
        const auto before=encodeShow(ui.show);
        check(!ui.pasteRowAfter(0),"Incompatible row paste unexpectedly succeeded");
        check(encodeShow(ui.show)==before,"Invalid paste mutated show state");
    });
    test("removing last row preserves usable show with its default column name",[] {
        MainComponent ui(false);
        ui.deleteRow(0);
        check(ui.show.rows.size()==1 && ui.show.rows[0].cells.size()==1,
              "Last row removal left no blank cue");
        check(ui.show.columns[0].name=="Audio 1","Default column title lost on last-row delete");
        check(validateShow(ui.show).wasOk(),"Deletion corrupted default show");
    });
}
