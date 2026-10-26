#pragma once
#include "PianoRoll.h"
inline void pianoRollTests() {
    test("MIDI pause GO remains available without an output device",[] {
        MainComponent ui(false); addColumn(ui.show,ColumnType::midi,{});
        ui.select(0,1);
        for (auto* child:ui.getChildren()) if (auto* combo=dynamic_cast<juce::ComboBox*>(child))
            if (combo->getItemText(0)=="Usa impostazioni celle") combo->setSelectedId(4,juce::sendNotificationSync);
        auto& row=ui.show.rows[0]; row.cells[1].midi=MidiClip{4,{{0,4,60,100,1}}};
        ui.engine.connected.store(true); ui.engine.publish(makeRenderState(ui.show,ui.clips));
        check(ui.engine.send(row.id,true),"Cannot queue initial GO"); ui.engine.processBlock(nullptr,0,1,100);
        check(ui.engine.getCellPlayback(row.cells[1].id).playing,"Test clip not playing");
        ui.triggerRow(0); ui.engine.collectRetired(); ui.engine.processBlock(nullptr,0,1,100);
        check(ui.engine.getCellPlayback(row.cells[1].id).paused,"Missing output blocked pause GO");
    });
    test("piano roll creates snaps moves resizes and deletes notes",[] {
        PianoRollEditor editor(MidiClip{},[](const MidiClip&){}); auto& canvas=editor.getCanvas();
        using namespace cueViewFixtures;
        const juce::Point<float> start{70,1240};
        canvas.mouseDoubleClick(event(canvas,start,start,2));
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
    test("piano roll edits velocity channel and commits only on Apply",[] {
        MidiClip original{4,{{0,1,60,40,2}}}; int commits=0; MidiClip saved;
        PianoRollEditor editor(original,[&](const MidiClip& clip){ ++commits; saved=clip; }); auto& canvas=editor.getCanvas();
        cueViewFixtures::click(canvas,70,1240);
        for (auto* child:editor.getChildren()) {
            if (child->getName()=="midiChannel") dynamic_cast<juce::ComboBox*>(child)->setSelectedId(8,juce::sendNotificationSync);
            if (child->getName()=="midiVelocity") dynamic_cast<juce::Slider*>(child)->setValue(112,juce::sendNotificationSync);
            if (child->getName()=="midiLength") dynamic_cast<juce::Slider*>(child)->setValue(.25,juce::sendNotificationSync);
        }
        check(commits==0 && original.notes[0].velocity==40,"Draft changed original");
        near(editor.getClip().lengthBeats,1);
        for (auto* child:editor.getChildren()) if (auto* button=dynamic_cast<juce::TextButton*>(child))
            if (button->getButtonText()=="Applica") button->onClick();
        check(commits==1 && saved.notes[0].channel==8 && saved.notes[0].velocity==112,"Apply lost note edits");
        const auto folder=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("test-artifacts");
        folder.createDirectory(); auto out=folder.getChildFile("piano-roll-preview.png").createOutputStream();
        if (out) { out->setPosition(0); out->truncate(); juce::PNGImageFormat{}.writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()),*out); }
    });
    test("piano roll right-button marquee selects moves and deletes multiple notes",[] {
        PianoRollEditor editor(MidiClip{4,{{0,.25,60,100,1},{1,.25,61,100,1},{2,.25,60,100,1}}},[](const MidiClip&){});
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
        PianoRollEditor editor(MidiClip{},[](const MidiClip&){});
        auto& canvas=editor.getCanvas();
        for (auto* child:editor.getChildren()) if (child->getName()=="midiSnap")
            dynamic_cast<juce::ComboBox*>(child)->setSelectedId(5,juce::sendNotificationSync);
        const juce::Point<float> from{94,1240};
        canvas.mouseDoubleClick(cueViewFixtures::event(canvas,from,from,2));
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
    test("MIDI piano roll dialog applies to its cell and closes with owner",[] {
        juce::Component::SafePointer<juce::Component> dialog;
        {
            MainComponent ui(false); addColumn(ui.show,ColumnType::midi,{}); ui.select(0,1);
            ui.editMidiClip(); dialog=juce::ModalComponentManager::getInstance()->getModalComponent(0);
            check(dialog!=nullptr,"MIDI editor did not open");
            auto* editor=cueViewFixtures::find<PianoRollEditor>(*dialog); check(editor!=nullptr,"Piano roll absent");
            auto& canvas=editor->getCanvas(); const juce::Point<float> p{70,1240};
            canvas.mouseDoubleClick(cueViewFixtures::event(canvas,p,p,2));
            check(!ui.show.rows[0].cells[1].midi,"Draft leaked into show");
            for (auto* child:editor->getChildren()) if (auto* button=dynamic_cast<juce::TextButton*>(child))
                if (button->getButtonText()=="Applica") button->onClick();
            check(ui.show.rows[0].cells[1].midi && ui.show.rows[0].cells[1].midi->notes.size()==1,"Clip not committed to selected cell");
            check(ui.hasUnsavedChanges(),"MIDI edit not marked dirty");
            ui.setSize(1700,900);
            const auto folder=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("test-artifacts");
            auto out=folder.getChildFile("midi-session-preview.png").createOutputStream();
            if (out) { out->setPosition(0); out->truncate(); juce::PNGImageFormat{}.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*out); }
        }
        check(dialog==nullptr,"Piano roll outlived its owner");
    });
}
