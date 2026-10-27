#pragma once
#include "MainComponent.h"
#include "GainMeterFader.h"
#include <cmath>

void masterBusRoutingTests() {
    const auto tone=[](float value) {
        auto clip=std::make_shared<AudioClip>();
        clip->sampleRate=48000;
        clip->samples.setSize(2,64);
        for(int n=0;n<64;++n) for(int ch=0;ch<2;++ch)
            clip->samples.setSample(ch,n,value);
        return clip;
    };
    test("Master Bus and independent direct outputs can run simultaneously", [&] {
        auto show=makeShow();
        addColumn(show,ColumnType::audio,"B direct PRE");
        addColumn(show,ColumnType::audio,"C Master");
        show.columns[0].masterSend=true;
        show.columns[0].directOutputEnabled=true;
        show.columns[0].directPostFader=true;
        show.columns[0].output=2;
        show.columns[0].gain=.5f;
        show.columns[1].masterSend=false;
        show.columns[1].directOutputEnabled=true;
        show.columns[1].directPostFader=false;
        show.columns[1].output=4;
        show.columns[1].gain=.2f;
        show.columns[2].masterSend=true;
        show.columns[2].directOutputEnabled=false;
        show.masterOutput=0;
        show.masterGain=.5f;
        ClipMap media;
        media[show.rows[0].cells[0].id]=tone(.2f);
        media[show.rows[0].cells[1].id]=tone(.3f);
        media[show.rows[0].cells[2].id]=tone(.4f);
        auto state=makeRenderState(show,media);
        state->trigger(show.rows[0].id,true);
        juce::AudioBuffer<float> out(6,2);
        state->render(out.getArrayOfWritePointers(),6,2,48000);
        near(out.getSample(0,0),.25);
        near(out.getSample(1,0),.25);
        near(out.getSample(2,0),.1);
        near(out.getSample(3,0),.1);
        near(out.getSample(4,0),.3);
        near(out.getSample(5,0),.3);
        near(state->preLimiterPeakLeft,.25);
        near(state->columnMeters[1].second->left.load(),.06);
        check(!state->limiterReducing,"Limiter activated under threshold");
        state->masterGain=0.0f;
        state->render(out.getArrayOfWritePointers(),6,2,48000);
        near(out.getSample(0,0),0);
        near(out.getSample(2,0),.1);
        near(out.getSample(4,0),.3);
        state->voices[1].directPostFader=true;
        state->render(out.getArrayOfWritePointers(),6,2,48000);
        near(out.getSample(4,0),.06);
    });
    test("Routing without Master or Direct is silent but transport advances", [&] {
        auto show=makeShow();
        show.columns[0].masterSend=false;
        show.columns[0].directOutputEnabled=false;
        ClipMap media; media[show.rows[0].cells[0].id]=tone(.5f);
        auto state=makeRenderState(show,media);
        state->trigger(show.rows[0].cells[0].id,false);
        juce::AudioBuffer<float> out(2,2);
        state->render(out.getArrayOfWritePointers(),2,2,48000);
        near(out.getSample(0,0),0);
        check(state->voices[0].playing && state->voices[0].position==2,
              "Unrouted audio unexpectedly stopped");
    });
    test("Master limiter uses a fixed stereo-linked peak ceiling; meter is pre-limiter", [&] {
        auto show=makeShow();
        addColumn(show,ColumnType::audio,"Second");
        show.masterOutput=2;
        ClipMap media;
        for(const auto& cell:show.rows[0].cells) media[cell.id]=tone(.9f);
        auto state=makeRenderState(show,media);
        state->trigger(show.rows[0].id,true);
        juce::AudioBuffer<float> out(4,2);
        state->render(out.getArrayOfWritePointers(),4,2,48000);
        near(out.getSample(0,0),0);
        near(out.getSample(2,0),std::pow(10.0,-1.0/20.0));
        near(out.getSample(3,0),std::pow(10.0,-1.0/20.0));
        near(state->preLimiterPeakLeft,1.8);
        near(state->preLimiterPeakRight,1.8);
        check(state->limiterReducing && state->limiterGain<1.0f,
              "Safety limiter did not report gain reduction");
        state->safetyLimiterEnabled=false;
        state->render(out.getArrayOfWritePointers(),4,2,48000);
        near(out.getSample(2,0),1.0); // final output hard-clamp with limiter bypassed
        check(!state->limiterReducing,"Limiter indicator stayed on while bypassed");
    });
    test("Direct output bypasses Master mute, pan, gain and limiter", [&] {
        auto show=makeShow();show.masterMuted=true;show.masterPan=-1;
        show.columns[0].masterSend=false;
        show.columns[0].directOutputEnabled=true;
        show.columns[0].output=2;
        show.columns[0].pan=.5f;
        ClipMap media;media[show.rows[0].cells[0].id]=tone(.4f);
        auto state=makeRenderState(show,media);
        state->trigger(show.rows[0].cells[0].id,false);
        juce::AudioBuffer<float> out(4,1);
        state->render(out.getArrayOfWritePointers(),4,1,48000);
        near(out.getSample(0,0),0);
        near(out.getSample(2,0),.2);
        near(out.getSample(3,0),.4);
        near(state->preLimiterPeakLeft,0);
    });
    test("v10 routing fields survive roundtrip; old documents migrate to safe destinations", [] {
        auto show=makeShow();
        show.masterOutput=4;show.safetyLimiterEnabled=false;
        show.columns[0].masterSend=false;
        show.columns[0].directOutputEnabled=true;
        show.columns[0].directPostFader=false;
        show.columns[0].output=2;
        Show parsed;
        check(decodeShow(encodeShow(show),parsed).wasOk(),"New routing project unreadable");
        check(parsed.masterOutput==4 && !parsed.safetyLimiterEnabled &&
              !parsed.columns[0].masterSend && parsed.columns[0].directOutputEnabled &&
              !parsed.columns[0].directPostFader && parsed.columns[0].output==2,
              "New routing fields lost across save/load");
        auto legacy=juce::JSON::parse(encodeShow(show));
        auto* root=legacy.getDynamicObject();
        root->removeProperty("masterOutput");
        root->removeProperty("safetyLimiterEnabled");
        auto& columns=*root->getProperty("columns").getArray();
        columns.getReference(0).getDynamicObject()->removeProperty("masterSend");
        columns.getReference(0).getDynamicObject()->removeProperty("directOutputEnabled");
        columns.getReference(0).getDynamicObject()->removeProperty("directPostFader");
        check(decodeShow(juce::JSON::toString(legacy),parsed).wasOk(),
              "Legacy v10 routing project not accepted");
        check(parsed.masterOutput==0 && parsed.safetyLimiterEnabled &&
              !parsed.columns[0].masterSend && parsed.columns[0].directOutputEnabled &&
              parsed.columns[0].directPostFader,
              "Legacy nondefault direct output did not migrate correctly");
        columns.getReference(0).getDynamicObject()->setProperty("output",0);
        check(decodeShow(juce::JSON::toString(legacy),parsed).wasOk() &&
              parsed.columns[0].masterSend && !parsed.columns[0].directOutputEnabled,
              "Legacy default stereo path was not routed to Master");
        auto invalid=makeShow(); invalid.masterOutput=3;
        check(validateShow(invalid).failed(),"Odd Master stereo output was accepted");
    });
    test("Master/Direct output collision is flagged by preflight",[] {
        auto show=makeShow();
        show.columns[0].masterSend=true;
        show.columns[0].directOutputEnabled=true;
        show.columns[0].output=0;
        const auto report=preflightShow(show,true,{},true);
        check(report.warningCount()>0,
              "Collision on Main Out bypassed the operator preflight warning");
    });
    test("Master/Direct controls are present without changing MIDI and OSC strips",[] {
        MainComponent ui(false);
        addColumn(ui.show,ColumnType::midi,"MIDI");
        addColumn(ui.show,ColumnType::osc,"OSC");
        auto* utility=cueViewFixtures::find<ColumnUtilities>(ui);
        utility->refresh();ui.resized();
        auto* first=utility->getChildComponent(0);
        check(dynamic_cast<MasterSendButton*>(controlsV3::named(*first,"columnMasterSend"))!=nullptr,
              "Circular L/R control missing");
        check(dynamic_cast<juce::TextButton*>(controlsV3::named(*first,"columnDirectPrePost"))!=nullptr,
              "PRE/POST selector missing");
        check(controlsV3::named(ui,"masterMainOutput")!=nullptr,
              "Master Main Output selection missing");
        check(controlsV3::named(ui,"masterSafetyLimiter")!=nullptr &&
              controlsV3::named(ui,"masterLimiterIndicator")!=nullptr,
              "Master limiter toggle or reduction indicator missing");
        ui.setColumnMasterSend(0,false);
        ui.setColumnDirectEnabled(0,true);
        ui.setColumnOutput(0,2);
        ui.setColumnDirectPostFader(0,false);
        ui.setMasterOutput(4);
        ui.setSafetyLimiter(false);
        check(!ui.show.columns[0].masterSend && ui.show.columns[0].directOutputEnabled &&
              !ui.show.columns[0].directPostFader && ui.show.masterOutput==4 &&
              !ui.show.safetyLimiterEnabled,"Routing controls did not update the model");
        check(!controlsV3::named(*utility->getChildComponent(1),"columnGainDb")->isVisible(),
              "Audio changes made MIDI strip visible");
        check(!controlsV3::named(*utility->getChildComponent(2),"columnGainDb")->isVisible(),
              "Audio changes made OSC strip visible");
    });
}
