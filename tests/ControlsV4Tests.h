#pragma once
void controlsV4Tests() {
    using namespace cueViewFixtures;
    test("utility routing replaces inspector output and MIDI OSC have no audio controls", [] {
        MainComponent ui(false); addColumn(ui.show,ColumnType::midi,{}); addColumn(ui.show,ColumnType::osc,{});
        auto* utilities=find<ColumnUtilities>(ui); utilities->refresh();
        check(utilities->getNumChildComponents()==9,"Three utility strips and six Master controls expected");
        for (const auto* name: {"masterAudioDb","masterPan","masterMute","masterMainOutput","masterSafetyLimiter","masterLimiterIndicator"})
            check(controlsV3::named(*utilities,name)!=nullptr,"A Master control is missing");
        for (int c=0;c<3;++c) {
            auto* strip=utilities->getChildComponent(c);
            auto* output=dynamic_cast<juce::ComboBox*>(controlsV3::named(*strip,"columnOutput"));
            check(output && output->isVisible()==(c!=2),"Audio/MIDI routing or OSC destination visibility incorrect");
            if (c!=2) check(output->isEnabled(),"Audio/MIDI routing unavailable");
            else {
                auto* destination=dynamic_cast<juce::TextButton*>(controlsV3::named(*strip,"columnOscDestination"));
                check(destination && destination->isVisible(),"OSC UDP destination missing");
                check(destination->getButtonText()=="127.0.0.1:9000","OSC route default incorrect");
            }
            for (const auto* name : {"columnGainDb","columnPan","columnMute","columnPhase"})
                check(controlsV3::named(*strip,name)->isVisible()==(c==0),"MIDI OSC utility area is not blank");
        }
        int combos=0; for (auto* child : ui.getChildren()) if (dynamic_cast<juce::ComboBox*>(child)) ++combos;
        check(combos==2,"Unexpected combo box in inspector after meter selector removal");
    });
    test("column output combo reroutes actual PCM live without changing inspector drafts", [] {
        MainComponent ui(false); ui.show.columns[0].output=2;
        ui.show.columns[0].directOutputEnabled=true;
        ui.show.columns[0].masterSend=false;
        auto clip=std::make_shared<AudioClip>(); clip->sampleRate=4; clip->samples.setSize(1,8);
        for (int s=0;s<8;++s) clip->samples.setSample(0,s,.1f*(s+1));
        const int id=ui.show.rows[0].cells[0].id; ui.clips[id]=clip;
        ui.engine.connected.store(true); ui.engine.publish(makeRenderState(ui.show,ui.clips));
        auto* utilities=find<ColumnUtilities>(ui); utilities->refresh();
        auto* output=dynamic_cast<juce::ComboBox*>(controlsV3::named(*utilities,"columnOutput"));
        check(output->getSelectedId()==4,"Saved absent direct route lost");
        ui.triggerCell(); juce::AudioBuffer<float> out(4,1);
        auto block=[&] { ui.engine.collectRetired(); ui.engine.processBlock(out.getArrayOfWritePointers(),4,1,4); };
        block(); near(out.getSample(0,0),0); near(out.getSample(2,0),.1);
        auto* name=dynamic_cast<juce::TextEditor*>(controlsV3::named(ui,"cueName")); name->setText("Draft preserved",false);
        output->setSelectedId(2,juce::sendNotificationSync); block();
        near(out.getSample(0,0),.2); near(out.getSample(2,0),0); near(ui.engine.getCellPlayback(id).positionSeconds,.5);
        check(name->getText()=="Draft preserved","Routing discarded inspector text");
        check(ui.show.columns[0].output==0 && ui.hasUnsavedChanges(),"Routing model not updated");
    });
    test("auto stop row toggle controls overlap through sequential GO and persists", [] {
        MainComponent ui(false); addRow(ui.show,{}); ui.engine.connected.store(true);
        auto clip=std::make_shared<AudioClip>(); clip->sampleRate=4; clip->samples.setSize(1,16);
        for (int s=0;s<16;++s) clip->samples.setSample(0,s,.2f);
        for (const auto& row : ui.show.rows) ui.clips[row.cells[0].id]=clip;
        ui.engine.publish(makeRenderState(ui.show,ui.clips));
        auto* overlay=find<CueMetricsOverlay>(ui);check(overlay!=nullptr,"Sticky Auto Stop control missing");
        auto sample=[&] { float value=0; float* out[]{&value}; ui.engine.collectRetired(); ui.engine.processBlock(out,1,1,4); return value; };
        ui.goNext(); near(sample(),.2); ui.goNext(); near(sample(),.2);
        check(!ui.engine.getCellPlayback(ui.show.rows[0].cells[0].id).playing,"Prior cue still marked playing");
        click(*overlay,117.0f,(float)CueGrid::headerHeight+16);
        check(!ui.show.rows[0].stopOnNext,"Toggle hitbox did not disable");
        ui.select(0,0); ui.goNext(); near(sample(),.2); ui.goNext(); near(sample(),.4);
        check(ui.engine.getCellPlayback(ui.show.rows[0].cells[0].id).playing,"Overlap row was stopped");
        Show decoded; check(decodeShow(encodeShow(ui.show),decoded).wasOk(),"Toggle save invalid");
        check(!decoded.rows[0].stopOnNext,"Toggle not preserved");
    });
}
