#pragma once
#include "Theme.h"
#include "HeaderTelemetry.h"
#include <array>

namespace cuelist {

// Native V2 operator navigation geometry. This stage deliberately does not
// change the workspace, cue hitboxes, audio output or the original Master.
// On narrower windows, the trailing menus move into a real "More" popup
// rather than disappearing along with their commands.
struct OperatorNavigationLayout final {
    static constexpr int menuCount=7;
    static constexpr int overflowIndex=7;
    static constexpr int left=204;
    static constexpr int menuY=8;
    static constexpr int menuHeight=29;

    std::array<juce::Rectangle<int>,8> bounds{};
    int visibleMenuCount=0;

    static const char* title(int index) {
        constexpr const char* titles[]={
            "File","Edit","Show","Cuelists","Devices","View","Guide","More"
        };
        return index>=0 && index<=overflowIndex ? titles[index] : "";
    }

    static OperatorNavigationLayout atWidth(int width) {
        OperatorNavigationLayout result;
        constexpr std::array<int,menuCount> widths{{48,47,56,76,75,56,64}};
        constexpr int gap=4, overflowWidth=62;
        // Existing clock/CPU and status monitors keep their original areas
        // throughout this first native header migration.
        const int right=HeaderTelemetryLayout::atWidth(width).clock.getX()-12;
        int needed=0;
        for (int i=0;i<menuCount;++i) needed+=widths[(size_t)i]+gap;
        const bool allFit=left+needed-gap<=right;
        int x=left;
        for (int i=0;i<menuCount;++i) {
            // Reserve room for the overflow control whenever menus remain.
            const int spare=allFit ? 0 : overflowWidth+gap;
            if (x+widths[(size_t)i]+spare>right) break;
            result.bounds[(size_t)i]={x,menuY,widths[(size_t)i],menuHeight};
            ++result.visibleMenuCount;
            x+=widths[(size_t)i]+gap;
        }
        if (result.visibleMenuCount<menuCount)
            result.bounds[overflowIndex]={left+std::max(0,x-left),menuY,
                                          overflowWidth,menuHeight};
        return result;
    }
};

// Menu buttons share the V2 visual language without overriding global JUCE
// button styling or the untouched Master Audio widget.
class OperatorNavigationButton final : public juce::TextButton {
public:
    explicit OperatorNavigationButton(const juce::String& name)
        : juce::TextButton(name) {}
    void paintButton(juce::Graphics& g,bool over,bool down) override {
        const auto area=getLocalBounds().toFloat();
        if (over || down) {
            g.setColour(palette::raised.withAlpha(down ? .95f : .74f));
            g.fillRoundedRectangle(area.reduced(1.0f,2.0f),4.0f);
        }
        g.setColour(isEnabled() ? palette::text : palette::muted);
        g.setFont(font(13.0f));
        g.drawFittedText(getButtonText(),getLocalBounds(),
                         juce::Justification::centred,1);
    }
};
} // namespace cuelist
