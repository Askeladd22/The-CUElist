#pragma once
#include "MainComponent.h"
#include "GainMeterFader.h"
#include <cmath>

void cueTransportUXTests() {
    test("Realtime limiter indicator follows current gain reduction, not older peaks", [] {
        auto show=makeShow();
        auto clip=std::make_shared<AudioClip>();
        clip->sampleRate=48000;clip->samples.setSize(2,256);
        for(int ch=0;ch<2;++ch) for(int i=0;i<256;++i)
            clip->samples.setSample(ch,i,0.9f);
        ClipMap clips;clips[show.rows[0].cells[0].id]=clip;
        show.masterGain=2.0f;
        auto state=makeRenderState(show,clips);
        state->trigger(show.rows[0].cells[0].id,false);
        juce::AudioBuffer<float> output(2,32);
        state->render(output.getArrayOfWritePointers(),2,32,48000);
        check(state->limiterReduction>0.1f && state->limiterReducing,
              "Safety LED does not indicate active gain reduction");
        const float atPeak=state->limiterReduction;
        state->masterGain=0.5f;
        state->render(output.getArrayOfWritePointers(),2,32,48000);
        check(state->limiterReduction<atPeak,
              "Safety LED does not follow actual gain release");
        state->masterMuted=true;
        state->render(output.getArrayOfWritePointers(),2,32,48000);
        near(state->limiterReduction,0.0);
        check(!state->limiterReducing,"Limiter remained lit with no master signal");
    });
    test("MIDI and OSC compressed utilities retain both output mute and real-send LEDs",[] {
        MainComponent ui(false);ui.setSize(1500,900);
        addColumn(ui.show,ColumnType::midi,"M");
        addColumn(ui.show,ColumnType::osc,"O");
        auto* utility=cueViewFixtures::find<ColumnUtilities>(ui);
        utility->refresh();ui.resized();
        ui.toggleColumnCollapsed(1);
        ui.toggleColumnCollapsed(2);
        auto* midi=utility->getChildComponent(1);
        auto* osc=utility->getChildComponent(2);
        for(auto* strip:{midi,osc}) {
            auto* mute=dynamic_cast<juce::ToggleButton*>(
                controlsV3::named(*strip,"columnDataOutputMute"));
            auto* led=dynamic_cast<MidiActivityLed*>(controlsV3::named(
                *strip,strip==midi?"columnMidiOutputLed":"columnOscOutputLed"));
            check(mute && led && mute->isVisible() && led->isVisible(),
                  "Compressed data utility lost output mute/LED");
            check(mute->getWidth()<=30 && led->getWidth()<=25,
                  "Compressed data output controls exceed strip budget");
        }
        auto* midiMute=dynamic_cast<juce::ToggleButton*>(
            controlsV3::named(*midi,"columnDataOutputMute"));
        auto* oscMute=dynamic_cast<juce::ToggleButton*>(
            controlsV3::named(*osc,"columnDataOutputMute"));
        cueViewFixtures::click(*midiMute,midiMute->getWidth()*.5f,midiMute->getHeight()*.5f);
        cueViewFixtures::click(*oscMute,oscMute->getWidth()*.5f,oscMute->getHeight()*.5f);
        check(ui.show.columns[1].muted && ui.show.columns[2].muted,
              "Collapsed MIDI/OSC mute is not functional");
        check(ui.engine.getOscColumnSent(ui.show.columns[2].id)==0,
              "OSC LED counts untransmitted messages");
    });
    test("Row Play/Pause uses transport queue and preserves running PCM cursor",[] {
        auto show=makeShow();auto clip=std::make_shared<AudioClip>();
        clip->sampleRate=4;clip->samples.setSize(1,100);
        for(int i=0;i<100;++i)clip->samples.setSample(0,i,.1f);
        ClipMap clips;clips[show.rows[0].cells[0].id]=clip;
        AudioEngine engine;
        engine.publish(makeRenderState(show,clips));
        juce::AudioBuffer<float> out(2,1);
        check(engine.send(show.rows[0].id,true),"Initial row GO did not queue");
        engine.processBlock(out.getArrayOfWritePointers(),2,1,4);
        const int id=show.rows[0].cells[0].id;
        check(engine.getCellPlayback(id).playing,"Row did not start");
        check(engine.sendRowPauseToggle(show.rows[0].id),"Row pause did not queue");
        engine.processBlock(out.getArrayOfWritePointers(),2,1,4);
        const auto pause=engine.getCellPlayback(id);
        check(pause.paused && !pause.playing,"Row did not pause");
        engine.processBlock(out.getArrayOfWritePointers(),2,1,4);
        near(engine.getCellPlayback(id).positionSeconds,pause.positionSeconds);
        check(engine.sendRowPauseToggle(show.rows[0].id),"Row resume did not queue");
        engine.processBlock(out.getArrayOfWritePointers(),2,1,4);
        check(engine.getCellPlayback(id).playing && 
              engine.getCellPlayback(id).positionSeconds>pause.positionSeconds,
              "Row Play/Pause restarted a clip instead of resuming");
    });
    test("Cell cut/delete preserves target identity and clipboard, not source files",[] {
        MainComponent ui(false);
        addRow(ui.show,"Second");
        auto& original=ui.show.rows[0].cells[0];
        original.file="C:/test/example.wav";original.name="Footsteps";original.loop=true;
        const int id=original.id,dest=ui.show.rows[1].cells[0].id;
        check(ui.cutSelectedCell(),"Cut refused a selected cell");
        check(ui.show.rows[0].cells[0].id==id &&
              ui.show.rows[0].cells[0].file.isEmpty() &&
              !ui.show.rows[0].cells[0].loop,
              "Cut did not clear source cell generically");
        check(ui.select(1,0) && ui.pasteSelectedCell(),"Paste after cut failed");
        check(ui.show.rows[1].cells[0].id==dest &&
              ui.show.rows[1].cells[0].name=="Footsteps" &&
              ui.show.rows[1].cells[0].loop,
              "Cut clipboard or paste changed the destination identity");
        check(ui.keyPressed(juce::KeyPress(juce::KeyPress::backspaceKey)),
              "Backspace did not invoke generic cell delete");
        check(ui.show.rows[1].cells[0].id==dest && ui.show.rows[1].cells[0].name.isEmpty(),
              "Backspace deleted more than selected cell or failed to clear it");
    });
    test("Inspector sections are simultaneous; cell toggles persist through model",[] {
        MainComponent ui(false);
        auto* gain=controlsV3::named(ui,"cellGainDb");
        auto* name=controlsV3::named(ui,"cueName");
        auto* bpm=controlsV3::named(ui,"cueBpm");
        check(gain && gain->isVisible() && name && name->isVisible() &&
              bpm && bpm->isVisible(),"Inspector hides Q or Cell settings");
        ui.setCellLoop(0,0,true);
        ui.setCellFollowsRow(0,0,false);
        check(ui.show.rows[0].cells[0].loop && !ui.show.rows[0].cells[0].followsRow,
              "Inspector and cell menu toggles diverged");
        Show restored;
        check(decodeShow(encodeShow(ui.show),restored).wasOk() &&
              restored.rows[0].cells[0].loop &&
              !restored.rows[0].cells[0].followsRow,
              "Cell UI states do not survive reload");
        for(auto* child:ui.getChildren()) if(auto* button=dynamic_cast<juce::TextButton*>(child))
            check(!button->isVisible() || 
                  (button->getButtonText()!="Apply changes" &&
                   button->getButtonText()!="Play cell" &&
                   button->getButtonText()!="Stop"),
                  "Redundant Inspector transport or Apply still visible");
    });
}
