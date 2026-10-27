#pragma once
#include "WorkspaceLayout.h"

void workspaceLayoutTests() {
    test("Workspace geometry preserves the 0.9 desktop cue/mixer/timeline arrangement",[] {
        const auto geometry=calculateWorkspaceLayout({1647,895,0,467,1,72,62,184});
        check(geometry.editorX==1359 &&
              geometry.viewport==juce::Rectangle<int>(192,86,1159,355),
              "Desktop viewport changed the agreed cue-list gutters");
        check(geometry.utilities==juce::Rectangle<int>(192,441,1159,208) &&
              geometry.utilities.getY()==geometry.viewport.getBottom(),
              "Utility mixer no longer meets the cue-list viewport");
        check(geometry.timeline==juce::Rectangle<int>(192,663,1159,185),
              "Timeline position changed without a layout request");
        check(geometry.metrics==juce::Rectangle<int>(1167,86,184,355),
              "BPM/Auto Stop is no longer pinned to the viewport edge");
        check(geometry.inspectorPanel==juce::Rectangle<int>(1359,154,268,694),
              "Inspector outer card moved");
        check(geometry.gridSize.x<=geometry.viewport.getWidth(),
              "Single-column show acquired unwanted horizontal scrolling");
    });

    test("Compact and wide windows preserve top/right gutters and pinned metrics",[] {
        for (const auto& dimensions:std::initializer_list<juce::Point<int>>{
                 {1180,820},{1440,900},{1647,895},{1920,1080}}) {
            const auto g=calculateWorkspaceLayout({dimensions.x,dimensions.y,0,467,3,72,62,184});
            check(g.viewport.getX()==192 && g.viewport.getY()==86 &&
                  g.editorX-g.viewport.getRight()==8,
                  "Cue-list gutters changed on window resizing");
            check(g.metrics.getRight()==g.viewport.getRight() &&
                  g.metrics.getY()==g.viewport.getY() &&
                  g.metrics.getHeight()==g.viewport.getHeight(),
                  "Pinned metrics drifted when window dimensions changed");
            check(g.utilities.getX()==g.viewport.getX() &&
                  g.utilities.getWidth()==g.viewport.getWidth() &&
                  g.timeline.getX()==g.viewport.getX() &&
                  g.timeline.getWidth()==g.viewport.getWidth(),
                  "Mixer and timeline are no longer aligned with the cue list");
        }
    });

    test("Scrollable cue columns reserve pinned BPM/Auto Stop clearance",[] {
        const auto compact=calculateWorkspaceLayout({1180,820,0,467,1,72,62,184});
        check(compact.gridSize.x<=compact.viewport.getWidth(),
              "Small show created a phantom horizontal scrollbar");
        const auto many=calculateWorkspaceLayout({1180,820,0,315+9*152,25,72,62,184});
        check(many.gridSize.x>many.viewport.getWidth() &&
              many.gridSize.y>=72+25*62+44,
              "Long cue list failed to reserve scrolling space");
        check(many.metrics.getRight()==compact.metrics.getRight(),
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

    test("MainComponent applies the shared geometry without changing component placement",[] {
        MainComponent ui(false);
        for (const auto& dimensions:std::initializer_list<juce::Point<int>>{
                 {1180,820},{1440,900},{1647,895}}) {
            ui.setSize(dimensions.x,dimensions.y);
            auto* grid=cueViewFixtures::find<CueGrid>(ui);
            auto* overlay=cueViewFixtures::find<CueMetricsOverlay>(ui);
            auto* strips=cueViewFixtures::find<ColumnUtilities>(ui);
            juce::Viewport* viewport=nullptr;
            for (auto* child:ui.getChildren())
                if (auto* candidate=dynamic_cast<juce::Viewport*>(child)) {
                    viewport=candidate; break;
                }
            check(grid && overlay && strips && viewport,
                  "Workspace controls were not found");
            const auto expected=calculateWorkspaceLayout({dimensions.x,dimensions.y,0,
                grid->columnX((int)ui.show.columns.size()),(int)ui.show.rows.size(),
                CueGrid::headerHeight,CueGrid::rowHeight,CueMetricsOverlay::width});
            check(viewport->getBounds()==expected.viewport &&
                  strips->getBounds()==expected.utilities &&
                  overlay->getBounds()==expected.metrics &&
                  grid->getWidth()==expected.gridSize.x &&
                  grid->getHeight()==expected.gridSize.y,
                  "JUCE workspace bounds differ from pure geometry");
        }
    });
}
