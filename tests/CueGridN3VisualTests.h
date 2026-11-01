#pragma once
#include "CueGridGeometry.h"

// N3.2 native rendering/interaction regressions. The test observes the actual
// JUCE CueGrid paint and pinned views, not the stand-alone HTML mockup.
void cueGridN3VisualTests() {
    test("N3.2 native header has restrained type rules and original geometry",[] {
        using namespace cuelist;
        MainComponent ui(false);
        addColumn(ui.show,ColumnType::midi,"MIDI triggers");
        addColumn(ui.show,ColumnType::osc,"OSC lights");
        addColumn(ui.show,ColumnType::control,"Control");
        ui.setSize(1647,895);
        auto* grid=cueViewFixtures::find<CueGrid>(ui);
        check(grid!=nullptr,"Native Cue Grid is missing");
        const auto pixels=grid->createComponentSnapshot(
            {0,0,CueGridGeometry::nameWidth+
                 CueGridGeometry::columnWidth*4,CueGridGeometry::headerHeight});
        check(pixels.isValid() && pixels.getHeight()==CueGrid::headerHeight,
              "Pinned native header cannot be rendered");
        const auto cueRule=pixels.getPixelAt(18,1);
        const auto audioRule=pixels.getPixelAt(grid->columnX(0)+12,1);
        const auto midiRule=pixels.getPixelAt(grid->columnX(1)+12,1);
        const auto oscRule=pixels.getPixelAt(grid->columnX(2)+12,1);
        check(cueRule.getGreen()>cueRule.getRed() &&
              audioRule.getGreen()>audioRule.getRed() &&
              midiRule.getRed()>midiRule.getGreen() &&
              oscRule.getBlue()>oscRule.getGreen(),
              "Cue/header type accent colours have lost their distinctions");
        const auto title=CueGrid::rowNameBounds(0);
        const auto play=CueGrid::rowPlayBounds(0);
        const auto stop=CueGrid::rowStopBounds(0);
        const auto menu=CueGrid::rowMenuBounds(0);
        check(grid->columnX(0)==CueGridGeometry::nameWidth &&
              title.getWidth()==196 && !title.intersects(play) &&
              !title.intersects(stop) && !title.intersects(menu) &&
              play==juce::Rectangle<int>(CueGrid::nameWidth-114,31,29,24) &&
              stop==juce::Rectangle<int>(CueGrid::nameWidth-80,31,29,24) &&
              menu==juce::Rectangle<int>(CueGrid::nameWidth-46,31,29,24),
              "The extended cue title overlaps the relocated transport targets");
    });
    test("N3.2 pinned title hover reveals untruncated cue and column names",[] {
        using namespace cuelist;
        MainComponent ui(false);
        ui.show.rows[0].name="A very long technical cue name — full title";
        ui.show.columns[0].name="A very long AUDIO channel title";
        ui.setSize(1440,900);
        auto* pinned=cueViewFixtures::find<PinnedCueHeaders>(ui);
        auto* column=cueViewFixtures::find<PinnedColumnHeaders>(ui);
        check(pinned && column,"Pinned Cue/Column overlays disappeared");
        const auto cueY=(float)CueGridGeometry::rowTop(0)+16.0f;
        pinned->mouseMove(cueViewFixtures::event(
            *pinned,{115.0f,cueY},{115.0f,cueY}));
        check(pinned->getTooltip()==ui.show.rows[0].name,
              "Full cue title is unavailable under mouse hover");
        pinned->mouseMove(cueViewFixtures::event(
            *pinned,{54.0f,cueY},{54.0f,cueY}));
        check(pinned->getTooltip().isEmpty(),
              "Cue title tooltip covers the independent GO hitbox");
        column->mouseMove(cueViewFixtures::event(
            *column,{12.0f,20.0f},{12.0f,20.0f}));
        check(column->getTooltip()==ui.show.columns[0].name,
              "Column full title is unavailable in pinned heading");
        column->mouseMove(cueViewFixtures::event(
            *column,{-100.0f,20.0f},{-100.0f,20.0f}));
        check(column->getTooltip().isEmpty(),
              "Column tooltip leaked outside its actual hit area");
    });
    test("N3.2 selecting the cue name never triggers GO or Stop",[] {
        using namespace cuelist;
        MainComponent ui(false);
        addRow(ui.show,"Second cue");
        ui.setSize(1647,895);
        auto* pinned=cueViewFixtures::find<PinnedCueHeaders>(ui);
        check(pinned!=nullptr,"Pinned cue header is missing");
        const auto second=(float)CueGridGeometry::rowTop(1)+
                          CueGridGeometry::rowContentInset+15.0f;
        cueViewFixtures::click(*pinned,113.0f,second);
        check(ui.selectedRow==1 &&
              ui.show.rows[1].name=="Second cue" &&
              ui.show.rows.size()==2,
              "Click on Cue Name must only select, never start or delete a cue");
    });
    test("N3.2 follow-up selects the extended cue title without GO and renames inline",[] {
        using namespace cuelist;
        MainComponent ui(false);
        addRow(ui.show,"Second cue");
        auto* pinned=cueViewFixtures::find<PinnedCueHeaders>(ui);
        auto* editor=dynamic_cast<juce::TextEditor*>(
            controlsV3::named(ui,"inlineNameEditor"));
        check(pinned!=nullptr && editor!=nullptr,
              "Pinned Cue header or native inline rename editor missing");
        ui.select(1,0);
        const auto title=CueGrid::rowNameBounds(0);
        const float x=(float)title.getRight()-8.0f;
        const float y=(float)(CueGrid::headerHeight+
            CueGrid::rowContentInset+title.getCentreY());
        cueViewFixtures::click(*pinned,x,y);
        check(ui.selectedRow==0 && ui.nextCueIndex()==0,
              "Click on newly expanded title triggered GO or selected wrong cue");
        pinned->mouseDoubleClick(cueViewFixtures::event(
            *pinned,{x,y},{x,y},2));
        check(editor->isVisible() &&
              editor->getWidth()==title.getWidth() &&
              editor->getHeight()==title.getHeight(),
              "Inline rename editor is not aligned to the new title bounds");
        editor->setText("New cue name",juce::dontSendNotification);
        editor->onReturnKey();
        check(ui.show.rows[0].name=="New cue name" && !editor->isVisible(),
              "Inline rename must still commit after the two-tier change");
    });

    test("N3.3 selected row spans empty columns and trailing grid",[] {
        using namespace cuelist;
        MainComponent ui(false);
        addRow(ui.show,"Standby");
        addColumn(ui.show,ColumnType::midi,"MIDI 1");
        ui.setSize(1647,895);
        ui.resized();
        auto* grid=cueViewFixtures::find<CueGrid>(ui);
        check(grid!=nullptr,"Native CueGrid is missing");
        const int sampleX=grid->columnX(2)+12; // after both lanes
        const int sampleY=CueGridGeometry::rowTop(0)+51;
        const auto before=grid->createComponentSnapshot(
            {sampleX,sampleY,1,1});
        check(before.isValid(),"Cannot inspect selected row background");
        const auto colourBefore=before.getPixelAt(0,0);
        ui.select(1,0);
        const auto after=grid->createComponentSnapshot(
            {sampleX,sampleY,1,1});
        check(after.isValid() && colourBefore!=after.getPixelAt(0,0),
              "Selection did not extend across full grid width");
    });

    test("N3.3 empty slot hover changes paint but never show or transport",[] {
        using namespace cuelist;
        MainComponent ui(false);
        addRow(ui.show,"Second");
        addColumn(ui.show,ColumnType::osc,"OSC");
        ui.setSize(1647,895);
        ui.resized();
        auto* grid=cueViewFixtures::find<CueGrid>(ui);
        check(grid!=nullptr,"CueGrid is missing");
        const int cellX=grid->columnX(1);
        const int cellY=CueGridGeometry::rowTop(1);
        auto sample=[&] {
            const auto bitmap=grid->createComponentSnapshot(
                {cellX+12,cellY+14,1,1});
            check(bitmap.isValid(),"Cell snapshot could not be rendered");
            return bitmap.getPixelAt(0,0);
        };
        const auto modelBefore=encodeShow(ui.show);
        const int beforeNext=ui.nextCueIndex();
        const int beforeSelected=ui.selectedRow;
        const auto idle=sample();
        const juce::Point<float> cursor{(float)cellX+12,(float)cellY+14};
        grid->mouseMove(cueViewFixtures::event(*grid,cursor,cursor));
        const auto hover=sample();
        check(hover!=idle,"Empty-cell hover is not visually distinguishable");
        grid->mouseExit(cueViewFixtures::event(*grid,cursor,cursor));
        check(sample()==idle,"Empty cell hover persisted after leaving");
        check(encodeShow(ui.show)==modelBefore &&
              ui.nextCueIndex()==beforeNext &&
              ui.selectedRow==beforeSelected,
              "Purely visual hover modified the show, selection or transport");
    });

    test("N3.3 real media accent and missing-media warning stay visible",[] {
        using namespace cuelist;
        MainComponent ui(false);
        addRow(ui.show,"Second");
        ui.setSize(1647,895);
        auto* grid=cueViewFixtures::find<CueGrid>(ui);
        check(grid!=nullptr,"CueGrid is missing");
        // A missing file must produce a visible error without creating a clip.
        ui.show.rows[1].cells[0].file="C:/missing-production-media.wav";
        ui.resized();
        const int x=grid->columnX(0);
        const int y=CueGridGeometry::rowTop(1)+CueGrid::rowContentInset;
        auto pixel=[&](int cellX,int cellY) {
            const auto bitmap=grid->createComponentSnapshot(
                {cellX,cellY,1,1});
            check(bitmap.isValid(),"Cell preview unavailable");
            return bitmap.getPixelAt(0,0);
        };
        const auto errorMark=pixel(x+5,y+21);
        check(errorMark.getRed()>errorMark.getGreen(),
              "Missing media lost its red error marker");
        ui.show.rows[1].cells[0].file.clear();
        auto pcm=std::make_shared<AudioClip>();
        pcm->sampleRate=48000;
        pcm->samples.setSize(1,64);
        pcm->samples.clear();
        ui.clips[ui.show.rows[1].cells[0].id]=pcm;
        ui.resized();
        const auto loadedMark=pixel(x+5,y+21);
        check(loadedMark.getGreen()>loadedMark.getRed(),
              "Loaded audio lost its type-colour cue");
    });

    test("N3.3 empty cell has no invisible transport dead zones",[] {
        using namespace cuelist;
        MainComponent ui(false);
        addRow(ui.show,"Second");
        addColumn(ui.show,ColumnType::midi,"MIDI");
        ui.setSize(1647,895);
        ui.resized();
        auto* grid=cueViewFixtures::find<CueGrid>(ui);
        check(grid!=nullptr,"CueGrid missing");
        ui.select(0,0);
        const int x=grid->columnX(1)+grid->columnWidthAt(1)-20;
        const int y=CueGridGeometry::rowTop(1)+
                    CueGrid::rowContentInset+34;
        cueViewFixtures::click(*grid,(float)x,(float)y);
        check(ui.selectedRow==1 && ui.selectedColumn==1 &&
              ui.show.rows.size()==2,
              "Hidden disabled Stop region prevented empty cell selection");
    });

}
