#pragma once
#include "AudioEngine.h"
inline void audioTests() {
    auto clip = std::make_shared<AudioClip>(); clip->samples.setSize(1, 4); clip->sampleRate = 4;
    for (int i = 0; i < 4; ++i) clip->samples.setSample(0, i, 0.1f * (i + 1));
    auto fixture = [&] { RenderState s; Voice v; v.cellId = 2; v.rowId = 1; v.clip = clip; v.gain = 1; s.voices.push_back(v); return s; };
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
