#pragma once
#include "AudioEngine.h"
#include "Theme.h"
namespace cuelist {
class MainComponent;
class CueGrid final : public juce::Component, public juce::FileDragAndDropTarget {
public:
    explicit CueGrid(MainComponent& parent) : owner(parent) {}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&,int,int) override;
    static constexpr int rowHeight=54, headerHeight=68, nameWidth=310, columnWidth=160;
private:
    MainComponent& owner;
    int dragRow=-1, dragColumn=-1, dropRow=-1, dropColumn=-1;
    bool dragging=false;
};
class Timeline final : public juce::Component, private juce::ScrollBar::Listener {
public:
    explicit Timeline(MainComponent& p);
    void resized() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    MainComponent& owner;
    int trackScroll=0, dragColumn=-1, dragRowId=-1, viewportRowId=-1;
    bool seeking=false;
    double dragStartSeconds=0, dragLength=10, dragViewStart=0, dragPixels=1;
    double zoom=1, viewStart=0;
    juce::ScrollBar horizontalScroll{false};
    double fullLength() const;
    double visibleLength() const;
    void syncViewport();
    void changeZoom(double factor,double anchor);
    void scrollBarMoved(juce::ScrollBar*,double) override;
};
class ColumnUtilities final : public juce::Component {
public:
    explicit ColumnUtilities(MainComponent&);
    ~ColumnUtilities() override;
    void refresh();
    void syncScroll(int);
    void resized() override;
    void paint(juce::Graphics&) override;
private:
    struct Strip;
    MainComponent& owner;
    std::vector<std::unique_ptr<Strip>> strips;
    int scrollX=0;
};
class MainComponent final : public juce::Component, private juce::Timer {
public:
    explicit MainComponent(bool startAudio = true);
    ~MainComponent() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    void requestQuit();
    bool select(int row,int column);
    void importFile(const juce::File&);
    void importDialog();
    void triggerRow(int row);
    void triggerCell();
    void triggerCellAt(int row,int column);
    void stopCellAt(int row,int column);
    void stopRow(int row);
    void setColumnUtilities(int column,float gain,float pan,bool muted,bool inverted);
    void renameTitle(bool column,int index);
    void reorderRow(int index,int delta);
    void reorderColumn(int index,int delta);
    void deleteRow(int index);
    void deleteColumn(int index);
    void moveClip(int,int,int,int);
    void setClipStart(int column,double seconds);
    void seekTimeline(double seconds);
    void goNext();
    int nextCueIndex() const { return nextCue; }
    bool hasUnsavedChanges() const { return dirty || inspectorDiffers(); }
    Show show=makeShow();
    ClipMap clips;
    int selectedRow=0, selectedColumn=0;
    AudioEngine engine;
private:
    friend class CueGrid; friend class Timeline;
    Theme theme;
    juce::TooltipWindow tooltip{this,600};
    CueGrid grid{*this}; Timeline timeline{*this}; juce::Viewport viewport;
    ColumnUtilities utilities{*this};
    juce::TextButton openButton{"Apri"},saveButton{"Salva"},newButton{"Nuovo"},addRowButton{"+ Cue"},addColumnButton{"+ Colonna"};
    juce::TextButton goButton{"GO  /  SPAZIO"},panicButton{"PANIC  /  ESC"},deviceButton{"Dispositivi audio"};
    juce::TextButton importButton{"Importa WAV / MP3"},playCellButton{"Avvia cella"},stopCellButton{"Ferma"},clearButton{"Rimuovi audio"};
    juce::TextButton cellTab{"Cella"},rowTab{"Intera cue"};
    juce::TextButton tempoButton{"Mappa tempo"},applyButton{"Applica modifiche"};
    juce::TextEditor rowName,cellName,bpm;
    juce::ComboBox behaviour,meter,output;
    juce::ToggleButton follow{"Partecipa al GO della riga"},loop{"Ripeti in loop"};
    juce::Slider gain;
    juce::Slider rowGain;
    juce::ComboBox rowBehaviour;
    juce::ToggleButton rowMute{"Mute intera cue"},rowLoop{"Loop di tutte le clip"};
    bool rowSettings=false;
    int nextCue=0;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Component::SafePointer<juce::DialogWindow> deviceDialog;
    juce::File projectFile;
    juce::String status="Pronto. Seleziona una cella Audio e importa un WAV.";
    bool dirty=false;
    int editorX=0;
    float displayPeak=0;
    void timerCallback() override;
    void refresh();
    void refreshInspector();
    void changed(const juce::String&,bool stopPlayback=false);
    bool applyInspector();
    bool inspectorDiffers() const;
    void openProject();
    void saveProject();
    void withDiscardConfirmation(std::function<void()>);
    void showDevices();
    void editTempo();
    void message(const juce::String&);
    bool send(int id,bool row,bool stop=false);
    void stopAll();
    bool memoryFits(const ClipMap&) const;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
}
