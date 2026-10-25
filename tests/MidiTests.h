#pragma once
#include "MidiClip.h"
namespace midiFixtures {
struct Sink final : MidiEventSink {
    struct Event { int route,status,pitch,velocity,offset; };
    std::vector<Event> events;
    Sink() { events.reserve(10000); }
    void emit(int r,int s,int p,int v,int o) noexcept override { events.push_back({r,s,p,v,o}); }
};
inline Show show() {
    auto s=makeShow(); s.columns[0].type=ColumnType::midi; s.columns[0].midiOutputId="test-device";
    s.rows[0].cells[0].midi=MidiClip{4,{{0,1,60,90,2},{2,1,64,80,1}}}; return s;
}
inline void bind(RenderState& state,Sink& sink) { for (auto& v:state.voices) { v.midiSink=&sink; v.midiRoute=7; } }
}
inline void registerMidiTests() {
    test("MIDI missing device recovery stays silent without rerouting",[] {
        MidiOutputRouter router;
        const juce::String missing="cuelist-nonexistent-"+juce::Uuid().toString();
        check(router.open(missing).failed(),"Missing device opened");
        check(router.route(missing)==-1,"Missing device silently rerouted");
        check(router.open(missing,true).failed(),"Missing device recovery succeeded unexpectedly");
        router.refresh(); router.emergencyStop();
        check(router.route(missing)==-1,"Refresh selected another device");
        check(router.open({},true).wasOk(),"Unassigned output rejected");
    });
    test("MIDI scheduled loop events retain epoch while stop and seek cancel pending notes",[] {
        struct EpochSink final : MidiEventSink {
            MidiStream* stream=nullptr; std::vector<uint64_t> epochs;
            EpochSink() { epochs.reserve(100); }
            void selectStream(MidiStream* s) noexcept override { stream=s; }
            void emit(int,int,int,int,int) noexcept override { epochs.push_back(stream->epoch.load()); }
        } sink;
        auto s=midiFixtures::show(); auto& c=s.rows[0].cells[0]; c.loop=true;
        c.midi=MidiClip{.25,{{0,.25,60,90,1}}};
        auto state=makeRenderState(s,{}); auto stream=std::make_shared<MidiStream>();
        state->voices[0].midiStream=stream; state->voices[0].midiSink=&sink;
        state->trigger(s.rows[0].id,true); const auto epoch=stream->epoch.load();
        state->render(nullptr,0,10,40);
        check(sink.epochs.size()==4,"Expected two complete short loops in one block");
        for (auto emitted:sink.epochs) check(emitted==epoch,"Natural loop cancelled queued events");
        check(stream->epoch.load()==epoch,"Loop changed cancellation epoch");
        state->seekRow(s.rows[0].id,.01); check(stream->epoch.load()>epoch,"Seek did not invalidate scheduled notes");
        const auto seekEpoch=stream->epoch.load(); state->trigger(c.id,false,true);
        check(stream->epoch.load()>seekEpoch,"Stop did not invalidate scheduled notes");
        c.loop=false; state=makeRenderState(s,{}); state->voices[0].midiStream=stream; state->voices[0].midiSink=&sink;
        state->trigger(c.id,false); const auto finalEpoch=stream->epoch.load(); sink.epochs.clear();
        state->render(nullptr,0,10,40);
        check(sink.epochs.size()==2 && stream->epoch.load()==finalEpoch,"Natural finish cancelled short clip notes");
        for (auto emitted:sink.epochs) check(emitted==finalEpoch,"Natural finish invalidated scheduled note");
    });
    test("MIDI model v5 roundtrip embeds notes and stable route",[] {
        auto s=midiFixtures::show(); Show decoded;
        check(decodeShow(encodeShow(s),decoded).wasOk(),"MIDI roundtrip failed");
        check(decoded.columns[0].midiOutputId=="test-device","Route lost");
        const auto& m=*decoded.rows[0].cells[0].midi;
        near(m.lengthBeats,4); check(m.notes.size()==2 && m.notes[0].channel==2 && m.notes[0].velocity==90,"Notes lost");
        decoded.columns[0].type=ColumnType::audio; check(validateShow(decoded).failed(),"MIDI allowed in audio column");
    });
    test("MIDI validation rejects bad bounds and accepts empty clips",[] {
        MidiClip m; check(validateMidiClip(m).wasOk(),"Empty clip rejected");
        m.notes.push_back({3,2,60,100,1}); check(validateMidiClip(m).failed(),"Out of bounds note accepted");
        m.notes[0]={0,1,128,100,1}; check(validateMidiClip(m).failed(),"Invalid pitch accepted");
        m.notes[0]={0,1,60,0,1}; check(validateMidiClip(m).failed(),"Zero velocity accepted");
        m.notes[0]={0,1,60,100,17}; check(validateMidiClip(m).failed(),"Invalid channel accepted");
    });
    test("MIDI note timing follows cue tempo including changes",[] {
        auto s=midiFixtures::show(); s.rows[0].tempo={{0,120,4,4},{2,60,4,4}};
        auto state=makeRenderState(s,{}); midiFixtures::Sink sink; midiFixtures::bind(*state,sink);
        state->trigger(s.rows[0].id,true); state->render(nullptr,0,21,10);
        check(sink.events.size()==4,"Incorrect note event count");
        check(sink.events[0].offset==0 && sink.events[1].offset==5 && sink.events[2].offset==10 && sink.events[3].offset==20,"Tempo offsets incorrect");
        check(sink.events[0].status==0x91 && sink.events[0].route==7,"Channel or route wrong");
    });
    test("MIDI stop pause seek and panic release held notes",[] {
        auto s=midiFixtures::show(); auto& c=s.rows[0].cells[0]; c.behaviour=Behaviour::pauseResume;
        auto state=makeRenderState(s,{}); midiFixtures::Sink sink; midiFixtures::bind(*state,sink);
        state->trigger(c.id,false); state->render(nullptr,0,1,10);
        state->trigger(c.id,false); check(sink.events.back().status==0x81,"Pause left note held");
        state->trigger(c.id,false); state->render(nullptr,0,1,10); check(sink.events.back().status==0x91,"Resume failed to chase note");
        state->seekRow(s.rows[0].id,1); check(sink.events.back().status==0x81,"Seek left note held");
        state->trigger(c.id,false,true); check(!state->voices[0].playing,"Stop failed");
        state->trigger(c.id,false); state->render(nullptr,0,1,10); state->panic();
        check(sink.events.back().status==0x81 && !state->voices[0].playing,"Panic failed");
    });
    test("MIDI loops release boundary notes and preserve unchanged snapshots",[] {
        auto s=midiFixtures::show(); auto& c=s.rows[0].cells[0]; c.midi=MidiClip{1,{{0,1,60,100,1}}}; c.loop=true;
        auto state=makeRenderState(s,{}); midiFixtures::Sink sink; midiFixtures::bind(*state,sink);
        state->trigger(c.id,false); state->render(nullptr,0,6,10);
        check(sink.events.size()==3 && sink.events[1].status==0x80 && sink.events[2].status==0x90,"Loop boundary failed");
        auto next=makeRenderState(s,{}); midiFixtures::bind(*next,sink); next->inheritPlayback(*state);
        near(next->voices[0].position,state->voices[0].position); check(next->voices[0].playing,"Publication stopped MIDI");
        next->panic(); check(sink.events.back().status==0x80,"Inherited notes not released");
    });
    test("MIDI auto stop applies when next cue has no clips",[] {
        auto s=midiFixtures::show(); addRow(s,{}); auto state=makeRenderState(s,{});
        midiFixtures::Sink sink; midiFixtures::bind(*state,sink); state->trigger(s.rows[0].id,true); state->render(nullptr,0,1,10);
        state->trigger(s.rows[1].id,true); check(sink.events.back().status==0x81 && !state->voices[0].playing,"Auto stop failed");
    });
    test("MIDI import PPQ and rejection preserve destination",[] {
        auto file=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("cuelist-midi-test",".mid");
        struct Cleanup { juce::File f; ~Cleanup(){ f.deleteFile(); } } cleanup{file};
        juce::MidiMessageSequence track; auto on=juce::MidiMessage::noteOn(3,67,(juce::uint8)110); on.setTimeStamp(240); track.addEvent(on);
        auto off=juce::MidiMessage::noteOff(3,67); off.setTimeStamp(720); track.addEvent(off);
        track.addEvent(juce::MidiMessage::tempoMetaEvent(500000));
        juce::MidiFile midi; midi.setTicksPerQuarterNote(480); midi.addTrack(track);
        { auto out=file.createOutputStream(); check(out && midi.writeTo(*out,1),"Fixture write failed"); }
        MidiClip clip; juce::String warning; check(loadMidiClip(file,clip,warning).wasOk(),"Valid import rejected");
        check(clip.notes.size()==1 && warning.isNotEmpty(),"Notes or warning absent"); near(clip.notes[0].beat,.5); near(clip.notes[0].length,1);
        file.deleteFile();
        { auto out=file.createOutputStream(); check(out && midi.writeTo(*out,2),"Type2 fixture write failed"); }
        check(loadMidiClip(file,clip,warning).failed(),"Type2 MIDI accepted");
        file.deleteFile(); midi.setSmpteTimeFormat(25,40);
        { auto out=file.createOutputStream(); check(out && midi.writeTo(*out,1),"SMPTE fixture write failed"); }
        check(loadMidiClip(file,clip,warning).failed(),"SMPTE MIDI accepted");
        file.replaceWithText("invalid"); check(loadMidiClip(file,clip,warning).failed(),"Malformed MIDI accepted");
        check(clip.notes.size()==1,"Failed import replaced clip");
    });
}
