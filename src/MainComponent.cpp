#include "MainComponent.h"
#include "ProjectSafety.h"
#include "PianoRoll.h"
#include "OscEditor.h"
#include "MidiClip.h"
#include "BinaryData.h"
#include <sstream>

namespace cuelist {
namespace {
juce::String typeName(ColumnType t) { return t==ColumnType::audio ? "AUDIO" : t==ColumnType::midi ? "MIDI" : t==ColumnType::osc ? "OSC" : "CONTROL"; }
juce::String duration(double seconds) {
    int s=(int)seconds; return juce::String(s/60).paddedLeft('0',2)+":"+juce::String(s%60).paddedLeft('0',2);
}
void prompt(juce::Component* parent,const juce::String& title,const juce::String& hint,const juce::String& initial,
            bool multiline,std::function<void(juce::String)> result) {
    auto* alert=new juce::AlertWindow(title,hint,juce::MessageBoxIconType::NoIcon,parent);
    alert->addTextEditor("value",initial,{},false); auto* editor=alert->getTextEditor("value");
    if (multiline) { editor->setMultiLine(true); editor->setReturnKeyStartsNewLine(true); editor->setSize(440,180); }
    if (!multiline) editor->onReturnKey=[alert] { alert->exitModalState(1); };
    alert->addButton("Apply",1); alert->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
    alert->enterModalState(true,juce::ModalCallbackFunction::create([alert,result](int answer) {
        if (answer==1) result(alert->getTextEditorContents("value"));
    }),true);
    editor->grabKeyboardFocus(); editor->selectAll();
    juce::MessageManager::callAsync([safe=juce::Component::SafePointer<juce::TextEditor>(editor)] {
        if (safe && safe->isShowing()) { safe->grabKeyboardFocus(); safe->selectAll(); }
    });
}
}

MainComponent::MainComponent(bool startAudio) {
    setLookAndFeel(&theme); setWantsKeyboardFocus(true); setSize(1440,900);
    for (juce::Component* c : std::initializer_list<juce::Component*>{&openButton,&saveButton,&newButton,&addRowButton,&addColumnButton,
         &goButton,&panicButton,&deviceButton,&controlButton,&dockResize,&importButton,&midiEditButton,&oscEditButton,&playCellButton,&stopCellButton,&clearButton,&tempoButton,&applyButton,
         &cellTab,&rowTab,&rowGain,&rowBehaviour,&rowMute,&rowLoop,&rowName,&cellName,&bpm,&behaviour,&follow,&loop,&cellAutoStop,&gain,&viewport,&timeline,&utilities,
         &clipCollapseButton,&clipCloseButton,&cueMetrics,&preflightButton,&diagnosticsButton}) addAndMakeVisible(c);
    for (auto* b : {&openButton,&saveButton,&newButton,&addRowButton,&addColumnButton,&goButton,&panicButton,&deviceButton,&controlButton,
                    &importButton,&midiEditButton,&oscEditButton,&playCellButton,&stopCellButton,&clearButton,&tempoButton,&applyButton,
                    &preflightButton,&diagnosticsButton}) b->setWantsKeyboardFocus(false);
    viewport.setViewedComponent(&grid,false); viewport.setScrollBarsShown(true,true);
    goButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff097b70));
    goButton.setColour(juce::TextButton::textColourOffId,palette::text);
    panicButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff923b49));
    panicButton.setColour(juce::TextButton::textColourOffId,palette::text);
    importButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff097b70));
    behaviour.addItemList({"Retrigger","Pause / Restart","Pause / Resume","Continue"},1);
    gain.setSliderStyle(juce::Slider::LinearHorizontal); gain.setTextBoxStyle(juce::Slider::TextBoxRight,false,52,24);
    for (auto* slider : {&gain,&rowGain}) {
        slider->setSliderStyle(juce::Slider::LinearHorizontal); slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,72,24);
        slider->setRange(-60,12,0.1); slider->setNumDecimalPlacesToDisplay(1); slider->setTextValueSuffix(" dB"); slider->setDoubleClickReturnValue(true,0);
    }
    gain.setName("cellGainDb"); rowGain.setName("rowGainDb");
    masterSlider.setName("masterAudioDb");
    masterSlider.setTooltip("MASTER AUDIO: PCM output fader and L/R meter. Double-click: 0 dB.");
    utilities.addAndMakeVisible(masterSlider);
    masterSlider.onGainChanged=[this](float db) {
        setMasterAudio(juce::Decibels::decibelsToGain(db,-60.0f));
    };
    configureMixerPan(masterPan);
    masterPan.setName("masterPan");
    masterPan.setTooltip("Overall PCM output stereo balance. Double-click: center.");
    utilities.addAndMakeVisible(masterPan);
    masterPan.onValueChange=[this] { setMasterPan((float)masterPan.getValue()); };
    masterMute.setName("masterMute");
    masterMute.setTooltip("Mute the PCM master without stopping clips or MIDI/OSC messages.");
    masterMute.setWantsKeyboardFocus(false);
    utilities.addAndMakeVisible(masterMute);
    masterMute.onClick=[this] { setMasterMute(masterMute.getToggleState()); };
    rowBehaviour.addItemList({"Use cell settings","Retrigger","Pause / Restart","Pause / Resume","Continue"},1);
    cellTab.setWantsKeyboardFocus(false); rowTab.setWantsKeyboardFocus(false);
    cellTab.onClick=[this] { if (applyInspector()) { rowSettings=false; refresh(); } };
    rowTab.onClick=[this] { if (applyInspector()) { rowSettings=true; refresh(); } };
    rowName.setSelectAllWhenFocused(true); cellName.setSelectAllWhenFocused(true); bpm.setInputRestrictions(6,"0123456789.");
    rowName.setName("cueName"); cellName.setName("cellName"); bpm.setName("cueBpm");
    for (auto* editor : {&rowName,&cellName,&bpm}) editor->setEscapeAndReturnKeysConsumed(false);
    for (auto* editor : {&rowName,&cellName,&bpm})
        editor->setFont(font(14.0f));
    bpm.setSelectAllWhenFocused(true);
    for (auto* editor : {&rowName,&cellName,&bpm}) editor->onReturnKey=[this] { if (applyInspector()) grabKeyboardFocus(); };
    gain.onValueChange=[this] { applyInspector(); }; rowGain.onValueChange=[this] { applyInspector(); };
    for (auto* combo : {&behaviour,&rowBehaviour}) combo->onChange=[this] { applyInspector(); };
    for (auto* toggle : {&follow,&loop,&rowMute,&rowLoop}) toggle->onClick=[this] { applyInspector(); };
    cellAutoStop.setName("cellAutoStop");
    cellAutoStop.setTooltip("The cell stops on another cue's GO only if its row's Auto Stop is enabled.");
    cellAutoStop.onClick=[this] { setCellAutoStop(selectedRow,selectedColumn,cellAutoStop.getToggleState()); };
    dockResize.setName("clipEditorResizeHandle");
    dockResize.setTooltip("Drag up or down to resize the docked Clip Editor.");
    clipCollapseButton.setName("clipEditorCollapse");
    clipCloseButton.setName("clipEditorClose");
    clipCollapseButton.setVisible(false); clipCloseButton.setVisible(false);
    clipCollapseButton.onClick=[this] {
        clipCollapsed=!clipCollapsed;
        clipCollapseButton.setButtonText(clipCollapsed ? "Expand" : "Collapse");
        resized(); repaint();
    };
    clipCloseButton.onClick=[this] { closeMidiClip(); closeOscClip(); };
    newButton.onClick=[this] { withDiscardConfirmation([this] { closeMidiClip(); closeOscClip(); editHistory.clearUndoHistory(); stopAll(); show=makeShow(); clips.clear(); projectFile={}; selectedRow=selectedColumn=nextCue=0;
        resetActiveMapping(); refreshControlSettings();
        dirty=false; if (recoveryEnabled) {ProjectRecovery::discard(ProjectRecovery::defaultDirectory());preserveDeferredRecovery=false;}
        engine.publish(makeRenderState(show,clips)); refresh(); status="New show. Import files into Audio cells."; }); };
    openButton.onClick=[this] { withDiscardConfirmation([this] { openProject(); }); };
    preflightButton.setName("showPreflight");
    diagnosticsButton.setName("showDiagnostics");
    preflightButton.setTooltip("Check show media, output devices and controller availability before a performance.");
    diagnosticsButton.setTooltip("Live engine status and counters for dropped MIDI/OSC/audio commands.");
    preflightButton.onClick=[this] { showPreflight(); };
    diagnosticsButton.onClick=[this] { showDiagnostics(); };
    saveButton.onClick=[this] { saveProject(); };
    addRowButton.onClick=[this] { if (!applyInspector()) return; addRow(show,{}); selectedRow=nextCue=(int)show.rows.size()-1; changed("Cue added. Playback stopped.",true); };
    addColumnButton.onClick=[this] {
        if (!applyInspector()) return;
        juce::PopupMenu menu; menu.addItem(1,"Audio"); menu.addItem(2,"MIDI"); menu.addItem(3,"OSC / UDP"); menu.addItem(4,"Control");
        auto safe=juce::Component::SafePointer<MainComponent>(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&addColumnButton),[safe](int choice) {
            if (!safe || choice==0) return; auto type=(ColumnType)(choice-1);
            addColumn(safe->show,type,{});
            safe->selectedColumn=(int)safe->show.columns.size()-1; safe->changed("Column added. Playback stopped.",true);
        });
    };
    goButton.onClick=[this] { goNext(); }; panicButton.onClick=[this] { stopAll(); };
    deviceButton.onClick=[this] { showDevices(); };
    controlButton.setTooltip("MIDI input, MIDI Learn, and global/per-cue mappings.");
    controlButton.onClick=[this] {
        if (show.columns[(size_t)selectedColumn].type==ColumnType::control)
            editControlSet(selectedRow,selectedColumn);
        else openControlPanel(false); // legacy global/preset editor remains available
    };
    importButton.onClick=[this] { importDialog(); };
    playCellButton.onClick=[this] { triggerCell(); }; stopCellButton.onClick=[this] { send(show.rows[selectedRow].cells[selectedColumn].id,false,true); };
    clearButton.onClick=[this] {
        auto& c=show.rows[selectedRow].cells[selectedColumn];
        clips.erase(c.id); c.file.clear(); c.name.clear(); c.midi.reset(); c.osc.reset();
        if (show.columns[(size_t)selectedColumn].type==ColumnType::control)
            c.controlSet.reset();
        closeOscClip();
        refreshControlSettings(); changed("Cell content removed. Original files retained.",true);
    };
    midiEditButton.onClick=[this] { editMidiClip(); };
    oscEditButton.onClick=[this] {
        if (show.columns[(size_t)selectedColumn].type==ColumnType::control)
            editControlSet(selectedRow,selectedColumn);
        else editOscClip();
    };
    applyButton.onClick=[this] { applyInspector(); }; tempoButton.onClick=[this] { editTempo(); };
    importButton.setTooltip("Mono/stereo WAV and MP3. Importing stops all playback.");
    applyButton.setTooltip("Apply parameters without stopping playback. Enter confirms names.");
    addColumnButton.setTooltip("Add columns. Double-click a title to rename; use arrows to reorder.");
    if (startAudio) { auto error=engine.initialise(); if (error.isNotEmpty()) status="Audio unavailable: "+error; }
    else status="Offline mode: audio device not opened.";
    refreshControlSettings();
    engine.publish(makeRenderState(show,clips)); refresh(); startTimerHz(25);
    recoveryEnabled=juce::JUCEApplication::getInstance()!=nullptr;
    if (recoveryEnabled && ProjectRecovery::exists(ProjectRecovery::defaultDirectory())) {
        auto safe=juce::Component::SafePointer<MainComponent>(this);
        juce::MessageManager::callAsync([safe] {if (safe) safe->offerCrashRecovery();});
    }
}
MainComponent::~MainComponent() {
    stopTimer(); engine.panic(); chooser.reset();
    if (recoveryEnabled && !preserveDeferredRecovery)
        ProjectRecovery::discard(ProjectRecovery::defaultDirectory());
    // The device selector holds a reference to engine.devices: destroy it first.
    closeMidiClip();
    closeOscClip();
    delete deviceDialog.getComponent();
    delete controlDialog.getComponent();
    viewport.setViewedComponent(nullptr,false); setLookAndFeel(nullptr);
}

