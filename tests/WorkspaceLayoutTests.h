#pragma once
#include "WorkspaceLayout.h"

void workspaceLayoutTests() {
    test("Workspace geometry preserves the 0.9 desktop cue/mixer/timeline arrangement",[] {
        const auto geometry=calculateWorkspaceLayout({1647,895,0,467,1,72,62,184});
        check(geometry.editorX==1359 &&
              geometry.viewport==juce::Rectangle<int>(182,74,985,367),
              "Desktop viewport changed the agreed cue-list gutters");
        check(geometry.utilities==juce::Rectangle<int>(182,441,985,208) &&
              geometry.utilities.getY()==geometry.viewport.getBottom(),
              "Utility mixer no longer meets the cue-list viewport");
        check(geometry.timeline==juce::Rectangle<int>(182,663,1169,185),
              "Timeline position changed without a layout request");
        check(geometry.metrics==juce::Rectangle<int>(1167,74,184,367),
              "BPM/Auto Stop is no longer pinned to the viewport edge");
        check(geometry.inspectorPanel==juce::Rectangle<int>(1359,76,268,771),
              "Inspector outer card moved");
        check(geometry.gridSize.x<=geometry.viewport.getWidth(),
              "Single-column show acquired unwanted horizontal scrolling");
    });

    test("Compact and wide windows preserve top/right gutters and pinned metrics",[] {
        for (const auto& dimensions:std::initializer_list<juce::Point<int>>{
                 {1180,820},{1440,900},{1647,895},{1920,1080}}) {
            const auto g=calculateWorkspaceLayout({dimensions.x,dimensions.y,0,467,3,72,62,184});
            check(g.viewport.getX()==182 && g.viewport.getY()==74 &&
                  g.editorX-g.metrics.getRight()==8,
                  "Cue-list gutters changed on window resizing");
            check(g.metrics.getX()==g.viewport.getRight() &&
                  g.metrics.getY()==g.viewport.getY() &&
                  g.metrics.getHeight()==g.viewport.getHeight(),
                  "Pinned metrics drifted when window dimensions changed");
            check(g.utilities.getX()==g.viewport.getX() &&
                  g.utilities.getWidth()==g.viewport.getWidth() &&
                  g.timeline.getX()==g.viewport.getX() &&
                  g.timeline.getRight()==g.editorX-8,
                  "Mixer and timeline are no longer aligned with the cue list");
        }
    });

    test("Scrollable cue columns reserve pinned BPM/Auto Stop clearance",[] {
        const auto compact=calculateWorkspaceLayout({1180,820,0,467,1,72,62,184});
        // BPM/Auto Stop is outside the viewport. A one-column show no
        // longer pays a phantom right-overlay scrolling penalty.
        check(compact.gridSize.x<=compact.viewport.getWidth(),
              "Single-column grid should not acquire phantom horizontal scrolling");
        const auto wide=calculateWorkspaceLayout({1647,895,0,467,1,72,62,184});
        check(wide.gridSize.x<=wide.viewport.getWidth(),
              "Roomy one-column show acquired unwanted horizontal scroll");
        const auto many=calculateWorkspaceLayout({1180,820,0,290+9*152,25,72,62,184});
        check(many.gridSize.x>many.viewport.getWidth() &&
              many.gridSize.y>=72+25*62+44,
              "Long cue list failed to reserve scrolling space");
        check(many.metrics.getX()==compact.metrics.getX(),
              "Pinned BPM location changed with column count");
    });

    test("Docked and collapsed editors preserve independent mixer and timeline bounds",[] {
        const auto plain=calculateWorkspaceLayout({1440,900,0,467,2,72,62,184});
        const auto collapsed=calculateWorkspaceLayout({1440,900,36,467,2,72,62,184});
        const auto opened=calculateWorkspaceLayout({1440,900,208,467,2,72,62,184});
        check(collapsed.dockEditor.getHeight()==36 &&
              opened.dockEditor.getHeight()==208 &&
              plain.dockEditor.getHeight()==0,
              "Clip Editor dock height was not respected");
        for (const auto* g:{&plain,&collapsed,&opened}) {
            check(g->utilities.getY()==g->viewport.getBottom() &&
                  g->timeline.getY()==g->dockEditor.getBottom() &&
                  g->dockEditor.getY()==g->dockY,
                  "Docked layout allowed panels to overlap");
            check(g->dockResize.getY()==g->dockY-8 &&
                  g->dockCollapse.getY()==g->dockY+4 &&
                  g->dockClose.getY()==g->dockY+4,
                  "Docked editor buttons are offset from their panel");
        }
    });

    test("Inspector card artwork and scaled controls use one vertical transform",[] {
        for (int height:{790,820,900,1080}) {
            const auto g=calculateWorkspaceLayout({1440,height,0,467,1,72,62,184});
            const auto field=g.scaleInspectorControl({g.inspectorLeft,541,236,31});
            const auto originalTop=juce::Point<float>((float)g.inspectorLeft,541.f);
            const auto paintedTop=originalTop.transformedBy(g.inspectorTransform());
            check(std::abs((float)field.getY()-paintedTop.y)<=1.0f,
                  "Inspector editor hitbox and painted label use different transforms");
            const auto cardTop=juce::Point<float>(0.f,(float)g.inspectorCellCard.getY())
                                   .transformedBy(g.inspectorTransform()).y;
            const auto titleTop=juce::Point<float>(0.f,489.f)
                                    .transformedBy(g.inspectorTransform()).y;
            check(titleTop>cardTop && field.getHeight()>=22,
                  "CELL title sits above its card or text input became too small");
        }
    });

    test("Expanded editor fits compact windows without hiding the timeline",[] {
        for (const auto size:std::initializer_list<juce::Point<int>>{
                 {1180,820},{1440,900},{1647,895},{1920,1080}}) {
            const auto layout=calculateWorkspaceLayout({
                size.x,size.y,400,467,6,72,62,184});
            check(layout.effectiveDockHeight>=0 &&
                  layout.effectiveDockHeight<=400,
                  "Responsive dock height outside requested range");
            check(layout.dockEditor.getHeight()==layout.effectiveDockHeight &&
                  layout.dockEditor.getBottom()==layout.timeline.getY(),
                  "Dock does not meet timeline after fitting");
            check(layout.viewport.getHeight()>=170 &&
                  layout.utilities.getBottom()<layout.dockEditor.getY(),
                  "Dock enlargement concealed the cue grid or mixer");
            check(layout.timeline.getHeight()>=75 &&
                  layout.timeline.getBottom()<=size.y-47,
                  "Oversized dock clipped the timeline into the status/footer area");
            check(layout.metrics.getHeight()==layout.viewport.getHeight(),
                  "Pinned metrics height diverged from cue viewport");
        }
        const auto small=calculateWorkspaceLayout({1180,820,400,467,6,72,62,184});
        const auto medium=calculateWorkspaceLayout({1440,900,400,467,6,72,62,184});
        const auto tall=calculateWorkspaceLayout({1920,1080,400,467,6,72,62,184});
        check(small.effectiveDockHeight==220 &&
              medium.effectiveDockHeight==300 &&
              tall.effectiveDockHeight==400,
              "Expanded dock fails to respond to available vertical space");
    });

    test("Default and collapsed dock retain 0.9 positions on a compact desktop",[] {
        const auto normal=calculateWorkspaceLayout({1180,820,208,467,2,72,62,184});
        const auto collapsed=calculateWorkspaceLayout({1180,820,36,467,2,72,62,184});
        const auto closed=calculateWorkspaceLayout({1180,820,0,467,2,72,62,184});
        check(normal.effectiveDockHeight==208 &&
              collapsed.effectiveDockHeight==36 &&
              closed.effectiveDockHeight==0,
              "Previously fitting dock states must remain unchanged");
        check(normal.viewport.getY()==74 && normal.viewport.getHeight()==182 &&
              normal.timeline.getBottom()==773,
              "Default compact layout geometry unexpectedly changed");
        check(collapsed.timeline.getBottom()==773 &&
              closed.timeline.getBottom()==773,
              "Collapsed and closed dock lost their original footer gap");
    });

    test("Live MIDI dock clamps at window minimum and restores full height when enlarged",[] {
        MainComponent ui(false);
        addColumn(ui.show,ColumnType::midi,"MIDI");
        check(ui.select(0,1),"Could not select MIDI column for dock resizing");
        ui.editMidiClip();
        ui.setSize(1180,820);
        auto* handle=controlsV3::named(ui,"clipEditorResizeHandle");
        auto* editor=ui.activeClipEditor();
        auto* timeline=cueViewFixtures::find<Timeline>(ui);
        check(handle && handle->isVisible() && editor && timeline,
              "Live dock resize components unavailable");

        const juce::Point<float> from{45.f,6.f},grown{45.f,-294.f};
        handle->mouseDown(cueViewFixtures::event(*handle,from,from));
        handle->mouseDrag(cueViewFixtures::event(*handle,grown,from,1,true));
        handle->mouseUp(cueViewFixtures::event(*handle,grown,from,1,true));
        check(editor->getHeight()==220 && timeline->getBounds().getBottom()<=773,
              "Oversized dock obscured the timeline at minimum window height");

        ui.setSize(1440,1080);
        check(editor->getHeight()==400 && timeline->getBottom()<=1033,
              "Requested dock height did not return when space became available");

        ui.setSize(1180,820);
        check(editor->getHeight()==220,"Dock did not shrink after reducing window height");
        const juce::Point<float> shorter{45.f,21.f};
        handle->mouseDown(cueViewFixtures::event(*handle,from,from));
        handle->mouseDrag(cueViewFixtures::event(*handle,shorter,from,1,true));
        handle->mouseUp(cueViewFixtures::event(*handle,shorter,from,1,true));
        check(editor->getHeight()==205,
              "Dock drag used stale preferred height instead of visible height");
    });

    test("MainComponent applies the shared geometry without changing component placement",[] {
        MainComponent ui(false);
        for (const auto& dimensions:std::initializer_list<juce::Point<int>>{
                 {1180,820},{1440,900},{1647,895}}) {
            ui.setSize(dimensions.x,dimensions.y);
            auto* grid=cueViewFixtures::find<CueGrid>(ui);
            auto* overlay=cueViewFixtures::find<CueMetricsOverlay>(ui);
            auto* strips=cueViewFixtures::find<ColumnUtilities>(ui);
            auto* timeline=cueViewFixtures::find<Timeline>(ui);
            juce::Viewport* viewport=nullptr;
            for (auto* child:ui.getChildren())
                if (auto* candidate=dynamic_cast<juce::Viewport*>(child)) {
                    viewport=candidate; break;
                }
            check(grid && overlay && strips && timeline && viewport,
                  "Workspace controls were not found");
            const auto expected=calculateWorkspaceLayout({dimensions.x,dimensions.y,0,
                grid->columnX((int)ui.show.columns.size()),(int)ui.show.rows.size(),
                CueGrid::headerHeight,CueGrid::rowHeight,CueMetricsOverlay::width});
            check(viewport->getBounds()==expected.viewport &&
                  strips->getBounds()==expected.utilities &&
                  timeline->getBounds()==expected.timeline &&
                  overlay->getBounds()==expected.metrics &&
                  grid->getWidth()==expected.gridSize.x &&
                  grid->getHeight()==expected.gridSize.y,
                  "JUCE workspace bounds differ from pure geometry");
        }
    });

    test("Full-width Timeline and docked MIDI/OSC editor do not inherit BPM clipping",[] {
        for (int width:{1180,1440,1647,1920})
            for (int dock:{0,36,208}) {
                const auto g=calculateWorkspaceLayout({width,900,dock,746,3,72,62,184,290});
                check(g.metrics.getX()==g.viewport.getRight() &&
                      g.utilities.getRight()==g.metrics.getX(),
                      "Fixed BPM gutter no longer excludes upper Utility columns");
                check(g.timeline.getRight()==g.editorX-8 &&
                      g.dockEditor.getRight()==g.editorX-8 &&
                      g.timeline.getWidth()==g.utilities.getWidth()+g.metrics.getWidth(),
                      "Lower MIDI OSC or Timeline was clipped at BPM boundary");
                check(g.timeline.getY()==g.dockEditor.getBottom(),
                      "Timeline no longer follows the dock");
            }
    });
    test("Timeline resize redistributes height without shifting pinned cues",[] {
        const auto initial=calculateWorkspaceLayout({1440,900,0,746,3,72,62,184,290,0});
        const auto expanded=calculateWorkspaceLayout({1440,900,0,746,3,72,62,184,290,72});
        const auto reduced=calculateWorkspaceLayout({1440,900,0,746,3,72,62,184,290,-65});
        check(expanded.timeline.getHeight()==initial.timeline.getHeight()+72 &&
              reduced.timeline.getHeight()==initial.timeline.getHeight()-65,
              "Timeline vertical drag had the wrong gain or direction");
        for (const auto* g:{&initial,&expanded,&reduced})
            check(g->cueHeaders.getY()==initial.cueHeaders.getY() &&
                  g->cueHeaders.getX()==initial.cueHeaders.getX() &&
                  g->metrics.getX()==g->viewport.getRight() &&
                  g->utilities.getRight()==g->metrics.getX() &&
                  std::abs(g->timelineResize.getCentreY()-g->timeline.getY())<=1 &&
                  g->timeline.getBottom()<=853,
                  "Timeline resize shifted pinned headers or caused clipping");
        const auto compact=calculateWorkspaceLayout({1180,820,208,746,3,72,62,184,290,400});
        check(compact.timeline.getHeight()>=75 &&
              compact.timeline.getBottom()<=773 &&
              compact.effectiveDockHeight>=130,
              "Compact window lost its minimum Timeline or editor lane");
        const auto withDock=calculateWorkspaceLayout({1440,900,208,746,3,72,62,184,290,65});
        const auto withoutResize=calculateWorkspaceLayout({1440,900,208,746,3,72,62,184,290,0});
        check(withDock.timeline.getHeight()==withoutResize.timeline.getHeight()+65 &&
              withDock.dockEditor.getHeight()==withoutResize.dockEditor.getHeight()-65 &&
              withDock.utilities.getY()==withoutResize.utilities.getY(),
              "Timeline could not grow with docked MIDI editor or changed upper grid");
    });
    test("Live Timeline splitter drag does not move pinned BPM or cue headers",[] {
        MainComponent ui(false);
        ui.setSize(1440,900);
        auto* splitter=controlsV3::named(ui,"timelineResizeHandle");
        auto* timeline=cueViewFixtures::find<Timeline>(ui);
        auto* headers=cueViewFixtures::find<PinnedCueHeaders>(ui);
        auto* metrics=cueViewFixtures::find<CueMetricsOverlay>(ui);
        check(splitter && timeline && headers && metrics,"Timeline splitter missing");
        const int height=timeline->getHeight();
        const int cueY=headers->getY(),bpmX=metrics->getX();
        const juce::Point<float> from{45.f,5.f},up{45.f,-47.f};
        splitter->mouseDown(cueViewFixtures::event(*splitter,from,from));
        splitter->mouseDrag(cueViewFixtures::event(*splitter,up,from,1,true));
        splitter->mouseUp(cueViewFixtures::event(*splitter,up,from,1,true));
        check(timeline->getHeight()==height+52 && headers->getY()==cueY &&
              metrics->getX()==bpmX && timeline->getRight()==1144,
              "Actual splitter did not resize Timeline or shifted fixed controls");
    });

}
