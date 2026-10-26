#pragma once
#include "GainMeterFader.h"

void gainMeterFaderTests() {
    test("integrated dB geometry round-trips gain and meter values without drift", [] {
        const float top=12, bottom=124;
        for (const float db : {-60.0f,-48.0f,-36.0f,-24.0f,-18.0f,-12.0f,-6.0f,0.0f,6.0f,12.0f}) {
            const auto y=GainMeterFader::yForDb(db,top,bottom);
            near(GainMeterFader::dbForY(y,top,bottom),db);
            check(y>=top && y<=bottom,"Gain/meter label drawn outside calibrated range");
        }
        check(GainMeterFader::yForDb(12,top,bottom)
            < GainMeterFader::yForDb(0,top,bottom),"Positive gain drawn below unity");
        check(GainMeterFader::yForDb(-60,top,bottom)
            > GainMeterFader::yForDb(-6,top,bottom),"Floor drawn above strong signal");
        near(GainMeterFader::dbForY(-100,top,bottom),12);
        near(GainMeterFader::dbForY(500,top,bottom),-60);
    });
    test("GainMeterFader owns one mouse/keyboard control and preserves saved dB range", [] {
        GainMeterFader component;
        component.setSize(92,148);
        check(component.getNumChildComponents()==0,"Gain/meter is implemented as stacked child widgets");
        int callbacks=0; float delivered=0;
        component.onGainChanged=[&](float db) { ++callbacks; delivered=db; };
        component.setGainDb(-6.3f,true);
        near(component.getGainDb(),-6.3);
        near(delivered,-6.3);
        check(callbacks==1,"Drag/edit callback did not fire exactly once");
        component.setGainDb(-6.3f,true);
        check(callbacks==1,"Identical value produced unnecessary render state rebuild");
        component.setGainDb(0);
        near(component.getGainDb(),0);
        check(callbacks==1,"UI synchronisation must not notify the engine");
        component.setGainDb(900,true);
        near(component.getGainDb(),12);
        component.setGainDb(-900,true);
        near(component.getGainDb(),-60);
        check(component.getGainDb()>=GainMeterFader::minDb,"Gain fell below floor");
        near(juce::Decibels::decibelsToGain(-60.0f,-60.0f),0);
    });
    test("stereo meter displays independent L/R dBFS, silence and decay/hold", [] {
        GainMeterFader component;
        component.setSize(92,148);
        component.setStereoPeaks(1.0f,0.1f);
        near(component.getDisplayedPeakDb(0),0.0);
        near(component.getDisplayedPeakDb(1),-20.0);
        const float held=component.getHeldPeakDb(0);
        for (int i=0;i<5;++i) component.setStereoPeaks(0,0);
        check(component.getDisplayedPeakDb(0)<0 && component.getDisplayedPeakDb(1)<-20,
              "Stereo meters must decay independently when silent");
        near(component.getHeldPeakDb(0),held);
        for (int i=0;i<100;++i) component.setStereoPeaks(0,0);
        near(component.getDisplayedPeakDb(0),-60);
        near(component.getDisplayedPeakDb(1),-60);
        near(component.getHeldPeakDb(0),-60);
        component.setStereoPeaks(0.00000001f,0.95f);
        near(component.getDisplayedPeakDb(0),-60);
        check(component.getDisplayedPeakDb(1)>-1.0f,"Near-clipping peak not displayed");
    });
    test("pre-master stereo telemetry measures actual summed columns after gain/pan", [] {
        auto show=makeShow();
        addRow(show,"Layer");
        auto source=std::make_shared<AudioClip>();
        source->sampleRate=4;
        source->samples.setSize(2,8);
        for (int i=0;i<8;++i) {
            source->samples.setSample(0,i,.20f);
            source->samples.setSample(1,i,.40f);
        }
        ClipMap clips;
        clips[show.rows[0].cells[0].id]=source;
        clips[show.rows[1].cells[0].id]=source;
        show.masterGain=.1f;
        show.columns[0].gain=.5f;
        show.columns[0].pan=0.5f; // side gains: left x0.5, right x1.0
        auto state=makeRenderState(show,clips);
        for (const auto& row:show.rows)
            state->trigger(row.cells[0].id,false);
        juce::AudioBuffer<float> output(2,1);
        state->render(output.getArrayOfWritePointers(),2,1,4);
        // two clips: left 2*.2*.5*.5=.1; right 2*.4*.5=.4
        near(state->columnMeters[0].second->left.load(),.1);
        near(state->columnMeters[0].second->right.load(),.4);
        near(state->columnMeters[0].second->peak.load(),.4);
        near(output.getSample(0,0),.01); // Master is post-meter
        near(output.getSample(1,0),.04);
        show.columns[0].muted=true;
        auto muted=makeRenderState(show,clips);
        muted->trigger(show.rows[0].cells[0].id,false);
        muted->render(output.getArrayOfWritePointers(),2,1,4);
        near(muted->columnMeters[0].second->left.load(),0);
        near(muted->columnMeters[0].second->right.load(),0);
    });
    test("simultaneous opposite polarity voices cancel in the actual column meter", [] {
        auto show=makeShow(); addRow(show,"Opposite");
        auto a=std::make_shared<AudioClip>(); a->sampleRate=4; a->samples.setSize(2,8);
        auto b=std::make_shared<AudioClip>(); b->sampleRate=4; b->samples.setSize(2,8);
        for (int i=0;i<8;++i) {
            a->samples.setSample(0,i,.6f); a->samples.setSample(1,i,.3f);
            b->samples.setSample(0,i,-.6f); b->samples.setSample(1,i,-.3f);
        }
        ClipMap clips; clips[show.rows[0].cells[0].id]=a; clips[show.rows[1].cells[0].id]=b;
        auto state=makeRenderState(show,clips);
        for (const auto& row:show.rows) state->trigger(row.cells[0].id,false);
        juce::AudioBuffer<float> audio(2,1);
        state->render(audio.getArrayOfWritePointers(),2,1,4);
        near(audio.getSample(0,0),0);
        near(audio.getSample(1,0),0);
        near(state->columnMeters[0].second->left.load(),0);
        near(state->columnMeters[0].second->right.load(),0);
    });
    test("AudioEngine publishes separate peak channels to UI without master affecting taps", [] {
        auto show=makeShow();
        auto source=std::make_shared<AudioClip>();
        source->sampleRate=4; source->samples.setSize(2,8);
        for (int i=0;i<8;++i) {
            source->samples.setSample(0,i,.8f);
            source->samples.setSample(1,i,.1f);
        }
        ClipMap clips; clips[show.rows[0].cells[0].id]=source;
        const int id=show.columns[0].id;
        show.masterGain=0;
        AudioEngine engine;
        engine.publish(makeRenderState(show,clips));
        check(engine.send(show.rows[0].cells[0].id,false),"Failed to enqueue test voice");
        juce::AudioBuffer<float> audio(2,1);
        engine.processBlock(audio.getArrayOfWritePointers(),2,1,4);
        auto peaks=engine.getColumnStereoPeak(id);
        near(peaks[0],.8);
        near(peaks[1],.1);
        near(engine.getColumnPeak(id),.8);
        near(audio.getSample(0,0),0);
        near(audio.getSample(1,0),0);
        auto missing=engine.getColumnStereoPeak(-12);
        near(missing[0],0); near(missing[1],0);
    });
}
