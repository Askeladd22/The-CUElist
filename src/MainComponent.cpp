#include "MainComponent.h"
#include "ProjectSafety.h"
#include "ProjectMediaLoader.h"
#include "DeviceIdentity.h"
#include "PianoRoll.h"
#include "OscEditor.h"
#include "MidiClip.h"
#include "BinaryData.h"
#include <sstream>
#include <cmath>

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
         &cellTab,&rowTab,&rowGain,&rowBehaviour,&rowMute,&rowLoop,&rowName,&cellName,&bpm,&behaviour,&follow,&loop,&cellAutoStop,&gainDb,&viewport,&timeline,&utilities,
         &clipCollapseButton,&clipCloseButton,&cueMetrics,&preflightButton,&diagnosticsButton,&preferencesButton,&projectNameLabel}) addAndMakeVisible(c);
    for (auto* b : {&openButton,&saveButton,&newButton,&addRowButton,&addColumnButton,&goButton,&panicButton,&deviceButton,&controlButton,
                    &importButton,&midiEditButton,&oscEditButton,&playCellButton,&stopCellButton,&clearButton,&tempoButton,&applyButton,
                    &preflightButton,&diagnosticsButton,&preferencesButton}) b->setWantsKeyboardFocus(false);
    viewport.setViewedComponent(&grid,false); viewport.setScrollBarsShown(true,true);
    goButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff097b70));
    goButton.setColour(juce::TextButton::textColourOffId,palette::text);
    panicButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff923b49));
    panicButton.setColour(juce::TextButton::textColourOffId,palette::text);
    importButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff097b70));
    behaviour.addItemList({"Retrigger","Pause / Restart","Pause / Resume","Continue"},1);
    rowGain.setSliderStyle(juce::Slider::LinearHorizontal);
    rowGain.setTextBoxStyle(juce::Slider::TextBoxRight,false,72,24);
    rowGain.setRange(-60,12,0.1); rowGain.setNumDecimalPlacesToDisplay(1);
    rowGain.setTextValueSuffix(" dB"); rowGain.setDoubleClickReturnValue(true,0);
    rowGain.setName("rowGainDb");
    // The Inspector edits the same linear Cell gain as before, but no longer
    // exposes an additional fader beside the dedicated audio strip faders.
    gainDb.setName("cellGainDb");
    gainDb.setInputRestrictions(8,"0123456789.-");
    gainDb.setSelectAllWhenFocused(true);
    gainDb.setJustification(juce::Justification::centredRight);
    gainDb.setFont(font(13.5f));
    gainDb.setTooltip("Cell Gain (dB). Enter a value from -60.0 to +12.0; press Enter to apply.");
    gainDb.setEscapeAndReturnKeysConsumed(false);
    gainDb.onReturnKey=[this] { if (applyInspector()) grabKeyboardFocus(); };
    gainDb.onFocusLost=[this] { if (gainDb.isVisible() && inspectorDiffers()) applyInspector(); };
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
    masterOutput.setName("masterMainOutput");
    masterOutput.setTooltip("Stereo physical output for the Master Bus only. Direct outputs are unaffected.");
    masterOutput.onChange=[this] {
        if(masterOutput.getSelectedId()>0) setMasterOutput(masterOutput.getSelectedId()-1);
    };
    utilities.addAndMakeVisible(masterOutput);
    safetyLimiter.setName("masterSafetyLimiter");
    safetyLimiter.setTooltip("Fixed transparent -1 dBFS peak limiter (instant attack, 150 ms release). Master Bus only.");
    safetyLimiter.setToggleState(show.safetyLimiterEnabled,juce::dontSendNotification);
    safetyLimiter.setWantsKeyboardFocus(false);
    safetyLimiter.onClick=[this] {setSafetyLimiter(safetyLimiter.getToggleState());};
    utilities.addAndMakeVisible(safetyLimiter);
    rowBehaviour.addItemList({"Use cell settings","Retrigger","Pause / Restart","Pause / Resume","Continue"},1);
    cellTab.setWantsKeyboardFocus(false); rowTab.setWantsKeyboardFocus(false);
    // The Inspector displays Entire Q and Cell simultaneously; legacy tabs
    // are retained as non-visible compatibility members only.
    cellTab.setVisible(false);
    rowTab.setVisible(false);
    rowName.setSelectAllWhenFocused(true); cellName.setSelectAllWhenFocused(true); bpm.setInputRestrictions(6,"0123456789.");
    rowName.setName("cueName"); cellName.setName("cellName"); bpm.setName("cueBpm");
    for (auto* editor : {&rowName,&cellName,&bpm}) editor->setEscapeAndReturnKeysConsumed(false);
    for (auto* editor : {&rowName,&cellName,&bpm})
        editor->setFont(font(14.0f));
    bpm.setSelectAllWhenFocused(true);
    for (auto* editor : {&rowName,&cellName,&bpm}) editor->onReturnKey=[this] { if (applyInspector()) grabKeyboardFocus(); };
    rowGain.onValueChange=[this] { applyInspector(); };
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
    projectNameLabel.setName("projectName");
    projectNameLabel.setEditable(false,true,false);
    projectNameLabel.setColour(juce::Label::textColourId,palette::text);
    projectNameLabel.setFont(font(16.0f,true));
    projectNameLabel.setTooltip("Double-click to rename the project. The file path is unchanged.");
    projectNameLabel.onTextChange=[this] {
        const auto name=projectNameLabel.getText().trim();
        if(name.isEmpty() || name.length()>120) {
            projectNameLabel.setText(show.name,juce::dontSendNotification);
            return;
        }
        if(name!=show.name) {
            show.name=name; dirty=true; status="Project renamed (save to persist)."; repaint();
        }
    };
    preferencesButton.setName("projectPreferences");
    preferencesButton.setTooltip("Pre-flight checks and live diagnostics.");
    preferencesButton.onClick=[this] {
        juce::PopupMenu menu;
        menu.addItem(1,"Pre-flight"); menu.addItem(2,"Diagnostics");
        auto safe=juce::Component::SafePointer<MainComponent>(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&preferencesButton),[safe](int choice) {
            if (!safe) return;
            if(choice==1) safe->showPreflight();
            else if(choice==2) safe->showDiagnostics();
        });
    };
    newButton.onClick=[this] { withDiscardConfirmation([this] { if(projectLoader) projectLoader->cancel(); closeMidiClip(); closeOscClip(); editHistory.clearUndoHistory(); stopAll(); show=makeShow(); clips.clear(); projectFile={}; selectedRow=selectedColumn=nextCue=0;
        resetActiveMapping(); refreshControlSettings();
        dirty=false; if (recoveryEnabled) {
            if (recoveryWriter) recoveryWriter->flush();
            ProjectRecovery::discard(ProjectRecovery::defaultDirectory());
            preserveDeferredRecovery=false;
        }
        engine.publish(makeRenderState(show,clips)); refresh(); status="New show. Import files into Audio cells."; }); };
    openButton.onClick=[this] { withDiscardConfirmation([this] { openProject(); }); };
    preflightButton.setName("showPreflight");
    diagnosticsButton.setName("showDiagnostics");
    preflightButton.setTooltip("Check show media, output devices and controller availability before a performance.");
    diagnosticsButton.setTooltip("Live engine status and counters for dropped MIDI/OSC/audio commands.");
    preflightButton.onClick=[this] { showPreflight(); };
    diagnosticsButton.onClick=[this] { showDiagnostics(); };
    saveButton.onClick=[this] { saveProject(); };
    addRowButton.onClick=[this] { addCue(); };
    addColumnButton.onClick=[this] { showColumnMenu(addColumnButton.getScreenBounds().getCentre()); };
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
    addColumnButton.setTooltip("Add a column.");
    addRowButton.setTooltip("Add a cue.");
    if (startAudio) { auto error=engine.initialise(); if (error.isNotEmpty()) status="Audio unavailable: "+error; }
    else status="Offline mode: audio device not opened.";
    refreshControlSettings();
    engine.publish(makeRenderState(show,clips)); refresh(); startTimerHz(25);
    recoveryEnabled=juce::JUCEApplication::getInstance()!=nullptr;
    if (recoveryEnabled) {
        recoveryWriter=std::make_unique<RecoveryCheckpointWriter>(ProjectRecovery::defaultDirectory());
    }
    if (recoveryEnabled && ProjectRecovery::exists(ProjectRecovery::defaultDirectory())) {
        auto safe=juce::Component::SafePointer<MainComponent>(this);
        juce::MessageManager::callAsync([safe] {if (safe) safe->offerCrashRecovery();});
    }
}
MainComponent::~MainComponent() {
    stopTimer(); engine.panic(); chooser.reset();
    // Cancel in-flight background decoding before destroying UI/engine state.
    projectLoader.reset();
    // Join the writer before touching its destination on clean shutdown.
    recoveryWriter.reset();
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
    g.fillRoundedRectangle(10.0f,284.0f,160.0f,114.0f,8.0f);
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
    g.setColour(palette::teal); g.fillRect(232,69,65,3);
    label(g,"CONTROL / MAPPING v10",{310,25,185,22},12,palette::muted);

    g.setColour(engine.connected.load() ? palette::teal : palette::red);
    g.fillEllipse((float)w-195,23,9,9);
    label(g,engine.connected.load() ? "Audio Available" : "Audio Unavailable",{w-180,14,173,26},12,palette::muted);
    const double now=juce::Time::getMillisecondCounterHiRes();
    const auto activity=[&](int x,const juce::String& title,bool active) {
        g.setColour(active ? palette::teal : palette::line);
        g.fillEllipse((float)x,52.0f,7.0f,7.0f);
        label(g,title,{x+12,44,77,24},10,active ? palette::text : palette::muted,true);
    };
    activity(w-412,"MIDI RX",now<midiRxUntil);
    activity(w-309,"MIDI TX",now<midiTxUntil);
    activity(w-206,"OSC RX",false); // No OSC input endpoint currently exists.
    activity(w-103,"OSC TX",now<oscTxUntil);
    label(g,"WORKSPACE",{20,118,140,17},11,palette::muted,true);
    g.setColour(palette::teal.withAlpha(0.19f)); g.fillRoundedRectangle(12,131,156,45,8);
    g.setColour(palette::teal); g.fillRoundedRectangle(12,137,4,34,2);
    // The editable project title is a JUCE Label over this card.
    label(g,"PROJECT",{20,294,140,18},11,palette::muted,true);
    label(g,juce::String((int)show.rows.size())+" cues / "+juce::String((int)show.columns.size())+" columns",{20,321,150,22},13,palette::muted);
    label(g,hasUnsavedChanges() ? "Unsaved changes" : "No changes",{20,352,153,22},11,hasUnsavedChanges() ? palette::amber : palette::muted);
    g.setColour(palette::amber.withAlpha(.11f));
    g.fillRoundedRectangle(12.0f,(float)h-339,156.0f,51.0f,8.0f);
    label(g,"ON DECK",{20,h-333,145,16},11,palette::amber,true);
    label(g,nextCue<(int)show.rows.size() ? "NEXT  /  "+juce::String(nextCue+1).paddedLeft('0',2) :
          "END OF SHOW",{20,h-314,148,24},17,palette::text,true);
    // Output meter removed from the sidebar; audio peak telemetry remains
    // available in the Master and channel meters.
    label(g,"SPACE   Next cue",{20,h-84,150,20},12,palette::muted);
    label(g,"ESC          Panic",{20,h-61,150,20},12,palette::muted);
    constexpr int inspectorWidth=288, inspectorInner=236;
    auto panel=juce::Rectangle<int>(editorX,154,inspectorWidth-20,h-201);
    g.setColour(palette::panel); g.fillRoundedRectangle(panel.toFloat(),9);
    g.setColour(palette::line); g.drawRoundedRectangle(panel.toFloat(),9,1.25f);
    const int x=editorX+16;
    g.setColour(palette::teal); g.fillRoundedRectangle((float)editorX+13,163.0f,4.0f,49.0f,2.0f);
    label(g,"INSPECTOR",{x+10,169,200,19},13,palette::teal,true);
    const auto& col=show.columns[selectedColumn];
    label(g,"CUE "+juce::String(selectedRow+1).paddedLeft('0',2),{x+10,195,140,28},22,palette::text,true);
    label(g,typeName(col.type),{x+131,200,98,21},11,palette::type((int)col.type),true,juce::Justification::centredRight);
    g.saveState();
    { const float factor=juce::jlimit(0.72f,0.92f,(float)(h-232)/730.0f);
      g.addTransform(juce::AffineTransform(1,0,0,0,factor,226*(1-factor))); }
    // Paint both Inspector cards in the same scaled coordinate system as
    // their labels and editable controls: CELL was drifting above its card.
    g.setColour(palette::raised.withAlpha(.52f));
    g.fillRoundedRectangle((float)x-7,228.0f,(float)inspectorInner+14,245.0f,7.0f);
    g.fillRoundedRectangle((float)x-7,478.0f,(float)inspectorInner+14,337.0f,7.0f);
    label(g,"ENTIRE CUE",{x+3,239,inspectorInner-6,20},14,palette::teal,true);
    label(g,"Cue name",{x,270,inspectorInner,18},12,palette::text,true);
    label(g,"Initial BPM",{x,338,inspectorInner,18},12,palette::text,true);
    g.setColour(palette::line);g.drawHorizontalLine(473,(float)x,(float)x+inspectorInner);
    label(g,"CELL",{x+3,489,59,20},14,palette::teal,true);
    label(g,"/  "+col.name,{x+62,489,inspectorInner-68,20},13,palette::text,true);
    label(g,"Cell name",{x,520,inspectorInner,18},12,palette::text,true);
    if(col.type==ColumnType::audio) {
        label(g,"Gain",{x,598,48,18},12,palette::muted,true);
        label(g,"dB",{x+163,598,43,18},12,palette::muted,true);
    }
    if(col.type==ColumnType::osc)
        label(g,"UDP destination in column Utility",{x,711,inspectorInner,18},10,palette::muted);
    g.restoreState();
    label(g,status,{192,h-35,std::max(1,editorX-200),25},12,palette::muted);
}
void MainComponent::resized() {
    const int w=getWidth(),h=getHeight(); editorX=w-288;
    newButton.setBounds(13,86,49,29); openButton.setBounds(67,86,49,29); saveButton.setBounds(121,86,49,29);
    projectNameLabel.setBounds(28,138,132,30);
    deviceButton.setBounds(20,188,140,33);
    preferencesButton.setBounds(20,227,140,31);
    controlButton.setBounds(std::max(506,w-666),22,144,30);

    preflightButton.setVisible(false); diagnosticsButton.setVisible(false);
    addRowButton.setVisible(false); addColumnButton.setVisible(false);
    const bool docked=(clipEditor && !clipWindow) || (oscEditor && !oscWindow);
    const int activeDockHeight=docked ? (clipCollapsed ? 36 : this->dockHeight) : 0;
    // Prioritise the running cue list on show-sized desktops while preserving
    // mixer controls, the timeline and docked editors at all window heights.
    // Keep at least one complete timeline lane visible alongside the larger
    // cue rows. Mixer and cue list remain scrollable; no panel disappears.
    const int gridHeight=std::max(310,h-400-activeDockHeight);
    const int utilityHeight=h<800 ? 180 : 208;
    // Close the large unused band between the toolbar and cue grid, and
    // reduce the left/right gutters without covering the pinned metrics.
    constexpr int cueLeft=192, cueTop=86, cueRightGap=8;
    const int cueWidth=editorX-cueRightGap-cueLeft;
    viewport.setBounds(cueLeft,cueTop,cueWidth,
                       gridHeight-utilityHeight+154-cueTop);
    utilities.setBounds(cueLeft,154+gridHeight-utilityHeight,cueWidth,utilityHeight);
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
            if (!clipCollapsed) clipEditor->setBounds(cueLeft,dockY,cueWidth,activeDockHeight);
        }
        if (oscEditor && !oscWindow) {
            oscEditor->setVisible(!clipCollapsed);
            if (!clipCollapsed) oscEditor->setBounds(cueLeft,dockY,cueWidth,activeDockHeight);
        }
    }
    timeline.setBounds(cueLeft,dockY+activeDockHeight,cueWidth,std::max(75,h-gridHeight-activeDockHeight-215));
    const int x=editorX+16, inner=236;
    cellTab.setVisible(false);rowTab.setVisible(false);
    rowBehaviour.setVisible(false);rowMute.setVisible(false);rowGain.setVisible(false);
    behaviour.setVisible(false);cellAutoStop.setVisible(false);
    clearButton.setVisible(false);applyButton.setVisible(false);
    playCellButton.setVisible(false);stopCellButton.setVisible(false);
    rowName.setBounds(x,291,inner,33);
    bpm.setBounds(x,359,inner,32);
    tempoButton.setBounds(x,397,inner,29);
    rowLoop.setBounds(x-3,440,inner,25);
    cellName.setBounds(x,541,inner,31);
    gainDb.setBounds(x+57,594,99,27);
    follow.setBounds(x-3,630,inner,25);
    loop.setBounds(x-3,663,inner,25);
    midiEditButton.setBounds(x,714,inner,33);
    oscEditButton.setBounds(x,714,inner,33);
    importButton.setBounds(x,757,inner,33);
    // Compact inspector on smaller displays, keeping all actions reachable.
    {
        const float factor=juce::jlimit(0.72f,0.92f,(float)(h-232)/730.0f);
        for (juce::Component* c : std::initializer_list<juce::Component*>{&rowLoop,&rowName,&bpm,&tempoButton,&cellName,&follow,&loop,&gainDb,&importButton,&midiEditButton,&oscEditButton}) {
            auto b=c->getBounds(); b.setY(226+(int)((b.getY()-226)*factor)); b.setHeight(std::max(22,(int)(b.getHeight()*factor))); c->setBounds(b);
        }
    }
    goButton.setBounds(14,h-282,152,48);
    panicButton.setBounds(14,h-228,152,48);
    // Reserve overlay clearance only when the real columns exceed the
    // usable cue area: a one-column show should not acquire fake scrolling.
    const int cueContent=grid.columnX((int)show.columns.size())+44;
    const int clearWidth=viewport.getWidth()-CueMetricsOverlay::width-14;
    const int scrollWidth=cueContent+(cueContent>clearWidth ? CueMetricsOverlay::width : 0);
    grid.setSize(std::max(viewport.getWidth()-14,scrollWidth),
                 std::max(viewport.getHeight()-14,CueGrid::headerHeight+(int)show.rows.size()*CueGrid::rowHeight+44));
    cueMetrics.setBounds(viewport.getRight()-CueMetricsOverlay::width,viewport.getY(),
                         CueMetricsOverlay::width,viewport.getHeight());
    cueMetrics.toFront(false);
}
void MainComponent::refresh() {
    selectedRow=juce::jlimit(0,(int)show.rows.size()-1,selectedRow);
    selectedColumn=juce::jlimit(0,(int)show.columns.size()-1,selectedColumn);
    syncClipEditor(); syncOscEditor(); refreshInspector(); utilities.refresh(); resized();
    if (!projectNameLabel.isBeingEdited() && projectNameLabel.getText()!=show.name)
        projectNameLabel.setText(show.name,juce::dontSendNotification);
    repaint(); grid.repaint(); timeline.repaint(); cueMetrics.repaint();
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
    if (projectLoader) projectLoader->cancel(); // never overwrite newer edits
    if (stopPlayback) { engine.panic(); resetActiveMapping(); }
    engine.publish(makeRenderState(show,clips),!stopPlayback);
    dirty=true; status=text; refresh();
}
void MainComponent::message(const juce::String& text) { status=text; repaint(); juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"The Cuelist",text,"OK",this); }
bool MainComponent::memoryFits(const ClipMap& collection) const {
    return withinPcmBudget(collection);
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
    std::shared_ptr<const AudioClip> clip;
    const auto result=loadClip(file,clip);
    if (result.failed()) { message(result.getErrorMessage()); return; }
    auto updated=clips; auto& c=show.rows[selectedRow].cells[selectedColumn]; updated[c.id]=clip;
    if (!memoryFits(updated)) { message("Preload limit reached: 512 MB PCM per show. The file was not added."); return; }
    // Invalid imports no longer stop an ongoing performance; a successful
    // import still uses changed(...), preserving its original transport policy.
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
void MainComponent::toggleRowPlayPause(int row) {
    if(row<0 || row>=(int)show.rows.size() || !applyInspector()) return;
    // A pause/resume must not change the armed Next Cue or Inspector selection.
    bool active=false,paused=false;
    for(const auto& cell:show.rows[(size_t)row].cells) {
        if(!cell.followsRow) continue;
        const auto p=engine.getCellPlayback(cell.id);
        active=active||p.playing;
        paused=paused||p.paused;
    }
    if(!active && !paused) {triggerRow(row);return;}
    if(!engine.connected.load()) {message("No audio device available.");return;}
    if(!engine.sendRowPauseToggle(show.rows[(size_t)row].id)) {
        engine.panic();message("Transport command queue full.");return;
    }
    status=active ? "Cue paused." : "Cue resumed.";
    grid.repaint();repaint();
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
void MainComponent::setMasterOutput(int channel) {
    if(channel<0 || channel>1022 || channel%2!=0 || show.masterOutput==channel) return;
    show.masterOutput=channel; dirty=true;
    engine.publish(makeRenderState(show,clips));
    status="Master Bus Main Out set to "+juce::String(channel+1)+"/"+juce::String(channel+2)+".";
    utilities.refreshRouting(); repaint();
}
void MainComponent::setSafetyLimiter(bool enabled) {
    if(show.safetyLimiterEnabled==enabled) return;
    show.safetyLimiterEnabled=enabled; dirty=true;
    engine.publish(makeRenderState(show,clips));
    status=enabled ? "Master Safety Limiter enabled." : "Master Safety Limiter bypassed.";
    safetyLimiter.setToggleState(enabled,juce::dontSendNotification);
    repaint();
}
void MainComponent::setColumnMasterSend(int column,bool enabled) {
    if(column<0 || column>=(int)show.columns.size() ||
       show.columns[(size_t)column].type!=ColumnType::audio ||
       show.columns[(size_t)column].masterSend==enabled) return;
    show.columns[(size_t)column].masterSend=enabled;
    dirty=true; engine.publish(makeRenderState(show,clips));
    status=enabled ? "L/R: column sent to stereo Master Bus." : "L/R: column removed from Master Bus.";
    utilities.refresh(); repaint();
}
void MainComponent::setColumnDirectPostFader(int column,bool post) {
    if(column<0 || column>=(int)show.columns.size() ||
       show.columns[(size_t)column].type!=ColumnType::audio ||
       show.columns[(size_t)column].directPostFader==post) return;
    show.columns[(size_t)column].directPostFader=post;
    dirty=true; engine.publish(makeRenderState(show,clips));
    status=post ? "Direct Out: POST column fader." : "Direct Out: PRE column fader.";
    utilities.refresh(); repaint();
}
void MainComponent::setColumnDirectEnabled(int column,bool enabled) {
    if(column<0 || column>=(int)show.columns.size() ||
       show.columns[(size_t)column].type!=ColumnType::audio ||
       show.columns[(size_t)column].directOutputEnabled==enabled) return;
    show.columns[(size_t)column].directOutputEnabled=enabled;
    dirty=true; engine.publish(makeRenderState(show,clips));
    status=enabled ? "Direct Output enabled." : "Direct Output off (L/R unchanged).";
    utilities.refresh(); repaint();
}
void MainComponent::setDataOutputMuted(int column,bool muted) {
    if(column<0 || column>=(int)show.columns.size()) return;
    auto& col=show.columns[(size_t)column];
    if((col.type!=ColumnType::midi && col.type!=ColumnType::osc) || col.muted==muted) return;
    col.muted=muted;
    dirty=true;
    engine.publish(makeRenderState(show,clips));
    status=muted ? "Data output muted (cue timeline remains active)."
                 : "Data output unmuted.";
    utilities.refresh();
    repaint();
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
    show.columns[column].output=outputIndex;
    show.columns[column].directOutputEnabled=true; dirty=true;
    engine.publish(makeRenderState(show,clips));
    status="Column audio output updated."; utilities.refresh(); repaint();
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
        if (key.getKeyCode()=='X' || key.getKeyCode()=='x') return cutSelectedCell();
    }
    if (key.getModifiers().isCommandDown() && (key.getKeyCode()=='Z' || key.getKeyCode()=='z'))
        return key.getModifiers().isShiftDown() ? redoEdit() : undoEdit();
    if (key.getModifiers().isCommandDown() && (key.getKeyCode()=='Y' || key.getKeyCode()=='y')) return redoEdit();
    if (key.getKeyCode()==juce::KeyPress::backspaceKey &&
        !dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent()))
        return deleteSelectedCell();
    if (key==juce::KeyPress::escapeKey) { stopAll(); return true; }
    if (key.getModifiers().isCommandDown() && key.getKeyCode()=='S') { saveProject(); return true; }
    if (key==juce::KeyPress::spaceKey && !dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent())) { goNext(); return true; }
    if (key==juce::KeyPress::downKey) { select(std::min(selectedRow+1,(int)show.rows.size()-1),selectedColumn); return true; }
    if (key==juce::KeyPress::upKey) { select(std::max(0,selectedRow-1),selectedColumn); return true; } return false;
}
void MainComponent::timerCallback() {
    engine.collectRetired();
    if (projectLoader) {
        PreparedProject prepared;
        if (projectLoader->takeCompleted(prepared)) {
            if (prepared.status.failed())
                message(prepared.status.getErrorMessage()+
                    " The current show has not been replaced.");
            else
                commitPreparedProject(std::move(prepared.show),prepared.source,false,
                                      std::move(prepared.media));
        }
    }
    // Checkpoint only the validated in-memory model; never copy media or
    // touch a running audio callback. The update is atomic and recoverable.
    if (recoveryEnabled && ++recoveryTicks>=25*20) {
        recoveryTicks=0;
        // "Later" must preserve the previous crash snapshot. A new
        // session must not silently overwrite it before the user decides.
        if (dirty && !preserveDeferredRecovery) {
            const auto result=recoveryWriter ?
                recoveryWriter->enqueue(show) :
                juce::Result::fail("Recovery writer unavailable.");
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
            pressedInputs.clear(); // stale NoteOn states must not survive unplug
            status="MIDI input disconnected. Mappings suspended.";
        }
        // Reconnect only the device selected in the project, never whichever
        // controller Windows happens to enumerate first. No playback or GO
        // is triggered by reconnecting.
        if (!midiInput.connected() && show.controls.midiInputId.isNotEmpty()) {
            if (hasExactMidiIdentifier(juce::MidiInput::getAvailableDevices(),
                                       show.controls.midiInputId)) {
                ++midiInputReconnectAttempts;
                const auto restored=midiInput.connect(show.controls.midiInputId);
                if (restored.wasOk()) {
                    pressedInputs.clear();
                    status="MIDI controller reconnected (same device).";
                }
            }
        }
    }
    // Compare monotonic counters; LEDs reflect actual RX/TX, not queued timeline events.
    const auto diag=engine.diagnostics();
    const uint64_t rx=midiInput.receivedCount();
    const uint64_t tx=diag.midiOutputSent;
    const uint64_t oscTx=static_cast<uint64_t>(diag.oscSent)+controlOutput.sentCount();
    const double now=juce::Time::getMillisecondCounterHiRes();
    if(activityInitialized) {
        if(rx>lastMidiRx) midiRxUntil=now+170.0;
        if(tx>lastMidiTx) midiTxUntil=now+170.0;
        if(oscTx>lastOscTx) oscTxUntil=now+170.0;
    }
    lastMidiRx=rx; lastMidiTx=tx; lastOscTx=oscTx;
    activityInitialized=true;
    utilities.syncScroll(viewport.getViewPositionX()); utilities.refreshRouting(); grid.repaint();
    displayPeak=std::max(engine.peak.load(),displayPeak*.88f);
    utilities.refreshMeters(); repaint(); timeline.repaint(); cueMetrics.repaint();
}
void MainComponent::addCue() {
    if (!applyInspector()) return;
    addRow(show,{});
    selectedRow=nextCue=(int)show.rows.size()-1;
    changed("Cue added. Playback stopped.",true);
}
void MainComponent::showColumnMenu(juce::Point<int> screenPoint) {
    if (!applyInspector()) return;
    juce::PopupMenu menu;
    menu.addItem(1,"Audio"); menu.addItem(2,"MIDI");
    menu.addItem(3,"OSC / UDP"); menu.addItem(4,"Control");
    auto safe=juce::Component::SafePointer<MainComponent>(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(
        {screenPoint.x,screenPoint.y,1,1}),[safe](int choice) {
        if (!safe || choice==0) return;
        addColumn(safe->show,(ColumnType)(choice-1),{});
        safe->selectedColumn=(int)safe->show.columns.size()-1;
        safe->changed("Column added. Playback stopped.",true);
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
void MainComponent::setCellLoop(int row,int column,bool enabled) {
    if(row<0 || row>=(int)show.rows.size() || column<0 ||
       column>=(int)show.columns.size() || show.columns[(size_t)column].collapsed) return;
    if(!applyInspector()) return;
    auto& cell=show.rows[(size_t)row].cells[(size_t)column];
    if(cell.loop==enabled) return;
    cell.loop=enabled;
    changed(enabled ? "Cell loop enabled." : "Cell loop disabled.");
}
void MainComponent::setCellFollowsRow(int row,int column,bool enabled) {
    if(row<0 || row>=(int)show.rows.size() || column<0 ||
       column>=(int)show.columns.size() || show.columns[(size_t)column].collapsed) return;
    if(!applyInspector()) return;
    auto& cell=show.rows[(size_t)row].cells[(size_t)column];
    if(cell.followsRow==enabled) return;
    cell.followsRow=enabled;
    changed(enabled ? "Cell included in Row GO." : "Cell excluded from Row GO.");
}
bool MainComponent::deleteSelectedCell() {
    if(selectedRow<0 || selectedRow>=(int)show.rows.size() ||
       selectedColumn<0 || selectedColumn>=(int)show.columns.size() ||
       show.columns[(size_t)selectedColumn].collapsed ||
       (show.columns[(size_t)selectedColumn].type==ColumnType::control &&
        show.columns[(size_t)selectedColumn].controlLocked) || !applyInspector()) return false;
    auto& cell=show.rows[(size_t)selectedRow].cells[(size_t)selectedColumn];
    const int id=cell.id;
    clips.erase(id);
    // Clear content and settings with a stable cell identity.
    cell=Cell{};cell.id=id;
    closeMidiClip();closeOscClip();
    refreshControlSettings();
    changed("Cell deleted. Source files were not touched.");
    return true;
}
bool MainComponent::cutSelectedCell() {
    if(!copySelectedCell()) return false;
    return deleteSelectedCell();
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
