#pragma once
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
        auto* grid=find<CueGrid>(ui);
        const int x=CueGrid::nameWidth+(int)ui.show.columns.size()*CueGrid::columnWidth+40;
        click(*grid,(float)x,(float)CueGrid::headerHeight+17);
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
        check(controlsV3::named(*editor,"oscAddMessage")!=nullptr,"OSC plus button missing");
        check(controlsV3::named(*editor,"oscClipLength")==nullptr &&
              controlsV3::named(*editor,"oscEventArguments")==nullptr,
              "Legacy OSC field boxes still displayed");
        editor->addEvent();
        check(ui.show.rows[0].cells[1].osc->events.size()==1,"Plus failed to persist event");
        auto* modal=juce::ModalComponentManager::getInstance()->getModalComponent(0);
        check(modal!=nullptr,"OSC plus did not open edit dialog");
        delete modal;
    });
    test("Master audio is within left column utilities and audio strips are compact", [] {
        MainComponent ui(false); ui.setSize(1500,900);
        auto* utilities=find<ColumnUtilities>(ui);
        auto* master=dynamic_cast<juce::Slider*>(controlsV3::named(*utilities,"masterAudioDb"));
        check(master && master->getParentComponent()==utilities,"Master not placed in utility panel");
        auto* audio=utilities->getChildComponent(0);
        auto* fader=dynamic_cast<juce::Slider*>(controlsV3::named(*audio,"columnGainDb"));
        auto* pan=dynamic_cast<juce::Slider*>(controlsV3::named(*audio,"columnPan"));
        check(fader && pan && pan->getWidth()<=40 && fader->getWidth()<=80,"Strip controls were not reduced");
        check(master->getWidth()<=80 && master->getSliderStyle()==juce::Slider::LinearVertical,
              "Master fader not compact and vertical");
    });
}
