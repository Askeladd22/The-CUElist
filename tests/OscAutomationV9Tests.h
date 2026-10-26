#pragma once
#include "OscEditor.h"
#include <cstring>

void oscAutomationV9Tests() {
    test("OSC 1.0 boolean T and F encode without payload bytes",[] {
        OscEvent e; e.address="/go";
        e.arguments={{OscValueType::boolean,0,0,{},true},
                     {OscValueType::boolean,0,0,{},false}};
        OscClip clip; clip.events={e};
        check(validateOscClip(clip).wasOk(),"OSC booleans rejected");
        const auto encoded=encodeOscMessage(e);
        check(encoded.size()==8,"OSC T/F carried an illegal binary payload");
        check(std::memcmp(encoded.data()+4,",TF",3)==0,"OSC boolean type tags are wrong");
    });
    test("v9 persists detailed curves, booleans and leaves v8 projects unchanged",[] {
        auto show=makeShow(); addColumn(show,ColumnType::osc,"Lighting");
        OscClip osc; osc.useBeats=true; osc.length=12;
        osc.events.push_back({1,"/enabled",{{OscValueType::boolean,0,0,{},true}}});
        OscAutomation a; a.message.address="/dimmer";
        a.message.arguments={{OscValueType::integer,2,0,{}},
                             {OscValueType::decimal,0,0.25f,{}}};
        a.argumentIndex=1; a.time=2; a.duration=5;
        a.points={{0,0.25},{1.5,0.4},{3.5,0.7},{5,1}};
        osc.automations={a}; show.rows[0].cells[1].osc=osc;
        const auto encoded=encodeShow(show);
        near((int)juce::JSON::parse(encoded)["version"],10);
        Show restored; check(decodeShow(encoded,restored).wasOk(),"v9 OSC decode failed");
        const auto& actual=*restored.rows[0].cells[1].osc;
        check(actual.useBeats && actual.automations.size()==1 && actual.events.size()==1,
              "v9 OSC content missing");
        check(actual.events[0].arguments[0].boolean,"Boolean lost in v9 roundtrip");
        near(actual.automations[0].points[2].time,3.5);
        near(actual.automations[0].points[2].value,.7);
        check(actual.automations[0].argumentIndex==1,"Automation index lost");
        auto legacy=juce::JSON::parse(encoded);
        legacy.getDynamicObject()->setProperty("version",8);
        for (auto& row:*legacy.getProperty("rows",{}).getArray())
            for (auto& cell:*row.getProperty("cells",{}).getArray()) {
                auto* o=cell.getDynamicObject();
                if (!o->getProperty("osc").isObject()) continue;
                auto oscObj=o->getProperty("osc");
                oscObj.getDynamicObject()->removeProperty("automations");
                for (auto& ev:*oscObj.getProperty("events",{}).getArray())
                    for (auto& arg:*ev.getProperty("args",{}).getArray())
                        arg.getDynamicObject()->removeProperty("boolean");
            }
        Show earlier;
        check(decodeShow(juce::JSON::toString(legacy),earlier).wasOk(),"v8 OSC show cannot be opened");
        check(earlier.rows[0].cells[1].osc->automations.empty(),"v8 unexpectedly grew curves");
    });
    test("bad automation geometry and targets are rejected with no partial show mutation",[] {
        OscClip clip; clip.length=5;
        OscAutomation a; a.time=1; a.duration=2;
        a.message.address="/gain";
        a.message.arguments={{OscValueType::decimal,0,0,{}}};
        a.points={{0,0},{1,.5},{2,1}};
        clip.automations.push_back(a);
        check(validateOscClip(clip).wasOk(),"Valid curve rejected");
        auto broken=clip; broken.automations[0].points[1].time=2.2;
        check(validateOscClip(broken).failed(),"Out-of-order points accepted");
        broken=clip; broken.automations[0].argumentIndex=9;
        check(validateOscClip(broken).failed(),"Out-of-range arg index accepted");
        broken=clip; broken.automations[0].message.arguments[0].type=OscValueType::text;
        check(validateOscClip(broken).failed(),"String automated illegally");
        broken=clip; broken.automations.push_back(a);
        check(validateOscClip(broken).failed(),"Two curves writing the same arg concurrently accepted");
        auto show=makeShow(); addColumn(show,ColumnType::osc,"OSC");
        show.rows[0].cells[1].osc=clip;
        auto invalid=juce::JSON::parse(encodeShow(show));
        auto rows=invalid.getProperty("rows",{}).getArray();
        auto cell=(*(*rows)[0].getProperty("cells",{}).getArray())[1];
        auto osc=cell.getProperty("osc",{}).getDynamicObject();
        (*osc->getProperty("automations").getArray())[0].getDynamicObject()->setProperty("argumentIndex",999);
        Show keep; keep.name="Keep";
        check(decodeShow(juce::JSON::toString(invalid),keep).failed(),"Corrupt automation accepted");
        check(keep.name=="Keep","Invalid v9 mutated caller state");
    });
    test("automation compiler interpolates a chosen float while preserving other args",[] {
        OscClip clip; clip.length=2;
        OscAutomation a; a.message.address="/dimmer";
        a.message.arguments={{OscValueType::integer,9,0,{}},
                             {OscValueType::decimal,0,0,{}}};
        a.argumentIndex=1; a.time=0; a.duration=1;
        a.points={{0,0},{.5,1},{1,0}};
        clip.automations={a};
        const auto events=prepareOscClip(clip,[](double t){return t;},[](double t){return t;});
        check(events.size()==31,"30 Hz, including initial and final, not prepared");
        const auto& start=events.front().bytes;
        check(std::memcmp(start.data(),"/dimmer",7)==0,"Wrong automation address");
        // /dimmer padded to 8, typetag ,if padded to 4, int starts at offset 12.
        check(start.size()==20,"OSC float/int packet unexpected size");
        check(start[15]==9 && start[16]==0 && start[17]==0,"Wrong initial value bytes");
        const auto& mid=events[15].bytes;
        check(mid[15]==9 && mid[16]==0x3f && mid[17]==0x80 &&
            mid[18]==0 && mid[19]==0,"Automation did not peak at midpoint");
        const auto& end=events.back().bytes;
        check(end[16]==0 && end[17]==0 && end[18]==0 && end[19]==0,
              "Final automation value not emitted");
    });
    test("automation sends its final value at natural completion and immediately before loop wrap",[] {
        struct Sink : OscEventSink {
            std::vector<std::vector<uint8_t>> messages;
            void emit(int,const uint8_t* bytes,unsigned n,int) noexcept override {
                messages.emplace_back(bytes,bytes+n);
            }
        } sink;
        auto show=makeShow(); addColumn(show,ColumnType::osc,"OSC");
        OscClip clip; clip.length=1;
        OscAutomation a; a.message.address="/fade";
        a.message.arguments={{OscValueType::integer,0,0,{}}};
        a.duration=1; a.points={{0,0},{1,127}};
        clip.automations.push_back(a);
        show.rows[0].cells[1].osc=clip;
        auto state=makeRenderState(show,{});
        auto& voice=state->voices[0]; voice.oscSink=&sink; voice.oscRoute=5;
        voice.oscStream=std::make_shared<OscStream>();
        float samples[1000]{}; float* channels[]{samples};
        state->trigger(voice.cellId,false);
        state->render(channels,1,1000,1000);
        check(!sink.messages.empty(),"Fade failed to emit");
        check(sink.messages.back().back()==127,"Final automation value never emitted");
        const auto count=sink.messages.size();
        state->render(channels,1,100,1000);
        check(sink.messages.size()==count,"Completed clip sent trailing OSC updates");
        show.rows[0].cells[1].loop=true;
        auto looping=makeRenderState(show,{});
        auto& v=looping->voices[0]; v.oscSink=&sink; v.oscRoute=5;
        v.oscStream=std::make_shared<OscStream>();
        looping->trigger(v.cellId,false);
        const auto begin=sink.messages.size();
        looping->render(channels,1,1000,1000);
        const auto afterFirst=sink.messages.size();
        check(afterFirst>begin && sink.messages.back().back()==127,"Loop end missed final value");
        looping->render(channels,1,1,1000);
        check(sink.messages.size()==afterFirst+1 && sink.messages.back().back()==0,
              "Loop should immediately re-send the ramp's initial value");
    });
    test("two automations on different args share an OSC address without overwriting each other",[] {
        OscClip clip; clip.length=2;
        OscAutomation a; a.message.address="/xy";
        a.message.arguments={{OscValueType::decimal,0,0,{}},
                             {OscValueType::decimal,0,0,{}}};
        a.time=0; a.duration=1; a.argumentIndex=0; a.points={{0,1},{1,2}};
        auto b=a; b.argumentIndex=1; b.points={{0,10},{1,20}};
        clip.automations={a,b};
        check(validateOscClip(clip).wasOk(),"Distinct automation args wrongly conflicted");
        const auto list=prepareOscClip(clip,[](double t){return t;},[](double t){return t;});
        check(list.size()>=32,"Composite automation packets missing");
        // At t0 each automation must send an initial packet with both values.
        for (int i=0;i<2;++i) {
            const auto& bytes=list[(size_t)i].bytes;
            // /xy padded to 4, type tags ,ff padded to 4, two floats at [8..15].
            check(bytes[8]==0x3f && bytes[9]==0x80 && bytes[12]==0x41 && bytes[13]==0x20,
                  "Composite OSC packet reset the other automated argument");
        }
    });
    test("Stop does not emit reset and retrigger repeats the explicit initial value",[] {
        struct Sink:OscEventSink {
            std::vector<std::vector<uint8_t>> packets;
            void emit(int,const uint8_t* bytes,unsigned size,int) noexcept override {
                packets.emplace_back(bytes,bytes+size);
            }
        } sink;
        auto show=makeShow(); addColumn(show,ColumnType::osc,"OSC");
        OscClip clip; clip.length=2;
        OscAutomation automation; automation.message.address="/level";
        automation.message.arguments={{OscValueType::integer,0,0,{}}};
        automation.time=0; automation.duration=1; automation.points={{0,10},{1,20}};
        clip.automations={automation}; show.rows[0].cells[1].osc=clip;
        auto state=makeRenderState(show,{});
        check(state->voices.size()==1,"OSC automation voice not prepared");
        auto& v=state->voices[0]; v.oscSink=&sink; v.oscRoute=1;
        v.oscStream=std::make_shared<OscStream>();
        float out[1000]{}; float* channels[]{out};
        state->trigger(v.cellId,false);
        state->render(channels,1,1,1000);
        check(sink.packets.size()==1,"Initial OSC value was delayed");
        const auto initial=sink.packets.front();
        state->render(channels,1,500,1000);
        check(sink.packets.size()>1,"Automation updates missing");
        state->trigger(v.cellId,false,true);
        const auto stopped=sink.packets.size();
        state->render(channels,1,500,1000);
        check(sink.packets.size()==stopped,"Stop sent a reset or extra OSC messages");
        state->trigger(v.cellId,false);
        state->render(channels,1,1,1000);
        check(sink.packets.size()==stopped+1,"Retrigger must re-send initial OSC value");
        check(sink.packets.back()==initial,"Retrigger initial OSC value changed");
    });
    test("OSC timeline edits participate in application-wide Undo and Redo",[] {
        MainComponent ui(false);
        addColumn(ui.show,ColumnType::osc,"OSC");
        check(ui.select(0,1),"Cannot select OSC column");
        ui.editOscClip();
        auto* editor=ui.activeOscEditor();
        check(editor!=nullptr,"OSC editor missing");
        editor->setSize(940,360);
        const juce::Point<float> point{230,118};
        editor->mouseDown(cueViewFixtures::event(*editor,point,point));
        check(ui.show.rows[0].cells[1].osc->events.size()==1,"Mouse click did not insert event");
        check(ui.undoEdit(),"OSC event Undo unavailable");
        check(ui.show.rows[0].cells[1].osc->events.empty(),"OSC Undo did not restore old clip");
        check(ui.redoEdit(),"OSC event Redo unavailable");
        check(ui.show.rows[0].cells[1].osc->events.size()==1,"OSC Redo did not restore inserted event");
    });
    test("OSC automation respects a tempo change inside a beat-synced cue",[] {
        const std::vector<TempoPoint> tempo={{0,120,4,4},{4,60,4,4}};
        OscClip clip; clip.useBeats=true; clip.length=8;
        OscAutomation a; a.message.address="/light";
        a.message.arguments={{OscValueType::decimal,0,0,{}}};
        a.time=3; a.duration=2; a.points={{0,0},{2,1}};
        clip.automations.push_back(a);
        const auto secondsAt=[&](double beat){return secondsAtBeat(tempo,beat);};
        const auto beatAt=[&](double seconds){
            double lo=0,hi=10;
            for(int n=0;n<50;++n) {
                const auto mid=(lo+hi)*.5;
                if(secondsAt(mid)<seconds)lo=mid;else hi=mid;
            }
            return (lo+hi)*.5;
        };
        const auto messages=prepareOscClip(clip,secondsAt,beatAt);
        check(!messages.empty(),"Beat-synced automation did not compile");
        near(messages.front().seconds,1.5); // Beat three at 120 BPM
        near(messages.back().seconds,3.0);  // Beat five after change to 60 BPM
        bool foundHalf=false;
        for(const auto& msg:messages) if(std::abs(msg.seconds-2.0)<.000001) {
            check(msg.bytes.size()==16,"Unexpected OSC float payload");
            // /light padded to 8; ,f padded to 4; 0.5f = 0x3f000000.
            check(msg.bytes[12]==0x3f && msg.bytes[13]==0x00,
                  "Musical interpolation did not reach halfway at BPM change");
            foundHalf=true;
        }
        check(foundHalf,"Tempo change point missing from the prepared automation timeline");
    });
    test("seek into an OSC ramp does not repeat its already elapsed initial value",[] {
        struct Sink : OscEventSink {
            std::vector<std::vector<uint8_t>> received;
            void emit(int,const uint8_t* data,unsigned size,int) noexcept override {
                received.emplace_back(data,data+size);
            }
        } sink;
        auto show=makeShow(); addColumn(show,ColumnType::osc,"OSC");
        OscClip clip; clip.length=2;
        OscAutomation au; au.message.address="/seek";
        au.message.arguments={{OscValueType::integer,0,0,{}}};
        au.points={{0,10},{1,40}}; au.duration=1;
        clip.automations={au};
        show.rows[0].cells[1].osc=clip;
        auto state=makeRenderState(show,{});
        auto& voice=state->voices[0]; voice.oscSink=&sink; voice.oscRoute=3;
        voice.oscStream=std::make_shared<OscStream>();
        state->trigger(show.rows[0].id,true);
        state->seekRow(show.rows[0].id,.5);
        float samples[100]{}; float* channels[]{samples};
        state->render(channels,1,100,1000);
        check(!sink.received.empty(),"Seek into ramp did not resume interpolation");
        const auto& bytes=sink.received.front();
        check(bytes.size()>=4 && bytes.back()!=10,"Seek unexpectedly replayed the initial value");
        const auto before=sink.received.size();
        state->trigger(show.rows[0].id,true,true);
        state->render(channels,1,100,1000);
        check(sink.received.size()==before,"Explicit Stop emitted an OSC reset");
    });
    test("double-clicking the empty OSC lane creates a one-second 0-to-1 ramp",[] {
        OscClipEditor editor(OscClip{},{{0,120,4,4}},
            [](const OscClip&,const OscClip&,bool){});
        editor.setSize(940,360);
        const juce::Point<float> point{300,204};
        editor.mouseDoubleClick(cueViewFixtures::event(editor,point,point));
        check(editor.getClip().automations.size()==1,"Double click did not create the ramp");
        const auto& a=editor.getClip().automations[0];
        near(a.duration,1); near(a.points[0].value,0); near(a.points[1].value,1);
        check(a.message.address=="/parameter","Default address not assigned");
        check(juce::ModalComponentManager::getInstance()->getNumModalComponents()==0,
              "Double click unexpectedly opened a ramp modal");
        check(validateOscClip(editor.getClip()).wasOk(),"Created ramp invalid");
    });
    test("OSC editor creates timeline events and automation ramps with separate grid and snap",[] {
        OscClipEditor editor(OscClip{},{{0,120,4,4}},
            [](const OscClip&,const OscClip&,bool){});
        editor.setSize(940,360);
        check(controlsV3::named(editor,"oscGridVisible") &&
              controlsV3::named(editor,"oscSnapEnabled") &&
              controlsV3::named(editor,"oscAddAutomation"),"OSC piano-roll controls absent");
        editor.addAutomation();
        check(editor.getClip().automations.size()==1 &&
              editor.getClip().automations[0].points.size()==2,
              "Ramp button did not create a linear automation");
        check(juce::ModalComponentManager::getInstance()->getNumModalComponents()==0,
              "Creating a ramp must not open any configuration dialog");
        check(validateOscClip(editor.getClip()).wasOk(),"Editor created invalid OSC automation");
    });
}
