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
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&,int,int) override;
    static constexpr int rowHeight=54, headerHeight=68, nameWidth=210, columnWidth=152;
private: MainComponent& owner;
};
class Timeline final : public juce::Component {
public: explicit Timeline(MainComponent& p) : owner(p) {} void paint(juce::Graphics&) override;
private: MainComponent& owner;
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
    void columnMenu(int column);
    void rowMenu(int row);
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
    juce::TextButton openButton{"Apri"},saveButton{"Salva"},newButton{"Nuovo"},addRowButton{"+ Cue"},addColumnButton{"+ Colonna"};
    juce::TextButton goButton{"GO  /  SPAZIO"},panicButton{"PANIC  /  ESC"},deviceButton{"Dispositivi audio"};
    juce::TextButton importButton{"Importa WAV"},playCellButton{"Avvia cella"},stopCellButton{"Ferma"},clearButton{"Rimuovi audio"};
    juce::TextButton tempoButton{"Mappa tempo"},applyButton{"Applica modifiche"};
    juce::TextEditor rowName,cellName,bpm;
    juce::ComboBox behaviour,meter,output;
    juce::ToggleButton follow{"Partecipa al GO della riga"},loop{"Ripeti in loop"};
    juce::Slider gain;
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
    void changed(const juce::String&);
    bool applyInspector();
    bool inspectorDiffers() const;
    void openProject();
    void saveProject();
    void withDiscardConfirmation(std::function<void()>);
    void showDevices();
    void editTempo();
    void message(const juce::String&);
    void send(int id,bool row,bool stop=false);
    void stopAll();
    bool memoryFits(const ClipMap&) const;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
}
