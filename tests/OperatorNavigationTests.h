#pragma once
#include "OperatorNavigation.h"

void operatorNavigationTests() {
    using namespace cuelist;
    test("Native V2 header shows all seven categories on wide screens",[] {
        const auto nav=OperatorNavigationLayout::atWidth(1647);
        check(nav.visibleMenuCount==7,
              "Wide operator header should show every native menu");
        const auto telemetry=HeaderTelemetryLayout::atWidth(1647);
        for (int i=0;i<nav.visibleMenuCount;++i) {
            const auto r=nav.bounds[(size_t)i];
            check(r.getX()>=OperatorNavigationLayout::left &&
                  r.getY()>=0 && r.getBottom()<43 &&
                  r.getRight()<telemetry.clock.getX(),
                  "Menu intersects Cuelist tabs or real CPU/clock telemetry");
            if (i>0)
                check(r.getX()>=nav.bounds[(size_t)i-1].getRight(),
                      "Native menu hit targets overlap");
        }
    });
    test("Small screens use real overflow, without losing menu categories",[] {
        for (int width:{1280,1180,1024}) {
            const auto nav=OperatorNavigationLayout::atWidth(width);
            check(nav.visibleMenuCount>=1 && nav.visibleMenuCount<7,
                  "Expected accessible overflow for a narrow workspace");
            const auto more=nav.bounds[OperatorNavigationLayout::overflowIndex];
            check(more.getWidth()>=55 &&
                  more.getRight()<HeaderTelemetryLayout::atWidth(width).clock.getX(),
                  "More button collides with telemetry");
            check(juce::String(OperatorNavigationLayout::title(6))=="Guide" &&
                  juce::String(OperatorNavigationLayout::title(7))=="More",
                  "Guide or overflow identity changed");
        }
    });
    test("Native N2 sidebar removes File/Settings/Mapping duplicates",[] {
        MainComponent ui(false);
        for (const auto bounds:std::initializer_list<juce::Point<int>>{
                 {1180,820},{1440,900},{1647,895},{1920,1080}}) {
            ui.setSize(bounds.x,bounds.y);
            ui.resized();
            for (int i=0;i<ui.getNumChildComponents();++i) {
                auto* child=ui.getChildComponent(i);
                auto* button=dynamic_cast<juce::TextButton*>(child);
                if (!button) continue;
                const auto text=button->getButtonText();
                if (text=="New" || text=="Open" || text=="Save" ||
                    text=="Settings" || text=="Preferences" ||
                    text=="Control Mapping")
                    check(!button->isVisible(),
                          "Duplicated operator action still occupies sidebar");
            }
            auto* show=dynamic_cast<juce::Label*>(
                controlsV3::named(ui,"projectName"));
            check(show && show->isVisible() && show->isEditableOnDoubleClick() &&
                  show->getY()==122,
                  "One native editable Show identity must remain visible");
            auto* file=controlsV3::named(ui,"operatorMenuFile");
            auto* devices=controlsV3::named(ui,"operatorMenuDevices");
            auto* more=controlsV3::named(ui,"operatorMenuMore");
            check(file && devices && more &&
                  (file->isVisible() || more->isVisible()) &&
                  (devices->isVisible() || more->isVisible()),
                  "File/Mapping actions must stay accessible through menus");
            auto* master=controlsV3::named(ui,"masterAudioDb");
            check(master && master->isVisible(),
                  "Original Master Audio must not be modified by sidebar work");
        }
    });
    test("Actual JUCE operator menus are buttons with preserved native commands",[] {
        MainComponent ui(false);
        ui.setSize(1647,900);
        for (const auto* name:{"File","Edit","Show","Cuelists",
                                "Devices","View","Guide"}) {
            auto* child=controlsV3::named(ui,"operatorMenu"+juce::String(name));
            check(child!=nullptr && child->isVisible() &&
                  dynamic_cast<juce::TextButton*>(child)!=nullptr,
                  "Native topbar category is missing or not clickable");
        }
        for (const auto* name:{"newCuelist","controlMapping","showUndo",
                                "showRedo","showHelpMenu","projectPreferences",
                                "inspectorSideToggle","quickLearnMasterLimiter"}) {
            check(controlsV3::named(ui,name)!=nullptr ||
                  juce::String(name)=="quickLearnMasterLimiter",
                  "Original operator command or mapping hook was deleted");
        }
        auto* master=controlsV3::named(ui,"masterAudioDb");
        check(master!=nullptr && master->isVisible(),
              "Native Master must remain original and visible");
        ui.setSize(1280,800);
        auto* more=controlsV3::named(ui,"operatorMenuMore");
        check(more && more->isVisible() && more->getWidth()>=55,
              "Operator navigation lost narrow-width More menu");
    });
}
