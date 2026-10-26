#pragma once
#include "GainMeterFader.h"
#include <cmath>
void mixerV8Tests() {
    test("v8 master PCM gain roundtrip and v7 migration to unity", [] {
        auto original=makeShow();
        original.masterGain=.37f;
        const auto encoded=encodeShow(original);
        near((int)juce::JSON::parse(encoded)["version"],10);
        Show saved;
        check(decodeShow(encoded,saved).wasOk(),"v8 master show refused");
        near(saved.masterGain,.37);
        near(saved.masterPan,0);
        check(!saved.masterMuted,"Master mute default changed");
        auto legacy=juce::JSON::parse(encoded);
        legacy.getDynamicObject()->setProperty("version",7);
        legacy.getDynamicObject()->removeProperty("masterGain");
        Show old;
        check(decodeShow(juce::JSON::toString(legacy),old).wasOk(),"v7 master migration failed");
        near(old.masterGain,1.0);
        near(old.masterPan,0.0);
        check(!old.masterMuted,"Legacy show must load unmuted");
        for (float invalid : {-1.0f,4.1f,std::numeric_limits<float>::infinity()}) {
            auto broken=makeShow(); broken.masterGain=invalid;
            check(validateShow(broken).failed(),"Invalid master accepted");
        }
        auto malformed=juce::JSON::parse(encoded);
        malformed.getDynamicObject()->setProperty("masterGain","too loud");
        Show safe; safe.name="Keep";
        check(decodeShow(juce::JSON::toString(malformed),safe).failed(),"Malformed v8 master accepted");
        check(safe.name=="Keep","Failed v8 parse mutated caller's show");
    });
    test("master pan/mute persist without changing existing v10 shows", [] {
        auto s=makeShow(); s.masterPan=-.35f; s.masterMuted=true;
        Show decoded;
        check(decodeShow(encodeShow(s),decoded).wasOk(),"New master controls failed to decode");
        near(decoded.masterPan,-.35); check(decoded.masterMuted,"Master mute was not persisted");
        auto json=juce::JSON::parse(encodeShow(s));
        json.getDynamicObject()->removeProperty("masterPan");
        json.getDynamicObject()->removeProperty("masterMuted");
        check(decodeShow(juce::JSON::toString(json),decoded).wasOk(),
            "Previously saved v10 document failed after adding optional master controls");
        near(decoded.masterPan,0); check(!decoded.masterMuted,"Optional master defaults are incorrect");
    });
    test("master pan/mute affect PCM after channel meters and keep voices playing", [] {
        auto show=makeShow();
        auto clip=std::make_shared<AudioClip>(); clip->sampleRate=4;
        clip->samples.setSize(2,8);
        for(int n=0;n<8;++n) { clip->samples.setSample(0,n,.4f); clip->samples.setSample(1,n,.2f); }
        ClipMap clips; clips[show.rows[0].cells[0].id]=clip;
        show.masterPan=.5f;
        auto state=makeRenderState(show,clips); state->trigger(show.rows[0].cells[0].id,false);
        juce::AudioBuffer<float> out(2,1); state->render(out.getArrayOfWritePointers(),2,1,4);
        near(out.getSample(0,0),.2); near(out.getSample(1,0),.2);
        near(state->columnMeters[0].second->left.load(),.4);
        show.masterMuted=true;
        auto muted=makeRenderState(show,clips); muted->trigger(show.rows[0].cells[0].id,false);
        muted->render(out.getArrayOfWritePointers(),2,1,4);
        near(out.getSample(0,0),0); near(out.getSample(1,0),0);
        check(muted->voices[0].playing,"Master mute must not stop playback");
        near(muted->columnMeters[0].second->left.load(),.4);
    });
    test("master audio scales summed PCM before saturation and does not change strip settings", [] {
        auto show=makeShow(); addColumn(show,ColumnType::audio,"Extra");
        auto clip=std::make_shared<AudioClip>(); clip->sampleRate=4;
        clip->samples.setSize(1,8);
        for (int i=0;i<8;++i) clip->samples.setSample(0,i,.8f);
        ClipMap clips;
        for (const auto& cell:show.rows[0].cells) clips[cell.id]=clip;
        auto state=makeRenderState(show,clips);
        state->trigger(show.rows[0].id,true);
        juce::AudioBuffer<float> out(2,1);
        state->masterGain=.25f;
        state->render(out.getArrayOfWritePointers(),2,1,4);
        near(out.getSample(0,0),.4); // (0.8 + 0.8) * 0.25, before clipping
        near(out.getSample(1,0),.4);
        near(state->columnMeters[0].second->peak.load(),.8);
        near(state->columnMeters[1].second->peak.load(),.8);
        state->masterGain=0.0f;
        state->render(out.getArrayOfWritePointers(),2,1,4);
        near(out.getSample(0,0),0);
        near(state->columnMeters[0].second->peak.load(),.8); // pre-master
        near(show.columns[0].gain,1); near(show.columns[1].gain,1);
    });
    test("master updates live output and keeps MIDI OSC render voices intact", [] {
        MainComponent ui(false);
        addColumn(ui.show,ColumnType::midi,"M");
        addColumn(ui.show,ColumnType::osc,"O");
        MidiClip midi; midi.lengthBeats=8; midi.notes={{0,2,60,70,1}};
        OscClip osc; osc.length=3; osc.events={{1,"/cue",{{OscValueType::integer,5,0,{}}}}};
        ui.show.rows[0].cells[1].midi=midi;
        ui.show.rows[0].cells[2].osc=osc;
        auto clip=std::make_shared<AudioClip>(); clip->sampleRate=4;
        clip->samples.setSize(1,16);
        for (int i=0;i<16;++i) clip->samples.setSample(0,i,.4f);
        const int columnId=ui.show.columns[0].id;
        ui.clips[ui.show.rows[0].cells[0].id]=clip;
        ui.engine.publish(makeRenderState(ui.show,ui.clips));
        ui.engine.connected.store(true);
        // Exercise the transport directly: a real UI GO would require an
        // external MIDI port, which is intentionally unavailable in CI.
        check(ui.engine.send(ui.show.rows[0].id,true),"Failed to queue mixed-media GO");
        juce::AudioBuffer<float> output(2,1);
        auto render=[&] { ui.engine.collectRetired();
            ui.engine.processBlock(output.getArrayOfWritePointers(),2,1,4);
            return output.getSample(0,0);
        };
        near(render(),.4);
        const float previousGain=ui.show.columns[0].gain;
        ui.setMasterAudio(.5f);
        near(render(),.2);
        check(ui.show.columns[0].gain==previousGain,"Master modified channel fader");
        near(ui.engine.getColumnPeak(columnId),.4);
        ui.setMasterAudio(0);
        near(render(),0);
        near(ui.engine.getColumnPeak(columnId),.4); // unaffected by master mute
        check(ui.engine.activeVoices.load()==3,"Master mute should not stop audio/MIDI/OSC voices");
        check(ui.hasUnsavedChanges(),"Master change not marked dirty");
    });
    test("compact audio strips have vertical faders, rotary pan and independent peak meters", [] {
        MainComponent ui(false); ui.setSize(1500,900);
        addColumn(ui.show,ColumnType::midi,"M");
        addColumn(ui.show,ColumnType::osc,"O");
        auto* utilities=cueViewFixtures::find<ColumnUtilities>(ui);
        utilities->refresh(); ui.resized();
        auto* master=dynamic_cast<GainMeterFader*>(controlsV3::named(ui,"masterAudioDb"));
        check(master && master->getGainDb()==0,"Integrated master fader missing or not at 0 dB");
        auto* masterPan=dynamic_cast<juce::Slider*>(controlsV3::named(ui,"masterPan"));
        auto* masterMute=dynamic_cast<juce::ToggleButton*>(controlsV3::named(ui,"masterMute"));
        check(masterPan && masterMute,"Master pan and mute controls missing");
        auto* audio=utilities->getChildComponent(0);
        auto* midiStrip=utilities->getChildComponent(1);
        auto* oscStrip=utilities->getChildComponent(2);
        auto* fader=dynamic_cast<GainMeterFader*>(controlsV3::named(*audio,"columnGainDb"));
        auto* pan=dynamic_cast<juce::Slider*>(controlsV3::named(*audio,"columnPan"));
        check(fader && fader->getNumChildComponents()==0,"Stereo fader/meter must be one component, not layered widgets");
        check(pan && pan->getSliderStyle()==juce::Slider::RotaryVerticalDrag,"Pan not rotary");
        check(fader->getHeight()>90 && utilities->getHeight()>=180,"Compact mixer strip has insufficient height");
        check(!controlsV3::named(*midiStrip,"columnGainDb")->isVisible(),"MIDI strip displays PCM controls");
        check(!controlsV3::named(*oscStrip,"columnGainDb")->isVisible(),"OSC strip displays PCM controls");
        master->setGainDb(-6.0f,true);
        near(ui.show.masterGain,std::pow(10.,-6./20.));
        check(ui.hasUnsavedChanges(),"Master UI failed to mark show dirty");
        masterPan->setValue(-.4,juce::sendNotificationSync);
        near(ui.show.masterPan,-.4);
        masterMute->triggerClick();
        check(ui.show.masterMuted,"Master mute UI did not reach the model");
    });
}
