#pragma once
#include "MainComponent.h"
#include "GainMeterFader.h"

void utilityPolishTests() {
    test("Collapsed audio utility keeps functional compact gain and mute",[] {
        MainComponent ui(false);
        ui.setSize(1500,900);
        auto* utility=cueViewFixtures::find<ColumnUtilities>(ui);
        utility->refresh();ui.resized();
        ui.toggleColumnCollapsed(0);
        utility->refresh();ui.resized();
        auto* strip=utility->getChildComponent(0)->getChildComponent(0);
        auto* gain=dynamic_cast<GainMeterFader*>(controlsV3::named(*strip,"columnGainDb"));
        auto* mute=dynamic_cast<juce::ToggleButton*>(controlsV3::named(*strip,"columnMute"));
        check(gain && mute && gain->isVisible() && mute->isVisible() &&
              gain->isCompact() && gain->getWidth()<60,
              "Collapsed audio utility lost its compact gain/mute");
        gain->setGainDb(-6.0f,true);
        near(ui.show.columns[0].gain,std::pow(10.0,-6.0/20.0));
        check(ui.show.columns[0].collapsed,"Gain change expanded the column");
        cueViewFixtures::click(*mute,mute->getWidth()*.5f,mute->getHeight()*.5f);
        check(ui.show.columns[0].muted && ui.hasUnsavedChanges(),
              "Collapsed audio mute did not update the actual column");
    });
    test("Audio OUT is a paired compact control and MIDI OUT retains full width",[] {
        MainComponent ui(false);ui.setSize(1500,900);
        addColumn(ui.show,ColumnType::midi,"M");
        auto* utility=cueViewFixtures::find<ColumnUtilities>(ui);
        utility->refresh();ui.resized();
        auto* audio=utility->getChildComponent(0)->getChildComponent(0);
        auto* midi=utility->getChildComponent(0)->getChildComponent(1);
        auto* audioOut=dynamic_cast<juce::ComboBox*>(controlsV3::named(*audio,"columnOutput"));
        auto* midiOut=dynamic_cast<juce::ComboBox*>(controlsV3::named(*midi,"columnOutput"));
        auto* prepost=dynamic_cast<juce::TextButton*>(controlsV3::named(*audio,"columnDirectPrePost"));
        check(audioOut && midiOut && prepost,"Routing controls absent");
        check(midiOut->getWidth()>audioOut->getWidth()+40,
              "MIDI OUT incorrectly retains the compact Audio OUT width");
        check(audioOut->getY()==prepost->getY() && audioOut->getY()>4,
              "Audio OUT and PRE/POST are not aligned slightly below the panel top");
    });
    test("Unified limiter differentiates OFF, ON and gain reduction",[] {
        MainComponent ui(false);
        auto* button=dynamic_cast<LimiterToggle*>(controlsV3::named(ui,"masterSafetyLimiter"));
        check(button!=nullptr && button->getToggleState(),
              "Master limiter unified button missing or initially bypassed");
        button->setReducing(true);
        check(button->isReducing() && button->getToggleState(),"Active reduction state missing");
        ui.setSafetyLimiter(false);
        check(!button->getToggleState(),"Limiter cannot be bypassed from model");
        button->setReducing(false);
        check(!button->isReducing(),"Limiter reduction indicator cannot return idle");
    });
    test("Data output mute persists without silencing MIDI/OSC transport",[] {
        auto show=makeShow();
        addColumn(show,ColumnType::midi,"M");
        addColumn(show,ColumnType::osc,"O");
        MidiClip midi; midi.lengthBeats=8; midi.notes={{0,6,60,90,1}};
        OscClip osc; osc.length=3; osc.events={{1,"/cue",{{OscValueType::integer,5,0,{}}}},
                            {2.5,"/cue",{{OscValueType::integer,6,0,{}}}}};
        show.rows[0].cells[1].midi=midi;
        show.rows[0].cells[2].osc=osc;
        show.columns[1].muted=true;
        show.columns[2].muted=true;
        Show restored;
        check(decodeShow(encodeShow(show),restored).wasOk(),"Muting MIDI/OSC broke serialization");
        check(restored.columns[1].muted && restored.columns[2].muted,
              "MIDI/OSC mute not persisted");
        struct MidiCounter final: MidiEventSink {
            int messages=0;
            void emit(int,int,int,int,int) noexcept override {++messages;}
        } midiSink;
        struct OscCounter final: OscEventSink {
            int messages=0;
            void emit(int,const uint8_t*,unsigned,int) noexcept override {++messages;}
        } oscSink;
        auto state=makeRenderState(show,{});
        auto* midiVoice=static_cast<Voice*>(nullptr);
        auto* oscVoice=static_cast<Voice*>(nullptr);
        for(auto& voice:state->voices) {
            if(voice.isMidi) {midiVoice=&voice;voice.midiSink=&midiSink;voice.midiRoute=0;}
            if(voice.isOsc) {oscVoice=&voice;voice.oscSink=&oscSink;voice.oscRoute=0;}
        }
        check(midiVoice && oscVoice,"MIDI/OSC voices were not prepared");
        state->trigger(show.rows[0].id,true);
        juce::AudioBuffer<float> out(2,8);
        state->render(out.getArrayOfWritePointers(),2,8,4);
        check(midiSink.messages==0 && oscSink.messages==0,
              "Muted data columns still transmitted events");
        check(midiVoice->position>0 && oscVoice->position>0,
              "Data output mute stopped the cue timeline");
        midiVoice->columnMuted=false;
        midiVoice->midiNeedsSync=true;
        oscVoice->columnMuted=false;
        state->render(out.getArrayOfWritePointers(),2,8,4);
        check(midiSink.messages>0,
              "MIDI output remained muted after unmute/resync");
        check(oscSink.messages>0,
              "OSC output remained muted after unmute");
    });
    test("Data mute control affects only MIDI and OSC utility strips",[] {
        MainComponent ui(false);ui.setSize(1500,900);
        addColumn(ui.show,ColumnType::midi,"M");
        addColumn(ui.show,ColumnType::osc,"O");
        auto* utility=cueViewFixtures::find<ColumnUtilities>(ui);
        utility->refresh();ui.resized();
        auto* audio=utility->getChildComponent(0)->getChildComponent(0);
        auto* midi=utility->getChildComponent(0)->getChildComponent(1);
        auto* osc=utility->getChildComponent(0)->getChildComponent(2);
        check(!controlsV3::named(*audio,"columnDataOutputMute")->isVisible(),
              "Audio has unexpected data mute");
        auto* muteMidi=dynamic_cast<juce::ToggleButton*>(controlsV3::named(*midi,"columnDataOutputMute"));
        auto* muteOsc=dynamic_cast<juce::ToggleButton*>(controlsV3::named(*osc,"columnDataOutputMute"));
        auto* activity=dynamic_cast<MidiActivityLed*>(controlsV3::named(*midi,"columnMidiOutputLed"));
        check(muteMidi && muteOsc && muteMidi->isVisible() && muteOsc->isVisible() &&
              activity && activity->isVisible() && !activity->isActive(),
              "MIDI/OSC utility output controls absent");
        cueViewFixtures::click(*muteMidi,muteMidi->getWidth()*.5f,muteMidi->getHeight()*.5f);
        cueViewFixtures::click(*muteOsc,muteOsc->getWidth()*.5f,muteOsc->getHeight()*.5f);
        check(ui.show.columns[1].muted && ui.show.columns[2].muted,
              "UI data mute did not reach the actual model");
        check(ui.engine.getMidiColumnSent(ui.show.columns[1].id)==0,
              "MIDI output activity counted messages without device dispatch");
    });

    test("Expanded Audio utility restores the calibrated 92px fader and original Pan geometry",[] {
        MainComponent ui(false);ui.setSize(1440,900);
        auto* utility=cueViewFixtures::find<ColumnUtilities>(ui);
        check(utility,"Utility container missing");
        auto* first=utility->getChildComponent(0)->getChildComponent(0);
        auto* fader=dynamic_cast<GainMeterFader*>(
            controlsV3::named(*first,"columnGainDb"));
        auto* pan=controlsV3::named(*first,"columnPan");
        auto* mute=controlsV3::named(*first,"columnMute");
        check(fader && pan && mute,"Expanded Audio strip controls missing");
        check(CueGrid::columnWidth==152 && first->getWidth()==152 &&
              fader->getX()==7 && fader->getWidth()==92 &&
              pan->getWidth()==49 && pan->getX()==92 &&
              mute->getX()==75 && mute->getRight()<=first->getWidth(),
              "The original safe spacing of fader, meter, Pan and Mute was not restored");
        check(mute->getX()>=fader->getX()+fader->getWidth()-24,
              "Audio Mute unexpectedly moved behind the calibrated meter region");
    });
    test("Compact Master Mute is centred below Pan and not over its label",[] {
        MainComponent ui(false);ui.setSize(1440,900);
        auto* util=cueViewFixtures::find<ColumnUtilities>(ui);
        auto* masterPan=controlsV3::named(*util,"masterPan");
        auto* masterMute=controlsV3::named(*util,"masterMute");
        auto* output=controlsV3::named(*util,"masterMainOutput");
        check(masterPan && masterMute && output,"Master Audio controls missing");
        check(std::abs(masterPan->getBounds().getCentreX()-
                       masterMute->getBounds().getCentreX())<=2 &&
              masterMute->getY()>=masterPan->getBottom()+55 &&
              masterMute->getRight()<CueGrid::nameWidth &&
              output->getRight()<masterPan->getX(),
              "Master Audio Mute is not centered beneath Pan or overlaps output");
    });

}
