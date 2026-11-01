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

    test("N3.4 keeps only BPM and cue Auto Stop non-overlapping hit targets",[] {
        using namespace cuelist;
        const auto bpm=CueMetricsOverlay::bpmEditBounds(0);
        const auto toggle=CueMetricsOverlay::autoStopToggleBounds(0);
        check(CueMetricsOverlay::width==184 &&
              bpm==juce::Rectangle<int>(0,0,91,62) &&
              toggle==juce::Rectangle<int>(102,8,78,26) &&
              !bpm.intersects(toggle) &&
              toggle.getRight()<=CueMetricsOverlay::width,
              "N3.4 fixed metrics shrank or lost its independent controls");
    });

    test("N3.4 selected metric row matches the continuous CueGrid surface",[] {
        using namespace cuelist;
        MainComponent ui(false);
        addRow(ui.show,"Second");
        addColumn(ui.show,ColumnType::midi,"MIDI");
        ui.setSize(1920,1080);
        auto* metrics=cueViewFixtures::find<CueMetricsOverlay>(ui);
        auto* grid=cueViewFixtures::find<CueGrid>(ui);
        check(metrics && grid,"Metrics overlay or native grid missing");
        ui.select(1,0);
        const int rowY=CueGridGeometry::rowTop(1)+57;
        const int gridX=grid->columnX((int)ui.show.columns.size())+12;
        const auto cellImage=grid->createComponentSnapshot({gridX,rowY,1,1});
        const auto metricImage=metrics->createComponentSnapshot({84,rowY,1,1});
        check(cellImage.isValid() && metricImage.isValid() &&
              cellImage.getPixelAt(0,0)==metricImage.getPixelAt(0,0),
              "N3.4 fixed row fill differs from the N3.3 selected/NEXT surface");
    });

    test("N3.4 fixed Auto Stop targets only the correct scrolled cue",[] {
        using namespace cuelist;
        MainComponent ui(false);
        for (int r=1;r<13;++r) addRow(ui.show,"Scrolled cue");
        for (int c=1;c<11;++c)
            addColumn(ui.show,c%2==0?ColumnType::midi:ColumnType::osc,"Data");
        ui.setSize(1280,820);
        auto* metrics=cueViewFixtures::find<CueMetricsOverlay>(ui);
        juce::Viewport* view=nullptr;
        for (auto* child:ui.getChildren())
            if (auto* candidate=dynamic_cast<juce::Viewport*>(child)) {
                view=candidate;break;
            }
        check(metrics && view,"N3.4 scroll fixture incomplete");
        const int fixedX=metrics->getX();
        view->setViewPosition(200,115);
        check(metrics->getX()==fixedX &&
              view->getViewPositionY()==115,
              "Fixed metrics moved when columns or rows scrolled");
        const int rowIndex=4;
        const int y=CueGridGeometry::overlayRowTop(rowIndex,115);
        const bool before=ui.show.rows[(size_t)rowIndex].stopOnNext;
        const bool neighbor=ui.show.rows[(size_t)rowIndex+1].stopOnNext;
        cueViewFixtures::click(*metrics,118.0f,(float)y+17.0f);
        check(ui.show.rows[(size_t)rowIndex].stopOnNext!=before &&
              ui.show.rows[(size_t)rowIndex+1].stopOnNext==neighbor,
              "Auto Stop toggled the wrong row after Y scroll");
        const auto changed=encodeShow(ui.show);
        // This used to expand CELLS. It must now be visually and
        // functionally inert; edit exceptions via cell context menus.
        cueViewFixtures::click(*metrics,121.0f,(float)y+46.0f);
        check(encodeShow(ui.show)==changed,
              "Removed CELLS area still performs a hidden action");
    });

    test("N3.4 metric hover is visual only and restores when pointer exits",[] {
        using namespace cuelist;
        MainComponent ui(false);
        ui.setSize(1440,900);
        auto* metrics=cueViewFixtures::find<CueMetricsOverlay>(ui);
        check(metrics!=nullptr,"Fixed metrics overlay missing");
        const auto beforeShow=encodeShow(ui.show);
        const int beforeSelection=ui.selectedRow;
        const int beforeNext=ui.nextCueIndex();
        const int y=CueGridGeometry::rowTop(0);
        const auto sample=[&] {
            // Probe the toggle hover surface, away from rounded corners
            // and the painted switch itself (which must remain unchanged).
            const auto image=metrics->createComponentSnapshot({104,y+20,1,1});
            check(image.isValid(),"Metrics hover paint failed");
            return image.getPixelAt(0,0);
        };
        const auto idle=sample();
        const juce::Point<float> cursor{118.f,(float)y+16.f};
        metrics->mouseMove(cueViewFixtures::event(*metrics,cursor,cursor));
        check(sample()!=idle,"Auto Stop hover is not visible");
        metrics->mouseExit(cueViewFixtures::event(*metrics,cursor,cursor));
        check(sample()==idle,"Metrics hover survived after mouse exit");
        check(encodeShow(ui.show)==beforeShow &&
              ui.selectedRow==beforeSelection &&
              ui.nextCueIndex()==beforeNext,
              "Hover altered the cue document or playback selection");
    });

    test("N3.4B CONT. appears only for excluded Audio and Control cells",[] {
        using namespace cuelist;
        MainComponent ui(false);
        addColumn(ui.show,ColumnType::control,"Control");
        ui.setSize(1647,895);
        auto* grid=cueViewFixtures::find<CueGrid>(ui);
        check(grid!=nullptr,"CueGrid missing");
        const int y=CueGridGeometry::rowTop(0)+CueGrid::rowContentInset;
        const auto badge=CueGrid::cellContinueBadgeBounds(
            grid->columnX(0),grid->columnWidthAt(0),y);
        auto sample=[&] {
            const auto image=grid->createComponentSnapshot(
                {badge.getX()+2,badge.getY()+8,1,1});
            check(image.isValid(),"Native cell exception snapshot invalid");
            return image.getPixelAt(0,0);
        };
        const auto defaultPixel=sample();
        ui.setCellAutoStop(0,0,false);
        ui.setCellAutoStop(0,1,false);
        check(sample()!=defaultPixel,
              "CONT. marker not painted for cell excluded from Auto Stop");
        const auto cursor=badge.toFloat().getCentre();
        grid->mouseMove(cueViewFixtures::event(*grid,cursor,cursor));
        check(grid->getTooltip().contains("Auto Stop"),
              "CONT. badge has no explanation on hover");
        const juce::Point<float> elsewhere{
            (float)grid->columnX(0)+20.f,(float)y+30.f};
        grid->mouseMove(cueViewFixtures::event(*grid,elsewhere,elsewhere));
        check(grid->getTooltip().isEmpty(),
              "CONT. tooltip leaked outside its own indicator");
        cuelist::Show loaded;
        check(decodeShow(encodeShow(ui.show),loaded).wasOk() &&
              !loaded.rows[0].cells[0].stopOnNext &&
              !loaded.rows[0].cells[1].stopOnNext,
              "Cell Auto Stop exceptions no longer persist in existing show schema");
        ui.setCellAutoStop(0,0,true);
        check(sample()==defaultPixel &&
              !ui.show.rows[0].cells[1].stopOnNext,
              "Restoring one cell's default affected other cell exceptions");
    });

    test("N3.4B compressed cells keep exception visible without CELLS mode",[] {
        using namespace cuelist;
        MainComponent ui(false);
        ui.show.columns[0].collapsed=true;
        ui.setSize(1647,895);
        auto* grid=cueViewFixtures::find<CueGrid>(ui);
        check(grid && grid->columnWidthAt(0)==48,
              "Collapsed cue lane contract changed");
        const int x=grid->columnX(0)+grid->columnWidthAt(0)-7;
        const int y=CueGridGeometry::rowTop(0)+CueGrid::rowContentInset+36;
        const auto sample=[&] {
            const auto image=grid->createComponentSnapshot({x,y,1,1});
            check(image.isValid(),"Collapsed exception snapshot invalid");
            return image.getPixelAt(0,0);
        };
        const auto before=sample();
        ui.setCellAutoStop(0,0,false);
        const auto after=sample();
        check(before!=after && after.getRed()>after.getBlue(),
              "Collapsed cell lost its amber Auto Stop exception marker");
        const juce::Point<float> cursor{(float)x,(float)y};
        grid->mouseMove(cueViewFixtures::event(*grid,cursor,cursor));
        check(grid->getTooltip().contains("Auto Stop"),
              "Collapsed cell exception is unexplained");
    });

    test("N3.4C inline BPM Enter commits, Esc cancels, and model is validated",[] {
        using namespace cuelist;
        MainComponent ui(false);
        addRow(ui.show,"Second cue");
        ui.setSize(1440,900);
        auto* metrics=cueViewFixtures::find<CueMetricsOverlay>(ui);
        check(metrics!=nullptr,"Fixed metrics missing");
        auto& editor=metrics->bpmEditorForTests();
        const int y=CueGridGeometry::rowTop(0);
        cueViewFixtures::click(*metrics,24.f,(float)y+29.f);
        check(editor.isVisible() && metrics->isBpmEditing(),
              "Clicking the displayed BPM failed to open inline editor");
        const double original=ui.show.rows[0].tempo.front().bpm;
        editor.setText("137.5",juce::dontSendNotification);
        editor.onReturnKey();
        check(!editor.isVisible() && !metrics->isBpmEditing() &&
              std::abs(ui.show.rows[0].tempo.front().bpm-137.5)<.001,
              "Enter failed to save inline BPM");
        cueViewFixtures::click(*metrics,24.f,(float)y+29.f);
        editor.setText("200",juce::dontSendNotification);
        editor.onEscapeKey();
        check(!editor.isVisible() &&
              std::abs(ui.show.rows[0].tempo.front().bpm-137.5)<.001,
              "Esc must cancel BPM without modifying the persisted value");
        check(!ui.commitRowBpm(ui.show.rows[0].id,"19") &&
              !ui.commitRowBpm(ui.show.rows[0].id,"401") &&
              !ui.commitRowBpm(ui.show.rows[0].id,"120..0") &&
              !ui.commitRowBpm(ui.show.rows[0].id,"abc") &&
              !ui.commitRowBpm(ui.show.rows[0].id,"") &&
              std::abs(ui.show.rows[0].tempo.front().bpm-137.5)<.001,
              "Malformed/out-of-range inline BPM changed the show");
        check(original>0 && validateShow(ui.show).wasOk(),
              "Inline BPM commit damaged the show model");
    });

    test("N3.4C inline BPM edits stable row ID and follows vertical scroll",[] {
        using namespace cuelist;
        MainComponent ui(false);
        for (int i=1;i<13;++i) addRow(ui.show,"Cue");
        ui.setSize(1280,820);
        auto* metrics=cueViewFixtures::find<CueMetricsOverlay>(ui);
        juce::Viewport* view=nullptr;
        for (auto* child:ui.getChildren())
            if (auto* candidate=dynamic_cast<juce::Viewport*>(child)) {
                view=candidate;break;
            }
        check(metrics && view,"Scroll fixtures are missing");
        view->setViewPosition(0,115);
        const int row=4,rowId=ui.show.rows[(size_t)row].id;
        const int y=CueGridGeometry::overlayRowTop(row,115)+26;
        cueViewFixtures::click(*metrics,24.f,(float)y);
        auto& editor=metrics->bpmEditorForTests();
        check(editor.isVisible() && editor.getBounds().getCentreY()==
              CueGridGeometry::overlayRowTop(row,115)+31,
              "BPM inline editor is not centred on the scrolled row");
        editor.setText("156.5",juce::dontSendNotification);
        view->setViewPosition(0,150);
        check(metrics->isBpmEditing() &&
              editor.getBounds().getCentreY()==
              CueGridGeometry::overlayRowTop(row,150)+31,
              "Visible BPM editor failed to track its row while scrolling");
        view->setViewPosition(0,300);
        check(!metrics->isBpmEditing() &&
              std::abs(ui.show.rows[(size_t)row].tempo.front().bpm-156.5)<.001,
              "Offscreen BPM edit failed to commit or stayed under pinned heading");
        // Invalid offscreen edits must close without corrupting the model.
        view->setViewPosition(0,115);
        cueViewFixtures::click(*metrics,24.f,(float)y);
        check(editor.isVisible(),"Could not reopen inline BPM after scrolling");
        editor.setText("999",juce::dontSendNotification);
        view->setViewPosition(0,300);
        check(!metrics->isBpmEditing() && !editor.isVisible() &&
              std::abs(ui.show.rows[(size_t)row].tempo.front().bpm-156.5)<.001,
              "Invalid BPM editor remained under header or mutated the cue");
        check(ui.show.rows[(size_t)row].id==rowId,
              "BPM edit retargeted a different cue during scrolling");
    });

}
