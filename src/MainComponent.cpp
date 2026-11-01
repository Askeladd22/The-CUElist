#include "MainComponent.h"
#include "HeaderTelemetry.h"
#include "OperatorReportDialog.h"
#include "HelpGuide.h"
#include "MidiSessionPolicy.h"
#include "ProjectSafety.h"
#include "ProjectMediaLoader.h"
#include "DeviceIdentity.h"
#include "OperatorTextPrompt.h"
#include "PianoRoll.h"
#include "OscEditor.h"
#include "MidiClip.h"
#include "BinaryData.h"
#include <cmath>
#include <algorithm>
#include <limits>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace cuelist {
namespace {
juce::String typeName(ColumnType t) { return t==ColumnType::audio ? "AUDIO" : t==ColumnType::midi ? "MIDI" : t==ColumnType::osc ? "OSC" : "CONTROL"; }
juce::String duration(double seconds) {
    int s=(int)seconds; return juce::String(s/60).paddedLeft('0',2)+":"+juce::String(s%60).paddedLeft('0',2);
}
}

MainComponent::MainComponent(bool startAudio) {
    setLookAndFeel(&theme); setWantsKeyboardFocus(true); setSize(1440,900);
    for (juce::Component* c : std::initializer_list<juce::Component*>{&openButton,&saveButton,&newButton,&addRowButton,&addColumnButton,
         &goButton,&panicButton,&deviceButton,&controlButton,&collapseAllButton,&inspectorSideTab,
         &cuelistOverflowButton,&addCuelistButton,&cuelistMoveLeft,&cuelistMoveRight,&undoButton,&redoButton,&globalMidiLearn,&globalOscLearn,&quickControlTargets,&quickLearnMapSelector,&cancelQuickLearn,&goLearnHotspot,&panicLearnHotspot,&helpButton,&dockResize,&timelineResize,&importButton,&midiEditButton,&oscEditButton,&playCellButton,&stopCellButton,&clearButton,&tempoButton,&applyButton,
         &cellTab,&rowTab,&rowGain,&rowBehaviour,&rowMute,&rowLoop,&rowName,&cellName,&bpm,&cueNotes,&behaviour,&follow,&loop,&cellAutoStop,&gainDb,&viewport,&timeline,&utilities,
         &clipCollapseButton,&clipCloseButton,&cueHeaders,&columnHeaders,&cueMetrics,&preflightButton,&diagnosticsButton,&preferencesButton,&projectNameLabel,&inlineNameEditor}) addAndMakeVisible(c);
    for (juce::TextButton* b : std::initializer_list<juce::TextButton*>{&openButton,&saveButton,&newButton,&addRowButton,&addColumnButton,&goButton,&panicButton,&deviceButton,&controlButton,&collapseAllButton,&inspectorSideTab,
                    &cuelistOverflowButton,&addCuelistButton,&cuelistMoveLeft,&cuelistMoveRight,&undoButton,&redoButton,&globalMidiLearn,&globalOscLearn,&helpButton,
                    &importButton,&midiEditButton,&oscEditButton,&playCellButton,&stopCellButton,&clearButton,&tempoButton,&applyButton,
                    &preflightButton,&diagnosticsButton,&preferencesButton}) b->setWantsKeyboardFocus(false);
    // Native V2 menus reuse the original operator handlers and shortcuts.
    for (int i=0;i<=OperatorNavigationLayout::overflowIndex;++i) {
        operatorMenus[(size_t)i]=std::make_unique<OperatorNavigationButton>(
            OperatorNavigationLayout::title(i));
        auto& button=*operatorMenus[(size_t)i];
        button.setName("operatorMenu"+juce::String(
            OperatorNavigationLayout::title(i)));
        button.setTooltip("Show "+button.getButtonText()+" actions");
        button.onClick=[this,i] { showOperatorMenu(i); };
        addAndMakeVisible(button);
    }
    viewport.setViewedComponent(&grid,false); viewport.setScrollBarsShown(true,true);
    // Keep drag scrolling available, but place the vertical bar at the far
    // left edge. The right edge must meet BPM/Auto Stop without a dark gutter.
    viewport.setScrollBarThickness(9);
    viewport.setScrollBarPosition(false,true);
    // Immediately redraw pinned headers on wheel, scrollbar drag, resize or
    // keyboard scrolling. The grid body remains the only scrolled component.
    viewport.onScroll=[this] {
        cueHeaders.repaint();
        columnHeaders.repaint();
        cueMetrics.repaint();
        cueMetrics.syncBpmEditor();
        utilities.syncScroll(viewport.getViewPositionX());
        positionInlineNameEditor();
    };
    goButton.setColour(juce::TextButton::buttonColourId,palette::go);
    goButton.setColour(juce::TextButton::textColourOffId,palette::text);
    panicButton.setColour(juce::TextButton::buttonColourId,palette::panic);
    panicButton.setColour(juce::TextButton::textColourOffId,palette::text);
    importButton.setColour(juce::TextButton::buttonColourId,palette::go);
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
        setMasterAudio(GainMeterFader::toLinear(db));
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
    cueNotes.setName("cueNotes");
    cueNotes.setMultiLine(true,true);
    // Escape must bubble to MainComponent::keyPressed so Panic works even
    // when the operator is typing in the multiline cue notepad.
    cueNotes.setEscapeAndReturnKeysConsumed(false);
    cueNotes.setReturnKeyStartsNewLine(true);
    cueNotes.setScrollbarsShown(true);
    cueNotes.setInputRestrictions(8192);
    cueNotes.setFont(font(12.5f));
    cueNotes.setTextToShowWhenEmpty("Technical notes, instructions, stage reminders...",palette::muted);
    cueNotes.setTooltip("Operational notes saved with this cue. Focus change commits your edits.");
    cueNotes.onFocusLost=[this] { if (inspectorDiffers()) applyInspector(); };
    inspectorSideTab.setName("inspectorSideToggle");
    inspectorSideTab.setTooltip("Collapse Inspector to the right and expand the cue workspace.");
    inspectorSideTab.setColour(juce::TextButton::buttonColourId,palette::raised);
    inspectorSideTab.setColour(juce::TextButton::textColourOffId,palette::teal);
    inspectorSideTab.onClick=[this] {
        // Commit all pending edits (including cue notepad) before hiding.
        // Invalid edits keep the Inspector open so the operator can fix them.
        if (inspectorVisible && !applyInspector()) return;
        inspectorVisible=!inspectorVisible;
        refreshInspector();
        resized();
        repaint();
    };
    collapseAllButton.setName("toggleAllColumns");
    // The button already says "Collapse all" or "Expand all". A hover tooltip
    // would obscure the first cue's transport controls during a live show.
    collapseAllButton.onClick=[this] {
        const bool allCollapsed=std::all_of(show.columns.begin(),show.columns.end(),
            [](const Column& col){return col.collapsed;});
        setAllColumnsCollapsed(!allCollapsed);
    };
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
    // Edit the displayed name directly, never through a rename dialog.
    inlineNameEditor.setName("inlineNameEditor");
    inlineNameEditor.setVisible(false);
    inlineNameEditor.setMultiLine(false);
    inlineNameEditor.setReturnKeyStartsNewLine(false);
    inlineNameEditor.setEscapeAndReturnKeysConsumed(true);
    inlineNameEditor.setSelectAllWhenFocused(true);
    inlineNameEditor.setJustification(juce::Justification::centredLeft);
    inlineNameEditor.setFont(font(13.0f));
    inlineNameEditor.setColour(juce::TextEditor::backgroundColourId,palette::input);
    inlineNameEditor.setColour(juce::TextEditor::textColourId,palette::text);
    inlineNameEditor.setColour(juce::TextEditor::outlineColourId,palette::teal);
    inlineNameEditor.setColour(juce::TextEditor::focusedOutlineColourId,palette::teal);
    inlineNameEditor.onReturnKey=[this] {
        if (commitInlineRename()) grabKeyboardFocus();
    };
    inlineNameEditor.onEscapeKey=[this] {
        cancelInlineRename();
        grabKeyboardFocus();
    };
    inlineNameEditor.onFocusLost=[this] {
        if (inlineRenameKind!=InlineRenameKind::none && !commitInlineRename())
            cancelInlineRename();
    };
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
    helpButton.setName("showHelpMenu");
    helpButton.setTooltip("User Guide and About The Cuelist.");
    helpButton.onClick=[this] {
        juce::PopupMenu menu;
        menu.setLookAndFeel(&theme);
        menu.addItem(1,"Guide");
        menu.addItem(2,"About The Cuelist");
        auto safe=juce::Component::SafePointer<MainComponent>(this);
        menu.showMenuAsync(
            juce::PopupMenu::Options().withTargetComponent(&helpButton),
            [safe](int choice) {
                if (!safe) return;
                if (choice==1) safe->openUserGuide();
                else if (choice==2) safe->showAbout();
            });
    };
    preferencesButton.setName("projectPreferences");
    preferencesButton.setTooltip("Settings: MIDI devices, OSC ports, audio devices and clock.");
    // Application setting, never serialized into or applied destructively to a Show.
    juce::PropertiesFile::Options prefOptions;
    prefOptions.applicationName="The Cuelist";
    prefOptions.filenameSuffix="settings";
    prefOptions.folderName="The Cuelist";
    appPreferences=std::make_unique<juce::PropertiesFile>(prefOptions);
    faderCeilingDb=appPreferences->getIntValue("audioFaderCeilingDb",6)==0 ? 0 : 6;
    masterSlider.setMaximumDb(faderCeilingDb);
    restoreAdditionalMidiInputs();
    for (const auto& midi:juce::MidiOutput::getAvailableDevices())
        engine.setMidiOutputEnabled(midi.identifier,
            appPreferences->getBoolValue("midiOutputEnabled_"+midi.identifier,true));
    preferencesButton.onClick=[this] {
        juce::PopupMenu menu;
        menu.setLookAndFeel(&theme);
        menu.addItem(1,"Pre-flight"); menu.addItem(2,"Diagnostics");
        juce::PopupMenu faderMenu;
        faderMenu.setLookAndFeel(&theme);
        faderMenu.addItem(3,faderRangeMenuTitle(6),true,faderCeilingDb==6);
        faderMenu.addItem(4,faderRangeMenuTitle(0),true,faderCeilingDb==0);
        menu.addSubMenu("Fader Range",faderMenu);
        menu.addSeparator();
        juce::PopupMenu clockMenu;
        const bool twelve=appPreferences && appPreferences->getBoolValue("clock12Hour",false);
        clockMenu.addItem(60,"24-hour clock",true,!twelve);
        clockMenu.addItem(61,"12-hour clock",true,twelve);
        menu.addSubMenu("Clock",clockMenu);
        juce::PopupMenu midiMenu;
        const auto inputs=juce::MidiInput::getAvailableDevices();
        midiMenu.addSectionHeader("MIDI INPUT — ENABLE");
        midiMenu.addItem(100,"Disable main MIDI input",true,show.controls.midiInputId.isEmpty());
        for (int i=0;i<inputs.size() && i<100;++i)
            midiMenu.addItem(101+i,inputs[i].name,true,
                             show.controls.midiInputId==inputs[i].identifier ||
                             isAdditionalMidiInputEnabled(inputs[i].identifier));
        midiMenu.addSeparator();
        midiMenu.addSectionHeader("MIDI OUTPUT — ENABLE");
        const auto outputs=juce::MidiOutput::getAvailableDevices();
        for (int i=0;i<outputs.size() && i<100;++i) {
            const bool enabled=!appPreferences ||
                appPreferences->getBoolValue("midiOutputEnabled_"+outputs[i].identifier,true);
            midiMenu.addItem(301+i,outputs[i].name,true,enabled);
        }
        menu.addSubMenu("MIDI Devices",midiMenu);
        juce::PopupMenu oscMenu;
        oscMenu.addSectionHeader("Local IP: "+juce::IPAddress::getLocalAddress().toString());
        const auto ports=configuredOscPorts();
        oscMenu.addItem(70,"Receive UDP ports: "+(ports.isEmpty()?"not configured":ports));
        oscMenu.addItem(71,"Disable OSC input");
        menu.addSubMenu("OSC",oscMenu);
        menu.addItem(72,"Audio Devices...");
        menu.addSeparator();
        menu.addItem(5,"Rename active Cuelist");
        auto safe=juce::Component::SafePointer<MainComponent>(this);
        // The original Settings button is retained only as an action source.
        // Anchor its unchanged native settings popup to the visible
        // operator Show menu after removing the duplicate sidebar button.
        auto* anchor=preferencesButton.isVisible()
            ? static_cast<juce::Component*>(&preferencesButton)
            : static_cast<juce::Component*>(
                  operatorMenus[2]->isVisible()
                      ? operatorMenus[2].get()
                      : operatorMenus[OperatorNavigationLayout::overflowIndex].get());
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(anchor),
        [safe,inputs,outputs](int choice) {
            if (!safe) return;
            if(choice==1) safe->showPreflight();
            else if(choice==2) safe->showDiagnostics();
            else if(choice==3 || choice==4) safe->setAudioFaderCeilingDb(choice==3 ? 6 : 0);
            else if(choice==5) safe->renameActiveCuelist();
            else if(choice==60 || choice==61) {
                if(safe->appPreferences)
                    safe->appPreferences->setValue("clock12Hour",choice==61);
                safe->repaint();
            } else if(choice==72) safe->showDevices();
            else if(choice==70) {
                showOperatorTextPrompt(*safe,safe->theme,"OSC Input Ports",
                    "Comma-separated UDP ports, up to 16; empty disables listening.",
                    "Receive UDP ports",safe->configuredOscPorts(),false,"Apply",
                    [safe](juce::String value) {
                        if(safe) safe->configureOscInputPorts(value);
                    });
            } else if(choice==71) safe->configureOscInputPorts("");
            else if(choice==100) safe->selectControlInput({});
            else if(choice>=101 && choice<201 && choice-101<inputs.size()) {
                const auto device=inputs[choice-101].identifier;
                if (device==safe->show.controls.midiInputId)
                    safe->selectControlInput({});
                else if (safe->show.controls.midiInputId.isEmpty() &&
                         !safe->isAdditionalMidiInputEnabled(device))
                    safe->selectControlInput(device);
                else safe->toggleAdditionalMidiInput(device);
            }
            else if(choice>=301 && choice<401 && choice-301<outputs.size()) {
                const auto device=outputs[choice-301].identifier;
                const bool enabled=safe->appPreferences ?
                    !safe->appPreferences->getBoolValue("midiOutputEnabled_"+device,true) : false;
                if (safe->appPreferences)
                    safe->appPreferences->setValue("midiOutputEnabled_"+device,enabled);
                safe->engine.setMidiOutputEnabled(device,enabled);
                safe->status="MIDI output "+outputs[choice-301].name+
                             (enabled?" enabled.":" disabled.");
                safe->repaint();
            }
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
    deviceButton.setButtonText("Settings");
    deviceButton.onClick=[this] {preferencesButton.triggerClick();};
    // Each tab is a genuine Cuelist selector. The third tab must never
    // double as an overflow popup: the overflow appears only when needed.
    for (int i=0;i<(int)cuelistTabs.size();++i) {
        cuelistTabs[(size_t)i]=std::make_unique<CuelistTabButton>();
        cuelistCloseButtons[(size_t)i]=std::make_unique<juce::TextButton>("x");
        auto& tab=*cuelistTabs[(size_t)i];
        auto& close=*cuelistCloseButtons[(size_t)i];
        tab.setName("cuelistTab"+juce::String(i));
        close.setName("deleteCuelist"+juce::String(i));
        tab.setWantsKeyboardFocus(false);
        close.setWantsKeyboardFocus(false);
        tab.onClick=[this,i] {activateCuelist(i);};
        tab.onRename=[this,i] {renameCuelist(i);};
        tab.onDragMove=[this,i](int x) {
            cuelistDragSource=i;
            updateCuelistDropTarget(x);
        };
        tab.onDropped=[this,i](int x) {
            updateCuelistDropTarget(x);
            const int target=cuelistDragTarget;
            cuelistDragSource=cuelistDragTarget=-1;
            if (target>=0) moveCuelist(i,target);
            refreshCuelistTabs();
        };
        close.onClick=[this,i] {requestDeleteCuelist(i);};
        tab.setVisible(false);
        close.setVisible(false);
        addAndMakeVisible(tab);
        addAndMakeVisible(close);
    }
    cuelistOverflowButton.setName("cuelistOverflow");
    cuelistOverflowButton.setTooltip("Other Cuelists that do not fit in the tab bar.");
    cuelistOverflowButton.onClick=[this] {showCuelistOverflowMenu();};
    addCuelistButton.setName("newCuelist");
    addCuelistButton.setTooltip("Create another Cuelist in this show.");
    addCuelistButton.onClick=[this] {createCuelist();};
    cuelistMoveLeft.setName("cuelistMoveLeft");
    cuelistMoveRight.setName("cuelistMoveRight");
    cuelistMoveLeft.setTooltip("Move active Cuelist left (Ctrl+Shift+Left)");
    cuelistMoveRight.setTooltip("Move active Cuelist right (Ctrl+Shift+Right)");
    cuelistMoveLeft.onClick=[this] {
        moveCuelist(show.activeCuelist,show.activeCuelist-1);
    };
    cuelistMoveRight.onClick=[this] {
        moveCuelist(show.activeCuelist,show.activeCuelist+1);
    };
    undoButton.setName("showUndo");
    redoButton.setName("showRedo");
    undoButton.setTooltip("Undo the last MIDI/OSC clip edit (Ctrl+Z).");
    redoButton.setTooltip("Redo the last MIDI/OSC clip edit (Ctrl+Shift+Z).");
    undoButton.onClick=[this] {undoEdit();};
    redoButton.onClick=[this] {redoEdit();};
    undoButton.setEnabled(false);
    redoButton.setEnabled(false);
    quickControlTargets.setVisible(false);
    quickLearnMapSelector.setVisible(false);
    quickLearnMapSelector.setName("quickLearnMapSelector");
    quickLearnMapSelector.setTooltip("Choose Default Map or a cue Control Column override");
    quickLearnMapSelector.onChange=[this] {
        const int index=quickLearnMapSelector.getSelectedId()-1;
        if (index>=0 && index<(int)quickLearnMapLocations.size()) {
            const auto location=quickLearnMapLocations[(size_t)index];
            selectQuickLearnMap(location.first,location.second);
        }
    };
    quickControlTargets.onRename=[this](int id){renameQuickLearnFunction(id);};
    cancelQuickLearn.setVisible(false);
    quickControlTargets.onPick=[this](int functionId) {chooseQuickLearnTarget(functionId);};
    cancelQuickLearn.onClick=[this] {finishQuickLearn();status="Quick Learn ended.";repaint();};
    cancelQuickLearn.setName("cancelQuickLearn");
    goLearnHotspot.onPick=[this] {chooseQuickLearnGuiTarget(ControlTarget::go);};
    panicLearnHotspot.onPick=[this] {chooseQuickLearnGuiTarget(ControlTarget::panic);};
    masterGainLearnHotspot.onPick=[this] {chooseQuickLearnGuiTarget(ControlTarget::masterGain);};
    masterPanLearnHotspot.onPick=[this] {chooseQuickLearnGuiTarget(ControlTarget::masterPan);};
    masterLimiterLearnHotspot.onPick=[this] {chooseQuickLearnGuiTarget(ControlTarget::limiterToggle);};
    utilities.addAndMakeVisible(masterGainLearnHotspot);
    utilities.addAndMakeVisible(masterPanLearnHotspot);
    utilities.addAndMakeVisible(masterLimiterLearnHotspot);
    masterGainLearnHotspot.setVisible(false);
    masterPanLearnHotspot.setVisible(false);
    masterLimiterLearnHotspot.setName("quickLearnMasterLimiter");
    masterLimiterLearnHotspot.setVisible(false);
    goLearnHotspot.setVisible(false);
    panicLearnHotspot.setVisible(false);
    globalMidiLearn.setName("globalMidiLearn");
    globalOscLearn.setName("globalOscLearn");
    globalMidiLearn.setTooltip("Inline MIDI Learn: choose a parameter, move a control, done.");
    globalOscLearn.setTooltip("Inline OSC Learn: choose a parameter, send a message, done.");
    globalMidiLearn.onClick=[this] {beginQuickLearn(false);};
    globalOscLearn.onClick=[this] {beginQuickLearn(true);};
    controlButton.setName("controlMapping");
    controlButton.setTooltip("MIDI input, MIDI Learn, and global/per-cue mappings.");
    controlButton.onClick=[this] {openControlPanel(false);};
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

void MainComponent::setAudioFaderCeilingDb(int db) {
    const int chosen=db==0 ? 0 : 6;
    if (faderCeilingDb==chosen) return;
    faderCeilingDb=chosen;
    if (appPreferences) {
        appPreferences->setValue("audioFaderCeilingDb",chosen);
        appPreferences->saveIfNeeded();
    }
    // A range change must never alter Show gain, queued transport or MIDI/OSC.
    masterSlider.setMaximumDb(chosen);
    utilities.refresh();
    status=chosen==0 ? "Audio faders: -inf / 0 dB (unity at maximum)." :
        "Audio faders: -inf / +6 dB.";
    repaint();
}

WorkspaceLayout MainComponent::workspaceLayout() const {
    const bool docked=(clipEditor && !clipWindow) || (oscEditor && !oscWindow);
    const int activeDockHeight=docked ? (clipCollapsed ? 36 : dockHeight) : 0;
    return calculateWorkspaceLayout({getWidth(),getHeight(),activeDockHeight,
        grid.columnX((int)show.columns.size()),(int)show.rows.size(),
        CueGrid::headerHeight,CueGrid::rowHeight,CueMetricsOverlay::width,
        CueGrid::nameWidth,timelineHeightAdjustment,inspectorVisible});
}

void MainComponent::paint(juce::Graphics& g) {
    const int w=getWidth(),h=getHeight();
    const auto geometry=workspaceLayout();
    g.fillAll(palette::background);
    // Two quiet header tiers: 38px operator navigation and 34px Cuelists.
    // No change to the 72px workspace boundary or any child hitbox.
    g.setColour(palette::toolbar); g.fillRect(0,0,w,38);
    g.setColour(palette::toolbar.interpolatedWith(palette::panel,.43f));
    g.fillRect(0,38,w,35);
    g.setColour(palette::panel); g.fillRect(0,73,180,h-73);
    // A collapsed MIDI/OSC editor retains a 36px docking region while
    // its editor component is hidden. Paint the reserved panel background
    // so the Timeline splitter borders a real panel, not a black stripe.
    if (geometry.dockEditor.getHeight()>0) {
        g.setColour(palette::panel);
        g.fillRect(geometry.dockEditor);
    }
    g.setColour(palette::line.withAlpha(.38f));
    g.drawHorizontalLine(37,0,(float)std::min(w,HeaderTelemetryLayout::atWidth(w).clock.getX()-10));
    g.setColour(palette::line.withAlpha(.62f));
    g.drawHorizontalLine(72,0,(float)w);
    g.drawVerticalLine(180,73,(float)h);
    // Keep the project card, but remove the redundant static ENGINES panel.
    // Engine controls, routing and the live audio-status indicator are unchanged.
    g.setColour(palette::background.withAlpha(.30f));
    g.fillRoundedRectangle(10.0f,373.0f,160.0f,96.0f,8.0f);
    static const auto appIcon=juce::ImageFileFormat::loadFrom(
        BinaryData::thecuelist_icon_png,BinaryData::thecuelist_icon_pngSize);
    if (appIcon.isValid())
        g.drawImageWithin(appIcon,15,7,31,31,juce::RectanglePlacement::centred);
    else for (int i=0;i<3;++i) {
        g.setColour(i==1 ? palette::teal : palette::text);
        g.fillRoundedRectangle(17.0f,12.0f+i*7,23.0f-i*4,3,1.5f);
    }
    // Product wordmark: neutral "The" plus the restrained teal CUElist
    // accent from the approved reference (no font or icon replacement).
    label(g,"The",{53,7,38,32},19,palette::text,true);
    label(g,"CUElist",{91,7,105,32},19,palette::teal,true);
    // One coherent monitoring surface instead of three heavy cards. The
    // clock, CPU/RAM, I/O and audio values still use their tested positions.
    const auto telemetry=HeaderTelemetryLayout::atWidth(w);
    const auto monitorStrip=juce::Rectangle<float>(
        (float)telemetry.clock.getX(),7.f,
        (float)(w-telemetry.clock.getX()-10),58.f);
    g.setColour(palette::panel.withAlpha(.93f));
    g.fillRoundedRectangle(monitorStrip,5.f);
    g.setColour(palette::line.withAlpha(.45f));
    g.drawRoundedRectangle(monitorStrip,5.f,.8f);
    g.setColour(palette::line.withAlpha(.55f));
    g.drawVerticalLine(telemetry.usage.getX()-7,13.f,58.f);
    g.drawVerticalLine(w-375,13.f,58.f);
    // A large operator clock that remains readable without enlarging the
    // global 72px header or overlapping CPU, RAM, MIDI/OSC and Audio.
    // In 12h mode show AM/PM separately instead of shrinking all digits.
    const auto now=juce::Time::getCurrentTime();
    const bool clock12=appPreferences &&
        appPreferences->getBoolValue("clock12Hour",false);
    label(g,now.formatted(clock12?"%I:%M:%S":"%H:%M:%S"),
          clock12 ? telemetry.timeText : telemetry.clock.reduced(4,9),
          30,palette::text,true,
          juce::Justification::centred);
    if (clock12)
        label(g,now.formatted("%p"),telemetry.meridiemText,11,
              palette::teal,true,juce::Justification::centredRight);
    // Two balanced CPU/RAM rows, with one decimal only for CPU.
    label(g,"CPU",telemetry.cpuLabel,11,palette::text,true);
    label(g,HeaderTelemetryLayout::percentage(systemCpuPercent,true),
          telemetry.cpuValue,11,palette::text,true,
          juce::Justification::centredRight);
    label(g,"RAM",telemetry.ramLabel,11,palette::text,true);
    label(g,HeaderTelemetryLayout::percentage(systemRamPercent,false),
          telemetry.ramValue,11,palette::text,true,
          juce::Justification::centredRight);
    const auto meter=[&](juce::Rectangle<float> bounds,float percentage,
                         juce::Colour colour) {
        g.setColour(palette::line);
        g.fillRoundedRectangle(bounds,2.5f);
        if (std::isfinite(percentage) && percentage>=0) {
            g.setColour(colour);
            g.fillRoundedRectangle(bounds.withWidth(
                bounds.getWidth()*juce::jlimit(0.0f,1.0f,
                                              percentage/100.0f)),2.5f);
        }
    };
    meter(telemetry.cpuMeter,systemCpuPercent,palette::teal);
    meter(telemetry.ramMeter,systemRamPercent,palette::amber);

    // A compact but prominent shared signal monitor: 2x2 traffic
    // indicators and independent audio availability, all inside one card.
    // The values are real counters (OSC RX stays inactive: no listener).
    const int statusX=w-365;
    g.setColour(palette::line.withAlpha(.55f));
    g.drawVerticalLine(w-153,13.f,58.f);
    label(g,"I/O ACTIVITY",{statusX+10,11,82,14},
          10,palette::muted,true);
    const double now=juce::Time::getMillisecondCounterHiRes();
    const auto activity=[&](int x,int y,const juce::String& title,bool active) {
        g.setColour(active ? palette::teal : palette::line.brighter(.22f));
        g.fillEllipse((float)x,(float)y+4.0f,8.0f,8.0f);
        label(g,title,{x+14,y,83,17},11,
              active ? palette::text : palette::muted,true);
    };
    activity(statusX+12,30,"MIDI RX",now<midiRxUntil);
    activity(statusX+112,30,"MIDI TX",now<midiTxUntil);
    activity(statusX+12,47,"OSC RX",now<oscRxUntil);
    activity(statusX+112,47,"OSC TX",now<oscTxUntil);
    const bool audioConnected=engine.connected.load();
    label(g,"AUDIO",{w-137,13,119,16},10,palette::muted,true);
    g.setColour(audioConnected ? palette::teal : palette::red);
    g.fillEllipse((float)w-139,37.0f,9.0f,9.0f);
    label(g,audioConnected ? "AVAILABLE" : "UNAVAILABLE",
          {w-125,30,108,24},11,
          audioConnected ? palette::teal : palette::red,true);
    // N2: one editable Show identity instead of two redundant Show/Project
    // cards. File, Settings and Control Mapping are in genuine JUCE menus.
    label(g,"WORKSPACE",{20,91,140,17},11,palette::muted,true);
    g.setColour(palette::teal.withAlpha(0.19f));
    g.fillRoundedRectangle(12,112,156,54,8);
    g.setColour(palette::teal);
    g.fillRoundedRectangle(12,119,4,40,2);
    // The project title remains the original editable JUCE Label.
    // Do not repeat cue/column counts from the Cuelist header.
    if (hasUnsavedChanges()) {
        g.setColour(palette::amber);
        g.fillEllipse(20.0f,181.0f,7.0f,7.0f);
        label(g,"UNSAVED CHANGES",{34,174,130,21},10,palette::amber,true);
    }
    g.setColour(palette::amber.withAlpha(.11f));
    g.fillRoundedRectangle(12.0f,(float)h-339,156.0f,51.0f,8.0f);
    label(g,"ON DECK",{20,h-333,145,16},11,palette::amber,true);
    label(g,nextCue<(int)show.rows.size() ? "NEXT  /  "+juce::String(nextCue+1).paddedLeft('0',2) :
          "END OF SHOW",{20,h-314,148,24},17,palette::text,true);
    // Output meter removed from the sidebar; audio peak telemetry remains
    // available in the Master and channel meters.
    // Continue the non-scrollable metrics surface beneath the mixer. This
    // rightmost band is never a hidden extension of audio/MIDI/OSC strips.
    g.setColour(palette::panel);
    g.fillRect(geometry.metrics.getX(),geometry.utilities.getY(),
               geometry.metrics.getWidth(),geometry.utilities.getHeight());
    // Continue the exact 3px accent edge from the pinned metrics down
    // through the utility area, without a doubled boundary or dark gap.
    g.setColour(palette::teal);
    g.fillRect(geometry.metrics.getX(),geometry.utilities.getY(),
               3,geometry.utilities.getHeight());
    g.setColour(palette::line);
    g.drawVerticalLine(geometry.metrics.getX()+3,(float)geometry.utilities.getY(),
                       (float)geometry.utilities.getBottom());
    if (!inspectorVisible) {
        // A slim, permanently visible rail provides a full-height spatial
        // cue for reopening without stealing any BPM/Auto Stop hit targets.
        g.setColour(palette::panel);
        g.fillRect(geometry.editorX,76,w-geometry.editorX,h-123);
        g.setColour(palette::line);
        g.drawVerticalLine(geometry.editorX,76.0f,(float)h-47.0f);
    }
    if (inspectorVisible) {
    if (quickLearnMode!=QuickLearnMode::off) {
        const int x=geometry.inspectorLeft;
        g.setColour(palette::panel);
        g.fillRoundedRectangle(geometry.inspectorPanel.toFloat(),9.0f);
        g.setColour(palette::line);
        g.drawRoundedRectangle(geometry.inspectorPanel.toFloat(),9.0f,1.25f);
        const auto learnColour=quickLearnAccent();
        label(g,quickLearnMode==QuickLearnMode::midi ? "MIDI LEARN" : "OSC LEARN",
              {x+5,98,geometry.inspectorInnerWidth-10,25},19,learnColour,true);
        const bool midiInputMissing=quickLearnMode==QuickLearnMode::midi &&
                                    !midiInput.connected();
        label(g,"Map to edit",{x+5,126,geometry.inspectorInnerWidth-10,16},11,palette::muted,true);
        label(g,quickLearnFunctionId>0 ?
              (midiInputMissing ? "MIDI input missing - select device" :
               "Selected target blinking - move your controller") :
              "Select a parameter or a named function",
              {x+5,155,geometry.inspectorInnerWidth-10,38},12,
              quickLearnFunctionId>0 ? learnColour : palette::text);
        if (quickLearnFeedback.isNotEmpty())
            label(g,quickLearnFeedback,
                  {x+5,184,geometry.inspectorInnerWidth-10,20},
                  10,palette::teal,true);
        label(g,"Available parameters / functions",
              {x+4,205,geometry.inspectorInnerWidth-8,24},11,learnColour,true);
    } else {
    const int inspectorInner=geometry.inspectorInnerWidth;
    auto panel=geometry.inspectorPanel;
    g.setColour(palette::panel); g.fillRoundedRectangle(panel.toFloat(),9);
    g.setColour(palette::line); g.drawRoundedRectangle(panel.toFloat(),9,1.25f);
    const int x=geometry.inspectorLeft;
    g.setColour(palette::teal); g.fillRoundedRectangle((float)geometry.editorX+13,93.0f,4.0f,49.0f,2.0f);
    label(g,"INSPECTOR",{x+10,99,200,19},13,palette::teal,true);
    const auto& col=show.columns[selectedColumn];
    label(g,"CUE "+juce::String(selectedRow+1).paddedLeft('0',2),{x+10,129,140,28},22,palette::text,true);
    label(g,typeName(col.type),{x+131,134,98,21},11,palette::type((int)col.type),true,juce::Justification::centredRight);
    g.saveState();
    g.addTransform(geometry.inspectorTransform());
    // Paint both Inspector cards in the same scaled coordinate system as
    // their labels and editable controls: CELL was drifting above its card.
    g.setColour(palette::raised.withAlpha(.52f));
    g.fillRoundedRectangle(geometry.inspectorCueCard.toFloat(),7.0f);
    g.fillRoundedRectangle(geometry.inspectorCellCard.toFloat(),7.0f);
    label(g,"ENTIRE CUE",{x+3,173,inspectorInner-6,20},14,palette::teal,true);
    label(g,"Cue name",{x,204,inspectorInner,18},12,palette::text,true);
    label(g,"Initial BPM",{x,272,inspectorInner,18},12,palette::text,true);
    g.setColour(palette::line);g.drawHorizontalLine(407,(float)x,(float)x+inspectorInner);
    label(g,"CELL",{x+3,423,59,20},14,palette::teal,true);
    label(g,"/  "+col.name,{x+62,423,inspectorInner-68,20},13,palette::text,true);
    label(g,"Cell name",{x,454,inspectorInner,18},12,palette::text,true);
    if(col.type==ColumnType::audio) {
        label(g,"Gain",{x,532,48,18},12,palette::muted,true);
        label(g,"dB",{x+163,532,43,18},12,palette::muted,true);
    }
    if(col.type==ColumnType::osc)
        label(g,"UDP destination in column Utility",{x,645,inspectorInner,18},10,palette::muted);
    g.restoreState();
    // A dedicated cue-scoped notepad; it is never superimposed on the
    // audio gain, MIDI/OSC editor buttons or row-level switches.
    const int notesAfterActions=geometry.scaleInspectorControl({0,728,1,25}).getBottom()+8;
    const int notesTop=std::max(notesAfterActions,geometry.inspectorPanel.getBottom()-167);
    const int notesHeight=std::max(45,geometry.inspectorPanel.getBottom()-notesTop-5);
    g.setColour(palette::raised.withAlpha(.52f));
    g.fillRoundedRectangle((float)x-6,(float)notesTop,(float)inspectorInner+12,
                           (float)notesHeight,7.0f);
    g.setColour(palette::line.withAlpha(.8f));
    g.drawRoundedRectangle((float)x-6,(float)notesTop,(float)inspectorInner+12,
                           (float)notesHeight,7.0f,1.0f);
    label(g,"NOTES  /  CUE "+juce::String(selectedRow+1).paddedLeft('0',2),
          {x+3,notesTop+5,inspectorInner-6,20},11,palette::teal,true);
    }
    }
    label(g,status,{geometry.viewport.getX(),h-35,std::max(1,geometry.editorX-200),25},12,palette::muted);
}
void MainComponent::showCuelistOverflowMenu() {
    juce::PopupMenu menu;
    menu.setLookAndFeel(&theme);
    for (int i=0;i<(int)show.cuelists.size();++i)
        if (!cuelistTabs[(size_t)i]->isVisible())
            menu.addItem(100+i,show.cuelists[(size_t)i].name,true,
                         i==show.activeCuelist);

    // Off-screen tabs must remain manageable, not just selectable.
    juce::PopupMenu renameMenu;
    renameMenu.setLookAndFeel(&theme);
    for (int i=0;i<(int)show.cuelists.size();++i)
        if (!cuelistTabs[(size_t)i]->isVisible())
            renameMenu.addItem(300+i,"Rename "+show.cuelists[(size_t)i].name);
    if (renameMenu.getNumItems()>0) {
        menu.addSeparator();
        menu.addSubMenu("Rename hidden Cuelist...",renameMenu);
    }
    juce::PopupMenu deleteMenu;
    deleteMenu.setLookAndFeel(&theme);
    if (show.cuelists.size()>1)
        for (int i=0;i<(int)show.cuelists.size();++i)
            if (!cuelistTabs[(size_t)i]->isVisible())
                deleteMenu.addItem(200+i,"Delete "+show.cuelists[(size_t)i].name);
    if (deleteMenu.getNumItems()>0) {
        menu.addSeparator();
        menu.addSubMenu("Delete hidden Cuelist...",deleteMenu);
    }
    juce::PopupMenu moveLeft,moveRight;
    moveLeft.setLookAndFeel(&theme);
    moveRight.setLookAndFeel(&theme);
    for (int i=0;i<(int)show.cuelists.size();++i)
        if (!cuelistTabs[(size_t)i]->isVisible()) {
            moveLeft.addItem(400+i,show.cuelists[(size_t)i].name,i>0);
            moveRight.addItem(500+i,show.cuelists[(size_t)i].name,
                              i+1<(int)show.cuelists.size());
        }
    if (moveLeft.getNumItems()>0) {
        menu.addSeparator();
        menu.addSubMenu("Move hidden Cuelist left...",moveLeft);
        menu.addSubMenu("Move hidden Cuelist right...",moveRight);
    }
    if (menu.getNumItems()==0) return;
    auto safe=juce::Component::SafePointer<MainComponent>(this);
    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(&cuelistOverflowButton),
        [safe](int choice) {
            if (!safe) return;
            if (choice>=100 && choice<112) safe->activateCuelist(choice-100);
            if (choice>=200 && choice<212) safe->requestDeleteCuelist(choice-200);
            if (choice>=300 && choice<312) safe->renameCuelist(choice-300);
            if (choice>=400 && choice<412) safe->moveCuelist(choice-400,choice-401);
            if (choice>=500 && choice<512) safe->moveCuelist(choice-500,choice-499);
        });
}

void MainComponent::updateCuelistDropTarget(int rootX) {
    int candidate=-1,nearest=std::numeric_limits<int>::max();
    for (int i=0;i<(int)show.cuelists.size();++i) {
        auto* tab=cuelistTabs[(size_t)i].get();
        if (!tab || !tab->isVisible()) continue;
        const int distance=std::abs(rootX-tab->getBounds().getCentreX());
        if (distance<nearest) {nearest=distance;candidate=i;}
    }
    if (cuelistDragTarget!=candidate) {
        cuelistDragTarget=candidate;
        refreshCuelistTabs(); // highlighted destination tab
    }
}
void MainComponent::refreshCuelistTabs() {
    // setSize() can call resized() before dynamic tab allocation in the ctor.
    if (!cuelistTabs[0]) return;

    constexpr int left=190,slot=84,buttonHeight=27;
    const int count=(int)show.cuelists.size();
    // Stop tabs before the fixed clock/CPU panels. This gives nine real tabs
    // at 1647px, three at 1180px, and overflows only when space runs out.
    const int available=std::max(150,getWidth()-650-8-left);
    const int plusWidth=count<12 ? 32 : 0;
    const bool overflow=count*slot+plusWidth>available;
    const int overflowWidth=overflow ? 42 : 0;
    const int displayed=overflow
        ? juce::jlimit(1,count,(available-plusWidth-overflowWidth)/slot)
        : count;
    // Keep displayed indices contiguous, including the active Cuelist.
    const int first=overflow
        ? juce::jlimit(0,count-displayed,show.activeCuelist-displayed+1)
        : 0;
    for (int i=0;i<(int)cuelistTabs.size();++i) {
        auto& tab=*cuelistTabs[(size_t)i];
        auto& close=*cuelistCloseButtons[(size_t)i];
        const bool visible=i>=first && i<first+displayed && i<count;
        tab.setVisible(visible);
        close.setVisible(visible && count>1);
        if (!visible) continue;

        const int x=left+(i-first)*slot;
        const bool active=i==show.activeCuelist;
        tab.setActiveVisual(active);
        const auto colour=i==cuelistDragTarget ? palette::teal.withAlpha(.45f) :
                          active ? palette::selectedTab : palette::panel;
        auto name=show.cuelists[(size_t)i].name;
        tab.setButtonText(name.length()>13 ? name.substring(0,11)+"..." : name);
        tab.setTooltip(name+" - click: select; double-click: rename; drag: move.");
        tab.setColour(juce::TextButton::buttonColourId,colour);
        tab.setBounds(x,43,count>1 ? 64 : 81,buttonHeight);
        close.setColour(juce::TextButton::buttonColourId,colour);
        close.setColour(juce::TextButton::textColourOffId,palette::muted);
        close.setBounds(x+64,43,18,buttonHeight);
        close.setTooltip("Delete "+name+" (confirmation required).");
        close.toFront(false);
    }
    const int trailingX=left+displayed*slot;
    cuelistOverflowButton.setVisible(overflow);
    if (overflow) {
        cuelistOverflowButton.setBounds(trailingX,43,38,buttonHeight);
        cuelistOverflowButton.setColour(juce::TextButton::buttonColourId,palette::raised);
    }
    cuelistMoveLeft.setEnabled(count>1 && show.activeCuelist>0);
    cuelistMoveRight.setEnabled(count>1 && show.activeCuelist<count-1);
    addCuelistButton.setVisible(count<12);
    if (count<12) addCuelistButton.setBounds(trailingX+overflowWidth,43,28,buttonHeight);
}

// N3: A single function location per operator action. The menus are
// functional, not a reduced mirror of the former Preferences popup. All
// existing MIDI, OSC, clock and fader-range options remain accessible.
void MainComponent::showOperatorMenu(int index) {
    if (index<0 || index>OperatorNavigationLayout::overflowIndex) return;
    auto* target=operatorMenus[(size_t)index].get();
    if (!target || !target->isVisible()) return;

    // Capture device identities *when the menu opens*, not when it closes.
    // Enumeration order may change when a USB MIDI device is reconnected.
    const auto inputs=juce::MidiInput::getAvailableDevices();
    const auto outputs=juce::MidiOutput::getAvailableDevices();
    const int faderRange=faderCeilingDb;
    const bool twelve=appPreferences && appPreferences->getBoolValue("clock12Hour",false);

    const auto populate=[this,&inputs,&outputs,faderRange,twelve]
                        (juce::PopupMenu& m,int group) {
        const int base=(group+1)*100;
        switch (group) {
        case 0: // File: no duplicate Pre-flight or Diagnostics.
            m.addItem(base+1,"New show");
            m.addItem(base+2,"Open show...");
            m.addItem(base+3,"Save show");
            break;
        case 1: // Edit actions are only here (plus direct cue affordances).
            m.addItem(base+1,"Undo",undoButton.isEnabled());
            m.addItem(base+2,"Redo",redoButton.isEnabled());
            m.addSeparator();
            m.addItem(base+3,"Add cue");
            m.addItem(base+4,"Add column...");
            break;
        case 2: // Show: all operational checks in one place.
            m.addItem(base+1,"Rename show...");
            m.addSeparator();
            m.addItem(base+2,"Pre-flight...");
            m.addItem(base+3,"Diagnostics...");
            break;
        case 3: // Do not clutter a one-Cuelist project with inert actions.
            m.addItem(base+1,"New Cuelist",show.cuelists.size()<12);
            m.addItem(base+2,"Rename active Cuelist");
            if (show.cuelists.size()>1) {
                m.addSeparator();
                if (show.activeCuelist>0)
                    m.addItem(base+3,"Move active Cuelist left");
                if (show.activeCuelist+1<(int)show.cuelists.size())
                    m.addItem(base+4,"Move active Cuelist right");
                m.addItem(base+5,"Delete active Cuelist...");
            }
            break;
        case 4: { // Devices: full legacy MIDI/OSC controls, not "Settings".
            m.addItem(base+1,"Audio Devices...");
            juce::PopupMenu midiIn;
            midiIn.addSectionHeader("MIDI INPUT — ENABLE");
            midiIn.addItem(10000,"Disable main MIDI input",true,
                           show.controls.midiInputId.isEmpty());
            for (int i=0;i<inputs.size() && i<100;++i) {
                const auto& device=inputs.getReference(i);
                midiIn.addItem(10001+i,device.name,true,
                    show.controls.midiInputId==device.identifier ||
                    isAdditionalMidiInputEnabled(device.identifier));
            }
            m.addSubMenu("MIDI Inputs",midiIn);

            juce::PopupMenu midiOut;
            midiOut.addSectionHeader("MIDI OUTPUT — ENABLE");
            for (int i=0;i<outputs.size() && i<100;++i) {
                const auto& device=outputs.getReference(i);
                const bool enabled=!appPreferences ||
                    appPreferences->getBoolValue(
                        "midiOutputEnabled_"+device.identifier,true);
                midiOut.addItem(11001+i,device.name,true,enabled);
            }
            m.addSubMenu("MIDI Outputs",midiOut,!outputs.isEmpty());

            juce::PopupMenu osc;
            osc.addSectionHeader("Local IP: "+
                                 juce::IPAddress::getLocalAddress().toString());
            const auto ports=configuredOscPorts();
            osc.addItem(base+2,"Receive UDP ports: "+
                         (ports.isEmpty()?"not configured":ports));
            osc.addItem(base+3,"Disable OSC input",ports.isNotEmpty());
            m.addSubMenu("OSC Input",osc);
            m.addSeparator();
            // MIDI/OSC Learn already have dedicated always-visible status
            // buttons in the live header. Do not duplicate them here.
            m.addItem(base+4,"Control Mapping...");
            break;
        }
        case 5: { // View: old Fader Range and Clock were not removed.
            m.addItem(base+1,inspectorVisible ? "Collapse Inspector" :
                                              "Expand Inspector");
            // Collapse all remains directly on the cue header; a second
            // identical menu command only adds operator noise.
            m.addSeparator();
            juce::PopupMenu faderMenu;
            faderMenu.addItem(base+10,faderRangeMenuTitle(6),true,faderRange==6);
            faderMenu.addItem(base+11,faderRangeMenuTitle(0),true,faderRange==0);
            m.addSubMenu("Fader Range",faderMenu);
            juce::PopupMenu clockMenu;
            clockMenu.addItem(base+12,"24-hour clock",true,!twelve);
            clockMenu.addItem(base+13,"12-hour clock",true,twelve);
            m.addSubMenu("Clock",clockMenu);
            break;
        }
        case 6:
            m.addItem(base+1,"User Guide");
            m.addItem(base+2,"About The CUElist");
            break;
        default: break;
        }
    };

    juce::PopupMenu menu;
    menu.setLookAndFeel(&theme);
    if (index==OperatorNavigationLayout::overflowIndex) {
        const int first=OperatorNavigationLayout::atWidth(getWidth())
                            .visibleMenuCount;
        for (int i=first;i<OperatorNavigationLayout::menuCount;++i) {
            juce::PopupMenu sub;
            sub.setLookAndFeel(&theme);
            populate(sub,i);
            menu.addSubMenu(OperatorNavigationLayout::title(i),sub);
        }
    } else populate(menu,index);

    auto safe=juce::Component::SafePointer<MainComponent>(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(target),
        [safe,inputs,outputs](int chosen) {
            if (!safe || chosen<=0) return;
            if (chosen>=10000) {
                safe->dispatchOperatorDeviceChoice(chosen,inputs,outputs);
            } else {
                safe->dispatchOperatorMenu(chosen/100-1,chosen%100);
            }
        });
}

void MainComponent::dispatchOperatorDeviceChoice(int choice,
    const juce::Array<juce::MidiDeviceInfo>& inputs,
    const juce::Array<juce::MidiDeviceInfo>& outputs) {
    if (choice==10000) {
        selectControlInput({});
    } else if (choice>=10001 && choice<10101) {
        const int index=choice-10001;
        if (index>=inputs.size()) return;
        const auto device=inputs.getReference(index).identifier;
        if (device==show.controls.midiInputId)
            selectControlInput({});
        else if (show.controls.midiInputId.isEmpty() &&
                 !isAdditionalMidiInputEnabled(device))
            selectControlInput(device);
        else toggleAdditionalMidiInput(device);
    } else if (choice>=11001 && choice<11101) {
        const int index=choice-11001;
        if (index>=outputs.size()) return;
        const auto& device=outputs.getReference(index);
        const bool enabled=appPreferences ?
            !appPreferences->getBoolValue(
                "midiOutputEnabled_"+device.identifier,true) : false;
        if (appPreferences)
            appPreferences->setValue("midiOutputEnabled_"+device.identifier,
                                     enabled);
        engine.setMidiOutputEnabled(device.identifier,enabled);
        status="MIDI output "+device.name+
               (enabled?" enabled.":" disabled.");
        repaint();
    }
}

void MainComponent::dispatchOperatorMenu(int group,int command) {
    switch (group) {
    case 0: // File: preserve original discard confirmation and save.
        if (command==1 && newButton.onClick) newButton.onClick();
        else if (command==2 && openButton.onClick) openButton.onClick();
        else if (command==3 && saveButton.onClick) saveButton.onClick();
        break;
    case 1:
        if (command==1 && undoButton.isEnabled()) undoEdit();
        else if (command==2 && redoButton.isEnabled()) redoEdit();
        else if (command==3) addCue();
        else if (command==4)
            showColumnMenu(viewport.getScreenBounds().getCentre());
        break;
    case 2: // Show: checks appear only here.
        if (command==1) projectNameLabel.showEditor();
        else if (command==2) showPreflight();
        else if (command==3) showDiagnostics();
        break;
    case 3:
        if (command==1 && show.cuelists.size()<12) createCuelist();
        else if (command==2) renameActiveCuelist();
        else if (command==3 && show.activeCuelist>0)
            moveCuelist(show.activeCuelist,show.activeCuelist-1);
        else if (command==4 &&
                 show.activeCuelist+1<(int)show.cuelists.size())
            moveCuelist(show.activeCuelist,show.activeCuelist+1);
        else if (command==5 && show.cuelists.size()>1)
            requestDeleteCuelist(show.activeCuelist);
        break;
    case 4: // Devices: same native handlers used in the original Settings.
        if (command==1) showDevices();
        else if (command==2) {
            auto safe=juce::Component::SafePointer<MainComponent>(this);
            showOperatorTextPrompt(*this,theme,"OSC Input Ports",
                "Comma-separated UDP ports, up to 16; empty disables listening.",
                "Receive UDP ports",configuredOscPorts(),false,"Apply",
                [safe](juce::String value) {
                    if (safe) safe->configureOscInputPorts(value);
                });
        }
        else if (command==3) configureOscInputPorts("");
        else if (command==4) openControlPanel(false);
        break;
    case 5:
        if (command==1) inspectorSideTab.triggerClick();
        else if (command==10 || command==11)
            setAudioFaderCeilingDb(command==10 ? 6 : 0);
        else if (command==12 || command==13) {
            if (appPreferences)
                appPreferences->setValue("clock12Hour",command==13);
            repaint();
        }
        break;
    case 6:
        if (command==1) openUserGuide();
        else if (command==2) showAbout();
        break;
    default: break;
    }
}

void MainComponent::resized() {
    const int h=getHeight();
    const auto geometry=workspaceLayout();
    editorX=geometry.editorX;
    // Reuse the existing command callbacks from the menus, but hide their
    // duplicated sidebar hitboxes. No action or shortcut is deleted.
    for (auto* control:{&newButton,&openButton,&saveButton,
                        &deviceButton,&preferencesButton,&controlButton})
        control->setVisible(false);
    projectNameLabel.setBounds(28,122,132,34);
    refreshCuelistTabs();
    const auto nav=OperatorNavigationLayout::atWidth(getWidth());
    for (int i=0;i<=OperatorNavigationLayout::overflowIndex;++i)
        if (auto* button=operatorMenus[(size_t)i].get()) {
            const bool visible=i<nav.visibleMenuCount ||
                (i==OperatorNavigationLayout::overflowIndex &&
                 nav.visibleMenuCount<OperatorNavigationLayout::menuCount);
            button->setVisible(visible);
            if (visible) button->setBounds(nav.bounds[(size_t)i]);
        }
    // Keep existing Undo/Redo and Guide callbacks/shortcuts. Their
    // former direct header controls now live in real popup menus.
    helpButton.setVisible(false);
    undoButton.setVisible(false);
    redoButton.setVisible(false);
    const int activityX=getWidth()-365;
    // Leave separation from the I/O ACTIVITY caption and the audio card.
    globalMidiLearn.setBounds(activityX+95,10,58,20);
    globalOscLearn.setBounds(activityX+154,10,58,20);
    // Collapse-all follows the pinned header after JUCE has computed the
    // actual left scrollbar inset (see placement below).
    const bool allCollapsed=std::all_of(show.columns.begin(),show.columns.end(),
        [](const Column& col){return col.collapsed;});
    collapseAllButton.setButtonText(allCollapsed ? "Expand all" : "Collapse all");
    // A minimal edge grip: 20 x 46px instead of the oversized 26 x 86px
    // handle. The 26px collapsed rail remains unchanged to preserve all
    // pinned metrics, mixer and Timeline geometry.
    constexpr int gripWidth=20,gripHeight=46;
    const int tabX=inspectorVisible ? geometry.editorX-gripWidth/2
                                   : geometry.editorX+3;
    const int tabY=juce::jlimit(150,std::max(150,h-135),h/2-gripHeight/2);
    inspectorSideTab.setBounds(tabX,tabY,gripWidth,gripHeight);
    inspectorSideTab.setInspectorOpen(inspectorVisible);
    inspectorSideTab.setTooltip(inspectorVisible
        ? "Collapse Inspector to the right"
        : "Expand Inspector to the left");

    preflightButton.setVisible(false); diagnosticsButton.setVisible(false);
    addRowButton.setVisible(false); addColumnButton.setVisible(false);
    const bool docked=(clipEditor && !clipWindow) || (oscEditor && !oscWindow);
    // All workspace geometry comes from a single model-independent module.
    // The component continues to own editor lifetime and visibility.
    viewport.setBounds(geometry.viewport);
    utilities.setBounds(geometry.utilities);
    utilities.syncScroll(viewport.getViewPositionX());
    clipCollapseButton.setVisible(docked);
    clipCloseButton.setVisible(docked);
    dockResize.setVisible(docked && !clipCollapsed);
    if (docked && !clipCollapsed) {
        dockResize.setBounds(geometry.dockResize);
        dockResize.toFront(false);
    }
    if (docked) {
        if (oscEditor && !oscWindow) {
            // OSC dock actions have fixed slots, clear of its controls.
            // The compact arrangement uses the second toolbar row.
            const bool compact=geometry.dockEditor.getWidth()<950;
            const int x=geometry.dockEditor.getX();
            // Collapsed docks are only 36px high: their two actions
            // must return to the first row instead of covering Timeline.
            const int y=geometry.dockY+
                (clipCollapsed ? 4 : compact ? 36 : 4);
            clipCollapseButton.setBounds(
                x+(clipCollapsed ? 14 : compact ? 290 : 541),y,86,26);
            clipCloseButton.setBounds(
                x+(clipCollapsed ? 109 : compact ? 383 : 634),y,82,26);
        } else {
            clipCollapseButton.setBounds(geometry.dockCollapse);
            clipCloseButton.setBounds(geometry.dockClose);
        }
        clipCollapseButton.toFront(false); clipCloseButton.toFront(false);
        if (clipEditor && !clipWindow) {
            clipEditor->setVisible(!clipCollapsed);
            if (!clipCollapsed) clipEditor->setBounds(geometry.dockEditor);
        }
        if (oscEditor && !oscWindow) {
            oscEditor->setVisible(!clipCollapsed);
            if (!clipCollapsed) oscEditor->setBounds(geometry.dockEditor);
        }
    }
    timeline.setBounds(geometry.timeline);
    timelineResize.setBounds(geometry.timelineResize);
    timelineResize.toFront(false);
    const int x=geometry.inspectorLeft, inner=geometry.inspectorInnerWidth;
    cellTab.setVisible(false);rowTab.setVisible(false);
    rowBehaviour.setVisible(false);rowMute.setVisible(false);rowGain.setVisible(false);
    behaviour.setVisible(false);cellAutoStop.setVisible(false);
    clearButton.setVisible(false);applyButton.setVisible(false);
    playCellButton.setVisible(false);stopCellButton.setVisible(false);
    rowName.setBounds(x,225,inner,33);
    bpm.setBounds(x,293,inner,32);
    tempoButton.setBounds(x,331,inner,29);
    rowLoop.setBounds(x-3,374,inner,25);
    cellName.setBounds(x,475,inner,31);
    gainDb.setBounds(x+57,528,99,27);
    follow.setBounds(x-3,564,inner,25);
    loop.setBounds(x-3,597,inner,25);
    midiEditButton.setBounds(x,648,inner,33);
    oscEditButton.setBounds(x,648,inner,33);
    importButton.setBounds(x,691,inner,33);
    // Place notes below the existing Inspector actions at every supported
    // height, without changing the safe minimum hitboxes of those actions.
    const int notesAfterActions=geometry.scaleInspectorControl({0,728,1,25}).getBottom()+8;
    const int notesTop=std::max(notesAfterActions,geometry.inspectorPanel.getBottom()-167);
    cueNotes.setBounds(x,notesTop+27,inner,
        std::max(38,geometry.inspectorPanel.getBottom()-notesTop-38));
    // Compact inspector on smaller displays, keeping all actions reachable.
    {
        for (juce::Component* c : std::initializer_list<juce::Component*>{&rowLoop,&rowName,&bpm,&tempoButton,&cellName,&follow,&loop,&gainDb,&importButton,&midiEditButton,&oscEditButton})
            c->setBounds(geometry.scaleInspectorControl(c->getBounds()));
    }
    goButton.setBounds(14,h-282,152,48);
    panicButton.setBounds(14,h-228,152,48);
    const bool learning=quickLearnMode!=QuickLearnMode::off;
    goLearnHotspot.setBounds(goButton.getBounds());
    panicLearnHotspot.setBounds(panicButton.getBounds());
    goLearnHotspot.setLearnActive(learning);
    panicLearnHotspot.setLearnActive(learning);
    grid.setSize(geometry.gridSize.x,geometry.gridSize.y);
    // With the scrollbar on the left, JUCE also moves the content origin.
    // Shift our separately pinned header by the identical visible inset so
    // its text and hitboxes stay pixel-aligned with their cue rows.
    const int leftScrollInset=viewport.getVerticalScrollBar().isVisible()
        ? viewport.getScrollBarThickness() : 0;
    cueHeaders.setBounds(geometry.cueHeaders.translated(leftScrollInset,0));
    cueHeaders.toFront(false);
    cuelistMoveLeft.setBounds(
        cueHeaders.getX()+CueGrid::nameWidth-146,cueHeaders.getY()+10,24,22);
    cuelistMoveRight.setBounds(
        cueHeaders.getX()+CueGrid::nameWidth-116,cueHeaders.getY()+10,24,22);
    cuelistMoveLeft.toFront(false);
    cuelistMoveRight.toFront(false);
    // Place an independent 72px-high heading over every visible column.
    // Its x is the exact right edge of the pinned cue-name lane. Column
    // content scrolls horizontally beneath it, but cannot rise through it.
    const int columnHeaderX=cueHeaders.getRight();
    const int columnHeaderWidth=std::max(0,viewport.getRight()-columnHeaderX);
    columnHeaders.setBounds(columnHeaderX,viewport.getY(),
                            columnHeaderWidth,CueGrid::headerHeight);
    columnHeaders.toFront(false);
    collapseAllButton.setBounds(cueHeaders.getX()+CueGrid::nameWidth-84,
                                cueHeaders.getY()+9,77,24);
    cueMetrics.setBounds(geometry.metrics);
    cueMetrics.syncBpmEditor();
    cueMetrics.toFront(false);
    collapseAllButton.toFront(false);
    cuelistMoveLeft.toFront(false);
    cuelistMoveRight.toFront(false);
    inspectorSideTab.toFront(false);
    if (learning) {
        goLearnHotspot.toFront(false);
        panicLearnHotspot.toFront(false);
    }
    const bool learnVisible=inspectorVisible && quickLearnMode!=QuickLearnMode::off;
    quickControlTargets.setVisible(learnVisible);
    quickLearnMapSelector.setVisible(learnVisible);
    cancelQuickLearn.setVisible(learnVisible);
    if (learnVisible) {
        // Learning temporarily replaces Inspector content; no extra window.
        for (juce::Component* c:std::initializer_list<juce::Component*>{
                 &rowName,&bpm,&tempoButton,&rowLoop,&cellName,&gainDb,&follow,
                 &loop,&midiEditButton,&oscEditButton,&importButton,&cueNotes,
                 &cellAutoStop,&rowMute,&rowGain,&rowBehaviour,&behaviour})
            c->setVisible(false);
        quickLearnMapSelector.setBounds(x+4,145,inner-8,27);
        cancelQuickLearn.setBounds(x+4,179,inner-8,29);
        const int top=235;
        quickControlTargets.setBounds(x+4,top,inner-8,
            std::max(60,geometry.inspectorPanel.getBottom()-top-20));
        quickControlTargets.toFront(false);
        quickLearnMapSelector.toFront(false);
        cancelQuickLearn.toFront(false);
    }
    // Overlays must stay above the grid, pinned headings and Cuelist tabs.
    positionInlineNameEditor();
}
void MainComponent::refresh() {
    selectedRow=juce::jlimit(0,(int)show.rows.size()-1,selectedRow);
    selectedColumn=juce::jlimit(0,(int)show.columns.size()-1,selectedColumn);
    visibleClipCount=0;
    for (const auto& row:show.rows)
        for (size_t c=0;c<row.cells.size() && c<show.columns.size();++c)
            if (show.columns[c].type!=ColumnType::control) {
                const auto& cell=row.cells[c];
                if (cell.midi || cell.osc || cell.file.isNotEmpty() ||
                    clips.find(cell.id)!=clips.end()) ++visibleClipCount;
            }
    syncClipEditor(); syncOscEditor(); refreshInspector(); utilities.refresh(); resized();
    cueHeaders.repaint();
    columnHeaders.repaint();
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
    // The top 72px are occupied by a pinned heading, not usable row space.
    // Otherwise selecting the first cue while scrolled can hide it under BPM.
    if (top<viewY+CueGrid::headerHeight)
        viewY=std::max(0,top-CueGrid::headerHeight);
    else if (bottom>viewY+viewport.getViewHeight())
        viewY=bottom-viewport.getViewHeight();
    viewport.setViewPosition(viewport.getViewPositionX(),std::max(0,viewY));
    repaint(); grid.repaint(); timeline.repaint(); return true;
}
void MainComponent::changed(const juce::String& text,bool stopPlayback) {
    if (projectLoader) projectLoader->cancel(); // never overwrite newer edits
    if (stopPlayback) { engine.panic(); resetActiveMapping(); }
    engine.publish(makeRenderState(show,clips),!stopPlayback);
    dirty=true; status=text; refresh();
}
void MainComponent::openUserGuide() {
    // Keep the Markdown file as the source of truth, but display it in a
    // native non-modal JUCE window so the operator is never sent to a
    // browser or an external text editor.
    const auto directory=juce::File::getSpecialLocation(
        juce::File::currentExecutableFile).getParentDirectory();
    const auto guide=directory.getChildFile("docs")
                              .getChildFile("complete-user-guide.md");
    if (!guide.existsAsFile()) {
        message("User Guide not found next to the executable. "
                "Rebuild the application or verify that "
                "docs/complete-user-guide.md is included with the package.");
        return;
    }
    const auto contents=guide.loadFileAsString();
    if (contents.trim().isEmpty()) {
        message("The bundled User Guide is empty or could not be read.");
        return;
    }
    if (!userGuideWindow)
        userGuideWindow=std::make_unique<GuideWindow>(contents);
    else
        userGuideWindow->setMarkdown(contents);

    userGuideWindow->setVisible(true);
    userGuideWindow->toFront(true);
}

void MainComponent::showAbout() {
    using Status=OperatorReportRow::Status;
    showOperatorReport(*this,theme,"About The Cuelist",
        "Version "+juce::String(THE_CUELIST_VERSION),{
            {"AUTHORS",{
                {"Davide Salvatore","Co-author",Status::neutral},
                {"Riccardo D'Ambrosio","Co-author",Status::neutral}
            }},
            {"APPLICATION",{
                {"Copyright","2026",Status::neutral},
                {"Rights","All rights reserved",Status::neutral}
            }},
            {"LICENCE",{
                {"Software","Proprietary",Status::neutral},
                {"Public source licence","Not granted",Status::caution}
            }}
        },
        "The Cuelist's copyright notice is in LICENSE. Third-party "
        "notices (including JUCE and embedded fonts) are in NOTICE.md.");
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
    // The engine already receives the mapped value immediately. Keep the
    // actual JUCE fader in lockstep without invoking its onGainChanged hook.
    masterSlider.setGainDb(GainMeterFader::fromLinear(show.masterGain));
    status="MASTER audio gain updated."; repaint();
}
void MainComponent::setMasterPan(float pan) {
    if (!std::isfinite(pan)) return;
    const float value=juce::jlimit(-1.0f,1.0f,pan);
    if (std::abs(show.masterPan-value)<1.0e-6f) return;
    show.masterPan=value; dirty=true;
    engine.publish(makeRenderState(show,clips));
    masterPan.setValue(show.masterPan,juce::dontSendNotification);
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
    utilities.syncAudioControls(column); repaint();
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
    // Mapped changes and direct GUI edits share the same one-way UI sync.
    // A plain repaint cannot move a GainMeterFader's stored thumb position.
    utilities.syncAudioControls(column);
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
void MainComponent::stopAll() {
    engine.panic(); controlOutput.clear(); resetActiveMapping();
    status="PANIC / audio, MIDI and OSC playback stopped."; repaint();
}
bool MainComponent::keyPressed(const juce::KeyPress& key) {
    // Explicit modifier combination so normal Left/Right arrows and all
    // text editing remain untouched.
    if (!dynamic_cast<juce::TextEditor*>(
            juce::Component::getCurrentlyFocusedComponent()) &&
        key.getModifiers().isCommandDown() &&
        key.getModifiers().isShiftDown()) {
        if (key.getKeyCode()==juce::KeyPress::leftKey)
            return moveCuelist(show.activeCuelist,show.activeCuelist-1);
        if (key.getKeyCode()==juce::KeyPress::rightKey)
            return moveCuelist(show.activeCuelist,show.activeCuelist+1);
        if (key.getKeyCode()==juce::KeyPress::upKey)
            return moveCueRow(selectedRow,selectedRow-1);
        if (key.getKeyCode()==juce::KeyPress::downKey)
            return moveCueRow(selectedRow,selectedRow+1);
    }
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
void MainComponent::refreshSystemTelemetry() {
#if JUCE_WINDOWS
    // Query OS counters on the UI thread at 1 Hz only; never in the audio
    // callback or MIDI dispatch path. Kernel time includes idle time.
    FILETIME idle{},kernel{},user{};
    const auto ticks=[](const FILETIME& t)->uint64_t {
        return (uint64_t(t.dwHighDateTime)<<32) | uint64_t(t.dwLowDateTime);
    };
    if (::GetSystemTimes(&idle,&kernel,&user)) {
        const auto i=ticks(idle),k=ticks(kernel),u=ticks(user);
        if (previousKernelTime>0 && k>=previousKernelTime &&
            u>=previousUserTime && i>=previousIdleTime) {
            const uint64_t deltaTotal=k-previousKernelTime+u-previousUserTime;
            const uint64_t deltaIdle=i-previousIdleTime;
            if (deltaTotal>0)
                systemCpuPercent=(float)juce::jlimit(0.0,100.0,
                    100.0*(1.0-double(deltaIdle)/double(deltaTotal)));
        }
        previousIdleTime=i;previousKernelTime=k;previousUserTime=u;
    }
    MEMORYSTATUSEX memory{};
    memory.dwLength=sizeof(memory);
    if (::GlobalMemoryStatusEx(&memory))
        systemRamPercent=(float)memory.dwMemoryLoad;
#else
    // Unsupported hosts display N/A, never invented telemetry.
    systemCpuPercent=-1.f;systemRamPercent=-1.f;
#endif
}

void MainComponent::timerCallback() {
    const bool canUndo=editHistory.canUndo(),canRedo=editHistory.canRedo();
    if (undoButton.isEnabled()!=canUndo) undoButton.setEnabled(canUndo);
    if (redoButton.isEnabled()!=canRedo) redoButton.setEnabled(canRedo);
    // Keep system telemetry sampled at approximately one second intervals,
    // rather than polling Windows counters on each 25 Hz GUI refresh.
    if (++systemTelemetryTicks>=25) {
        systemTelemetryTicks=0;
        refreshSystemTelemetry();
    }
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
    const bool consumeMidiTick=quickLearnMode==QuickLearnMode::midi;
    const auto receiveMidi=[this,consumeMidiTick](const ControlEvent& control) {
        if (consumeMidiTick) {
            if (quickLearnFunctionId>0 && controlLearnArmed &&
                !(control.kind==ControlKind::note && control.isNoteOff)) {
                controlLearnArmed=false;completeQuickLearn(control);
            } else physicalEdges.recordAndWasDown(control);
        } else if (!consumeMidiLearnEvent(controlLearnArmed,learnedInput,control))
            dispatchControl(control);
    };
    while (processed++<256 && midiInput.poll(control)) receiveMidi(control);
    for (auto& extra:additionalMidiInputs) {
        int received=0;
        while (received++<256 && extra.second->poll(control)) receiveMidi(control);
    }

    int oscProcessed=0;
    const bool consumeOscTick=quickLearnMode==QuickLearnMode::osc;
    while (oscProcessed++<256 && oscInput.poll(control)) {
        if (consumeOscTick) {
            if (quickLearnFunctionId>0 && oscLearnArmed) {
                oscLearnArmed=false;
                completeQuickLearn(control);
            }
        } else if (oscLearnArmed) {
            learnedInput=control;oscLearnArmed=false;
        } else dispatchControl(control);
    }
    if (++midiRefreshTicks>=25) {
        midiRefreshTicks=0;
        // Reapply persisted output switches for devices connected after startup.
        if (appPreferences) for (const auto& dev:juce::MidiOutput::getAvailableDevices())
            engine.setMidiOutputEnabled(dev.identifier,
                appPreferences->getBoolValue("midiOutputEnabled_"+dev.identifier,true));
        engine.refreshMidiOutputs();
        // Take one inventory snapshot per refresh. The policy checks exact
        // device identifiers and never substitutes a similarly named port.
        const auto devices=juce::MidiInput::getAvailableDevices();
        for (auto& extra:additionalMidiInputs) {
            if (!hasExactMidiIdentifier(devices,extra.first)) {
                if (extra.second->connected()) {
                    extra.second->disconnect();
                    physicalEdges.clear();controlMapping.clearPickup();
                }
            } else if (!extra.second->connected()) {
                if (extra.second->connect(extra.first).wasOk()) {
                    physicalEdges.clear();controlMapping.clearPickup();
                }
            }
        }
        const auto firstAction=planMidiSession(show.controls.midiInputId,
            midiInput.identifier(),midiInput.connected(),devices);
        if (firstAction==MidiSessionAction::disconnectMissing) {
            midiInput.disconnect();
            physicalEdges.clear();controlMapping.clearPickup(); // stale MIDI state cannot survive unplug
            status="MIDI input disconnected. Mappings suspended.";
        }
        // Preserve the original same-tick disconnect/reconnect flow, if a
        // different explicitly selected device is already present.
        if (planMidiSession(show.controls.midiInputId,midiInput.identifier(),
                midiInput.connected(),devices)==MidiSessionAction::reconnectSelected) {
            ++midiInputReconnectAttempts;
            const auto restored=midiInput.connect(show.controls.midiInputId);
            if (restored.wasOk()) {
                physicalEdges.clear();controlMapping.clearPickup();
                status="MIDI controller reconnected (same device).";
            }
        }
    }
    // Connection status can change while a function remains selected.
    // Keep the yellow blinking target armed, but show a truthful live
    // warning until its explicitly selected MIDI input becomes available.
    if (quickLearnMode==QuickLearnMode::midi && quickLearnFunctionId>0) {
        const juce::String warning=midiInput.connected() ? juce::String() :
                                   juce::String("MIDI input not connected");
        if (quickLearnFeedback!=warning) {
            quickLearnFeedback=warning;
            repaint();
        }
    }
    // Compare monotonic counters; LEDs reflect actual RX/TX, not queued timeline events.
    const auto diag=engine.diagnostics();
    const uint64_t rx=midiInput.receivedCount();
    const uint64_t tx=diag.midiOutputSent;
    const uint64_t oscTx=static_cast<uint64_t>(diag.oscSent)+controlOutput.sentCount();
    const uint64_t oscRx=oscInput.receivedCount();
    const double now=juce::Time::getMillisecondCounterHiRes();
    if(activityInitialized) {
        if(rx>lastMidiRx) midiRxUntil=now+170.0;
        if(tx>lastMidiTx) midiTxUntil=now+170.0;
        if(oscTx>lastOscTx) oscTxUntil=now+170.0;
        if(oscRx>lastOscRx) oscRxUntil=now+170.0;
    }
    lastMidiRx=rx; lastMidiTx=tx; lastOscTx=oscTx; lastOscRx=oscRx;
    activityInitialized=true;
    // The protocol stays visibly ON between assignments, even when
    // there is no currently selected target waiting for its next event.
    globalMidiLearn.setColour(juce::TextButton::buttonColourId,
        (quickLearnMode==QuickLearnMode::midi || controlLearnArmed) ?
        palette::amber.withAlpha(.55f) : palette::raised);
    globalOscLearn.setColour(juce::TextButton::buttonColourId,
        (quickLearnMode==QuickLearnMode::osc || oscLearnArmed) ?
        palette::purple.withAlpha(.55f) : palette::raised);
    // 25 Hz UI refresh: only the armed target pulses (~1.8 Hz).
    // Other available controls retain their steady protocol-coloured tint.
    if (quickLearnFunctionId>0 && quickLearnMode!=QuickLearnMode::off &&
        ++quickLearnPulseTicks>=7) {
        quickLearnPulseTicks=0;
        quickLearnPulseBright=!quickLearnPulseBright;
        updateQuickLearnVisuals();
    }
    utilities.syncScroll(viewport.getViewPositionX()); utilities.refreshRouting();
    grid.repaint(); cueHeaders.repaint(); columnHeaders.repaint();
    displayPeak=std::max(engine.peak.load(),displayPeak*.88f);
    utilities.refreshMeters(); repaint(); timeline.repaint(); cueMetrics.repaint();
}
void MainComponent::showDevices() {
    if (deviceDialog) { deviceDialog->toFront(true); return; }
    stopAll(); auto* selector=new juce::AudioDeviceSelectorComponent(engine.devices,0,0,0,64,false,false,true,false);
    selector->setSize(600,450);
    juce::DialogWindow::LaunchOptions options; options.content.setOwned(selector); options.dialogTitle="Audio devices / playback stopped";
    options.dialogBackgroundColour=palette::panel; options.escapeKeyTriggersCloseButton=true; options.useNativeTitleBar=true;
    options.componentToCentreAround=this; options.resizable=false; deviceDialog=options.launchAsync(); deviceDialog->setLookAndFeel(&theme);
}
}
