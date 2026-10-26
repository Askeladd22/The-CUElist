#pragma once
#include "AudioEngine.h"
#include <set>
#include "Theme.h"
namespace cuelist {
class MainComponent;
class PianoRollEditor;
class OscClipEditor;
class MidiUndoAction;
class CueGrid final : public juce::Component, public juce::FileDragAndDropTarget {
public:
    explicit CueGrid(MainComponent& parent) : owner(parent) {}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void showRowMenu(int row);
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&,int,int) override;
    static constexpr int rowHeight=54, headerHeight=68, nameWidth=310, columnWidth=160;
private:
    MainComponent& owner;
    int dragRow=-1, dragColumn=-1, dropRow=-1, dropColumn=-1;
    std::set<int> autoStopVisibleRows;
    bool dragging=false;
};
class Timeline final : public juce::Component, private juce::ScrollBar::Listener {
public:
    explicit Timeline(MainComponent& p);
    void resized() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    MainComponent& owner;
    int trackScroll=0, dragColumn=-1, dragRowId=-1, viewportRowId=-1;
    bool seeking=false,resizingClip=false;
    double dragStartDuration=0;
    double dragStartSeconds=0, dragLength=10, dragViewStart=0, dragPixels=1;
    double zoom=1, viewStart=0;
    // Visual grid and clip snapping have independent controls and subdivisions.
    class MagnetToggle final : public juce::TextButton {
    public:
        void paintButton(juce::Graphics&,bool hovered,bool pressed) override;
    };
    juce::ScrollBar horizontalScroll{false};
    juce::ComboBox gridDivision, snapDivision;
    juce::TextButton gridToggle{"Griglia ON"};
    MagnetToggle snapToggle;
    bool gridVisible=true, snapEnabled=true;
    double gridStepBeats() const;
    double snapStepBeats() const;
    double snapSeconds(double seconds) const;
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
    void refreshRouting();
    void refreshMeters();
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
    void editMidiClip();
    void editOscClip();
    void detachOscClip();
    void dockOscClip();
    void closeOscClip();
    void setOscDestination(int column,const juce::String& host,int port);
    OscClipEditor* activeOscEditor() const { return oscEditor.get(); }
    void detachMidiClip();
    void dockMidiClip();
    void closeMidiClip();
    bool undoEdit();
    bool redoEdit();
    PianoRollEditor* activeClipEditor() const { return clipEditor.get(); }
    bool isClipEditorFloating() const { return clipWindow != nullptr; }
    void setMidiOutput(int column,const juce::String& identifier);
    bool hasClip(int row,int column) const;
    double clipDuration(int row,int column) const;
    void triggerRow(int row);
    void triggerCell();
    void triggerCellAt(int row,int column);
    void stopCellAt(int row,int column);
    void stopRow(int row);
    void setColumnUtilities(int column,float gain,float pan,bool muted,bool inverted);
    void setMasterAudio(float gain);
    void setColumnOutput(int column,int output);
    void setAutoStop(int row,bool enabled);
    void setCellAutoStop(int row,int column,bool enabled);
    void renameTitle(bool column,int index);
    void reorderRow(int index,int delta);
    void reorderColumn(int index,int delta);
    bool copyRow(int index);
    bool pasteRowAfter(int index);
    bool duplicateRow(int index);
    void deleteRow(int index);
    void deleteColumn(int index);
    void moveClip(int,int,int,int);
    void setClipStart(int column,double seconds);
    void setClipDuration(int column,double durationSeconds);
    bool copySelectedCell();
    bool pasteSelectedCell();
    void editRowBpm(int row);
    void seekTimeline(double seconds);
    void goNext();
    int nextCueIndex() const { return nextCue; }
    bool hasUnsavedChanges() const { return dirty || inspectorDiffers(); }
    Show show=makeShow();
    ClipMap clips;
    int selectedRow=0, selectedColumn=0;
    AudioEngine engine;
private:
    friend class CueGrid; friend class Timeline; friend class MidiUndoAction; friend class ColumnUtilities;
    Theme theme;
    juce::TooltipWindow tooltip{this,600};
    CueGrid grid{*this}; Timeline timeline{*this}; juce::Viewport viewport;
    ColumnUtilities utilities{*this};
    juce::TextButton openButton{"Apri"},saveButton{"Salva"},newButton{"Nuovo"},addRowButton{"+ Cue"},addColumnButton{"+ Colonna"};
    juce::TextButton goButton{"GO  /  SPAZIO"},panicButton{"PANIC  /  ESC"},deviceButton{"Dispositivi audio"};
    juce::TextButton importButton{"Importa WAV / MP3"},playCellButton{"Avvia cella"},stopCellButton{"Ferma"},clearButton{"Rimuovi audio"};
    juce::TextButton midiEditButton{"Crea clip MIDI"};
    juce::TextButton oscEditButton{"Crea clip OSC"};
    juce::TextButton cellTab{"Cella"},rowTab{"Intera cue"};
    juce::TextButton tempoButton{"Mappa tempo"},applyButton{"Applica modifiche"};
    juce::TextEditor rowName,cellName,bpm;
    juce::ComboBox behaviour;
    juce::ToggleButton follow{"Partecipa al GO della riga"},loop{"Ripeti in loop"};
    juce::ToggleButton cellAutoStop{"Arresta al GO della prossima cue"};
    juce::Slider gain;
    juce::Slider rowGain;
    juce::Slider masterSlider;
    struct DockResizeHandle : juce::Component, juce::SettableTooltipClient {
        explicit DockResizeHandle(MainComponent& p) : owner(p) { setMouseCursor(juce::MouseCursor::UpDownResizeCursor); }
        void paint(juce::Graphics& g) override { g.setColour(palette::muted); g.fillRoundedRectangle(getLocalBounds().toFloat().withSizeKeepingCentre(76,3),1.5f); }
        void mouseDown(const juce::MouseEvent&) override { original=owner.dockHeight; }
        void mouseDrag(const juce::MouseEvent& e) override { owner.dockHeight=juce::jlimit(130,400,original-e.getDistanceFromDragStartY()); owner.resized(); }
        MainComponent& owner;
        int original=208;
    } dockResize{*this};
    juce::ComboBox rowBehaviour;
    juce::ToggleButton rowMute{"Mute intera cue"},rowLoop{"Loop di tutte le clip"};
    bool rowSettings=false;
    int nextCue=0;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Component::SafePointer<juce::DialogWindow> deviceDialog;
    std::unique_ptr<PianoRollEditor> clipEditor;
    std::unique_ptr<OscClipEditor> oscEditor;
    std::unique_ptr<juce::DocumentWindow> oscWindow;
    int oscEditorCellId=-1;
    std::unique_ptr<juce::DocumentWindow> clipWindow;
    int clipEditorCellId=-1;
    bool clipCollapsed=false;
    int dockHeight=208;
    std::optional<Row> copiedRow;
    std::vector<std::shared_ptr<const AudioClip>> copiedRowAudio;
    bool insertCopiedRowAfter(int index, const Row&, const std::vector<std::shared_ptr<const AudioClip>>&);
    std::optional<Cell> copiedCell;
    std::shared_ptr<const AudioClip> copiedAudio;
    ColumnType copiedType=ColumnType::audio;
    juce::TextButton clipCollapseButton{"Riduci"}, clipCloseButton{"Chiudi"};
    juce::UndoManager editHistory;
    juce::File projectFile;
    juce::String status="Pronto. Seleziona una cella Audio e importa un WAV.";
    bool dirty=false;
    int editorX=0;
    float displayPeak=0;
    int midiRefreshTicks=0;
    void timerCallback() override;
    void midiClipEdited(int cellId,const MidiClip& before,const MidiClip& after,bool complete);
    void restoreMidiClip(int cellId,const MidiClip& snapshot);
    void syncClipEditor();
    void syncOscEditor();

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
