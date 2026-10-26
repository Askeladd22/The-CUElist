#pragma once
#include "PianoRoll.h"
inline void pianoRollTests() {
    test("MIDI pause GO remains available without an output device",[] {
        MainComponent ui(false); addColumn(ui.show,ColumnType::midi,{});
        ui.select(0,1);
        for (auto* child:ui.getChildren()) if (auto* combo=dynamic_cast<juce::ComboBox*>(child))
            if (combo->getItemText(0)=="Use cell settings") combo->setSelectedId(4,juce::sendNotificationSync);
        auto& row=ui.show.rows[0]; row.cells[1].midi=MidiClip{4,{{0,4,60,100,1}}};
        ui.engine.connected.store(true); ui.engine.publish(makeRenderState(ui.show,ui.clips));
        check(ui.engine.send(row.id,true),"Cannot queue initial GO"); ui.engine.processBlock(nullptr,0,1,100);
        check(ui.engine.getCellPlayback(row.cells[1].id).playing,"Test clip not playing");
        ui.triggerRow(0); ui.engine.collectRetired(); ui.engine.processBlock(nullptr,0,1,100);
        check(ui.engine.getCellPlayback(row.cells[1].id).paused,"Missing output blocked pause GO");
    });
    test("piano roll creates snaps moves resizes and deletes notes",[] {
        PianoRollEditor editor(MidiClip{},[](const MidiClip&,const MidiClip&,bool){}); auto& canvas=editor.getCanvas();
        using namespace cueViewFixtures;
        const juce::Point<float> start{70,1240};
        canvas.mouseDown(event(canvas,start,start));
        check(editor.getClip().notes.size()==1,"Double click did not create note");
        near(editor.getClip().notes[0].beat,0); near(editor.getClip().notes[0].length,.25);
        check(editor.getClip().notes[0].pitch==60,"Wrong keyboard pitch");
        const juce::Point<float> moved{150,1222};
        canvas.mouseDown(event(canvas,start,start)); canvas.mouseDrag(event(canvas,moved,start,1,true)); canvas.mouseUp(event(canvas,moved,start,1,true));
        near(editor.getClip().notes[0].beat,1); check(editor.getClip().notes[0].pitch==61,"Pitch drag failed");
        const juce::Point<float> edge{162,1222}, extended{202,1222};
        canvas.mouseDown(event(canvas,edge,edge)); canvas.mouseDrag(event(canvas,extended,edge,1,true)); canvas.mouseUp(event(canvas,extended,edge,1,true));
        near(editor.getClip().notes[0].length,.75);
        check(canvas.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey)),"Delete ignored");
        check(editor.getClip().notes.empty(),"Note not deleted");
    });
    test("piano roll writes velocity channel and length immediately without Apply",[] {
        MidiClip original{4,{{0,1,60,40,2}}}; int commits=0; MidiClip saved;
        PianoRollEditor editor(original,[&](const MidiClip&,const MidiClip& clip,bool complete){
            if (complete) { ++commits; saved=clip; }
        });
        auto& canvas=editor.getCanvas();
        cueViewFixtures::click(canvas,70,1240);
        for (auto* child:editor.getChildren()) {
            if (child->getName()=="midiChannel") dynamic_cast<juce::ComboBox*>(child)->setSelectedId(8,juce::sendNotificationSync);
            if (child->getName()=="midiVelocity") dynamic_cast<juce::Slider*>(child)->setValue(112,juce::sendNotificationSync);
            if (child->getName()=="midiLength") dynamic_cast<juce::Slider*>(child)->setValue(1,juce::sendNotificationSync);
        }
        check(commits>=2 && original.notes[0].velocity==40,"Live edits not emitted or original value mutated");
        check(saved.notes[0].channel==8 && saved.notes[0].velocity==112,"Live note edits lost");
        near(editor.getClip().lengthBeats,1);
        const auto folder=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("test-artifacts");
        folder.createDirectory(); auto out=folder.getChildFile("piano-roll-preview.png").createOutputStream();
        if (out) { out->setPosition(0); out->truncate(); juce::PNGImageFormat{}.writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()),*out); }
    });
    test("piano roll right-button marquee selects moves and deletes multiple notes",[] {
        PianoRollEditor editor(MidiClip{4,{{0,.25,60,100,1},{1,.25,61,100,1},{2,.25,60,100,1}}},[](const MidiClip&,const MidiClip&,bool){});
        auto& canvas=editor.getCanvas(); const auto time=juce::Time::getCurrentTime();
        const juce::Point<float> a{60,1195},b{245,1260};
        auto right=[&](juce::Point<float> p,bool drag) {
            return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,
                juce::ModifierKeys(juce::ModifierKeys::rightButtonModifier),1,0,0,0,0,
                &canvas,&canvas,time,a,time,1,drag);
        };
        canvas.mouseDown(right(a,false)); canvas.mouseDrag(right(b,true)); canvas.mouseUp(right(b,true));
        const juce::Point<float> from{70,1240},to{90,1240};
        canvas.mouseDown(cueViewFixtures::event(canvas,from,from));
        canvas.mouseDrag(cueViewFixtures::event(canvas,to,from,1,true));
        canvas.mouseUp(cueViewFixtures::event(canvas,to,from,1,true));
        near(editor.getClip().notes[0].beat,.25);
        near(editor.getClip().notes[1].beat,1.25);
        near(editor.getClip().notes[2].beat,2.25);
        canvas.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey));
        check(editor.getClip().notes.empty(),"Marquee did not select and delete all notes");
    });
    test("piano roll snap Off permits unsnapped placement and movement",[] {
        PianoRollEditor editor(MidiClip{},[](const MidiClip&,const MidiClip&,bool){});
        auto& canvas=editor.getCanvas();
        for (auto* child:editor.getChildren()) if (child->getName()=="midiSnap")
            dynamic_cast<juce::ComboBox*>(child)->setSelectedId(5,juce::sendNotificationSync);
        const juce::Point<float> from{94,1240};
        canvas.mouseDown(cueViewFixtures::event(canvas,from,from));
        near(editor.getClip().notes[0].beat,.375);
        const juce::Point<float> dragFrom{100,1240},dragTo{112,1240};
        canvas.mouseDown(cueViewFixtures::event(canvas,dragFrom,dragFrom));
        canvas.mouseDrag(cueViewFixtures::event(canvas,dragTo,dragFrom,1,true));
        canvas.mouseUp(cueViewFixtures::event(canvas,dragTo,dragFrom,1,true));
        near(editor.getClip().notes[0].beat,.525);
    });
    test("MIDI grid drag and timeline offset preserve embedded notes",[] {
        MainComponent ui(false); addColumn(ui.show,ColumnType::midi,{}); addRow(ui.show,{});
        ui.show.rows[0].cells[1].midi=MidiClip{4,{{0,1,60,100,1}}};
        const int id=ui.show.rows[0].cells[1].id; ui.select(0,1); ui.moveClip(0,1,1,1);
        check(ui.show.rows[1].cells[1].id==id && ui.hasClip(1,1),"MIDI clip identity lost");
        auto* timeline=cueViewFixtures::find<Timeline>(ui); timeline->setSize(800,300);
        const juce::Point<float> from{160,136},to{224,136};
        timeline->mouseDown(cueViewFixtures::event(*timeline,from,from));
        timeline->mouseDrag(cueViewFixtures::event(*timeline,to,from,1,true)); timeline->mouseUp(cueViewFixtures::event(*timeline,to,from,1,true));
        near(ui.show.rows[1].cells[1].startSeconds,1); near(ui.clipDuration(1,1),2);
    });
    test("shared Clip Editor docks, floats, writes immediately and supports undo redo",[] {
        MainComponent ui(false); addColumn(ui.show,ColumnType::midi,{});
        ui.select(0,1);
        ui.editMidiClip();
        auto* editor=ui.activeClipEditor();
        check(editor!=nullptr && !ui.isClipEditorFloating(),"Editor not docked");
        check(cueViewFixtures::find<PianoRollEditor>(ui)==editor,"Dock is not part of the main window");
        const int cellId=ui.show.rows[0].cells[1].id;
        auto& canvas=editor->getCanvas();
        const juce::Point<float> p{70,1240};
        canvas.mouseDown(cueViewFixtures::event(canvas,p,p));
        check(ui.show.rows[0].cells[1].midi && ui.show.rows[0].cells[1].midi->notes.size()==1,
              "MIDI was not committed immediately");
        check(ui.hasUnsavedChanges(),"Live edit not marked dirty");
        check(ui.undoEdit(),"Undo failed");
        check(ui.show.rows[0].cells[1].midi->notes.empty(),"Undo did not restore empty MIDI");
        check(ui.redoEdit(),"Redo failed");
        check(ui.show.rows[0].cells[1].midi->notes.size()==1,"Redo did not restore MIDI");
        ui.detachMidiClip();
        check(ui.isClipEditorFloating() && ui.activeClipEditor()==editor,"Floating editor was not the same instance");
        ui.dockMidiClip();
        check(!ui.isClipEditorFloating() && ui.activeClipEditor()==editor,"Editor could not reattach");
        check(ui.show.rows[0].cells[1].id==cellId,"Docking moved the wrong cell");
        addRow(ui.show,{}); ui.select(1,1);
        check(editor->getClip().notes.empty(),"Changing MIDI cell did not update the shared editor");
        ui.closeMidiClip();
        check(ui.activeClipEditor()==nullptr,"Editor failed to close");
    });
}
