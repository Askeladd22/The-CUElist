#pragma once
#include <array>
#include "AudioEngine.h"
#include "ProjectSafety.h"
#include "ProjectMediaLoader.h"
#include "GainMeterFader.h"
#include "MixerControls.h"
#include "WorkspaceLayout.h"
#include "CueGridGeometry.h"
#include "OperatorNavigation.h"
#include "MidiInputManager.h"
#include "OscControlInput.h"
#include "ControlOutputRouter.h"
#include "QuickControlTargets.h"
#include "QuickLearnHotspot.h"
#include "ControlDispatchPolicy.h"
#include <set>
#include "Theme.h"
namespace cuelist {
class MainComponent;
class GuideWindow;
class PianoRollEditor;
class OscClipEditor;
class ControlPanel;
class MidiUndoAction;
// A tab supports direct double-click renaming while preserving normal
// single-click selection (and its transport-stop safety semantics).
// Vector panel-state icons stay crisp at Windows display scaling and avoid
// font glyph fallback. The two directions are intentionally distinct:
 // open -> collapse right; collapsed -> expand left.
class InspectorGripButton final : public juce::TextButton {
public:
    void setInspectorOpen(bool open) { inspectorOpen=open; repaint(); }
    void paintButton(juce::Graphics& g,bool hovered,bool pressed) override {
        const auto r=getLocalBounds().toFloat().reduced(.5f);
        const IconButtonState state{isEnabled(),hovered,pressed,false,
                                    hasKeyboardFocus(false)};
        drawIconButtonSurface(g,r,palette::teal,state,4.0f);

        const auto cx=r.getCentreX(), cy=r.getCentreY();
        // One outer hit surface, one slim panel rail and one chevron:
        // the previous inner rounded box made the 20px grip look crowded.
        g.setColour(palette::muted.withAlpha(.75f));
        g.drawLine(cx+4.0f,cy-6.5f,cx+4.0f,cy+6.5f,1.1f);
        juce::Path chevron;
        if (inspectorOpen) { // collapse towards the right
            chevron.startNewSubPath(cx-4.0f,cy-4.0f);
            chevron.lineTo(cx+0.0f,cy);
            chevron.lineTo(cx-4.0f,cy+4.0f);
        } else { // reopen towards the left
            chevron.startNewSubPath(cx+0.0f,cy-4.0f);
            chevron.lineTo(cx-4.0f,cy);
            chevron.lineTo(cx+0.0f,cy+4.0f);
        }
        g.setColour(hovered || pressed ? palette::text : palette::teal);
        g.strokePath(chevron,juce::PathStrokeType(1.8f,
            juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    }
private:
    bool inspectorOpen=true;
};

class CuelistTabButton final : public juce::TextButton {
public:
    std::function<void()> onRename;
    std::function<void(int)> onDragMove,onDropped;
    // Do not delegate a completed drag to Button::mouseUp: that could
    // also fire onClick and switch (stop) the live Cuelist.
    void mouseDown(const juce::MouseEvent& e) override {
        if (!e.mods.isLeftButtonDown() || e.mods.isPopupMenu()) {
            juce::TextButton::mouseDown(e);
            return;
        }
        pressing=true;wasDragged=false;
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (!pressing) return;
        if (!wasDragged && e.getDistanceFromDragStart()>=6)
            wasDragged=true;
        if (wasDragged && onDragMove && getParentComponent())
            onDragMove(e.getEventRelativeTo(getParentComponent()).x);
    }
    void mouseUp(const juce::MouseEvent& e) override {
        if (!pressing) {
            juce::TextButton::mouseUp(e);
            return;
        }
        pressing=false;
        if (wasDragged) {
            wasDragged=false;
            if (onDropped && getParentComponent())
                onDropped(e.getEventRelativeTo(getParentComponent()).x);
        } else if (e.getNumberOfClicks()==1 &&
                   getLocalBounds().contains(e.getPosition()) && onClick)
            onClick();
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override {
        if (e.mods.isLeftButtonDown() && onRename) onRename();
        else juce::TextButton::mouseDoubleClick(e);
    }
private:
    bool pressing=false,wasDragged=false;
};
class CueGrid final : public juce::Component, public juce::FileDragAndDropTarget,
                      public juce::SettableTooltipClient {
public:
    explicit CueGrid(MainComponent& parent) : owner(parent) {}
    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void showRowMenu(int row,juce::Point<int> clickPosition,int cellColumn=-1);
    void showCellMenu(int row,int column,juce::Point<int> screenPosition);
    int columnX(int index) const;
    int columnWidthAt(int index) const;
    int columnAt(int x) const;
    int totalColumnsWidth() const;
    void clickRowHeader(int row,int x,int localY,int clicks,juce::Point<int> screen);
    // Shared by the scrolled grid and the fixed column-header overlay.
    void clickColumnHeader(int gridX,int y,int clicks,juce::Point<int> screen);
    void doubleClickColumnHeader(int gridX,int y);
    static juce::Colour cueColour(int id);
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&,int,int) override;
    // Wider title/transport lane gives operators distinct Play, Stop and
    // actions targets without increasing cue row height.
    static constexpr int rowHeight=CueGridGeometry::rowHeight;
    static constexpr int headerHeight=CueGridGeometry::headerHeight;
    static constexpr int nameWidth=CueGridGeometry::nameWidth;
    static constexpr int columnWidth=CueGridGeometry::columnWidth;
    static constexpr int compressedWidth=CueGridGeometry::compressedWidth;
    // N3.2 follow-up: two visual tiers in the original 62px cue row.
    // The title, GO, Play, Stop and More remain independent hit targets.
    static juce::Rectangle<int> rowGoBounds(int y) { return {56,y+9,42,32}; }
    static juce::Rectangle<int> rowNameBounds(int y) { return {107,y+3,nameWidth-122,25}; }
    static juce::Rectangle<int> rowStatusBounds(int y) { return {107,y+31,94,20}; }
    static juce::Rectangle<int> rowPlayBounds(int y) { return {nameWidth-114,y+31,29,24}; }
    static juce::Rectangle<int> rowStopBounds(int y) { return {nameWidth-80,y+31,29,24}; }
    static juce::Rectangle<int> rowMenuBounds(int y) { return {nameWidth-46,y+31,29,24}; }
    static bool rowTitleHit(int x,int localY) {
        return rowNameBounds(0).contains(x,localY) ||
               rowStatusBounds(0).contains(x,localY);
    }
    // Shared by the painter and input handling to prevent discrepancies.
    static juce::Rectangle<int> cellPlayBounds(int x,int width,int y) {
        return {x+width-64,y+23,24,23};
    }
    static juce::Rectangle<int> cellStopBounds(int x,int width,int y) {
        return {x+width-32,y+23,24,23};
    }
    // A compact read-only marker; Auto Stop membership is edited through the
    // cell context menu or Inspector rather than an inline disclosure mode.
    static juce::Rectangle<int> cellContinueBadgeBounds(int x,int width,int y) {
        return {x+width-53,y+4,45,16};
    }
    // Small on-screen buttons; keep larger 24x23 mouse hit rectangles.
    static juce::Rectangle<int> cellButtonVisualBounds(juce::Rectangle<int> hit) {
        return hit.reduced(2,1);
    }
    static constexpr int rowContentInset=CueGridGeometry::rowContentInset; // shared paint/input offset
private:
    MainComponent& owner;
    int dragRow=-1, dragColumn=-1, dropRow=-1, dropColumn=-1;
    int hoverRow=-1, hoverColumn=-1; // visual only; never activates a cue
    bool dragging=false;
};
// Stays outside the horizontally scrolled content, but follows its vertical
// offset and reuses CueGrid rendering and row hit targets.
class PinnedCueHeaders final : public juce::Component,
                               public juce::SettableTooltipClient {
public:
    explicit PinnedCueHeaders(MainComponent& p):owner(p) {}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
private:
    MainComponent& owner;
    int dragCueId=-1,dropCueRow=-1;
    bool draggingCue=false;
};
// Draws the same column header as CueGrid, but stays above scrolling rows.
 // Horizontal scrolling changes its content offset, not its screen bounds.
class PinnedColumnHeaders final : public juce::Component,
                                  public juce::SettableTooltipClient {
public:
    explicit PinnedColumnHeaders(MainComponent& parent):owner(parent) {}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
    int gridXAt(int localX) const;
private:
    MainComponent& owner;
};
class CueMetricsOverlay final : public juce::Component {
public:
    explicit CueMetricsOverlay(MainComponent& parent):owner(parent) {}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
    static constexpr int width=CueGridGeometry::pinnedMetricsWidth;
    // N3.4: the same rectangles own drawing, hover and click hit-testing.
    // Keep the original 91px BPM editing target and the 78x26 Auto Stop
    // target; no Master Audio/inspector/layout dimensions change.
    static juce::Rectangle<int> bpmEditBounds(int rowTop) {
        return {0,rowTop,91,CueGridGeometry::rowHeight};
    }
    static juce::Rectangle<int> autoStopToggleBounds(int rowTop) {
        return {102,rowTop+8,78,26};
    }
private:
    enum class HoverTarget { none, bpm, autoStop };
    int hoverRow=-1;
    HoverTarget hoverTarget=HoverTarget::none;
    MainComponent& owner;
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
    juce::TextButton gridToggle{"Grid ON"};
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
    void refreshLearnVisuals();
    // Update only audio parameters changed by a mapped control. No layout,
    // port discovery or full editor refresh on every external CC/OSC sample.
    void syncAudioControls(int column);
    void refreshMeters();
    void resized() override;
    void paint(juce::Graphics&) override;
private:
    struct Strip;
    MainComponent& owner;
    juce::Component scrollableStrips;
    std::vector<std::unique_ptr<Strip>> strips;
    int scrollX=0;
    int masterRoutingChannels=-1, masterRoutingOutput=-1;
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
    void restoreOscClip(int cellId,const OscClip&);
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
    void toggleRowPlayPause(int row);
    void triggerCell();
    void triggerCellAt(int row,int column);
    void stopCellAt(int row,int column);
    void stopRow(int row);
    void setColumnUtilities(int column,float gain,float pan,bool muted,bool inverted);
    void setDataOutputMuted(int column,bool muted);
    void setColumnMasterSend(int column,bool enabled);
    void setColumnDirectPostFader(int column,bool post);
    void setColumnDirectEnabled(int column,bool enabled);
    void setMasterAudio(float gain);
    int audioFaderCeilingDb() const noexcept { return faderCeilingDb; }
    void setAudioFaderCeilingDb(int db);
    void setMasterPan(float pan);
    void setMasterMute(bool muted);
    void setMasterOutput(int channel);
    void setSafetyLimiter(bool enabled);
    void showDevices(); // Audio settings selector; directly testable for lifetime safety.
    void openControlPanel(bool editSet=false,int templateId=0);
    void editControlSet(int row,int column);
    bool createEmptyControlSet(int row,int column);
    bool duplicateControlSet(int sourceRow,int sourceColumn,int destRow,int destColumn);
    bool clearControlCellMap(int row,int column);
    int createReusableControlMap(const juce::String& name,const ControlSet& original);
    bool renameReusableControlMap(int mapId,const juce::String& name);
    bool deleteReusableControlMap(int mapId);
    int reusableControlMapUsage(int mapId) const;
    bool modifyReusableControlBinding(int mapId,int oldBindingId,ControlBinding binding,int operation);
    void showControlCellMenu(int row,int column,juce::Point<int> screenPosition);
    bool addControlSetBinding(int row,int column,ControlBinding);
    bool deleteControlSetBinding(int row,int column,int id);
    bool updateControlBinding(int row,int column,int id,ControlBinding updated,bool inControlSet);
    int saveControlSetTemplate(int row,int column,const juce::String& name);
    bool applyControlSetTemplate(int row,int column,int templateId);
    std::vector<EffectiveControl> effectiveControlSet(int cueId) const {
        int preset=0;
        for (const auto& row:show.rows)
            if (row.id==cueId) {preset=row.mappingPresetId;break;}
        return controlMapping.effectiveForCue(cueId,preset);
    }
    bool setControlColumnLocked(int column,bool locked,int sourceRow);
    bool setControlColumnPriority(int column,int priority);
    juce::Result selectControlInput(const juce::String&);
    void toggleAdditionalMidiInput(const juce::String&);
    void restoreAdditionalMidiInputs();
    bool isAdditionalMidiInputEnabled(const juce::String&) const;
    bool addMappingBinding(ControlBinding);
    bool deleteMappingBinding(int);
    int createAbstractControlFunction(const juce::String& name,ControlBinding destination);
    bool updateAbstractControlFunction(int id,const juce::String& name,
                                       ControlBinding destination);
    bool deleteAbstractControlFunction(int id);
    bool moveAbstractControlFunction(int id,int direction);
    juce::String exportControlPresetText() const;
    juce::Result importControlPresetText(const juce::String&);
    int createMappingPreset(const juce::String&);
    bool assignMappingPreset(int rowId,int presetId);
    void armControlLearn() { oscLearnArmed=false;controlLearnArmed=true; }
    void beginQuickLearn(bool osc);
    void chooseQuickLearnTarget(int functionId);
    void refreshQuickLearnMaps();
    void selectQuickLearnMap(int rowId,int columnId);
    void renameQuickLearnFunction(int functionId);
    void chooseQuickLearnGuiTarget(ControlTarget target,int columnId=0);
    bool isQuickLearnActive() const noexcept {return quickLearnMode!=QuickLearnMode::off;}
    bool isQuickLearnAwaitingInput() const noexcept {return quickLearnFunctionId>0 && isQuickLearnActive();}
    juce::Colour quickLearnAccent() const noexcept {
        return quickLearnMode==QuickLearnMode::osc ? palette::purple : palette::amber;
    }
    bool quickLearnPulseIsBright() const noexcept {return quickLearnPulseBright;}
    bool isQuickLearnTargetSelected(ControlTarget target,int columnId=0) const;
    void updateQuickLearnVisuals();
    void refreshQuickLearnTargets();
    void completeQuickLearn(const ControlEvent& event);
    void finishQuickLearn();
    void ensureStarterControlFunctions();
    bool startOscControlInput(int port);
    bool configureOscInputPorts(const juce::String& csv);
    juce::String configuredOscPorts() const;
    void armOscControlLearn() { controlLearnArmed=false;oscLearnArmed=true; }
    std::optional<ControlEvent> consumeLearnedInput() { auto result=learnedInput; learnedInput.reset(); return result; }
    void dispatchControlForTest(const ControlEvent& event) { dispatchControl(event); }
    void refreshForTest() { refresh(); } // Test-only bridge; refresh remains private.
    int activeCueMappingId() const noexcept { return activeCueId; }
    void setColumnOutput(int column,int output);
    void setAutoStop(int row,bool enabled);
    void setRowColour(int row,int colour);
    void toggleColumnCollapsed(int column);
    void setAllColumnsCollapsed(bool collapsed);
    void activateCuelist(int index);
    void createCuelist();
    bool moveCuelist(int from,int to);
    void deleteCuelist(int index);
    void requestDeleteCuelist(int index);
    void renameActiveCuelist();
    void renameCuelist(int index);
    bool setCuelistName(int index,const juce::String& name);
    void setCellAutoStop(int row,int column,bool enabled);
    void renameTitle(bool column,int index);
    // Shared non-modal, in-place renaming for tabs, cues and columns.
    bool commitInlineRename();
    void cancelInlineRename();
    bool isInlineRenaming() const noexcept {
        return inlineRenameKind!=InlineRenameKind::none;
    }
    void reorderRow(int index,int delta);
    bool moveCueRow(int from,int to);
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
    bool cutSelectedCell();
    bool deleteSelectedCell();
    void setCellLoop(int row,int column,bool enabled);
    void setCellFollowsRow(int row,int column,bool enabled);
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
    friend class CueGrid; friend class PinnedCueHeaders; friend class PinnedColumnHeaders; friend class CueMetricsOverlay; friend class Timeline; friend class MidiUndoAction; friend class ColumnUtilities; friend class ControlPanel;
    Theme theme;
    std::unique_ptr<juce::PropertiesFile> appPreferences;
    int faderCeilingDb=6;
    juce::TooltipWindow tooltip{this,600};
    // Native scrolling moves the grid; these overlay siblings keep the
    // entire 72px heading fixed and repaint on every visible-area change.
    struct CueViewport final : juce::Viewport {
        std::function<void()> onScroll;
        void visibleAreaChanged(const juce::Rectangle<int>&) override {
            if(onScroll) onScroll();
        }
    };
    CueGrid grid{*this}; PinnedCueHeaders cueHeaders{*this};
    PinnedColumnHeaders columnHeaders{*this}; CueMetricsOverlay cueMetrics{*this};
    Timeline timeline{*this}; CueViewport viewport;
    ColumnUtilities utilities{*this};
    juce::TextButton openButton{"Open"},saveButton{"Save"},newButton{"New"},addRowButton{"+"},addColumnButton{"+"};
    juce::TextButton preferencesButton{"Preferences"},helpButton{"Help"};
    // V2 operator navigation. Existing command handlers are reused; the
    // original mixer and Master Audio are deliberately outside this phase.
    std::array<std::unique_ptr<OperatorNavigationButton>,8> operatorMenus;
    void showOperatorMenu(int index);
    void dispatchOperatorMenu(int index,int command);
    void dispatchOperatorDeviceChoice(int choice,
        const juce::Array<juce::MidiDeviceInfo>& inputs,
        const juce::Array<juce::MidiDeviceInfo>& outputs);
    juce::Label projectNameLabel;
    enum class InlineRenameKind {none,cuelist,cue,column};
    InlineRenameKind inlineRenameKind=InlineRenameKind::none;
    int inlineRenameIndex=-1,inlineRenameModelId=-1;
    bool inlineRenamePinned=false;
    juce::TextEditor inlineNameEditor;
    void beginInlineRename(InlineRenameKind kind,int index,bool pinned=false);
    void positionInlineNameEditor();
    juce::TextButton preflightButton{"Preflight"},diagnosticsButton{"Diagnostics"};
    juce::TextButton goButton{"GO  /  SPACE"},panicButton{"PANIC  /  ESC"},deviceButton{"Audio devices"},controlButton{"Control Mapping"};
    juce::TextButton collapseAllButton{"Collapse all"};
    InspectorGripButton inspectorSideTab;
    // Up to twelve independent Cuelists; only the physically fitting tabs
    // are displayed. Their tab and close controls have stable identity.
    std::array<std::unique_ptr<CuelistTabButton>,12> cuelistTabs;
    std::array<std::unique_ptr<juce::TextButton>,12> cuelistCloseButtons;
    juce::TextButton cuelistOverflowButton{"..."},addCuelistButton{"+"};
    juce::TextButton cuelistMoveLeft{"<"},cuelistMoveRight{">"};
    int cuelistDragSource=-1,cuelistDragTarget=-1;
    void updateCuelistDropTarget(int rootX);
    juce::TextButton undoButton{"Undo"},redoButton{"Redo"};
    juce::TextButton globalMidiLearn{"M Learn"},globalOscLearn{"O Learn"};
    QuickControlTargets quickControlTargets;
    juce::ComboBox quickLearnMapSelector;
    std::vector<std::pair<int,int>> quickLearnMapLocations;
    int quickLearnMapRowId=0,quickLearnMapColumnId=0;
    juce::TextButton cancelQuickLearn{"Cancel Learn"};
    QuickLearnHotspot goLearnHotspot{"Go"},panicLearnHotspot{"Panic"};
    QuickLearnHotspot masterGainLearnHotspot{"Master Gain"},masterPanLearnHotspot{"Master Pan"},
        masterLimiterLearnHotspot{"Limiter On/Off"};
    void refreshCuelistTabs();
    void showCuelistOverflowMenu();
    std::unique_ptr<GuideWindow> userGuideWindow;
    void openUserGuide();
    void showAbout();
    juce::TextButton importButton{"Import WAV / MP3"},playCellButton{"Play cell"},stopCellButton{"Stop"},clearButton{"Remove audio"};
    juce::TextButton midiEditButton{"Create MIDI clip"};
    juce::TextButton oscEditButton{"Create OSC clip"};
    juce::TextButton cellTab{"Cell"},rowTab{"Entire cue"};
    juce::TextButton tempoButton{"Tempo map"},applyButton{"Apply changes"};
    juce::TextEditor rowName,cellName,bpm,cueNotes;
    juce::ComboBox behaviour;
    juce::ToggleButton follow{"Include in row GO"},loop{"Loop playback"};
    juce::ToggleButton cellAutoStop{"Stop on next cue GO"};
    juce::TextEditor gainDb;
    juce::Slider rowGain;
    GainMeterFader masterSlider;
    juce::Slider masterPan;
    UtilityIcon masterMute{false};
    juce::ComboBox masterOutput;
    LimiterToggle safetyLimiter;
    struct TimelineResizeHandle : juce::Component, juce::SettableTooltipClient {
        explicit TimelineResizeHandle(MainComponent& p) : owner(p) {
            setName("timelineResizeHandle");
            setTooltip("Drag vertically to resize the Timeline");
            setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        }
        void paint(juce::Graphics& g) override {
            // Recessed splitter, not a detached bright scrollbar-like line.
            const auto area=getLocalBounds().toFloat();
            g.setColour(palette::background.withAlpha(.92f));
            g.fillRoundedRectangle(area.reduced(0.0f,1.0f),3.0f);
            g.setColour(palette::line.withAlpha(.80f));
            g.fillRoundedRectangle(area.withSizeKeepingCentre(57.0f,3.0f),
                                   1.5f);
        }
        void mouseDown(const juce::MouseEvent&) override {
            original=owner.workspaceLayout().effectiveTimelineAdjustment;
        }
        void mouseDrag(const juce::MouseEvent& e) override {
            owner.timelineHeightAdjustment=juce::jlimit(-600,400,
                original-e.getDistanceFromDragStartY());
            owner.resized();
            owner.repaint();
        }
        MainComponent& owner;
        int original=0;
    } timelineResize{*this};
    struct DockResizeHandle : juce::Component, juce::SettableTooltipClient {
        explicit DockResizeHandle(MainComponent& p) : owner(p) { setMouseCursor(juce::MouseCursor::UpDownResizeCursor); }
        void paint(juce::Graphics& g) override { g.setColour(palette::muted); g.fillRoundedRectangle(getLocalBounds().toFloat().withSizeKeepingCentre(76,3),1.5f); }
        void mouseDown(const juce::MouseEvent&) override {
            // Drag from the dock's actual visible height, not a remembered
            // preferred height that may be larger than the current window.
            original=owner.workspaceLayout().effectiveDockHeight;
        }
        void mouseDrag(const juce::MouseEvent& e) override { owner.dockHeight=juce::jlimit(130,400,original-e.getDistanceFromDragStartY()); owner.resized(); }
        MainComponent& owner;
        int original=208;
    } dockResize{*this};
    juce::ComboBox rowBehaviour;
    juce::ToggleButton rowMute{"Mute entire cue"},rowLoop{"Loop all clips"};
    bool rowSettings=false;
    int nextCue=0;
    MidiInputManager midiInput;
    std::vector<std::pair<juce::String,std::unique_ptr<MidiInputManager>>> additionalMidiInputs;
    OscControlInput oscInput;
    ControlMappingEngine controlMapping;
    ControlOutputRouter controlOutput;
    int activeCueId=0;
    enum class QuickLearnMode {off,midi,osc};
    QuickLearnMode quickLearnMode=QuickLearnMode::off;
    int quickLearnFunctionId=0;
    int quickLearnArmedEntryId=0;
    int quickLearnPulseTicks=0;
    bool quickLearnPulseBright=true;
    juce::String quickLearnFeedback;
    bool controlLearnArmed=false,oscLearnArmed=false;
    std::optional<ControlEvent> learnedInput;
    juce::Component::SafePointer<juce::DialogWindow> controlDialog;
    PhysicalControlEdges physicalEdges;
    void resetActiveMapping();
    void dispatchControl(const ControlEvent&);
    void refreshControlSettings();
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
    int timelineHeightAdjustment=0;
    std::optional<Row> copiedRow;
    std::vector<std::shared_ptr<const AudioClip>> copiedRowAudio;
    bool insertCopiedRowAfter(int index, const Row&, const std::vector<std::shared_ptr<const AudioClip>>&);
    std::optional<Cell> copiedCell;
    std::shared_ptr<const AudioClip> copiedAudio;
    ColumnType copiedType=ColumnType::audio;
    juce::TextButton clipCollapseButton{"Collapse"}, clipCloseButton{"Close"};
    juce::UndoManager editHistory;
    juce::File projectFile;
    juce::String status="Ready. Select an Audio cell and import a WAV file.";
    bool dirty=false;
    bool inspectorVisible=true; // UI-only; never persisted in the Show file
    int visibleClipCount=0;
    float systemCpuPercent=-1.f,systemRamPercent=-1.f;
    int systemTelemetryTicks=0;
    uint64_t previousIdleTime=0,previousKernelTime=0,previousUserTime=0;
    void refreshSystemTelemetry();
    WorkspaceLayout workspaceLayout() const;
    int editorX=0;
    float displayPeak=0;
    int midiRefreshTicks=0;
    uint64_t lastMidiRx=0,lastMidiTx=0,lastOscTx=0,lastOscRx=0;
    bool activityInitialized=false;
    double midiRxUntil=0,midiTxUntil=0,oscTxUntil=0,oscRxUntil=0;
    unsigned midiInputReconnectAttempts=0;
    int recoveryTicks=0;
    bool recoveryEnabled=false;
    bool preserveDeferredRecovery=false;
    std::unique_ptr<RecoveryCheckpointWriter> recoveryWriter;
    std::unique_ptr<ProjectLoadWorker> projectLoader;
    void offerCrashRecovery();
    bool activateProject(Show candidate,const juce::File& source,bool recovering);
    bool commitPreparedProject(Show candidate,const juce::File& source,bool recovering,
                               PreparedShowMedia prepared);
    void showPreflight();
    void showDiagnostics();
    void showColumnMenu(juce::Point<int> screenPoint);
    void addCue();
    void timerCallback() override;
    void midiClipEdited(int cellId,const MidiClip& before,const MidiClip& after,bool complete);
    void restoreMidiClip(int cellId,const MidiClip& snapshot);
    void syncClipEditor();
    void oscClipEdited(int cellId,const OscClip& before,const OscClip& after,bool complete);
    void syncOscEditor();

    void refresh();
    void refreshInspector();
    void changed(const juce::String&,bool stopPlayback=false);
    bool applyInspector();
    bool inspectorDiffers() const;
    void openProject();
    void saveProject();
    void withDiscardConfirmation(std::function<void()>);
    void editTempo();
    void message(const juce::String&);
    bool send(int id,bool row,bool stop=false);
    void stopAll();
    bool memoryFits(const ClipMap&) const;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
}
