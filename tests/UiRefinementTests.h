#pragma once
#include "GainMeterFader.h"
#include "OscEditor.h"
void uiRefinementTests() {
    using namespace cueViewFixtures;
    test("Ctrl+C/V style cell clipboard preserves target identity and copies audio media", [] {
        MainComponent ui(false); addRow(ui.show,"Next");
        auto media=std::make_shared<AudioClip>(); media->sampleRate=48000;
        media->samples.setSize(1,48000); media->samples.clear();
        const int oldId=ui.show.rows[1].cells[0].id;
        ui.clips[ui.show.rows[0].cells[0].id]=media;
        ui.show.rows[0].cells[0].name="Applausi";
        ui.show.rows[0].cells[0].loop=true;
        check(ui.copySelectedCell(),"Audio cell copy failed");
        check(ui.select(1,0),"Could not select destination audio cell");
        check(ui.pasteSelectedCell(),"Audio paste failed");
        check(ui.show.rows[1].cells[0].id==oldId,"Paste modified target stable ID");
        check(ui.show.rows[1].cells[0].name=="Applausi" && ui.show.rows[1].cells[0].loop,
              "Paste lost audio metadata");
        check(ui.clips[oldId]==media,"Paste lost media sample reference");
        addColumn(ui.show,ColumnType::midi,"M");
        check(ui.select(1,1),"MIDI destination not selected");
        // Mismatched types are rejected by pasteSelectedCell; avoid opening a
        // warning modal here, which could interfere with subsequent GUI tests.
    });
    test("MIDI and OSC cells copy their embedded content independently", [] {
        MainComponent ui(false); addRow(ui.show,"Next");
        addColumn(ui.show,ColumnType::midi,"M");
        addColumn(ui.show,ColumnType::osc,"O");
        ui.show.rows[0].cells[1].midi=MidiClip{8,{{0,2,60,100,1}}};
        ui.show.rows[0].cells[2].osc=OscClip{false,5,{{2,"/scene",{{OscValueType::integer,4,0,{}}}}}};
        check(ui.select(0,1) && ui.copySelectedCell(),"MIDI source copy failed");
        const int midiId=ui.show.rows[1].cells[1].id;
        check(ui.select(1,1) && ui.pasteSelectedCell(),"MIDI paste failed");
        check(ui.show.rows[1].cells[1].id==midiId && ui.show.rows[1].cells[1].midi->notes.size()==1,
              "MIDI pasted with wrong identity or data");
        check(ui.select(0,2) && ui.copySelectedCell(),"OSC source copy failed");
        const int oscId=ui.show.rows[1].cells[2].id;
        check(ui.select(1,2) && ui.pasteSelectedCell(),"OSC paste failed");
        check(ui.show.rows[1].cells[2].id==oscId && ui.show.rows[1].cells[2].osc->events[0].address=="/scene",
              "OSC pasted with wrong identity or data");
        ui.show.rows[0].cells[2].osc->events[0].address="/changed";
        check(ui.show.rows[1].cells[2].osc->events[0].address=="/scene",
              "OSC clipboard pasted mutable shared message data");
    });
    test("BPM row label opens direct edit dialog without metric control", [] {
        MainComponent ui(false);
        auto* metrics=find<CueMetricsOverlay>(ui);
        check(metrics!=nullptr,"Fixed BPM / Auto Stop overlay missing");
        click(*metrics,22.0f,(float)CueGrid::headerHeight+17);
        auto* modal=dynamic_cast<juce::AlertWindow*>(juce::ModalComponentManager::getInstance()->getModalComponent(0));
        check(modal!=nullptr && modal->getTextEditor("value")!=nullptr,"BPM row label did not open editor");
        check(modal->getTextEditor("value")->getText().contains("120"),"BPM dialog initial value incorrect");
        delete modal;
    });
    test("MIDI and OSC timeline edges resize durations without moving clip start", [] {
        MainComponent ui(false);
        addColumn(ui.show,ColumnType::midi,"M"); addColumn(ui.show,ColumnType::osc,"O");
        ui.show.rows[0].cells[1].midi=MidiClip{4,{{0,1,60,100,1}}};
        OscClip osc; osc.length=4; osc.events.push_back({1,"/stage",{}});
        ui.show.rows[0].cells[2].osc=osc;
        auto* timeline=find<Timeline>(ui); timeline->setSize(800,250);
        const juce::Point<float> midiFrom{268,136}, midiTo{332,136};
        timeline->mouseDown(event(*timeline,midiFrom,midiFrom));
        timeline->mouseDrag(event(*timeline,midiTo,midiFrom,1,true));
        timeline->mouseUp(event(*timeline,midiTo,midiFrom,1,true));
        near(ui.show.rows[0].cells[1].startSeconds,0);
        near(ui.show.rows[0].cells[1].midi->lengthBeats,6);
        // At 120 BPM, a 4-second OSC clip occupies 256 px of the 10-second view.
        const juce::Point<float> oscFrom{396,178}, oscTo{460,178};
        timeline->mouseDown(event(*timeline,oscFrom,oscFrom));
        timeline->mouseDrag(event(*timeline,oscTo,oscFrom,1,true));
        timeline->mouseUp(event(*timeline,oscTo,oscFrom,1,true));
        near(ui.show.rows[0].cells[2].startSeconds,0);
        near(ui.show.rows[0].cells[2].osc->length,5);
    });
    test("embedded Clip Editor height can be adjusted by dragging handle", [] {
        MainComponent ui(false);
        addColumn(ui.show,ColumnType::midi,"M"); ui.select(0,1); ui.editMidiClip();
        auto* handle=controlsV3::named(ui,"clipEditorResizeHandle");
        check(handle && handle->isVisible(),"Dock resize handle absent");
        auto* editor=ui.activeClipEditor();
        check(editor!=nullptr,"No docked editor");
        const int oldHeight=editor->getHeight();
        const juce::Point<float> from{45,6},to{45,-34};
        handle->mouseDown(event(*handle,from,from));
        handle->mouseDrag(event(*handle,to,from,1,true));
        handle->mouseUp(event(*handle,to,from,1,true));
        check(editor->getHeight()>oldHeight,"Editor dock did not grow after drag");
    });
    test("compact OSC editor exposes plus and double-click editing, not persistent argument forms", [] {
        MainComponent ui(false); addColumn(ui.show,ColumnType::osc,"OSC");
        check(ui.select(0,1),"Could not select OSC column");
        ui.editOscClip();
        auto* editor=ui.activeOscEditor();
        check(editor!=nullptr,"OSC editor missing");
        check(controlsV3::named(*editor,"oscAddAutomation")!=nullptr &&
              controlsV3::named(*editor,"oscAddMessage")==nullptr,
              "OSC plus must add a track; the redundant event button must be removed");
        check(controlsV3::named(*editor,"oscClipLength")==nullptr &&
              controlsV3::named(*editor,"oscEventArguments")==nullptr,
              "Legacy OSC field boxes still displayed");
        editor->addEvent();
        check(ui.show.rows[0].cells[1].osc->events.size()==1,"Event creation failed to persist an OSC message");
        auto* modal=juce::ModalComponentManager::getInstance()->getModalComponent(0);
        check(modal!=nullptr,"Explicit OSC event editing did not open its dialog");
        delete modal;
    });
    test("Master audio is within left column utilities and audio strips are compact", [] {
        MainComponent ui(false); ui.setSize(1500,900);
        auto* utilities=find<ColumnUtilities>(ui);
        auto* master=dynamic_cast<GainMeterFader*>(controlsV3::named(*utilities,"masterAudioDb"));
        check(master && master->getParentComponent()==utilities,"Integrated Master not placed in utility panel");
        auto* masterPan=dynamic_cast<juce::Slider*>(controlsV3::named(*utilities,"masterPan"));
        auto* masterMute=dynamic_cast<juce::ToggleButton*>(controlsV3::named(*utilities,"masterMute"));
        check(masterPan && masterMute,"Master pan and mute missing from utilities");
        auto* audio=utilities->getChildComponent(0);
        auto* fader=dynamic_cast<GainMeterFader*>(controlsV3::named(*audio,"columnGainDb"));
        auto* pan=dynamic_cast<juce::Slider*>(controlsV3::named(*audio,"columnPan"));
        check(fader && pan && pan->getWidth()>=43 && pan->getWidth()<=50 && fader->getWidth()<=96,"Strip controls were not reduced");
        check(master->getWidth()<=96 && master->getHeight()>90 &&
              master->getNumChildComponents()==0,
              "Master does not use the compact integrated meter/fader");
    });
    test("Midnight Console layout enlarges fields and meters without dropping controls",[] {
        MainComponent ui(false);
        ui.setSize(1640,900);
        auto* cue=controlsV3::named(ui,"cueName");
        auto* bpm=controlsV3::named(ui,"cueBpm");
        auto* cell=controlsV3::named(ui,"cellName");
        auto* grid=cueViewFixtures::find<CueGrid>(ui);
        auto* mixer=cueViewFixtures::find<ColumnUtilities>(ui);
        auto* ruler=cueViewFixtures::find<Timeline>(ui);
        auto* master=controlsV3::named(*mixer,"masterAudioDb");
        auto* pan=controlsV3::named(*mixer,"masterPan");
        auto* mute=controlsV3::named(*mixer,"masterMute");
        check(cue && bpm && cell && cue->getWidth()>=220 &&
              bpm->getWidth()>=220 && cell->getWidth()>=220,
              "Inspector restyling removed or clipped text inputs");
        check(grid && mixer && ruler && master && pan && mute &&
              mixer->getHeight()>=200 && master->getHeight()>=155 &&
              ruler->getHeight()>=175,
              "Performance layout clipped the mixer or made the timeline unusable");
        check(controlsV3::named(*ruler,"timelineGridVisibility") &&
              controlsV3::named(*ruler,"timelineSnapToggle"),
              "Grid and snap controls disappeared during restyling");
        check(CueGrid::rowHeight<=66 && CueGrid::columnWidth<=156 &&
              CueGrid::compressedWidth<=50,
              "Performance-sized cue cards or name lane are missing");
        check(palette::background!=palette::panel &&
              palette::panel!=palette::raised &&
              palette::muted!=palette::text,
              "Dark palette lost its text or panel contrast");
        ui.setSize(1450,768);
        check(cue->getWidth()>=220 && cue->getBottom()<ui.getHeight() &&
              bpm->getBottom()<ui.getHeight(),
              "Compact-height window clipped important Inspector fields");
    });

}
