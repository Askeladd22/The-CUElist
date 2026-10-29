#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace cuelist {
// Only real mapping destinations use this temporary hit target.
// It sits above the normal control while Learn is active, preventing
// playback or changing a gain/pan value when the operator selects it.
class QuickLearnHotspot final : public juce::Component, public juce::SettableTooltipClient {
public:
    explicit QuickLearnHotspot(juce::String label):title(std::move(label)) {
        setName("quickLearn"+title.removeCharacters(" "));
        setTooltip("Select "+title+" for MIDI/OSC Learn");
        setWantsKeyboardFocus(false);
        setInterceptsMouseClicks(true,false);
        setVisible(false);
    }
    std::function<void()> onPick;
    void setLearnPresentation(juce::Colour colour,bool armed,bool bright) {
        if (accent==colour && selected==armed && pulseBright==bright) return;
        accent=colour;selected=armed;pulseBright=bright;repaint();
    }
    juce::Colour getLearnAccent() const noexcept {return accent;}
    bool isArmedTarget() const noexcept {return selected;}
    bool isPulseBright() const noexcept {return pulseBright;}
    void setLearnActive(bool active) {
        setVisible(active);
        if (active) toFront(false);
        repaint();
    }
    void mouseDown(const juce::MouseEvent&) override {
        if (isVisible() && onPick) onPick();
    }
    void paint(juce::Graphics& g) override {
        const auto bounds=getLocalBounds().toFloat().reduced(1.0f);
        if (bounds.isEmpty()) return;
        // MIDI = amber, OSC = violet. The currently selected target
        // pulses while all other available controls stay steadily tinted.
        g.setColour(accent.withAlpha(selected ? (pulseBright ? .38f : .075f) : .11f));
        g.fillRoundedRectangle(bounds,4.0f);
        g.setColour(accent.withAlpha(selected ? (pulseBright ? 1.0f : .32f) : .70f));
        g.drawRoundedRectangle(bounds,4.0f,selected ? 2.6f : 1.5f);
        if (getWidth()>=40 && getHeight()>=30) {
            g.setFont(juce::Font(juce::FontOptions(10.0f,juce::Font::bold)));
            g.drawText("LEARN",2,2,getWidth()-4,15,
                       juce::Justification::centred,true);
        }
    }
private:
    juce::String title;
    juce::Colour accent{0xfff9c76e};
    bool selected=false,pulseBright=true;
};
} // namespace cuelist
