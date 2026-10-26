#pragma once
#include "ShowModel.h"
#include "Theme.h"
#include <set>
#include <functional>

namespace cuelist {
// A reusable editing view: it can be parented by the main dock or a floating window.
// Every logical mutation is pushed to the model immediately; drag previews are not
// separate undo steps, and the last drag event completes one undo transaction.
class PianoRollEditor final : public juce::Component {
public:
    using OnEdit = std::function<void(const MidiClip& before,const MidiClip& after,bool complete)>;
    explicit PianoRollEditor(MidiClip,OnEdit);
    ~PianoRollEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    const MidiClip& getClip() const { return clip; }
    juce::Component& getCanvas();
    void loadClip(const MidiClip& replacement);
    void setFloating(bool floating);
    std::function<void()> onDetach, onUndo, onRedo, onClose;
private:
    class Canvas;
    MidiClip clip;
    OnEdit onEdit;
    Theme theme;
    struct RollViewport : juce::Viewport {
        std::function<void()> onScroll;
        void visibleAreaChanged(const juce::Rectangle<int>&) override { if (onScroll) onScroll(); }
    } viewport;
    std::unique_ptr<Canvas> canvas;
    juce::ComboBox snap, channel;
    juce::Slider velocity, length;
    juce::TextButton remove{"Elimina"}, zoomOut{"-"}, zoomIn{"+"},
                     detach{"Stacca"}, undo{"Undo"}, redo{"Redo"};
    juce::String hint;
    int selected=-1;
    std::set<int> selectedNotes;
    double pixelsPerBeat=80;
    double gridStep() const;
    bool snapEnabled() const;
    void selectionChanged();
    void deleteSelected();
    void updateCanvas();
    void notify(const MidiClip& before,bool complete=true);
};
}
