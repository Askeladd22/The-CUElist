#pragma once

void timelineV3Tests() {
    using namespace cueViewFixtures;
    test("timeline zoom controls pan and fit keep ruler coordinates consistent", [] {
        MainComponent ui(false);
        auto* timeline=find<Timeline>(ui); check(timeline!=nullptr,"Timeline absent"); timeline->setSize(800,250);
        auto* scroll=find<juce::ScrollBar>(*timeline); check(scroll!=nullptr,"Timeline scrollbar absent");
        click(*timeline,708,22);
        near(scroll->getCurrentRangeSize(),5); near(scroll->getCurrentRangeStart(),2.5);
        const auto seek=[&](float x) {
            click(*timeline,x,60);
            float samples[16]{}; float* outputs[]{samples}; ui.engine.processBlock(outputs,1,16,48000);
            return ui.engine.playheadSeconds.load();
        };
        near(seek(300),3.75);
        scroll->setCurrentRangeStart(5,juce::sendNotificationSync);
        near(seek(300),6.25);
        click(*timeline,674,22); near(scroll->getCurrentRangeSize(),10); near(scroll->getCurrentRangeStart(),0);
        click(*timeline,708,22); click(*timeline,754,22);
        near(scroll->getCurrentRangeSize(),10); near(seek(460),5);
    });
    test("timeline zoomed clip drag uses frozen scale through resize and fit includes offsets", [] {
        MainComponent ui(false);
        auto clip=std::make_shared<AudioClip>(); clip->samples.setSize(1,48000); clip->samples.clear();
        ui.clips[ui.show.rows[0].cells[0].id]=clip;
        ui.setClipStart(0,3);
        auto* timeline=find<Timeline>(ui); timeline->setSize(800,250);
        click(*timeline,708,22);
        const juce::Point<float> from{236,94}, to{300,94};
        timeline->mouseDown(event(*timeline,from,from)); timeline->setSize(1000,250);
        timeline->mouseDrag(event(*timeline,to,from,1,true)); timeline->mouseUp(event(*timeline,to,from,1,true));
        near(ui.show.rows[0].cells[0].startSeconds,3.5);
        ui.setClipStart(0,20); click(*timeline,954,22);
        auto* scroll=find<juce::ScrollBar>(*timeline);
        near(scroll->getCurrentRangeSize(),23.1); near(scroll->getCurrentRangeStart(),0);
    });
    test("timeline magnet snaps audio clips by musical tempo and can be disabled",[] {
        MainComponent ui(false);
        auto clip=std::make_shared<AudioClip>(); clip->samples.setSize(1,48000); clip->samples.clear();
        ui.clips[ui.show.rows[0].cells[0].id]=clip;
        auto* timeline=find<Timeline>(ui); timeline->setSize(800,250);
        const juce::Point<float> from{160,94},to{195,94};
        timeline->mouseDown(event(*timeline,from,from));
        timeline->mouseDrag(event(*timeline,to,from,1,true));
        timeline->mouseUp(event(*timeline,to,from,1,true));
        near(ui.show.rows[0].cells[0].startSeconds,.5);
        for (auto* component:timeline->getChildren())
            if (component->getName()=="timelineSnapToggle") dynamic_cast<juce::TextButton*>(component)->onClick();
        ui.setClipStart(0,0);
        timeline->mouseDown(event(*timeline,from,from));
        timeline->mouseDrag(event(*timeline,to,from,1,true));
        timeline->mouseUp(event(*timeline,to,from,1,true));
        near(ui.show.rows[0].cells[0].startSeconds,35.0*10/640);
    });
    test("timeline MIDI double-click opens existing clip editor",[] {
        MainComponent ui(false); addColumn(ui.show,ColumnType::midi,{});
        ui.show.rows[0].cells[1].midi=MidiClip{4,{{0,1,60,100,1}}};
        auto* timeline=find<Timeline>(ui); timeline->setSize(800,250);
        const juce::Point<float> p{160,136};
        timeline->mouseDoubleClick(event(*timeline,p,p,2));
        check(cueViewFixtures::find<PianoRollEditor>(ui)==nullptr,"Editor accidentally nested in main view");
        check(juce::ModalComponentManager::getInstance()->getModalComponent(0)!=nullptr,"MIDI timeline double-click did not open editor");
    });
    test("timeline Ctrl wheel anchors time and changing row cancels a seek", [] {
        MainComponent ui(false); addRow(ui.show,{});
        auto* timeline=find<Timeline>(ui); timeline->setSize(800,250);
        const auto time=juce::Time::getCurrentTime();
        const juce::MouseEvent e{juce::Desktop::getInstance().getMainMouseSource(),{300,60},
            juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),1,0,0,0,0,
            timeline,timeline,time,{300,60},time,1,false};
        juce::MouseWheelDetails wheel{}; wheel.deltaY=1.0f/3;
        timeline->mouseWheelMove(e,wheel);
        auto* scroll=find<juce::ScrollBar>(*timeline);
        check(std::abs(scroll->getCurrentRangeSize()-5)<.001,"Ctrl wheel did not zoom");
        check(std::abs(scroll->getCurrentRangeStart()-1.25)<.001,"Ctrl wheel anchor moved");
        timeline->mouseDown(event(*timeline,{300,60},{300,60}));
        ui.select(1,0);
        timeline->mouseDrag(event(*timeline,{460,60},{300,60},1,true));
        timeline->mouseUp(event(*timeline,{460,60},{300,60},1,true));
        near(scroll->getCurrentRangeSize(),10); near(scroll->getCurrentRangeStart(),0);
    });
}
