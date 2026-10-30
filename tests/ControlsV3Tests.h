#pragma once
#include "GainMeterFader.h"
#include "OperatorTextPrompt.h"
namespace controlsV3 {
juce::Component* named(juce::Component& root,const juce::String& name) {
    if (root.getName()==name) return &root;
    for (auto* child : root.getChildren()) if (auto* found=named(*child,name)) return found;
    return nullptr;
}
}
void controlsV3Tests() {
    using namespace cueViewFixtures;
    test("Inline cue rename selects existing text without a popup", [] {
        MainComponent ui(false);
        auto* title=dynamic_cast<juce::TextEditor*>(controlsV3::named(ui,"cueName"));
        title->setText("Scena",false); title->onReturnKey();
        ui.renameTitle(false,0);
        auto* editor=dynamic_cast<juce::TextEditor*>(
            controlsV3::named(ui,"inlineNameEditor"));
        check(editor && editor->isVisible(),
              "Inline cue rename must use a visible in-place editor.");
        check(editor->getHighlightedRegion().getLength()==5,
              "Renaming should preselect the complete current name.");
        check(juce::ModalComponentManager::getInstance()->getNumModalComponents()==0,
              "Renaming must not open a modal popup.");
        editor->onEscapeKey();
    });
    test("cell play stop and row stop hitboxes are independent and preserve sequence", [] {
        MainComponent ui(false); addRow(ui.show,{}); addColumn(ui.show,ColumnType::audio,{});
        ui.show.rows[0].cells[1].followsRow=false;
        for (int r=0;r<2;++r) for (int c=0;c<2;++c) {
            auto clip=std::make_shared<AudioClip>(); clip->samples.setSize(1,48000);
            for (int s=0;s<48000;++s) clip->samples.setSample(0,s,r==0 ? (c==0 ? .1f : .2f) : .4f);
            ui.clips[ui.show.rows[r].cells[c].id]=clip;
        }
        ui.select(1,0); ui.engine.connected.store(true); ui.engine.publish(makeRenderState(ui.show,ui.clips));
        auto* grid=find<CueGrid>(ui);
        auto sample=[&] { float value=0; float* out[]{&value}; ui.engine.collectRetired(); ui.engine.processBlock(out,1,1,48000); return value; };
        // Click the actual painted centres, not historical offsets.
        // In particular, columnWidth-40 is now the exclusive right edge
        // of the expanded cell Play button, not an interior point.
        const auto cellPoint=[&](int row,int col,bool stop) {
            const int x=CueGrid::nameWidth+col*CueGrid::columnWidth;
            const int y=CueGrid::headerHeight+row*CueGrid::rowHeight;
            const auto rect=stop
                ? CueGrid::cellStopBounds(x,CueGrid::columnWidth,y)
                : CueGrid::cellPlayBounds(x,CueGrid::columnWidth,y);
            return juce::Point<float>((float)rect.getCentreX(),
                                      (float)(rect.getCentreY()+CueGrid::rowContentInset));
        };
        const auto clickCell=[&](int row,int col,bool stop) {
            const auto point=cellPoint(row,col,stop);
            click(*grid,point.x,point.y);
        };
        clickCell(0,0,false); near(sample(),.1);
        check(ui.nextCueIndex()==1,"Cell play changed sequential GO target");
        clickCell(0,1,false); near(sample(),.3);
        clickCell(1,0,false); near(sample(),.7);
        clickCell(0,0,true); near(sample(),.6);
        const auto rowStop=CueGrid::rowStopBounds(CueGrid::headerHeight);
        click(*grid,(float)rowStop.getCentreX(),
              (float)(rowStop.getCentreY()+CueGrid::rowContentInset)); near(sample(),.4);
        check(ui.nextCueIndex()==1,"Stop changed sequential GO target");
    });
    test("column mixer widgets affect live PCM and stay aligned when scrolled", [] {
        MainComponent ui(false); auto clip=std::make_shared<AudioClip>(); clip->samples.setSize(2,48000);
        for (int s=0;s<48000;++s) { clip->samples.setSample(0,s,.2f); clip->samples.setSample(1,s,.4f); }
        ui.clips[ui.show.rows[0].cells[0].id]=clip; ui.engine.connected.store(true); ui.engine.publish(makeRenderState(ui.show,ui.clips));
        auto* utilities=find<ColumnUtilities>(ui); check(utilities!=nullptr,"Utility strip absent");
        auto* gain=dynamic_cast<GainMeterFader*>(controlsV3::named(*utilities,"columnGainDb"));
        auto* pan=dynamic_cast<juce::Slider*>(controlsV3::named(*utilities,"columnPan"));
        auto* mute=dynamic_cast<juce::ToggleButton*>(controlsV3::named(*utilities,"columnMute"));
        auto* phase=dynamic_cast<juce::ToggleButton*>(controlsV3::named(*utilities,"columnPhase"));
        check(gain && pan && mute && phase,"Missing mixer widget");
        ui.triggerCell();
        auto render=[&] { juce::AudioBuffer<float> out(2,1); ui.engine.collectRetired(); ui.engine.processBlock(out.getArrayOfWritePointers(),2,1,48000); return out; };
        near(render().getSample(0,0),.2);
        gain->setGainDb(-6,true); const auto level=std::pow(10.0,-6.0/20); near(render().getSample(0,0),.2*level);
        pan->setValue(1,juce::sendNotificationSync); auto out=render(); near(out.getSample(0,0),0); near(out.getSample(1,0),.4*level);
        phase->setToggleState(true,juce::dontSendNotification); phase->onClick(); near(render().getSample(1,0),-.4*level);
        mute->setToggleState(true,juce::dontSendNotification); mute->onClick(); near(render().getSample(1,0),0);
        check(ui.engine.activeVoices.load()==1,"Mute stopped playback");
        const int before=gain->getParentComponent()->getX(); utilities->syncScroll(100);
        check(gain->getParentComponent()->getX()==before-100,"Mixer not aligned to horizontal scroll");
        ui.show.columns[0].gain=.5f; ui.show.columns[0].pan=.123f; utilities->refresh();
        mute->setToggleState(false,juce::dontSendNotification); mute->onClick();
        near(ui.show.columns[0].gain,.5); near(ui.show.columns[0].pan,.123);
    });
}
