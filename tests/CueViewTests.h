#pragma once

namespace cueViewFixtures {
template <typename T> T* find(juce::Component& parent) {
    if (auto* result=dynamic_cast<T*>(&parent)) return result;
    for (auto* child : parent.getChildren()) if (auto* result=find<T>(*child)) return result;
    return nullptr;
}
juce::MouseEvent event(juce::Component& target, juce::Point<float> position,
                       juce::Point<float> down, int clicks=1, bool dragged=false) {
    const auto time=juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),position,
            juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,
            &target,&target,time,down,time,clicks,dragged};
}
void click(juce::Component& target, float x,float y,int clicks=1) {
    target.mouseDown(event(target,{x,y},{x,y},clicks));
    target.mouseUp(event(target,{x,y},{x,y},clicks));
}
}
void cueViewTests() {
    using namespace cueViewFixtures;
    test("grid arrow hitboxes move middle rows and columns one step", [] {
        MainComponent ui(false); addRow(ui.show,{}); addRow(ui.show,{});
        addColumn(ui.show,ColumnType::audio,{}); addColumn(ui.show,ColumnType::audio,{});
        auto* grid=find<CueGrid>(ui); check(grid!=nullptr,"Grid absent");
        const int row=ui.show.rows[1].id, column=ui.show.columns[1].id;
        click(*grid,16,CueGrid::headerHeight+CueGrid::rowHeight+CueGrid::rowContentInset+22); check(ui.show.rows[0].id==row,"Middle row up arrow did not move one step");
        click(*grid,16,CueGrid::headerHeight+CueGrid::rowContentInset+36); check(ui.show.rows[1].id==row,"Row down arrow did not move one step");
        click(*grid,40,CueGrid::headerHeight+CueGrid::rowHeight+CueGrid::rowContentInset+22,2); check(ui.show.rows[1].id==row,"Second double click reordered again");
        click(*grid,CueGrid::nameWidth+CueGrid::columnWidth+(CueGrid::columnWidth-54),21); check(ui.show.columns[0].id==column,"Middle column left arrow did not move one step");
        click(*grid,CueGrid::nameWidth+CueGrid::columnWidth-34,21); check(ui.show.columns[1].id==column,"Column right arrow did not move one step");
        click(*grid,CueGrid::nameWidth+CueGrid::columnWidth+(CueGrid::columnWidth-34),21,2); check(ui.show.columns[1].id==column,"Second double click moved column");
    });
    test("grid clip drag swaps real identities and retains loaded audio", [] {
        MainComponent ui(false); addRow(ui.show,{}); addColumn(ui.show,ColumnType::audio,{});
        auto clip=std::make_shared<AudioClip>(); clip->samples.setSize(1,48000); clip->samples.clear();
        const int source=ui.show.rows[0].cells[0].id, target=ui.show.rows[1].cells[1].id;
        ui.clips[source]=clip;
        auto* grid=find<CueGrid>(ui); check(grid!=nullptr,"Grid absent");
        const juce::Point<float> from{(float)CueGrid::nameWidth+10,
                                      (float)CueGrid::headerHeight+CueGrid::rowContentInset+20};
        const juce::Point<float> to{(float)CueGrid::nameWidth+CueGrid::columnWidth+10,
                                    (float)CueGrid::headerHeight+CueGrid::rowHeight+CueGrid::rowContentInset+20};
        grid->mouseDown(event(*grid,from,from)); grid->mouseDrag(event(*grid,to,from,1,true)); grid->mouseUp(event(*grid,to,from,1,true));
        check(ui.show.rows[1].cells[1].id==source && ui.show.rows[0].cells[0].id==target,"Grid drag did not swap");
        check(ui.clips.at(source)==clip && ui.selectedRow==1 && ui.selectedColumn==1,"Dragged clip identity or selection lost");
    });
    test("timeline clip drag changes offset and ruler drag seeks audio transport", [] {
        MainComponent ui(false);
        auto clip=std::make_shared<AudioClip>(); clip->samples.setSize(1,48000); clip->samples.clear();
        ui.clips[ui.show.rows[0].cells[0].id]=clip;
        auto* timeline=find<Timeline>(ui); check(timeline!=nullptr,"Timeline absent"); timeline->setSize(800,250);
        const juce::Point<float> from{160,94}, to{224,94};
        timeline->mouseDown(event(*timeline,from,from)); timeline->mouseDrag(event(*timeline,to,from,1,true)); timeline->mouseUp(event(*timeline,to,from,1,true));
        near(ui.show.rows[0].cells[0].startSeconds,1);
        const juce::Point<float> rulerFrom{268,60}, rulerTo{460,60};
        timeline->mouseDown(event(*timeline,rulerFrom,rulerFrom)); timeline->mouseDrag(event(*timeline,rulerTo,rulerFrom,1,true)); timeline->mouseUp(event(*timeline,rulerTo,rulerFrom,1,true));
        float samples[16]{}; float* outputs[]{samples}; ui.engine.processBlock(outputs,1,16,48000);
        check(ui.engine.monitoredRowId.load()==ui.show.rows[0].id,"Ruler did not select row transport");
        near(ui.engine.playheadSeconds.load(),5);
    });
    test("last cue and column delete hitboxes reset to blank usable show", [] {
        MainComponent ui(false); auto* grid=find<CueGrid>(ui); check(grid!=nullptr,"Grid absent");
        const auto rowId=ui.show.rows[0].id, columnId=ui.show.columns[0].id;
        click(*grid,36,CueGrid::headerHeight+CueGrid::rowContentInset+22); check(ui.show.rows.size()==1 && ui.show.rows[0].id!=rowId,"Last cue delete was disabled");
        click(*grid,CueGrid::nameWidth+CueGrid::columnWidth-12,21); check(ui.show.columns.size()==1 && ui.show.columns[0].id!=columnId,"Last column delete was disabled");
        check(validateShow(ui.show).wasOk(),"Delete hitboxes left invalid show");
    });
}
