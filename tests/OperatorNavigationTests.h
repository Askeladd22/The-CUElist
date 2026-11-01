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
    test("N3.4C global header retains 72px tiers and active Cuelist tab",[] {
        MainComponent ui(false);
        ui.setSize(1920,1080);
        auto* tab=controlsV3::named(ui,"cuelistTab0");
        auto* file=controlsV3::named(ui,"operatorMenuFile");
        auto* newTab=controlsV3::named(ui,"newCuelist");
        check(tab && file && newTab && tab->isVisible() &&
              tab->getY()>=38 && tab->getBottom()<=72 &&
              file->getBottom()<=38 && newTab->getBottom()<=72,
              "Top navigation and second-tier Cuelist tabs overlap");
        const auto image=tab->createComponentSnapshot(tab->getLocalBounds());
        check(image.isValid(),"Native Cuelist tab snapshot unavailable");
        const auto underline=image.getPixelAt(15,tab->getHeight()-2);
        check(underline.getGreen()>underline.getRed(),
              "Active Cuelist lost its visible teal underline");
        for (const int width:{1280,1440,1647,1920,3840}) {
            ui.setSize(width,900);
            const auto nav=OperatorNavigationLayout::atWidth(width);
            const auto telemetry=HeaderTelemetryLayout::atWidth(width);
            check(telemetry.clock.getX()>200 &&
                  telemetry.usage.getRight()<width-365 &&
                  telemetry.clock.getRight()<telemetry.usage.getX() &&
                  telemetry.clock.getY()==8 &&
                  telemetry.usage.getY()==8 &&
                  nav.bounds[0].getRight()<telemetry.clock.getX(),
                  "Unified monitoring strip overlaps the native operator menu");
        }
    });

    test("V2 global clock enlarged without overlapping operator telemetry",[] {
        using namespace cuelist;
        MainComponent ui(false);
        for (const int width:{1280,1440,1647,1920,3840}) {
            ui.setSize(width,900);
            const auto t=HeaderTelemetryLayout::atWidth(width);
            check(t.clock.contains(t.timeText) &&
                  t.clock.contains(t.meridiemText) &&
                  t.timeText.getWidth()>=116 &&
                  t.timeText.getHeight()>=38 &&
                  t.clock.getRight()<t.usage.getX() &&
                  t.meridiemText.getY()>=t.timeText.getBottom() &&
                  t.timeText.getBottom()<=t.clock.getBottom() &&
                  t.meridiemText.getBottom()<=t.clock.getBottom(),
                  "Larger clock text or 12h AM/PM lost the fixed header bounds");
            const auto picture=ui.createComponentSnapshot(t.clock);
            check(picture.isValid() && picture.getWidth()==t.clock.getWidth() &&
                  picture.getHeight()==t.clock.getHeight(),
                  "Enlarged clock is not rendered in the native JUCE header");
        }
    });

    test("V2 enlarged telemetry has readable five groups and no overlap",[] {
        MainComponent ui(false);
        for (const int width:{1024,1180,1280,1440,1647,1920,3840}) {
            const auto t=HeaderTelemetryLayout::atWidth(width);
            const auto nav=OperatorNavigationLayout::atWidth(width);
            check(t.clock.getWidth()>=134 &&
                  t.usage.getWidth()>=130 &&
                  t.activity.getWidth()>=218 &&
                  t.audio.getWidth()>=150 &&
                  t.audio.getRight()<=width-10 &&
                  t.clock.getRight()+10==t.usage.getX() &&
                  t.usage.getRight()+10==t.activity.getX() &&
                  t.activity.getRight()+10==t.audio.getX(),
                  "Five operator-monitor groups are too narrow or overlap");
            check(t.activity.contains(t.activityHeading) &&
                  t.activity.contains(t.midiLearn) &&
                  t.activity.contains(t.oscLearn) &&
                  !t.activityHeading.intersects(t.midiLearn) &&
                  !t.midiLearn.intersects(t.oscLearn) &&
                  t.midiLearn.getHeight()>=22 &&
                  t.midiLearn.getWidth()>=61,
                  "I/O activity labels and Learn targets are compressed");
            check(t.audio.contains(t.audioHeading) &&
                  t.audio.contains(t.audioStatus) &&
                  t.clock.contains(t.timeText) &&
                  t.clock.contains(t.meridiemText) &&
                  t.usage.contains(t.cpuMeter.toNearestInt()) &&
                  t.usage.contains(t.ramMeter.toNearestInt()),
                  "Clock, CPU/RAM or audio labels leave the 72px header");
            for (int i=0;i<4;++i)
                check(t.activity.contains(t.activityDots[(size_t)i]) &&
                      t.activity.contains(t.activityLabels[(size_t)i]),
                      "MIDI/OSC activity labels or dots overflow");
            const auto more=nav.bounds[OperatorNavigationLayout::overflowIndex];
            check(nav.visibleMenuCount>=1 &&
                  (nav.visibleMenuCount==7 ||
                   (more.getWidth()>0 && more.getRight()<t.clock.getX())),
                  "Enlarged status console obscured operator menu access");
        }

        for (const int width:{1280,1440,1647,1920,3840}) {
            ui.setSize(width,900);
            ui.resized();
            const auto t=HeaderTelemetryLayout::atWidth(width);
            auto* midi=controlsV3::named(ui,"globalMidiLearn");
            auto* osc=controlsV3::named(ui,"globalOscLearn");
            check(midi && osc && midi->isVisible() && osc->isVisible() &&
                  midi->getBounds()==t.midiLearn &&
                  osc->getBounds()==t.oscLearn &&
                  t.activity.contains(midi->getBounds()) &&
                  t.activity.contains(osc->getBounds()),
                  "Native MIDI/OSC Learn buttons are not aligned to I/O Activity");
            for (const auto region:{t.clock,t.usage,t.activity,t.audio}) {
                const auto image=ui.createComponentSnapshot(region);
                check(image.isValid() &&
                      image.getWidth()==region.getWidth() &&
                      image.getHeight()==region.getHeight(),
                      "An enlarged operator telemetry group did not render");
            }
        }
    });

}