void MainComponent::paint(juce::Graphics& g) {
    const int w=getWidth(),h=getHeight(); g.fillAll(palette::background);
    g.setColour(palette::raised); g.fillRect(0,0,w,72);
    g.setColour(palette::panel); g.fillRect(0,73,180,h-73);
    g.setColour(palette::teal); g.fillRect(0,0,w,3);
    g.setColour(palette::line); g.drawHorizontalLine(72,0,(float)w);
    g.drawVerticalLine(180,73,(float)h);
    // Keep the project card, but remove the redundant static ENGINES panel.
    // Engine controls, routing and the live audio-status indicator are unchanged.
    g.setColour(palette::background.withAlpha(.30f));
    g.fillRoundedRectangle(10.0f,238.0f,160.0f,143.0f,8.0f);
    static const auto appIcon=juce::ImageFileFormat::loadFrom(
        BinaryData::thecuelist_icon_png,BinaryData::thecuelist_icon_pngSize);
    if (appIcon.isValid())
        g.drawImageWithin(appIcon,19,16,38,39,juce::RectanglePlacement::centred);
    else for (int i=0;i<3;++i) {
        g.setColour(i==1 ? palette::teal : palette::text);
        g.fillRoundedRectangle(23.0f,23.0f+i*9,23.0f-i*4,4,2);
    }
    label(g,"The Cuelist",{60,14,165,32},24,palette::text,true);
    label(g,"SHOW CONTROL",{61,45,130,16},11,palette::muted,true);
    label(g,"SESSION",{232,25,100,22},12,palette::teal,true);
    g.setColour(palette::teal); g.fillRect(232,69,65,3);
    label(g,"CONTROL / MAPPING v10",{334,25,210,22},12,palette::muted);

    g.setColour(engine.connected.load() ? palette::teal : palette::red); g.fillEllipse((float)w-377,31,9,9);
    label(g,engine.connected.load() ? "Audio available" : "Audio unavailable",{w-359,22,172,25},12,palette::muted);
    label(g,"WORKSPACE",{20,99,140,18},11,palette::muted,true);
    g.setColour(palette::teal.withAlpha(0.19f)); g.fillRoundedRectangle(12,131,156,45,8);
    g.setColour(palette::teal); g.fillRoundedRectangle(12,137,4,34,2);
    label(g,"Cuelist",{28,138,125,30},16,palette::text,true);
    label(g,"PROJECT",{20,242,140,18},11,palette::muted,true);
    label(g,show.name,{20,268,143,30},17,palette::text,true);
    label(g,juce::String((int)show.rows.size())+" cues / "+juce::String((int)show.columns.size())+" columns",{20,302,150,22},13,palette::muted);
    label(g,hasUnsavedChanges() ? "Unsaved changes" : "No changes",{20,330,153,22},11,hasUnsavedChanges() ? palette::amber : palette::muted);
    g.setColour(palette::amber.withAlpha(.11f));
    g.fillRoundedRectangle(12.0f,(float)h-339,156.0f,51.0f,8.0f);
    label(g,"ON DECK",{20,h-333,145,16},11,palette::amber,true);
    label(g,nextCue<(int)show.rows.size() ? "NEXT  /  "+juce::String(nextCue+1).paddedLeft('0',2) :
          "END OF SHOW",{20,h-314,148,24},17,palette::text,true);
    // Output meter removed from the sidebar; audio peak telemetry remains
    // available in the Master and channel meters.
    label(g,"SPACE   Next cue",{20,h-84,150,20},12,palette::muted);
    label(g,"ESC          Panic",{20,h-61,150,20},12,palette::muted);
    g.setColour(palette::teal); g.fillRoundedRectangle(204.0f,88.0f,4.0f,45.0f,2.0f);
    label(g,"SESSION / CUE CONTROL",{219,89,390,32},25,palette::text,true);
    label(g,"SHOW WORKSPACE   •   AUDIO  /  MIDI  /  OSC  /  CONTROL",{219,124,510,20},12,palette::muted,true);

    constexpr int inspectorWidth=288, inspectorInner=236;
    auto panel=juce::Rectangle<int>(editorX,154,inspectorWidth-20,h-201);
    g.setColour(palette::panel); g.fillRoundedRectangle(panel.toFloat(),9);
    g.setColour(palette::line); g.drawRoundedRectangle(panel.toFloat(),9,1.25f);
    const int x=editorX+16;
    g.setColour(palette::teal); g.fillRoundedRectangle((float)editorX+13,163.0f,4.0f,49.0f,2.0f);
    g.setColour(palette::raised.withAlpha(.52f));
    g.fillRoundedRectangle((float)x-7,228.0f,(float)inspectorInner+14,177.0f,7.0f);
    g.fillRoundedRectangle((float)x-7,414.0f,(float)inspectorInner+14,
                           (float)std::max(130,h-476),7.0f);
    label(g,"INSPECTOR",{x+10,169,200,19},13,palette::teal,true);
    const auto& col=show.columns[selectedColumn];
    label(g,"CUE "+juce::String(selectedRow+1).paddedLeft('0',2),{x+10,195,140,28},22,palette::text,true);
    label(g,typeName(col.type),{x+131,200,98,21},11,palette::type((int)col.type),true,juce::Justification::centredRight);
    g.saveState();
    { const float factor=juce::jlimit(0.72f,0.92f,(float)(h-232)/730.0f);
      g.addTransform(juce::AffineTransform(1,0,0,0,factor,226*(1-factor))); }
    label(g,"Cue name",{x,235,inspectorInner,18},13,palette::text,true);
    label(g,"Initial BPM",{x,303,inspectorInner,18},13,palette::text,true);
    g.setColour(palette::line); g.drawHorizontalLine(405,(float)x,(float)x+inspectorInner);
    if (!rowSettings) {
    label(g,"CELL  /  "+col.name,{x,420,inspectorInner,20},14,palette::teal,true);
    label(g,"Playback mode",{x,490,inspectorInner,18},13,palette::text,true);
    if (col.type==ColumnType::audio) label(g,"Gain",{x,621,70,20},12,palette::muted);
    if (col.type==ColumnType::osc)
        label(g,"UDP destination in utilities",{x,707,inspectorInner,18},11,palette::teal);
    } else {
        label(g,"WHOLE CUE SETTINGS",{x,420,inspectorInner,20},14,palette::teal,true);
        label(g,"Playback on row GO",{x,450,inspectorInner,20},12,palette::muted);
        label(g,"Cue audio gain (dB)",{x,580,inspectorInner,20},12,palette::muted);
        label(g,"Gain affects audio; MIDI uses velocity.",{x,652,inspectorInner,40},11,palette::muted);
    }
    g.restoreState();
    label(g,status,{204,h-35,std::max(1,editorX-218),25},12,palette::muted);
}
void MainComponent::resized() {
    const int w=getWidth(),h=getHeight(); editorX=w-288;
    newButton.setBounds(w-190,23,68,29); openButton.setBounds(w-116,23,46,29); saveButton.setBounds(w-64,23,52,29);
    deviceButton.setBounds(20,188,140,33);
    controlButton.setBounds(std::max(506,w-666),22,144,30);

    preflightButton.setBounds(editorX-431,98,105,33);
    diagnosticsButton.setBounds(editorX-316,98,117,33);
    addRowButton.setBounds(editorX-188,98,76,33); addColumnButton.setBounds(editorX-104,98,92,33);
    const bool docked=(clipEditor && !clipWindow) || (oscEditor && !oscWindow);
    const int activeDockHeight=docked ? (clipCollapsed ? 36 : this->dockHeight) : 0;
    // Prioritise the running cue list on show-sized desktops while preserving
    // mixer controls, the timeline and docked editors at all window heights.
    // Keep at least one complete timeline lane visible alongside the larger
    // cue rows. Mixer and cue list remain scrollable; no panel disappears.
    const int gridHeight=std::max(310,h-400-activeDockHeight);
    const int utilityHeight=h<800 ? 180 : 208;
    viewport.setBounds(204,154,editorX-220,gridHeight-utilityHeight);
    utilities.setBounds(204,154+gridHeight-utilityHeight,editorX-220,utilityHeight);
    utilities.syncScroll(viewport.getViewPositionX());
    const int dockY=168+gridHeight;
    clipCollapseButton.setVisible(docked);
    clipCloseButton.setVisible(docked);
    dockResize.setVisible(docked && !clipCollapsed);
    if (docked && !clipCollapsed) {
        dockResize.setBounds(std::max(700,editorX-245),dockY-8,94,11);
        dockResize.toFront(false);
    }
    if (docked) {
        clipCollapseButton.setBounds(522,dockY+4,86,26);
        clipCloseButton.setBounds(615,dockY+4,82,26);
        clipCollapseButton.toFront(false); clipCloseButton.toFront(false);
        if (clipEditor && !clipWindow) {
            clipEditor->setVisible(!clipCollapsed);
            if (!clipCollapsed) clipEditor->setBounds(204,dockY,editorX-220,activeDockHeight);
        }
        if (oscEditor && !oscWindow) {
            oscEditor->setVisible(!clipCollapsed);
            if (!clipCollapsed) oscEditor->setBounds(204,dockY,editorX-220,activeDockHeight);
        }
    }
    timeline.setBounds(204,dockY+activeDockHeight,editorX-220,std::max(75,h-gridHeight-activeDockHeight-215));
    const int x=editorX+16, inner=236;
    cellTab.setBounds(x,198,112,30); rowTab.setBounds(x+118,198,118,30);
    rowBehaviour.setBounds(x,479,inner,33); rowMute.setBounds(x,527,inner,24);
    rowLoop.setBounds(x,553,inner,24); rowGain.setBounds(x,610,inner,29);
    rowName.setBounds(x,258,inner,35); bpm.setBounds(x,326,inner,35);
    tempoButton.setBounds(x,368,inner,30); cellName.setBounds(x,450,inner,33);
    behaviour.setBounds(x,512,inner,33);
    follow.setBounds(x-3,556,inner,24); loop.setBounds(x-3,587,inner,24);
    gain.setBounds(x+55,619,inner-55,28);
    cellAutoStop.setBounds(x-3,647,inner,24);
    midiEditButton.setBounds(x,673,inner,34);
    oscEditButton.setBounds(x,673,inner,34);
    importButton.setBounds(x,722,inner,35);
    playCellButton.setBounds(x,765,132,33); stopCellButton.setBounds(x+138,765,inner-138,33);
    clearButton.setBounds(x,804,105,31); applyButton.setBounds(x+111,804,inner-111,31);
    // Compact inspector on smaller displays, keeping all actions reachable.
    {
        const float factor=juce::jlimit(0.72f,0.92f,(float)(h-232)/730.0f);
        for (juce::Component* c : std::initializer_list<juce::Component*>{&rowBehaviour,&rowMute,&rowLoop,&rowGain,&rowName,&bpm,&tempoButton,&cellName,&behaviour,&follow,&loop,&cellAutoStop,&gain,&importButton,&midiEditButton,&oscEditButton,&playCellButton,&stopCellButton,&clearButton,&applyButton}) {
            auto b=c->getBounds(); b.setY(226+(int)((b.getY()-226)*factor)); b.setHeight(std::max(22,(int)(b.getHeight()*factor))); c->setBounds(b);
        }
    }
    goButton.setBounds(14,h-282,152,48);
    panicButton.setBounds(14,h-228,152,48);
    grid.setSize(std::max(viewport.getWidth()-14,grid.columnX((int)show.columns.size())+CueMetricsOverlay::width),
                 std::max(viewport.getHeight()-14,CueGrid::headerHeight+(int)show.rows.size()*CueGrid::rowHeight));
    cueMetrics.setBounds(viewport.getRight()-CueMetricsOverlay::width,viewport.getY(),
                         CueMetricsOverlay::width,viewport.getHeight());
    cueMetrics.toFront(false);
}
void MainComponent::refresh() {
    selectedRow=juce::jlimit(0,(int)show.rows.size()-1,selectedRow);
    selectedColumn=juce::jlimit(0,(int)show.columns.size()-1,selectedColumn);
    syncClipEditor(); syncOscEditor(); refreshInspector(); utilities.refresh(); resized();
    repaint(); grid.repaint(); timeline.repaint(); cueMetrics.repaint();
}
void MainComponent::refreshInspector() {
    const auto& row=show.rows[selectedRow]; const auto& c=row.cells[selectedColumn]; const auto& col=show.columns[selectedColumn];
    masterSlider.setGainDb(juce::Decibels::gainToDecibels(show.masterGain,-60.0f));
    masterPan.setValue(show.masterPan,juce::dontSendNotification);
    masterMute.setToggleState(show.masterMuted,juce::dontSendNotification);
    engine.monitoredRowId.store(row.id);
    rowName.setTextToShowWhenEmpty("Cue name",palette::muted);
    rowName.setText(row.name,false); cellName.setText(c.name,false); cellName.setTextToShowWhenEmpty("Cell name",palette::muted);
    bpm.setText(juce::String(row.tempo.front().bpm,1),false);
    behaviour.setSelectedId((int)c.behaviour+1,juce::dontSendNotification);
    follow.setToggleState(c.followsRow,juce::dontSendNotification); loop.setToggleState(c.loop,juce::dontSendNotification);
    cellAutoStop.setToggleState(c.stopOnNext,juce::dontSendNotification);
    cellAutoStop.setTooltip(row.stopOnNext ? "ON: participates in Auto Stop. OFF: continues after the next GO."
        : "Cue Auto Stop is OFF: this cell option applies when the cue is set to ON.");
    gain.setValue(juce::Decibels::gainToDecibels(c.gain,-60.0f),juce::dontSendNotification);
    rowGain.setValue(juce::Decibels::gainToDecibels(row.gain,-60.0f),juce::dontSendNotification);
    rowMute.setToggleState(row.muted,juce::dontSendNotification); rowLoop.setToggleState(row.loop,juce::dontSendNotification);
    rowBehaviour.setSelectedId(row.overrideBehaviour ? (int)row.behaviour+2 : 1,juce::dontSendNotification);
    for (juce::Component* component : std::initializer_list<juce::Component*>{&cellName,&behaviour,&follow,&loop,&cellAutoStop,&gain,&importButton,&midiEditButton,&oscEditButton,&playCellButton,&stopCellButton,&clearButton}) component->setVisible(!rowSettings);
    for (juce::Component* component : std::initializer_list<juce::Component*>{&rowBehaviour,&rowGain,&rowMute,&rowLoop}) component->setVisible(rowSettings);
    bool audio=col.type==ColumnType::audio; const bool midi=col.type==ColumnType::midi;
    for (juce::Component* component : std::initializer_list<juce::Component*>{&importButton,&midiEditButton,&oscEditButton,&playCellButton,&stopCellButton,&clearButton,&gain,&loop,&behaviour,&follow,&cellName}) component->setEnabled(true);
    gain.setVisible(!rowSettings && audio);
    midiEditButton.setEnabled(midi); oscEditButton.setEnabled(col.type==ColumnType::osc);
    midiEditButton.setVisible(!rowSettings && midi);
    oscEditButton.setVisible(!rowSettings && col.type==ColumnType::osc);
    oscEditButton.setButtonText(c.osc ? "Edit OSC clip" : "Create OSC clip");
    midiEditButton.setButtonText(c.midi ? "Edit piano roll" : "Create MIDI clip");
    importButton.setVisible(!rowSettings && (audio || midi));
    importButton.setButtonText(midi ? "Import MIDI" : "Import WAV / MP3");
    importButton.setTooltip(midi ? "Files .mid/.midi: import notes using cue tempo." : "Mono or stereo WAV / MP3.");
    clearButton.setButtonText(col.type==ColumnType::control ? "Clear set" :
        col.type==ColumnType::osc ? "Remove OSC" : midi ? "Remove MIDI" : "Remove audio");
    const bool control=col.type==ColumnType::control;
    const bool loaded=hasClip(selectedRow,selectedColumn);
    playCellButton.setVisible(!rowSettings && !control);
    stopCellButton.setVisible(!rowSettings && !control);
    clearButton.setVisible(!rowSettings && (!control || !col.controlLocked));
    clearButton.setEnabled(!control && (loaded || c.file.isNotEmpty()) ||
        control && c.controlSet.has_value() && !col.controlLocked);
    playCellButton.setEnabled(loaded); stopCellButton.setEnabled(loaded);
    oscEditButton.setVisible(!rowSettings && (col.type==ColumnType::osc || control));
    if (control) {
        oscEditButton.setButtonText(col.controlLocked ? "Edit default Control Set" : "Edit Control Set");
        oscEditButton.setEnabled(true);
    }
    if (col.collapsed && !rowSettings)
        for (juce::Component* component : std::initializer_list<juce::Component*>{
             &cellName,&behaviour,&follow,&loop,&gain,&cellAutoStop,
             &importButton,&midiEditButton,&oscEditButton,
             &playCellButton,&stopCellButton,&clearButton})
            component->setEnabled(false);
}
bool MainComponent::select(int row,int column) {
    if (row<0 || row>=(int)show.rows.size() || column<0 || column>=(int)show.columns.size() || !applyInspector()) return false;
    // Collapsed cells remain selectable as cues, but not editable.
    nextCue=row; selectedRow=row; selectedColumn=column; syncClipEditor(); syncOscEditor(); refreshInspector();
    const int top=CueGrid::headerHeight+row*CueGrid::rowHeight, bottom=top+CueGrid::rowHeight;
    int viewY=viewport.getViewPositionY();
    if (top<viewY) viewY=top; else if (bottom>viewY+viewport.getViewHeight()) viewY=bottom-viewport.getViewHeight();
    viewport.setViewPosition(viewport.getViewPositionX(),std::max(0,viewY));
    repaint(); grid.repaint(); timeline.repaint(); return true;
}
void MainComponent::changed(const juce::String& text,bool stopPlayback) {
    if (stopPlayback) { engine.panic(); resetActiveMapping(); }
    engine.publish(makeRenderState(show,clips),!stopPlayback);
    dirty=true; status=text; refresh();
}
bool MainComponent::inspectorDiffers() const {
    const auto& row=show.rows[selectedRow]; const auto& cell=row.cells[selectedColumn];
    if (rowName.getText().trim()!=row.name || bpm.getText()!=juce::String(row.tempo.front().bpm,1)) return true;
    if (rowMute.getToggleState()!=row.muted || rowLoop.getToggleState()!=row.loop
        || rowBehaviour.getSelectedId()!=(row.overrideBehaviour ? (int)row.behaviour+2 : 1)
        || std::abs(rowGain.getValue()-juce::Decibels::gainToDecibels(row.gain,-60.0f))>0.051) return true;
    if (show.columns[selectedColumn].collapsed) return false;
    if (show.columns[selectedColumn].type==ColumnType::osc)
        return cellName.getText().trim()!=cell.name || behaviour.getSelectedId()!=(int)cell.behaviour+1
            || follow.getToggleState()!=cell.followsRow || loop.getToggleState()!=cell.loop;
    return cellName.getText().trim()!=cell.name || behaviour.getSelectedId()!=(int)cell.behaviour+1 || follow.getToggleState()!=cell.followsRow
        || loop.getToggleState()!=cell.loop || std::abs(gain.getValue()-juce::Decibels::gainToDecibels(cell.gain,-60.0f))>0.051;
}
bool MainComponent::applyInspector() {
    if (!inspectorDiffers()) return true;
    auto candidate=show; auto& row=candidate.rows[selectedRow]; auto& cell=row.cells[selectedColumn];
    row.name=rowName.getText().trim();
    row.muted=rowMute.getToggleState(); row.loop=rowLoop.getToggleState();
    row.overrideBehaviour=rowBehaviour.getSelectedId()>1;
    if (row.overrideBehaviour) row.behaviour=(Behaviour)(rowBehaviour.getSelectedId()-2);
    row.gain=juce::Decibels::decibelsToGain((float)rowGain.getValue(),-60.0f);
    if (bpm.getText()!=juce::String(row.tempo.front().bpm,1)) row.tempo.front().bpm=bpm.getText().getDoubleValue();
    if (!candidate.columns[(size_t)selectedColumn].collapsed) {
        cell.name=cellName.getText().trim(); cell.behaviour=(Behaviour)(behaviour.getSelectedId()-1);
        cell.followsRow=follow.getToggleState(); cell.loop=loop.getToggleState();
        if (candidate.columns[selectedColumn].type!=ColumnType::osc)
            cell.gain=juce::Decibels::decibelsToGain((float)gain.getValue(),-60.0f);
    }
    auto result=validateShow(candidate); if (result.failed()) { message(result.getErrorMessage()); return false; }
    show=std::move(candidate); changed("Settings applied. BPM does not change audio playback speed.");
    return true;
}
void MainComponent::message(const juce::String& text) { status=text; repaint(); juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"The Cuelist",text,"OK",this); }
bool MainComponent::memoryFits(const ClipMap& collection) const {
    // Pasted Audio cells intentionally share one immutable PCM object; counting
    // the same shared media once per cell would reject valid large shows.
    int64_t bytes=0;
    std::set<const AudioClip*> counted;
    for (const auto& item:collection) if (item.second && counted.insert(item.second.get()).second)
        bytes+=(int64_t)item.second->samples.getNumSamples()*item.second->samples.getNumChannels()*4;
    return bytes<=512LL*1024*1024;
}
void MainComponent::importDialog() {
    if (show.columns[selectedColumn].type==ColumnType::osc ||
        show.columns[selectedColumn].collapsed) return;
    if (!applyInspector()) return;
    const int cellId=show.rows[selectedRow].cells[selectedColumn].id;
    const bool midi=show.columns[selectedColumn].type==ColumnType::midi;
    chooser=std::make_unique<juce::FileChooser>(midi ? "Import MIDI clip (notes)" : "Import mono or stereo WAV or MP3",juce::File{},midi ? "*.mid;*.midi" : "*.wav;*.mp3");
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
        [safe=juce::Component::SafePointer<MainComponent>(this),cellId](const juce::FileChooser& fc) {
            if (!safe || fc.getResult()==juce::File{}) return;
            for (size_t r=0;r<safe->show.rows.size();++r) for (size_t c=0;c<safe->show.columns.size();++c)
                if (safe->show.rows[r].cells[c].id==cellId) { if (safe->select((int)r,(int)c)) safe->importFile(fc.getResult()); return; }
        });
}
void MainComponent::importFile(const juce::File& file) {
    if (show.columns[(size_t)selectedColumn].collapsed) return;
    if (show.columns[selectedColumn].type==ColumnType::midi) {
        if (!applyInspector()) return;
        MidiClip imported; juce::String warning;
        auto result=loadMidiClip(file,imported,warning);
        if (result.failed()) { message(result.getErrorMessage()); return; }
        auto& cell=show.rows[selectedRow].cells[selectedColumn]; cell.midi=std::move(imported);
        cell.file.clear(); cell.name=file.getFileNameWithoutExtension();
        changed("MIDI clip imported. Double-click to open the piano roll.",true);
        if (warning.isNotEmpty()) message(warning);
        return;
    }
    if (show.columns[selectedColumn].type!=ColumnType::audio) { message("Import audio into an Audio column."); return; }
    engine.panic(); std::shared_ptr<const AudioClip> clip; auto result=loadClip(file,clip);
    if (result.failed()) { message(result.getErrorMessage()); return; }
    auto updated=clips; auto& c=show.rows[selectedRow].cells[selectedColumn]; updated[c.id]=clip;
    if (!memoryFits(updated)) { message("Preload limit reached: 512 MB PCM per show. The file was not added."); return; }
    clips=std::move(updated); c.file=file.getFullPathName(); c.name=file.getFileNameWithoutExtension();
    changed("Imported "+file.getFileName()+". Double-click the cell to play.");
}
bool MainComponent::send(int id,bool row,bool stop) {
    if (!engine.connected.load()) { message("No audio device available. Open Audio devices."); return false; }
    if (!engine.send(id,row,stop)) { engine.panic(); message("Command queue full: playback stopped. Try again."); return false; }
    status=stop ? "Cell stopped." : "Command sent to audio / MIDI transport."; return true;
}
void MainComponent::triggerRow(int row) {
    if (!select(row,selectedColumn)) return;
    bool rowPausing=false;
    for (const auto& cell:show.rows[row].cells) {
        const auto action=show.rows[row].overrideBehaviour ? show.rows[row].behaviour : cell.behaviour;
        if (cell.followsRow && (action==Behaviour::pauseResume || action==Behaviour::pauseRestart)
            && engine.getCellPlayback(cell.id).playing) rowPausing=true;
    }
    for (size_t c=0;c<show.columns.size();++c) if (show.columns[c].type==ColumnType::midi
        && show.rows[row].cells[c].midi && show.rows[row].cells[c].followsRow && !show.rows[row].cells[c].midi->notes.empty()) {
        const auto& cell=show.rows[row].cells[c];
        const auto action=show.rows[row].overrideBehaviour ? show.rows[row].behaviour : cell.behaviour;
        if (action==Behaviour::stop || ((action==Behaviour::pauseResume || action==Behaviour::pauseRestart) && rowPausing)
            || (action==Behaviour::continuePlaying && engine.getCellPlayback(cell.id).playing)) continue;
        const auto& identifier=show.columns[c].midiOutputId;
        if (identifier.isEmpty()) { message("Select the column MIDI output "+juce::String((int)c+1)+" before GO."); return; }
        auto ready=engine.setMidiOutput(identifier); if (ready.failed()) { message(ready.getErrorMessage()); return; }
    }
    engine.publish(makeRenderState(show,clips));
    if (send(show.rows[row].id,true)) {
        nextCue=row+1;
        // The mapping changes when the audio callback acknowledges execution,
        // not when this command is merely accepted into its lock-free queue.
        // Physical-button edges remain tracked across the pending transition.
    }
    repaint(); grid.repaint();
}
void MainComponent::goNext() { if (nextCue<(int)show.rows.size()) triggerRow(nextCue); else { status="End of sequence. Select a cue to restart."; repaint(); } }
void MainComponent::triggerCell() {
    if (!applyInspector()) return;
    auto& c=show.rows[selectedRow].cells[selectedColumn];
    if (show.columns[selectedColumn].type==ColumnType::control) {
        editControlSet(selectedRow,selectedColumn); return;
    }
    if (show.columns[selectedColumn].type==ColumnType::osc && !c.osc) { editOscClip(); return; }
    if (!hasClip(selectedRow,selectedColumn)) { if (show.columns[selectedColumn].type==ColumnType::midi) editMidiClip(); else if (show.columns[selectedColumn].type==ColumnType::osc) editOscClip(); else importDialog(); return; }
    const bool playing=engine.getCellPlayback(c.id).playing;
    const bool needsOutput=c.behaviour!=Behaviour::stop && !(playing && (c.behaviour==Behaviour::pauseResume
        || c.behaviour==Behaviour::pauseRestart || c.behaviour==Behaviour::continuePlaying));
    if (show.columns[selectedColumn].type==ColumnType::midi && needsOutput) {
        const auto& identifier=show.columns[selectedColumn].midiOutputId;
        if (identifier.isEmpty()) { message("Select a MIDI output in the column utilities."); return; }
        auto ready=engine.setMidiOutput(identifier); if (ready.failed()) { message(ready.getErrorMessage()); return; }
        engine.publish(makeRenderState(show,clips));
    }
    send(c.id,false);
}
void MainComponent::triggerCellAt(int row,int column) {
    const int armed=nextCue;
    if (!select(row,column)) return;
    nextCue=armed; triggerCell(); grid.repaint();
}
void MainComponent::stopCellAt(int row,int column) {
    if (row<0 || row>=(int)show.rows.size() || column<0 || column>=(int)show.columns.size()) return;
    send(show.rows[row].cells[column].id,false,true); repaint();
}
void MainComponent::stopRow(int row) {
    if (row<0 || row>=(int)show.rows.size()) return;
    if (send(show.rows[row].id,true,true)) status="All clips in cue "+juce::String(row+1)+" stopped.";
    repaint();
}
void MainComponent::setMasterAudio(float level) {
    if (!std::isfinite(level)) return;
    const float value=juce::jlimit(0.0f,4.0f,level);
    if (std::abs(show.masterGain-value)<1.0e-6f) return;
    show.masterGain=value;
    dirty=true;
    engine.publish(makeRenderState(show,clips));
    status="MASTER audio gain updated."; repaint();
}
void MainComponent::setMasterPan(float pan) {
    if (!std::isfinite(pan)) return;
    const float value=juce::jlimit(-1.0f,1.0f,pan);
    if (std::abs(show.masterPan-value)<1.0e-6f) return;
    show.masterPan=value; dirty=true;
    engine.publish(makeRenderState(show,clips));
    status="MASTER pan updated."; utilities.repaint(); repaint();
}
void MainComponent::setMasterMute(bool muted) {
    if (show.masterMuted==muted) return;
    show.masterMuted=muted; dirty=true;
    engine.publish(makeRenderState(show,clips));
    status=muted ? "PCM MASTER muted." : "PCM MASTER unmuted.";
    utilities.repaint(); repaint();
}
void MainComponent::setColumnUtilities(int column,float level,float pan,bool muted,bool inverted) {
    if (column<0 || column>=(int)show.columns.size() || !std::isfinite(level) || !std::isfinite(pan)) return;
    auto& col=show.columns[column];
    col.gain=juce::jlimit(0.0f,4.0f,level); col.pan=juce::jlimit(-1.0f,1.0f,pan);
    col.muted=muted; col.phaseInverted=inverted;
    dirty=true; engine.publish(makeRenderState(show,clips));
    status="Column utilities updated."; repaint();
}
void MainComponent::setColumnOutput(int column,int outputIndex) {
    if (column<0 || column>=(int)show.columns.size() || show.columns[column].type!=ColumnType::audio
        || outputIndex<0 || outputIndex>1022) return;
    show.columns[column].output=outputIndex; dirty=true;
    engine.publish(makeRenderState(show,clips));
    status="Column audio output updated."; repaint();
}
void MainComponent::setRowColour(int row,int colour) {
    if (row<0 || row>=(int)show.rows.size() || colour<0 || colour>7) return;
    if (show.rows[(size_t)row].colour==colour) return;
    show.rows[(size_t)row].colour=colour;
    dirty=true;
    status="Cue colour updated.";
    refresh();
}
void MainComponent::toggleColumnCollapsed(int column) {
    if (column<0 || column>=(int)show.columns.size()) return;
    if (!applyInspector()) return;
    auto& target=show.columns[(size_t)column];
    target.collapsed=!target.collapsed; // visual state only, playback is unchanged
    if (target.collapsed && column==selectedColumn) {
        for (int i=0;i<(int)show.columns.size();++i)
            if (!show.columns[(size_t)i].collapsed) { selectedColumn=i; break; }
    }
    dirty=true;
    status=target.collapsed ? "Column collapsed (read-only preview)." :
                             "Column expanded (editing enabled).";
    refreshControlSettings();
    refresh();
}
void MainComponent::setAutoStop(int row,bool enabled) {
    if (row<0 || row>=(int)show.rows.size()) return;
    show.rows[row].stopOnNext=enabled; dirty=true; engine.publish(makeRenderState(show,clips));
    status=enabled ? "The row will stop on another cue's GO." : "The row will continue after another cue's GO.";
    grid.repaint(); cueMetrics.repaint(); repaint();
}
void MainComponent::setCellAutoStop(int row,int column,bool enabled) {
    if (row<0 || row>=(int)show.rows.size() || column<0 || column>=(int)show.columns.size()) return;
    auto& cell=show.rows[(size_t)row].cells[(size_t)column];
    if (cell.stopOnNext==enabled) return;
    cell.stopOnNext=enabled;
    dirty=true;
    engine.publish(makeRenderState(show,clips));
    if (row==selectedRow && column==selectedColumn) cellAutoStop.setToggleState(enabled,juce::dontSendNotification);
    status=enabled ? "Cell included in cue Auto Stop." : "Cell excluded from Auto Stop: continues after the next GO.";
    grid.repaint(); repaint();
}
void MainComponent::stopAll() {
    engine.panic(); controlOutput.clear(); resetActiveMapping();
    status="PANIC / audio, MIDI and OSC playback stopped."; repaint();
}
bool MainComponent::keyPressed(const juce::KeyPress& key) {
    if (key.getModifiers().isCommandDown() && !dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent())) {
        if (key.getKeyCode()=='C' || key.getKeyCode()=='c') return copySelectedCell();
        if (key.getKeyCode()=='V' || key.getKeyCode()=='v') return pasteSelectedCell();
    }
    if (key.getModifiers().isCommandDown() && (key.getKeyCode()=='Z' || key.getKeyCode()=='z'))
        return key.getModifiers().isShiftDown() ? redoEdit() : undoEdit();
    if (key.getModifiers().isCommandDown() && (key.getKeyCode()=='Y' || key.getKeyCode()=='y')) return redoEdit();
    if (key==juce::KeyPress::escapeKey) { stopAll(); return true; }
    if (key.getModifiers().isCommandDown() && key.getKeyCode()=='S') { saveProject(); return true; }
    if (key==juce::KeyPress::spaceKey && !dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent())) { goNext(); return true; }
    if (key==juce::KeyPress::downKey) { select(std::min(selectedRow+1,(int)show.rows.size()-1),selectedColumn); return true; }
    if (key==juce::KeyPress::upKey) { select(std::max(0,selectedRow-1),selectedColumn); return true; } return false;
}
void MainComponent::timerCallback() {
    engine.collectRetired();
    // Checkpoint only the validated in-memory model; never copy media or
    // touch a running audio callback. The update is atomic and recoverable.
    if (recoveryEnabled && ++recoveryTicks>=25*20) {
        recoveryTicks=0;
        if (dirty) {
            const auto result=ProjectRecovery::checkpoint(show,ProjectRecovery::defaultDirectory());
            if (result.failed()) {
                juce::Logger::writeToLog("Recovery checkpoint: "+result.getErrorMessage());
                status="Recovery checkpoint unavailable. Save your project.";
            }
        }
    }
    // Consume transport confirmation before hardware events in this UI tick.
    // Empty cue GO is acknowledged too; merely selecting a row is not.
    const int confirmed=engine.acknowledgedCueId.load(std::memory_order_acquire);
    if (confirmed!=activeCueId) {
        activeCueId=0; int preset=0;
        if (confirmed>0) for (const auto& row:show.rows) if (row.id==confirmed) {
            activeCueId=confirmed; preset=row.mappingPresetId; break;
        }
        controlMapping.setActiveCue(activeCueId,preset);
    }
    ControlEvent control; int processed=0;
    while (processed++<256 && midiInput.poll(control)) {
        if (controlLearnArmed) { learnedInput=control; controlLearnArmed=false; }
        else dispatchControl(control);
    }
    if (++midiRefreshTicks>=25) {
        midiRefreshTicks=0; engine.refreshMidiOutputs();
        if (midiInput.connected() && !midiInput.stillAvailable()) {
            midiInput.disconnect();
            status="MIDI input disconnected. Mappings suspended.";
        }
    }
    utilities.syncScroll(viewport.getViewPositionX()); utilities.refreshRouting(); grid.repaint();
    displayPeak=std::max(engine.peak.load(),displayPeak*.88f);
    utilities.refreshMeters(); repaint(); timeline.repaint(); cueMetrics.repaint();
}
void MainComponent::withDiscardConfirmation(std::function<void()> action) {
    if (!hasUnsavedChanges()) { action(); return; }
    juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon)
        .withTitle("Unsaved changes").withMessage("Continuing will discard unsaved changes. To save them, cancel and click Save.")
        .withButton("Discard and continue").withButton("Cancel").withAssociatedComponent(this),
        [safe=juce::Component::SafePointer<MainComponent>(this),action](int result) { if (safe && result==1) action(); });
}
void MainComponent::requestQuit() { withDiscardConfirmation([this] { stopAll(); juce::JUCEApplication::getInstance()->quit(); }); }
void MainComponent::saveProject() {
    if (!applyInspector()) return;
    chooser=std::make_unique<juce::FileChooser>("Save show",projectFile==juce::File{} ? juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("Show.cuelist") : projectFile,"*.cuelist");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,
        [safe=juce::Component::SafePointer<MainComponent>(this)](const juce::FileChooser& fc) {
            if (!safe || fc.getResult()==juce::File{}) return;
            auto file=fc.getResult(); auto result=saveShow(safe->show,file);
            if (result.failed()) { safe->message(result.getErrorMessage()); return; }
            Show portable; if (loadShow(file,portable).wasOk()) safe->show=std::move(portable);
            safe->projectFile=file; safe->dirty=false; safe->status="Saved: "+file.getFullPathName();
            if (safe->recoveryEnabled) {
                ProjectRecovery::discard(ProjectRecovery::defaultDirectory());
                safe->preserveDeferredRecovery=false;
            }
            safe->repaint();
        });
}
void MainComponent::showPreflight() {
    juce::StringArray outputs;
    for(const auto& d:juce::MidiOutput::getAvailableDevices()) outputs.add(d.identifier);
    const auto report=preflightShow(show,engine.connected.load(),outputs,midiInput.connected());
    const auto icon=report.criticalCount()>0 ? juce::MessageBoxIconType::WarningIcon :
        report.warningCount()>0 ? juce::MessageBoxIconType::InfoIcon :
        juce::MessageBoxIconType::NoIcon;
    juce::AlertWindow::showMessageBoxAsync(icon,"Show preflight",report.describe(),"Close",this);
    status=report.criticalCount()>0 ? "Preflight: critical problems detected." :
           report.warningCount()>0 ? "Preflight: warnings to review." :
           "Preflight complete: no critical project errors.";
    repaint();
}
void MainComponent::showDiagnostics() {
    const auto stats=engine.diagnostics();
    juce::String details="ENGINE DIAGNOSTICS\n";
    details+="Audio device: "+juce::String(engine.connected.load()?"ONLINE":"OFFLINE");
    details+="\nActive audio/MIDI/OSC cells: "+juce::String(engine.activeVoices.load());
    details+="\nAudio callback commands dropped: "+juce::String(stats.audioCommandsDropped);
    details+="\nMIDI input: "+juce::String(midiInput.connected()?"CONNECTED":"DISCONNECTED");
    details+="\nMIDI input messages dropped: "+juce::String(midiInput.droppedCount());
    details+="\nMIDI output messages sent: "+juce::String(stats.midiOutputSent);
    details+="\nMIDI output messages dropped: "+juce::String(stats.midiOutputDropped);
    details+="\nOSC UDP packets sent: "+juce::String(stats.oscSent);
    details+="\nOSC UDP packets dropped / send failures: "+juce::String(stats.oscDropped);
    details+="\n\nThese counters are local to the current process. UDP sent does not prove receiver delivery.";
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
        "Diagnostics",details,"Close",this);
}
bool MainComponent::activateProject(Show candidate,const juce::File& source,bool recovering) {
    // PREPARE before committing: an unreadable or oversized new show must
    // never panic, unload or corrupt the current live show.
    ClipMap loaded;
    juce::StringArray issues;
    for(const auto& row:candidate.rows) for(const auto& c:row.cells) if(c.file.isNotEmpty()) {
        std::shared_ptr<const AudioClip> clip;
        const auto read=loadClip(juce::File(c.file),clip);
        if(read.failed()) {issues.add(read.getErrorMessage());continue;}
        loaded[c.id]=clip;
        if(!memoryFits(loaded)) {
            message("Show exceeds 512 MB PCM. The current show has not been replaced.");
            return false;
        }
    }
    // Only after all memory checks pass do we stop the previous show.
    engine.panic();
    for(const auto& column:candidate.columns)
        if(column.type==ColumnType::midi && column.midiOutputId.isNotEmpty()) {
            const auto opened=engine.setMidiOutput(column.midiOutputId);
            if(opened.failed()) issues.add(opened.getErrorMessage());
        }
    closeMidiClip();closeOscClip();editHistory.clearUndoHistory();
    show=std::move(candidate);clips=std::move(loaded);
    projectFile=recovering ? juce::File{} : source;dirty=recovering;
    selectedRow=selectedColumn=nextCue=0;
    resetActiveMapping();refreshControlSettings();
    engine.publish(makeRenderState(show,clips));refresh();
    status=recovering ? "Recovered unsaved show. Use Save to choose a project filename." :
                        "Project opened: "+source.getFileName();
    if(!recovering && recoveryEnabled) {
        ProjectRecovery::discard(ProjectRecovery::defaultDirectory());
        preserveDeferredRecovery=false;
    }
    if(!issues.isEmpty())
        message(juce::String(issues.size())+" missing media or MIDI output issues.\n\n"+
                issues.joinIntoString("\n").substring(0,1500));
    return true;
}
void MainComponent::offerCrashRecovery() {
    if(!ProjectRecovery::exists(ProjectRecovery::defaultDirectory())) return;
    auto safe=juce::Component::SafePointer<MainComponent>(this);
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon)
            .withTitle("Recover an unsaved show?")
            .withMessage("A recovery checkpoint from an interrupted session was found. "
                         "Restoring does not overwrite the original project or any media. "
                         "Playback will not start automatically.")
            .withButton("Restore").withButton("Discard checkpoint").withButton("Later")
            .withAssociatedComponent(this),[safe](int answer) {
        if(!safe) return;
        if(answer==1) {
            Show candidate;
            const auto result=ProjectRecovery::restore(ProjectRecovery::defaultDirectory(),candidate);
            if(result.failed()) safe->message(result.getErrorMessage());
            else safe->activateProject(std::move(candidate),{},true);
        } else if(answer==2) ProjectRecovery::discard(ProjectRecovery::defaultDirectory());
        else safe->preserveDeferredRecovery=true;
    });
}
void MainComponent::openProject() {
    chooser=std::make_unique<juce::FileChooser>("Open show",projectFile,"*.cuelist;*.json;*.bak");
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
        [safe=juce::Component::SafePointer<MainComponent>(this)](const juce::FileChooser& fc) {
            if(!safe || fc.getResult()==juce::File{}) return;
            const auto file=fc.getResult();
            Show candidate;
            const auto result=loadShow(file,candidate);
            if(result.failed()) {safe->message(result.getErrorMessage());return;}
            safe->activateProject(std::move(candidate),file,false);
        });
}
void MainComponent::showDevices() {
    if (deviceDialog) { deviceDialog->toFront(true); return; }
    stopAll(); auto* selector=new juce::AudioDeviceSelectorComponent(engine.devices,0,0,0,64,false,false,true,false);
    selector->setSize(600,450);
    juce::DialogWindow::LaunchOptions options; options.content.setOwned(selector); options.dialogTitle="Audio devices / playback stopped";
    options.dialogBackgroundColour=palette::panel; options.escapeKeyTriggersCloseButton=true; options.useNativeTitleBar=true;
    options.componentToCentreAround=this; options.resizable=false; deviceDialog=options.launchAsync(); deviceDialog->setLookAndFeel(&theme);
}
void MainComponent::editTempo() {
    if (!applyInspector()) return;
    stopAll();
    const int rowId=show.rows[selectedRow].id; juce::String text;
    for (const auto& p : show.rows[selectedRow].tempo) text+=juce::String(p.beat)+" "+juce::String(p.bpm)+"\n";
    prompt(this,"Cue tempo map","One line per change: beat BPM. Beats count quarter notes from zero. Does not change WAV playback speed.",text,true,
        [safe=juce::Component::SafePointer<MainComponent>(this),rowId](juce::String input) {
            if (!safe) return;
            std::vector<TempoPoint> points;
            for (const auto& line : juce::StringArray::fromLines(input)) {
                if (line.trim().isEmpty()) continue;
                std::istringstream stream(line.toStdString()); TempoPoint p; std::string extra;
                if (!(stream>>p.beat>>p.bpm) || stream>>extra) { safe->message("Required format: 0 120. One line per tempo change."); return; }
                points.push_back(p);
            }
            auto candidate=safe->show; for (auto& row : candidate.rows) if (row.id==rowId) row.tempo=points;
            auto valid=validateShow(candidate); if (valid.failed()) { safe->message(valid.getErrorMessage()); return; }
            safe->show=std::move(candidate); safe->changed("Tempo map saved for this cue. Playback stopped.");
        });
}
void MainComponent::renameTitle(bool column,int index) {
    if (!applyInspector()) return;
    const int id=column ? show.columns[index].id : show.rows[index].id;
    prompt(this,column ? "Rename column" : "Rename cue","Press Enter to confirm; the name may be empty.",
        column ? show.columns[index].name : show.rows[index].name,false,
        [safe=juce::Component::SafePointer<MainComponent>(this),column,id](juce::String name) {
            if (!safe) return;
            if (column) { for (auto& c : safe->show.columns) if (c.id==id) c.name=name.trim(); }
            else { for (auto& r : safe->show.rows) if (r.id==id) r.name=name.trim(); }
            safe->changed("Title updated.");
        });
}
void MainComponent::reorderRow(int index,int delta) {
    int target=index+delta;
    if (index<0 || index>=(int)show.rows.size() || target<0 || target>=(int)show.rows.size() || !applyInspector()) return;
    std::swap(show.rows[index],show.rows[target]); selectedRow=nextCue=target; changed("Cue order updated.",true);
}
void MainComponent::reorderColumn(int index,int delta) {
    int target=index+delta;
    if (index<0 || index>=(int)show.columns.size() || target<0 || target>=(int)show.columns.size() || !applyInspector()) return;
    std::swap(show.columns[index],show.columns[target]);
    for (auto& row : show.rows) std::swap(row.cells[index],row.cells[target]);
    selectedColumn=target;
    // Equal-priority Control Column conflicts are resolved by visible order.
    // Rebuild the layered mapping immediately when the columns are reordered.
    refreshControlSettings();
    changed("Column order updated.",true);
}
bool MainComponent::copyRow(int index) {
    if (index<0 || index>=(int)show.rows.size() || !applyInspector()) return false;
    const auto& row=show.rows[(size_t)index];
    copiedRow=row;
    copiedRowAudio.clear();
    copiedRowAudio.reserve(row.cells.size());
    for (const auto& cell:row.cells) {
        const auto it=clips.find(cell.id);
        copiedRowAudio.push_back(it==clips.end() ? nullptr : it->second);
    }
    status="Cue copied. Right-click a row to paste below.";
    repaint();
    return true;
}
bool MainComponent::insertCopiedRowAfter(int index,const Row& source,
        const std::vector<std::shared_ptr<const AudioClip>>& sourceAudio) {
    if (index<0 || index>=(int)show.rows.size() || !applyInspector()
        || source.cells.size()!=show.columns.size() || sourceAudio.size()!=source.cells.size())
        return false;
    if ((int64_t)show.nextId+1+(int64_t)source.cells.size()>=1000000000) {
        message("Show identifier limit reached."); return false;
    }
    auto candidate=show;
    auto media=clips;
    Row inserted=source;
    inserted.id=candidate.nextId++;
    for (size_t c=0;c<inserted.cells.size();++c) {
        inserted.cells[c].id=candidate.nextId++;
        if (sourceAudio[c]) media[inserted.cells[c].id]=sourceAudio[c];
    }
    for (auto& cell:inserted.cells) if (cell.controlSet)
        for (auto& binding:cell.controlSet->bindings) binding.id=candidate.controls.nextId++;
    candidate.rows.insert(candidate.rows.begin()+index+1,std::move(inserted));
    const auto valid=validateShow(candidate);
    if (valid.failed()) { message(valid.getErrorMessage()); return false; }
    if (!memoryFits(media)) { message("Show PCM memory limit reached."); return false; }
    closeMidiClip(); closeOscClip();
    show=std::move(candidate);
    clips=std::move(media);
    refreshControlSettings();
    selectedRow=nextCue=index+1;
    changed("Cue inserted with new row and cell identities.",true);
    return true;
}
bool MainComponent::pasteRowAfter(int index) {
    if (!copiedRow) return false;
    return insertCopiedRowAfter(index,*copiedRow,copiedRowAudio);
}
bool MainComponent::duplicateRow(int index) {
    if (index<0 || index>=(int)show.rows.size() || !applyInspector()) return false;
    Row snapshot=show.rows[(size_t)index];
    snapshot.name=snapshot.name.isEmpty() ? "Cue (copy)" : snapshot.name+" (copy)";
    std::vector<std::shared_ptr<const AudioClip>> media;
    media.reserve(snapshot.cells.size());
    for (const auto& cell:snapshot.cells) {
        const auto it=clips.find(cell.id);
        media.push_back(it==clips.end() ? nullptr : it->second);
    }
    return insertCopiedRowAfter(index,snapshot,media);
}
void MainComponent::deleteRow(int index) {
    if (index<0 || index>=(int)show.rows.size() || !applyInspector()) return;
    const int removedId=show.rows[(size_t)index].id;
    for (auto& c : show.rows[index].cells) clips.erase(c.id);
    show.rows.erase(show.rows.begin()+index); if (show.rows.empty()) addRow(show,{});
    auto& bindings=show.controls.bindings;
    bindings.erase(std::remove_if(bindings.begin(),bindings.end(),
        [removedId](const ControlBinding& b) {
            return b.scope==ControlScope::cue && b.scopeId==removedId;
        }),bindings.end());
    refreshControlSettings();
    selectedRow=nextCue=std::min(index,(int)show.rows.size()-1); changed("Cue deleted. Original files retained.",true);
}
void MainComponent::deleteColumn(int index) {
    if (index<0 || index>=(int)show.columns.size() || !applyInspector()) return;
    const int removedId=show.columns[(size_t)index].id;
    for (auto& row : show.rows) { clips.erase(row.cells[index].id); row.cells.erase(row.cells.begin()+index); }
    show.columns.erase(show.columns.begin()+index); if (show.columns.empty()) addColumn(show,ColumnType::audio,{});
    auto& bindings=show.controls.bindings;
    bindings.erase(std::remove_if(bindings.begin(),bindings.end(),
        [removedId](const ControlBinding& b) {
            return (b.scope==ControlScope::column || b.target==ControlTarget::columnGain ||
                b.target==ControlTarget::midiCc || b.target==ControlTarget::oscFloat)
                    && b.targetColumnId==removedId;
        }),bindings.end());
    // A deleted target cannot remain in any other Control Set.
    const auto purge=[removedId](ControlSet& set) {
        auto& bindings=set.bindings;
        bindings.erase(std::remove_if(bindings.begin(),bindings.end(),
            [removedId](const ControlBinding& b){
                return b.targetColumnId==removedId &&
                    (b.target==ControlTarget::columnGain ||
                     b.target==ControlTarget::midiCc || b.target==ControlTarget::oscFloat);
            }),bindings.end());
    };
    for (auto& col:show.columns) purge(col.controlDefaultSet);
    for (auto& row:show.rows) for (auto& cell:row.cells)
        if (cell.controlSet) purge(*cell.controlSet);
    for (auto& saved:show.controls.templates) purge(saved.set);
    refreshControlSettings();
    selectedColumn=std::min(index,(int)show.columns.size()-1); changed("Column deleted. Original files retained.",true);
}
void MainComponent::moveClip(int sr,int sc,int tr,int tc) {
    if (sc>=0 && tc>=0 && sc<(int)show.columns.size() && tc<(int)show.columns.size() &&
        (show.columns[(size_t)sc].collapsed || show.columns[(size_t)tc].collapsed))
        return;
    if (sc>=0 && sc<(int)show.columns.size() && tr>=0 && tr<(int)show.rows.size() &&
        tc>=0 && tc<(int)show.columns.size() &&
        ((show.columns[(size_t)sc].type==ColumnType::control &&
          show.columns[(size_t)sc].controlLocked) ||
         (show.columns[(size_t)tc].type==ColumnType::control &&
          show.columns[(size_t)tc].controlLocked))) {
        status="Unlock the Control Column before moving its cue-specific sets.";
        repaint(); return;
    }
    if (!applyInspector()) return;
    auto result=moveCell(show,sr,sc,tr,tc); if (result.failed()) { status=result.getErrorMessage(); repaint(); return; }
    selectedRow=tr; selectedColumn=tc; nextCue=tr;
    refreshControlSettings(); changed("Cells swapped.",true);
}
bool MainComponent::copySelectedCell() {
    if (selectedRow<0 || selectedRow>=(int)show.rows.size() ||
        selectedColumn<0 || selectedColumn>=(int)show.columns.size()) return false;
    if (show.columns[(size_t)selectedColumn].type==ColumnType::control &&
        show.columns[(size_t)selectedColumn].controlLocked) {
        status="Locked Control Column cells cannot be copied individually.";
        repaint();return false;
    }
    const auto& source=show.rows[(size_t)selectedRow].cells[(size_t)selectedColumn];
    copiedCell=source;
    copiedType=show.columns[(size_t)selectedColumn].type;
    const auto found=clips.find(source.id);
    copiedAudio=found==clips.end() ? nullptr : found->second;
    status="Cell copied: press Ctrl+V on another cell of the same type."; repaint();
    return true;
}
bool MainComponent::pasteSelectedCell() {
    if (selectedColumn>=0 && selectedColumn<(int)show.columns.size() &&
        show.columns[(size_t)selectedColumn].collapsed) return false;
    if (!copiedCell || selectedRow<0 || selectedRow>=(int)show.rows.size() ||
        selectedColumn<0 || selectedColumn>=(int)show.columns.size()) return false;
    if (show.columns[(size_t)selectedColumn].type==ColumnType::control &&
        show.columns[(size_t)selectedColumn].controlLocked) {
        status="Unlock this Control Column before pasting cue-specific sets.";
        repaint();return false;
    }
    if (show.columns[(size_t)selectedColumn].type!=copiedType) {
        message("Copy and paste require columns of the same type."); return false;
    }
    if (!applyInspector()) return false;
    auto updatedClips=clips;
    const int id=show.rows[(size_t)selectedRow].cells[(size_t)selectedColumn].id;
    if (copiedAudio) updatedClips[id]=copiedAudio;
    else updatedClips.erase(id);
    if (!memoryFits(updatedClips)) {
        message("Cannot paste: media memory limit exceeded."); return false;
    }
    auto candidate=show;
    auto& target=candidate.rows[(size_t)selectedRow].cells[(size_t)selectedColumn];
    target=*copiedCell;
    target.id=id; // Stable destination cell identity.
    if (copiedType==ColumnType::control && target.controlSet)
        for (auto& binding:target.controlSet->bindings)
            binding.id=candidate.controls.nextId++; // never alias another set
    if (auto result=validateShow(candidate);result.failed()) {
        message(result.getErrorMessage()); return false;
    }
    closeMidiClip(); closeOscClip();
    show=std::move(candidate);
    clips=std::move(updatedClips);
    if (copiedType==ColumnType::control) refreshControlSettings();
    changed("Cell content and settings pasted.");
    return true;
}
void MainComponent::editRowBpm(int index) {
    if (index<0 || index>=(int)show.rows.size() || !applyInspector()) return;
    const int id=show.rows[(size_t)index].id;
    prompt(this,"Cue BPM","New initial BPM (20–400). Any later tempo changes remain unchanged.",
        juce::String(show.rows[(size_t)index].tempo.front().bpm,1),false,
        [safe=juce::Component::SafePointer<MainComponent>(this),id](juce::String text) {
            if (!safe) return;
            const auto value=text.trim();
            if (value.isEmpty() || value.retainCharacters("0123456789.")!=value) {
                safe->message("Invalid BPM."); return;
            }
            auto candidate=safe->show;
            bool changed=false;
            for (auto& row:candidate.rows) if (row.id==id) {
                row.tempo.front().bpm=value.getDoubleValue(); changed=true; break;
            }
            if (!changed) return;
            if (auto result=validateShow(candidate); result.failed()) {
                safe->message(result.getErrorMessage()); return;
            }
            safe->show=std::move(candidate);
            safe->changed("Cue initial BPM updated.",true);
        });
}
void MainComponent::setClipDuration(int column,double seconds) {
    if (column<0 || column>=(int)show.columns.size() ||
        selectedRow<0 || selectedRow>=(int)show.rows.size() || !std::isfinite(seconds)) return;
    auto& row=show.rows[(size_t)selectedRow];
    auto& cell=row.cells[(size_t)column];
    if (!cell.midi && !cell.osc) return;
    const auto& tempo=row.tempo;
    const auto beatAt=[&](double duration) {
        double low=0,high=std::max(1.0,duration*4);
        while (secondsAtBeat(tempo,high)<duration && high<1000000.0) high*=2;
        for (int i=0;i<48;++i) {
            double mid=(low+high)*.5;
            if (secondsAtBeat(tempo,mid)<duration) low=mid; else high=mid;
        }
        return (low+high)*.5;
    };
    double length=cell.midi || cell.osc->useBeats ? beatAt(std::max(.01,seconds)) : std::max(.01,seconds);
    if (cell.midi) {
        for (const auto& note:cell.midi->notes) length=std::max(length,note.beat+note.length+.0001);
        length=juce::jlimit(.25,4096.0,length);
        if (std::abs(cell.midi->lengthBeats-length)<.0001) return;
        cell.midi->lengthBeats=length;
    } else {
        for (const auto& event:cell.osc->events) length=std::max(length,event.time+.001);
        for (const auto& automation:cell.osc->automations)
            length=std::max(length,automation.time+automation.duration);
        length=juce::jlimit(.01,86400.0,length);
        if (std::abs(cell.osc->length-length)<.0001) return;
        cell.osc->length=length;
    }
    dirty=true;
    engine.publish(makeRenderState(show,clips));
    if (cell.midi && clipEditor && clipEditorCellId==cell.id) clipEditor->loadClip(*cell.midi);
    if (cell.osc && oscEditor && oscEditorCellId==cell.id) oscEditor->loadClip(*cell.osc);
    status="Clip length updated in timeline.";
    timeline.repaint(); grid.repaint(); repaint();
}
void MainComponent::setClipStart(int column,double seconds) {
    if (column<0 || column>=(int)show.columns.size() || !std::isfinite(seconds)) return;
    auto& cell=show.rows[selectedRow].cells[column]; seconds=juce::jlimit(0.0,86400.0,std::round(seconds*100)/100);
    if (cell.startSeconds==seconds) return;
    cell.startSeconds=seconds; dirty=true; engine.publish(makeRenderState(show,clips));
    status="Clip start: "+juce::String(seconds,2)+" s"; timeline.repaint(); repaint();
}
void MainComponent::seekTimeline(double seconds) {
    if (!std::isfinite(seconds)) return;
    if (!engine.seekRow(show.rows[selectedRow].id,juce::jlimit(0.0,86400.0,seconds))) { status="Command queue full."; }
    timeline.repaint();
}
}
