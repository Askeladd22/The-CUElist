#pragma once
void engineV4Tests() {
    using namespace cuelist;
    test("large empty show GO resets flagged transports and retains overlap clocks", [&] {
        Show show; addColumn(show,ColumnType::audio,"");
        for(int i=0;i<10000;++i) addRow(show,"");
        show.rows[1].stopOnNext=false;
        auto state=makeRenderState(show,{});
        for(auto& row:state->rows) { row.seconds=3; row.playing=true; row.pendingSeek=true; row.ended=true; }
        state->trigger(show.rows.back().id,true);
        for(size_t i=0;i+1<state->rows.size();++i) {
            const auto& row=state->rows[i];
            if(i==1) { near(row.seconds,3); check(row.playing && row.pendingSeek && row.ended,"Overlap transport changed"); }
            else { near(row.seconds,0); check(!row.playing && !row.pendingSeek && !row.ended,"Flagged transport not reset"); }
        }
    });
    test("row GO stops previous row actual PCM by default", [&] {
        auto show=makeShow(); addRow(show,"");
        auto clip=std::make_shared<AudioClip>(); clip->sampleRate=4; clip->samples.setSize(1,16);
        for(int i=0;i<16;++i) clip->samples.setSample(0,i,0.2f);
        ClipMap clips; for(const auto& r:show.rows) clips[r.cells[0].id]=clip;
        auto state=makeRenderState(show,clips); juce::AudioBuffer<float> out(2,1);
        state->trigger(show.rows[0].id,true); state->render(out.getArrayOfWritePointers(),2,1,4);
        state->trigger(show.rows[1].id,true); state->render(out.getArrayOfWritePointers(),2,1,4);
        near(out.getSample(0,0),0.2); check(!state->voices[0].playing,"Previous row still playing");
    });
    test("auto stop resets telemetry while opted out row continues after policy update", [&] {
        auto show=makeShow(); addRow(show,""); addRow(show,"");
        auto clip=std::make_shared<AudioClip>(); clip->sampleRate=4; clip->samples.setSize(1,16);
        for(int i=0;i<16;++i) clip->samples.setSample(0,i,0.1f);
        ClipMap clips; for(const auto& row:show.rows) clips[row.cells[0].id]=clip;
        AudioEngine engine; juce::AudioBuffer<float> out(2,1);
        auto block=[&] { engine.processBlock(out.getArrayOfWritePointers(),2,1,4); engine.collectRetired(); };
        engine.publish(makeRenderState(show,clips));
        engine.send(show.rows[0].cells[0].id,false); engine.send(show.rows[1].cells[0].id,false); block();
        show.rows[1].stopOnNext=false; engine.publish(makeRenderState(show,clips));
        engine.send(show.rows[2].id,true); block(); near(out.getSample(0,0),0.2);
        auto stopped=engine.getCellPlayback(show.rows[0].cells[0].id);
        near(stopped.positionSeconds,0); check(!stopped.playing && !stopped.paused && !stopped.waiting,"Auto stop telemetry not reset");
        auto continued=engine.getCellPlayback(show.rows[1].cells[0].id);
        near(continued.positionSeconds,0.5); check(continued.playing,"Updated overlap policy lost in inheritance");
        show.rows[1].stopOnNext=true; engine.publish(makeRenderState(show,clips));
        engine.send(show.rows[2].id,true); block(); near(out.getSample(0,0),0.1);
        check(!engine.getCellPlayback(show.rows[1].cells[0].id).playing,"Updated stop policy lost in inheritance");
    });
    test("v4 stopOnNext persists and v1-v3 migrate enabled", [&] {
        auto show=makeShow(); check(show.rows[0].stopOnNext,"New row must stop on next");
        show.rows[0].stopOnNext=false; Show restored;
        check(decodeShow(encodeShow(show),restored).wasOk(),"v4 decode failed");
        check(!restored.rows[0].stopOnNext,"Flag did not persist");
        for(int version=1;version<=3;++version) {
            auto json=juce::JSON::parse(encodeShow(show)); json.getDynamicObject()->setProperty("version",version);
            json["rows"].getArray()->getReference(0).getDynamicObject()->removeProperty("stopOnNext");
            check(decodeShow(juce::JSON::toString(json),restored).wasOk(),"Legacy decode failed");
            check(restored.rows[0].stopOnNext,"Legacy flag must default true");
        }
    });
    test("empty and opted out row GO stop flagged audio; invalid row does not", [&] {
        auto show=makeShow(); addRow(show,"");
        auto clip=std::make_shared<AudioClip>(); clip->sampleRate=4; clip->samples.setSize(1,16);
        for(int i=0;i<16;++i) clip->samples.setSample(0,i,0.2f);
        ClipMap clips {{show.rows[0].cells[0].id,clip}};
        for(bool excluded : {false,true}) {
            if(excluded) { clips[show.rows[1].cells[0].id]=clip; show.rows[1].cells[0].followsRow=false; }
            auto state=makeRenderState(show,clips); juce::AudioBuffer<float> out(2,1);
            state->trigger(show.rows[0].id,true); state->trigger(999,true);
            state->render(out.getArrayOfWritePointers(),2,1,4); near(out.getSample(0,0),0.2);
            state->trigger(show.rows[1].id,true); state->render(out.getArrayOfWritePointers(),2,1,4);
            near(out.getSample(0,0),0); check(!state->voices[0].playing,"Blank cue did not stop prior row");
        }
    });
    test("overlap opt out, direct launch, same row and explicit stop preserve other rows", [&] {
        auto show=makeShow(); addRow(show,""); addColumn(show,ColumnType::audio,"");
        show.rows[0].stopOnNext=false; show.rows[0].cells[1].followsRow=false;
        auto clip=std::make_shared<AudioClip>(); clip->sampleRate=4; clip->samples.setSize(1,16);
        for(int i=0;i<16;++i) clip->samples.setSample(0,i,0.1f);
        ClipMap clips; for(const auto& r:show.rows) for(const auto& c:r.cells) clips[c.id]=clip;
        auto state=makeRenderState(show,clips); juce::AudioBuffer<float> out(2,1);
        auto block=[&] { state->render(out.getArrayOfWritePointers(),2,1,4); };
        state->trigger(show.rows[0].id,true); state->trigger(show.rows[0].cells[1].id,false); block(); near(out.getSample(0,0),0.2);
        state->trigger(show.rows[0].id,true); block(); near(out.getSample(0,0),0.2);
        state->trigger(show.rows[1].id,true); block(); near(out.getSample(0,0),0.4);
        state->trigger(show.rows[1].id,true,true); block(); near(out.getSample(0,0),0.2);
        show.rows[0].stopOnNext=true; auto next=makeRenderState(show,clips); next->inheritPlayback(*state); state=std::move(next);
        state->trigger(show.rows[1].cells[0].id,false); block(); near(out.getSample(0,0),0.3);
        state->trigger(show.rows[1].id,true); block(); near(out.getSample(0,0),0.2);
        check(!state->voices[1].playing,"Opted out direct voice survived auto stop");
    });
    test("cell telemetry follows rendered wait pause seek loop stop and snapshot changes", [&] {
        auto show=makeShow(); auto& row=show.rows[0]; auto& cell=row.cells[0];
        const int id=cell.id; cell.startSeconds=0.5; cell.behaviour=Behaviour::pauseResume;
        auto clip=std::make_shared<AudioClip>(); clip->sampleRate=4; clip->samples.setSize(1,4);
        for(int i=0;i<4;++i) clip->samples.setSample(0,i,0.1f*(i+1));
        ClipMap clips {{id,clip}}; AudioEngine engine; juce::AudioBuffer<float> out(2,1);
        auto block=[&] { engine.processBlock(out.getArrayOfWritePointers(),2,1,4); engine.collectRetired(); };
        engine.publish(makeRenderState(show,clips)); near(engine.getCellPlayback(id).durationSeconds,1);
        check(!engine.getCellPlayback(999).playing,"Unknown telemetry active");
        engine.send(row.id,true); block(); auto p=engine.getCellPlayback(id);
        check(p.playing && p.waiting && !p.paused,"Waiting telemetry incorrect"); near(p.positionSeconds,0);
        engine.send(row.id,true); block(); p=engine.getCellPlayback(id);
        check(p.paused && !p.playing && !p.waiting,"Paused waiting telemetry incorrect");
        engine.seekRow(row.id,1.0); block(); near(engine.getCellPlayback(id).positionSeconds,0.5);
        engine.send(row.id,true); block(); near(out.getSample(0,0),0.3); near(engine.getCellPlayback(id).positionSeconds,0.75);
        show.columns[0].gain=0.5f; engine.publish(makeRenderState(show,clips)); near(engine.getCellPlayback(id).positionSeconds,0.75);
        block(); check(!engine.getCellPlayback(id).playing && !engine.getCellPlayback(id).paused,"Completion looks paused");
        cell.loop=true; engine.publish(makeRenderState(show,clips)); engine.send(id,false); block();
        for(int i=0;i<4;++i) block(); near(engine.getCellPlayback(id).positionSeconds,0.25);
        engine.send(id,false,true); block(); p=engine.getCellPlayback(id); near(p.positionSeconds,0); check(!p.paused && !p.playing,"Stop telemetry incorrect");
        engine.send(id,false); block(); engine.panic(); block(); check(!engine.getCellPlayback(id).playing,"Panic telemetry active"); near(engine.getCellPlayback(id).positionSeconds,0);
        engine.send(id,false); block(); clips[id]=std::make_shared<AudioClip>(*clip); engine.publish(makeRenderState(show,clips));
        check(!engine.getCellPlayback(id).playing,"Replaced media copied stale telemetry"); block();
        clips.clear(); engine.publish(makeRenderState(show,clips)); near(engine.getCellPlayback(id).durationSeconds,0); block();
    });
}
