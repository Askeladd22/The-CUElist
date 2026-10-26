#pragma once
#include "OscEditor.h"
#include <cstring>
void oscTests() {
    test("OSC 1.0 encoding supports int float and string with network byte order",[] {
        OscEvent e;
        e.address="/cue/go";
        e.arguments={{OscValueType::integer,42,0,{}},
                     {OscValueType::decimal,0,0.5f,{}},
                     {OscValueType::text,0,0,"OK"}};
        OscClip clip; clip.events={e};
        check(validateOscClip(clip).wasOk(),"Valid OSC clip rejected");
        const auto encoded=encodeOscMessage(e);
        check(encoded.size()==32,"OSC packet size incorrect");
        const auto eq=[&](size_t offset,const char* pattern) {
            return std::memcmp(encoded.data()+offset,pattern,std::strlen(pattern))==0;
        };
        check(eq(0,"/cue/go") && encoded[7]==0,"OSC address encoded incorrectly");
        check(eq(8,",ifs") && encoded[12]==0,"OSC typetags encoded incorrectly");
        check(encoded[16]==0 && encoded[17]==0 && encoded[18]==0 && encoded[19]==42,"OSC integer not big-endian");
        check(encoded[20]==0x3f && encoded[21]==0x00 && encoded[22]==0 && encoded[23]==0,"OSC float not big-endian");
        check(eq(24,"OK"),"OSC string lost");
        auto invalid=clip;
        invalid.events[0].address="cue/no-slash";
        check(validateOscClip(invalid).failed(),"Invalid OSC path accepted");
    });
    test("v6 OSC clip roundtrip preserves routing, types, units and backward v5 loading",[] {
        auto show=makeShow(); addColumn(show,ColumnType::osc,"LX");
        auto& column=show.columns[1]; column.oscHost="192.168.1.22"; column.oscPort=7001;
        OscClip clip; clip.useBeats=true; clip.length=12;
        OscEvent e; e.time=5; e.address="/lx/level";
        e.arguments={{OscValueType::integer,4,0,{}},{OscValueType::decimal,0,0.75f,{}},
                     {OscValueType::text,0,0,"front"}};
        clip.events.push_back(e); show.rows[0].cells[1].osc=clip;
        Show decoded; check(decodeShow(encodeShow(show),decoded).wasOk(),"v6 roundtrip failed");
        check(decoded.columns[1].oscHost=="192.168.1.22" && decoded.columns[1].oscPort==7001,"OSC destination not persisted");
        check(decoded.rows[0].cells[1].osc && decoded.rows[0].cells[1].osc->useBeats,"OSC beat unit not persisted");
        near(decoded.rows[0].cells[1].osc->events[0].time,5);
        check(decoded.rows[0].cells[1].osc->events[0].arguments[2].text=="front","OSC typed string not persisted");
        // Rebuild a genuine v5 JSON: old files have no osc-related keys.
        auto json=juce::JSON::parse(encodeShow(makeShow()));
        json.getDynamicObject()->setProperty("version",5);
        for (auto& c:*json.getProperty("columns",{}).getArray()) {
            c.getDynamicObject()->removeProperty("oscHost");
            c.getDynamicObject()->removeProperty("oscPort");
        }
        for (auto& r:*json.getProperty("rows",{}).getArray())
            for (auto& cell:*r.getProperty("cells",{}).getArray())
                cell.getDynamicObject()->removeProperty("osc");
        Show previous;
        check(decodeShow(juce::JSON::toString(json),previous).wasOk(),"v5 project rejected");
        check(previous.columns[0].oscPort==9000 && previous.columns[0].oscHost=="127.0.0.1","Legacy defaults broken");
    });
    test("OSC events share audio-clock GO seek loop and Stop policies",[] {
        struct Sink : OscEventSink {
            std::vector<unsigned> lengths;
            void emit(int route,const uint8_t*,unsigned size,int) noexcept override {
                if (route==2) lengths.push_back(size);
            }
        } sink;
        auto show=makeShow(); addColumn(show,ColumnType::osc,"OSC");
        OscClip clip; clip.length=1; clip.events={{0.1,"/one",{}},{0.6,"/two",{}}};
        show.rows[0].cells[1].osc=clip;
        auto state=makeRenderState(show,{});
        check(state->voices.size()==1 && state->voices[0].isOsc,"OSC voice not produced");
        state->voices[0].oscSink=&sink;
        state->voices[0].oscRoute=2;
        state->voices[0].oscStream=std::make_shared<OscStream>();
        float audio[1000]{}; float* outputs[]{audio};
        state->trigger(show.rows[0].id,true);
        state->render(outputs,1,1000,1000);
        check(sink.lengths.size()==2,"Scheduled OSC events not emitted");
        state->trigger(show.rows[0].id,true);
        state->seekRow(show.rows[0].id,0.5);
        state->render(outputs,1,300,1000);
        check(sink.lengths.size()==3,"Seek should skip earlier OSC and fire later event");
        state->trigger(show.rows[0].cells[1].id,false,true);
        check(!state->voices[0].playing,"Direct OSC stop failed");
        const auto before=state->voices[0].oscStream->epoch.load();
        state->panic();
        check(state->voices[0].oscStream->epoch.load()>before,"Panic did not cancel queued OSC sends");
        show.rows[0].cells[1].loop=true;
        auto loop=makeRenderState(show,{});
        loop->voices[0].oscSink=&sink;
        loop->voices[0].oscRoute=2;
        loop->voices[0].oscStream=std::make_shared<OscStream>();
        loop->trigger(show.rows[0].cells[1].id,false);
        const auto original=sink.lengths.size();
        loop->render(outputs,1,1000,1000);
        loop->render(outputs,1,1000,1000);
        check(sink.lengths.size()==original+4,"OSC events did not repeat during loop");
    });
}
