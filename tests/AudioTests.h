#pragma once
#include "AudioEngine.h"
#include <limits>
inline void audioTests() {
    test("completed PauseResume and Continue rows replay offset and clock; true pause preserves cursor", [&] {
        for (auto behaviour : {Behaviour::pauseResume,Behaviour::continuePlaying}) {
            Show show; addColumn(show,ColumnType::audio,""); addRow(show,"");
            auto& row=show.rows[0]; row.cells[0].startSeconds=0.5;
            row.overrideBehaviour=true; row.behaviour=behaviour;
            auto clip=std::make_shared<AudioClip>(); clip->sampleRate=4; clip->samples.setSize(1,4);
            for(int i=0;i<4;++i) clip->samples.setSample(0,i,0.1f*(i+1));
            ClipMap clips; clips[row.cells[0].id]=clip; auto state=makeRenderState(show,clips);
            juce::AudioBuffer<float> out(2,6); state->trigger(row.id,true);
            state->render(out.getArrayOfWritePointers(),2,6,4); near(out.getSample(0,5),0.4);
            check(!state->rows[0].playing,"Completed row still running");
            state->trigger(row.id,true); near(state->rows[0].seconds,0);
            state->render(out.getArrayOfWritePointers(),2,3,4);
            near(out.getSample(0,0),0); near(out.getSample(0,1),0); near(out.getSample(0,2),0.1);
            if(behaviour==Behaviour::pauseResume) {
                state->trigger(row.id,true); state->render(out.getArrayOfWritePointers(),2,1,4);
                near(out.getSample(0,0),0); near(state->rows[0].seconds,0.75);
            }
            state->trigger(row.id,true); state->render(out.getArrayOfWritePointers(),2,1,4);
            near(out.getSample(0,0),0.2); near(state->rows[0].seconds,1.0);
        }
    });
    test("seek moves actual PCM while running and paused and can replay after end", [&] {
        Show show; addColumn(show,ColumnType::audio,""); addRow(show,"");
        auto& row=show.rows[0]; row.overrideBehaviour=true; row.behaviour=Behaviour::pauseResume;
        row.cells[0].startSeconds=0.5;
        auto media=std::make_shared<AudioClip>(); media->sampleRate=4; media->samples.setSize(1,4);
        for(int i=0;i<4;++i) media->samples.setSample(0,i,0.1f*(i+1));
        ClipMap clips; clips[row.cells[0].id]=media;
        auto state=makeRenderState(show,clips); juce::AudioBuffer<float> out(2,1);
        state->trigger(row.id,true); state->seekRow(row.id,0.75);
        state->render(out.getArrayOfWritePointers(),2,1,4); near(out.getSample(0,0),0.2);
        state->trigger(row.id,true); state->seekRow(row.id,1.0);
        state->render(out.getArrayOfWritePointers(),2,1,4); near(out.getSample(0,0),0);
        near(state->rows[0].seconds,1.0);
        state->trigger(row.id,true); state->render(out.getArrayOfWritePointers(),2,1,4);
        near(out.getSample(0,0),0.3);
        state->render(out.getArrayOfWritePointers(),2,1,4); near(out.getSample(0,0),0.4);
        check(!state->rows[0].playing,"Ended row remained running");
        state->seekRow(row.id,0.5); state->trigger(row.id,true);
        state->render(out.getArrayOfWritePointers(),2,1,4); near(out.getSample(0,0),0.1);
        state->trigger(row.id,true,true); state->trigger(row.id,true);
        state->render(out.getArrayOfWritePointers(),2,1,4); near(out.getSample(0,0),0);
    });
    test("stopped row retrigger respects a new seek once; direct starts immediately", [&] {
        Show show; addColumn(show,ColumnType::audio,""); addRow(show,"");
        auto& row=show.rows[0]; row.cells[0].startSeconds=0.5;
        auto media=std::make_shared<AudioClip>(); media->sampleRate=4; media->samples.setSize(1,4);
        for(int i=0;i<4;++i) media->samples.setSample(0,i,0.1f*(i+1));
        ClipMap clips; clips[row.cells[0].id]=media;
        AudioEngine engine; engine.monitoredRowId.store(row.id); engine.publish(makeRenderState(show,clips));
        juce::AudioBuffer<float> out(2,1);
        check(engine.seekRow(row.id,0.75),"Seek rejected"); engine.send(row.id,true);
        engine.processBlock(out.getArrayOfWritePointers(),2,1,4); near(out.getSample(0,0),0.2);
        near(engine.playheadSeconds.load(),1.0); check(engine.rowPlaying.load(),"Monitor not playing");
        engine.send(row.id,true); engine.processBlock(out.getArrayOfWritePointers(),2,1,4);
        near(out.getSample(0,0),0); near(engine.playheadSeconds.load(),0.25);
        engine.send(row.cells[0].id,false); engine.processBlock(out.getArrayOfWritePointers(),2,1,4);
        near(out.getSample(0,0),0.1);
        engine.panic(); engine.processBlock(out.getArrayOfWritePointers(),2,1,4);
        near(out.getSample(0,0),0); near(engine.playheadSeconds.load(),0);
        check(!engine.rowPlaying.load(),"Panic monitor remained running");
    });
    test("MP3 decodes real mono PCM", [&] {
        std::shared_ptr<const AudioClip> decoded;
        auto file=juce::File(__FILE__).getSiblingFile("fixtures").getChildFile("tone.mp3");
        check(loadClip(file,decoded).wasOk(),"MP3 fixture decode failed");
        check(decoded && decoded->samples.getNumSamples()>0,"MP3 has no PCM");
        check(decoded->samples.getMagnitude(0,decoded->samples.getNumSamples())>0.01f,"MP3 decoded silence");
    });
    test("row override pauses participating voices together and direct trigger keeps cell behaviour", [&] {
        Show show; addColumn(show,ColumnType::audio,""); addColumn(show,ColumnType::audio,""); addRow(show,"");
        auto& row=show.rows[0]; row.overrideBehaviour=true; row.behaviour=Behaviour::pauseResume;
        auto media=std::make_shared<AudioClip>(); media->sampleRate=4; media->samples.setSize(1,4);
        media->samples.clear(); media->samples.setSample(0,0,0.2f);
        ClipMap clips; for(auto& c:row.cells) clips[c.id]=media;
        auto state=makeRenderState(show,clips); state->trigger(row.cells[0].id,false);
        state->trigger(row.id,true);
        check(!state->voices[0].playing && !state->voices[1].playing,"Row pause left voices out of sync");
        state->trigger(row.id,true);
        check(state->voices[0].playing && state->voices[1].playing,"Row resume left voices out of sync");
        juce::AudioBuffer<float> out(2,1); state->render(out.getArrayOfWritePointers(),2,1,4);
        state->trigger(row.cells[0].id,false); near(state->voices[0].position,0);
    });
    test("row mute silences without stopping and row loop wraps", [&] {
        Show show; addColumn(show,ColumnType::audio,""); addRow(show,"");
        auto& row=show.rows[0]; row.muted=true; row.loop=true;
        auto media=std::make_shared<AudioClip>(); media->sampleRate=4; media->samples.setSize(1,4);
        for(int i=0;i<4;++i) media->samples.setSample(0,i,0.4f);
        ClipMap clips; clips[row.cells[0].id]=media;
        auto state=makeRenderState(show,clips); state->trigger(row.id,true);
        juce::AudioBuffer<float> out(2,6); state->render(out.getArrayOfWritePointers(),2,6,4);
        near(out.getMagnitude(0,6),0); check(state->voices[0].playing,"Row loop stopped");
        near(state->voices[0].position,2);
    });
    test("row GO honours timeline offset and row gain", [&] {
        Show show; addColumn(show,ColumnType::audio,""); addRow(show,"");
        auto& row=show.rows[0]; row.gain=0.5f; row.cells[0].startSeconds=0.5;
        auto media=std::make_shared<AudioClip>(); media->sampleRate=4; media->samples.setSize(1,4);
        for(int i=0;i<4;++i) media->samples.setSample(0,i,0.4f);
        ClipMap clips; clips[row.cells[0].id]=media;
        auto state=makeRenderState(show,clips); state->trigger(row.id,true);
        juce::AudioBuffer<float> out(2,4); state->render(out.getArrayOfWritePointers(),2,4,4);
        near(out.getSample(0,0),0); near(out.getSample(0,1),0); near(out.getSample(0,2),0.2);
    });
    test("fractional sample offset never starts before its timeline time", [&] {
        Show show; addColumn(show,ColumnType::audio,""); addRow(show,"");
        auto& row=show.rows[0]; row.cells[0].startSeconds=0.1;
        auto media=std::make_shared<AudioClip>(); media->sampleRate=4; media->samples.setSize(1,4);
        for(int i=0;i<4;++i) media->samples.setSample(0,i,0.2f);
        ClipMap clips; clips[row.cells[0].id]=media; auto state=makeRenderState(show,clips);
        state->trigger(row.id,true); juce::AudioBuffer<float> out(2,2);
        state->render(out.getArrayOfWritePointers(),2,2,4);
        near(out.getSample(0,0),0); near(out.getSample(0,1),0.2);
    });
    auto clip = std::make_shared<AudioClip>(); clip->samples.setSize(1, 4); clip->sampleRate = 4;
    for (int i = 0; i < 4; ++i) clip->samples.setSample(0, i, 0.1f * (i + 1));
    auto fixture = [&] { RenderState s; Voice v; v.cellId = 2; v.rowId = 1; v.clip = clip; v.gain = 1; s.voices.push_back(v); return s; };
    test("queued GO survives later parameter snapshots with latest gain", [&] {
        AudioEngine engine; juce::AudioBuffer<float> out(2,1);
        engine.publish(std::make_unique<RenderState>(fixture())); engine.send(2,false);
        auto changed=std::make_unique<RenderState>(fixture()); changed->voices[0].gain=0.5f;
        engine.publish(std::move(changed));
        engine.processBlock(out.getArrayOfWritePointers(),2,1,4); near(out.getSample(0,0),0.05);
    });
    test("structural publication invalidates old queued GO even when followed by parameter publication", [&] {
        AudioEngine engine; juce::AudioBuffer<float> out(2,1);
        engine.publish(std::make_unique<RenderState>(fixture())); engine.send(2,false);
        engine.publish(std::make_unique<RenderState>(fixture()),false);
        engine.publish(std::make_unique<RenderState>(fixture()));
        engine.processBlock(out.getArrayOfWritePointers(),2,1,4); near(out.getSample(0,0),0);
        engine.send(2,false); engine.processBlock(out.getArrayOfWritePointers(),2,1,4); near(out.getSample(0,0),0.1);
    });
    test("structural boundary survives replacement of its pending snapshot", [&] {
        AudioEngine engine; juce::AudioBuffer<float> out(2,1);
        engine.publish(std::make_unique<RenderState>(fixture())); engine.send(2,false);
        engine.processBlock(out.getArrayOfWritePointers(),2,1,4); near(out.getSample(0,0),0.1);
        engine.publish(std::make_unique<RenderState>(fixture()),false);
        engine.publish(std::make_unique<RenderState>(fixture()));
        engine.processBlock(out.getArrayOfWritePointers(),2,1,4); near(out.getSample(0,0),0);
    });
    test("mixed row behaviours let paused cells resume while retrigger cells run", [&] {
        auto state=fixture(); state.voices[0].behaviour=Behaviour::pauseResume;
        auto other=state.voices[0]; other.cellId=3; other.behaviour=Behaviour::retrigger;
        state.voices.push_back(other);
        state.trigger(1,true); state.trigger(1,true);
        check(!state.voices[0].playing && state.voices[1].playing,"Mixed behaviour pause failed");
        state.trigger(1,true);
        check(state.voices[0].playing && state.voices[1].playing,"Mixed behaviour resume failed");
    });
    test("very large finite seek cannot overflow PCM position", [&] {
        auto state=fixture(); state.seekRow(1,std::numeric_limits<double>::max());
        check(std::isfinite(state.voices[0].position),"Seek overflowed PCM cursor");
        state.voices[0].loop=true; state.seekRow(1,std::numeric_limits<double>::max());
        check(std::isfinite(state.voices[0].position),"Loop seek overflowed PCM cursor");
    });
    test("parameter publication preserves the actual playback cursor", [&] {
        AudioEngine engine; juce::AudioBuffer<float> out(2,1);
        engine.publish(std::make_unique<RenderState>(fixture())); engine.send(2,false);
        engine.processBlock(out.getArrayOfWritePointers(),2,1,4); near(out.getSample(0,0),0.1);
        auto next=std::make_unique<RenderState>(fixture()); next->voices[0].gain=0.5f;
        engine.publish(std::move(next)); engine.processBlock(out.getArrayOfWritePointers(),2,1,4);
        near(out.getSample(0,0),0.1);
    });
    test("behaviour edits preserve a paused cursor for Continue", [&] {
        AudioEngine engine; juce::AudioBuffer<float> out(2,1);
        auto initial=std::make_unique<RenderState>(fixture()); initial->voices[0].behaviour=Behaviour::pauseResume;
        engine.publish(std::move(initial)); engine.send(2,false);
        engine.processBlock(out.getArrayOfWritePointers(),2,1,4);
        engine.send(2,false); engine.processBlock(out.getArrayOfWritePointers(),2,1,4); near(out.getSample(0,0),0);
        auto next=std::make_unique<RenderState>(fixture()); next->voices[0].behaviour=Behaviour::continuePlaying;
        engine.publish(std::move(next)); engine.send(2,false);
        engine.processBlock(out.getArrayOfWritePointers(),2,1,4); near(out.getSample(0,0),0.2);
    });
    test("structural publication and replaced media never inherit playing state", [&] {
        AudioEngine engine; juce::AudioBuffer<float> out(2,1);
        engine.publish(std::make_unique<RenderState>(fixture())); engine.send(2,false);
        engine.processBlock(out.getArrayOfWritePointers(),2,1,4);
        engine.publish(std::make_unique<RenderState>(fixture()),false);
        engine.processBlock(out.getArrayOfWritePointers(),2,1,4); near(out.getSample(0,0),0);
        engine.collectRetired(); engine.send(2,false);
        engine.processBlock(out.getArrayOfWritePointers(),2,1,4);
        auto changed=std::make_unique<RenderState>(fixture());
        changed->voices[0].clip=std::make_shared<AudioClip>(*clip);
        engine.publish(std::move(changed)); engine.processBlock(out.getArrayOfWritePointers(),2,1,4);
        near(out.getSample(0,0),0); check(engine.activeVoices.load()==0,"Replacement media auto-started");
    });
    test("audio plays actual PCM on routed outputs and ends", [&] {
        auto s = fixture(); s.voices[0].output = 2; s.trigger(2, false);
        juce::AudioBuffer<float> out(4, 6); s.render(out.getArrayOfWritePointers(), 4, 6, 4);
        near(out.getSample(0,0), 0); near(out.getSample(2,0), 0.1); near(out.getSample(3,3), 0.4);
        near(out.getSample(2,4), 0); check(!s.voices[0].playing, "Voice did not end");
    });
    test("resampling preserves duration without a BPM dependency", [&] {
        auto s = fixture(); s.trigger(2,false); juce::AudioBuffer<float> out(2, 10);
        s.render(out.getArrayOfWritePointers(),2,10,8);
        near(out.getSample(0,1),0.15); near(out.getSample(0,6),0.4); near(out.getSample(0,8),0);
    });
    test("row trigger excludes opted out cells but direct trigger works", [&] {
        auto s = fixture(); s.voices[0].followsRow = false; s.trigger(1,true);
        check(!s.voices[0].playing,"Excluded cell started"); s.trigger(2,false);
        check(s.voices[0].playing,"Direct trigger ignored");
    });
    test("loop wraps and Panic silences next block", [&] {
        auto s = fixture(); s.voices[0].loop = true; s.trigger(2,false);
        juce::AudioBuffer<float> out(2,6); s.render(out.getArrayOfWritePointers(),2,6,4);
        near(out.getSample(0,4),0.1); s.panic(); s.render(out.getArrayOfWritePointers(),2,6,4);
        near(out.getMagnitude(0,6),0); near(s.voices[0].position,0);
    });
    test("pause resumes at position, continue does not restart, stop resets", [&] {
        auto s = fixture(); auto& v = s.voices[0]; v.behaviour = Behaviour::pauseResume;
        juce::AudioBuffer<float> out(2,1); s.trigger(2,false); s.render(out.getArrayOfWritePointers(),2,1,4);
        s.trigger(2,false); check(!v.playing,"Pause ignored"); near(v.position,1);
        s.trigger(2,false); s.render(out.getArrayOfWritePointers(),2,1,4); near(out.getSample(0,0),0.2);
        v.behaviour = Behaviour::continuePlaying; s.trigger(2,false); near(v.position,2);
        s.trigger(2,false,true); check(!v.playing,"Stop ignored"); near(v.position,0);
    });
    test("missing and non WAV imports fail without replacing clip", [&] {
        std::shared_ptr<const AudioClip> existing = clip;
        check(loadClip(juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("absent-cuelist.wav"), existing).failed(),"Missing file accepted");
        check(existing == clip,"Existing clip lost");
    });
    test("GO waits for its snapshot when reclamation delays adoption", [&] {
        AudioEngine engine;
        juce::AudioBuffer<float> out(2,1);
        engine.publish(std::make_unique<RenderState>(fixture()));
        engine.processBlock(out.getArrayOfWritePointers(),2,1,4);
        engine.publish(std::make_unique<RenderState>(fixture()));
        engine.processBlock(out.getArrayOfWritePointers(),2,1,4); // previous snapshot now retired
        engine.publish(std::make_unique<RenderState>(fixture()));
        check(engine.send(2,false),"Command rejected");
        engine.processBlock(out.getArrayOfWritePointers(),2,1,4); // cannot adopt yet
        near(out.getSample(0,0),0);
        engine.collectRetired();
        engine.processBlock(out.getArrayOfWritePointers(),2,1,4);
        near(out.getSample(0,0),0.1);
    });
    test("Panic invalidates queued GO even during snapshot changes", [&] {
        AudioEngine engine; juce::AudioBuffer<float> out(2,1);
        engine.publish(std::make_unique<RenderState>(fixture())); engine.send(2,false); engine.panic();
        engine.processBlock(out.getArrayOfWritePointers(),2,1,4); near(out.getSample(0,0),0);
        engine.send(2,false); engine.processBlock(out.getArrayOfWritePointers(),2,1,4); near(out.getSample(0,0),0.1);
        engine.panic(); engine.processBlock(out.getArrayOfWritePointers(),2,1,4); near(out.getSample(0,0),0);
    });
}
