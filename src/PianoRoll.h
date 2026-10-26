#pragma once
#include "ShowModel.h"
#include "Theme.h"
#include <set>

namespace cuelist {
class PianoRollEditor final : public juce::Component {
public:
    PianoRollEditor(MidiClip, std::function<void(const MidiClip&)>);
    ~PianoRollEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    const MidiClip& getClip() const { return clip; }
    juce::Component& getCanvas();
    void loadClip(const MidiClip& value);
    void undo();
    void redo();
    void setCloseHandler(std::function<void()> handler) { closeHandler=std::move(handler); }
    void setDetached(bool detached);
private:
    class Canvas;
    MidiClip clip;
    std::function<void(const MidiClip&)> commit;
    std::function<void()> closeHandler;
    std::vector<MidiClip> undoStack, redoStack;
    MidiClip previousGesture;
    bool gestureActive=false;
    void beginEdit();
    void endEdit();
    void applyChange();
    Theme theme;
    struct RollViewport : juce::Viewport {
        std::function<void()> onScroll;
        void visibleAreaChanged(const juce::Rectangle<int>&) override { if (onScroll) onScroll(); }
    } viewport;
    std::unique_ptr<Canvas> canvas;
    juce::ComboBox snap, channel;
    juce::Slider velocity, length;
    juce::TextButton remove{"Elimina nota"}, zoomOut{"-"}, zoomIn{"+"}, undoButton{"Annulla modifica"}, redoButton{"Ripeti"}, cancel{"Chiudi"};
    juce::String hint;
    int selected=-1;
    std::set<int> selectedNotes;
    double pixelsPerBeat=80;
    double gridStep() const;
    bool snapEnabled() const;
    void selectionChanged();
    void deleteSelected();
    void updateCanvas();
    void close();
};
}
