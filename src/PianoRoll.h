#pragma once
#include "ShowModel.h"
#include "Theme.h"

namespace cuelist {
class PianoRollEditor final : public juce::Component {
public:
    PianoRollEditor(MidiClip, std::function<void(const MidiClip&)>);
    ~PianoRollEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    const MidiClip& getClip() const { return clip; }
    juce::Component& getCanvas();
private:
    class Canvas;
    MidiClip clip;
    std::function<void(const MidiClip&)> commit;
    Theme theme;
    struct RollViewport : juce::Viewport {
        std::function<void()> onScroll;
        void visibleAreaChanged(const juce::Rectangle<int>&) override { if (onScroll) onScroll(); }
    } viewport;
    std::unique_ptr<Canvas> canvas;
    juce::ComboBox snap, channel;
    juce::Slider velocity, length;
    juce::TextButton remove{"Elimina nota"}, zoomOut{"-"}, zoomIn{"+"}, apply{"Applica"}, cancel{"Annulla"};
    juce::String hint;
    int selected=-1;
    double pixelsPerBeat=80;
    double gridStep() const;
    void selectionChanged();
    void deleteSelected();
    void updateCanvas();
    void close();
};
}
