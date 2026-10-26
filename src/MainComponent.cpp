#include "MainComponent.h"
#include "PianoRoll.h"
#include "OscEditor.h"
#include "MidiClip.h"
#include <sstream>

namespace cuelist {
namespace {
juce::String typeName(ColumnType t) { return t==ColumnType::audio ? "AUDIO" : t==ColumnType::midi ? "MIDI" : "OSC"; }
juce::String duration(double seconds) {
    int s=(int)seconds; return juce::String(s/60).paddedLeft('0',2)+":"+juce::String(s%60).paddedLeft('0',2);
}
void prompt(juce::Component* parent,const juce::String& title,const juce::String& hint,const juce::String& initial,
            bool multiline,std::function<void(juce::String)> result) {
    auto* alert=new juce::AlertWindow(title,hint,juce::MessageBoxIconType::NoIcon,parent);
    alert->addTextEditor("value",initial,{},false); auto* editor=alert->getTextEditor("value");
    if (multiline) { editor->setMultiLine(true); editor->setReturnKeyStartsNewLine(true); editor->setSize(440,180); }
    if (!multiline) editor->onReturnKey=[alert] { alert->exitModalState(1); };
    alert->addButton("Applica",1); alert->addButton("Annulla",0,juce::KeyPress(juce::KeyPress::escapeKey));
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
         &goButton,&panicButton,&deviceButton,&importButton,&midiEditButton,&oscEditButton,&playCellButton,&stopCellButton,&clearButton,&tempoButton,&applyButton,
         &cellTab,&rowTab,&rowGain,&rowBehaviour,&rowMute,&rowLoop,&rowName,&cellName,&bpm,&behaviour,&follow,&loop,&cellAutoStop,&gain,&viewport,&timeline,&utilities,
         &clipCollapseButton,&clipCloseButton}) addAndMakeVisible(c);
    for (auto* b : {&openButton,&saveButton,&newButton,&addRowButton,&addColumnButton,&goButton,&panicButton,&deviceButton,
                    &importButton,&midiEditButton,&oscEditButton,&playCellButton,&stopCellButton,&clearButton,&tempoButton,&applyButton}) b->setWantsKeyboardFocus(false);
    viewport.setViewedComponent(&grid,false); viewport.setScrollBarsShown(true,true);
    goButton.setColour(juce::TextButton::buttonColourId,palette::teal.darker(0.55f));
    panicButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff422133));
    panicButton.setColour(juce::TextButton::textColourOffId,palette::red);
    importButton.setColour(juce::TextButton::buttonColourId,palette::teal.darker(0.65f));
    behaviour.addItemList({"Retrigger","Pausa / Restart","Pausa / Resume","Continue"},1);
    gain.setSliderStyle(juce::Slider::LinearHorizontal); gain.setTextBoxStyle(juce::Slider::TextBoxRight,false,52,24);
    for (auto* slider : {&gain,&rowGain}) {
        slider->setSliderStyle(juce::Slider::LinearHorizontal); slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,72,24);
        slider->setRange(-60,12,0.1); slider->setNumDecimalPlacesToDisplay(1); slider->setTextValueSuffix(" dB"); slider->setDoubleClickReturnValue(true,0);
    }
    gain.setName("cellGainDb"); rowGain.setName("rowGainDb");
    rowBehaviour.addItemList({"Usa impostazioni celle","Retrigger","Pausa / Restart","Pausa / Resume","Continue"},1);
    cellTab.setWantsKeyboardFocus(false); rowTab.setWantsKeyboardFocus(false);
    cellTab.onClick=[this] { if (applyInspector()) { rowSettings=false; refresh(); } };
    rowTab.onClick=[this] { if (applyInspector()) { rowSettings=true; refresh(); } };
    rowName.setSelectAllWhenFocused(true); cellName.setSelectAllWhenFocused(true); bpm.setInputRestrictions(6,"0123456789.");
    rowName.setName("cueName"); cellName.setName("cellName"); bpm.setName("cueBpm");
    for (auto* editor : {&rowName,&cellName,&bpm}) editor->setEscapeAndReturnKeysConsumed(false);
    bpm.setSelectAllWhenFocused(true);
    for (auto* editor : {&rowName,&cellName,&bpm}) editor->onReturnKey=[this] { if (applyInspector()) grabKeyboardFocus(); };
    gain.onValueChange=[this] { applyInspector(); }; rowGain.onValueChange=[this] { applyInspector(); };
    for (auto* combo : {&behaviour,&rowBehaviour}) combo->onChange=[this] { applyInspector(); };
    for (auto* toggle : {&follow,&loop,&rowMute,&rowLoop}) toggle->onClick=[this] { applyInspector(); };
    cellAutoStop.setName("cellAutoStop");
    cellAutoStop.setTooltip("La cella si arresta al GO di un'altra cue solo se Auto Stop della sua riga e attivo.");
    cellAutoStop.onClick=[this] { setCellAutoStop(selectedRow,selectedColumn,cellAutoStop.getToggleState()); };
    clipCollapseButton.setName("clipEditorCollapse");
    clipCloseButton.setName("clipEditorClose");
    clipCollapseButton.setVisible(false); clipCloseButton.setVisible(false);
    clipCollapseButton.onClick=[this] {
        clipCollapsed=!clipCollapsed;
        clipCollapseButton.setButtonText(clipCollapsed ? "Espandi" : "Riduci");
        resized(); repaint();
    };
    clipCloseButton.onClick=[this] { closeMidiClip(); closeOscClip(); };
    newButton.onClick=[this] { withDiscardConfirmation([this] { closeMidiClip(); closeOscClip(); editHistory.clearUndoHistory(); stopAll(); show=makeShow(); clips.clear(); projectFile={}; selectedRow=selectedColumn=nextCue=0;
        dirty=false; engine.publish(makeRenderState(show,clips)); refresh(); status="Nuovo show. Importa i file nelle celle Audio."; }); };
    openButton.onClick=[this] { withDiscardConfirmation([this] { openProject(); }); };
    saveButton.onClick=[this] { saveProject(); };
    addRowButton.onClick=[this] { if (!applyInspector()) return; addRow(show,{}); selectedRow=nextCue=(int)show.rows.size()-1; changed("Cue aggiunta. Trasporto fermato.",true); };
    addColumnButton.onClick=[this] {
        if (!applyInspector()) return;
        juce::PopupMenu menu; menu.addItem(1,"Audio"); menu.addItem(2,"MIDI"); menu.addItem(3,"OSC / UDP");
        auto safe=juce::Component::SafePointer<MainComponent>(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&addColumnButton),[safe](int choice) {
            if (!safe || choice==0) return; auto type=(ColumnType)(choice-1);
            addColumn(safe->show,type,{});
            safe->selectedColumn=(int)safe->show.columns.size()-1; safe->changed("Colonna aggiunta. Trasporto fermato.",true);
        });
    };
    goButton.onClick=[this] { goNext(); }; panicButton.onClick=[this] { stopAll(); };
    deviceButton.onClick=[this] { showDevices(); }; importButton.onClick=[this] { importDialog(); };
    playCellButton.onClick=[this] { triggerCell(); }; stopCellButton.onClick=[this] { send(show.rows[selectedRow].cells[selectedColumn].id,false,true); };
    clearButton.onClick=[this] {
        auto& c=show.rows[selectedRow].cells[selectedColumn]; clips.erase(c.id); c.file.clear(); c.name.clear(); c.midi.reset(); c.osc.reset(); closeOscClip(); changed("Clip rimossa dalla cella. File originale conservato.",true);
    };
    midiEditButton.onClick=[this] { editMidiClip(); };
    oscEditButton.onClick=[this] { editOscClip(); };
    applyButton.onClick=[this] { applyInspector(); }; tempoButton.onClick=[this] { editTempo(); };
    importButton.setTooltip("WAV e MP3 mono/stereo. Importare ferma tutte le riproduzioni.");
    applyButton.setTooltip("Applica i parametri senza interrompere il trasporto. Invio conferma i nomi.");
    addColumnButton.setTooltip("Aggiungi colonne. Doppio click sul titolo per rinominare; frecce per riordinare.");
    if (startAudio) { auto error=engine.initialise(); if (error.isNotEmpty()) status="Audio non disponibile: "+error; }
    else status="Modalita offline: dispositivo audio non aperto.";
    engine.publish(makeRenderState(show,clips)); refresh(); startTimerHz(25);
}
MainComponent::~MainComponent() {
    stopTimer(); engine.panic(); chooser.reset();
    // The device selector holds a reference to engine.devices: destroy it first.
    closeMidiClip();
    closeOscClip();
    delete deviceDialog.getComponent();
    viewport.setViewedComponent(nullptr,false); setLookAndFeel(nullptr);
}

void MainComponent::paint(juce::Graphics& g) {
    const int w=getWidth(),h=getHeight(); g.fillAll(palette::background);
    g.setColour(palette::panel); g.fillRect(0,0,w,72); g.fillRect(0,73,180,h-73);
    g.setColour(palette::line); g.drawHorizontalLine(72,0,(float)w); g.drawVerticalLine(180,73,(float)h);
    for (int i=0;i<3;++i) { g.setColour(i==1 ? palette::teal : palette::text); g.fillRoundedRectangle(23.0f,23.0f+i*9,23.0f-i*4,4,2); }
    label(g,"The Cuelist",{60,16,150,30},23,palette::text,true);
    label(g,"SHOW CONTROL",{61,45,130,15},10,palette::muted,true);
    label(g,"SESSION",{232,25,100,22},12,palette::teal,true);
    g.setColour(palette::teal); g.fillRect(232,69,65,3);
    label(g,"0.5  /  PORTABLE",{334,25,160,22},11,palette::muted);
    g.setColour(engine.connected.load() ? palette::teal : palette::red); g.fillEllipse((float)w-367,32,6,6);
    label(g,engine.connected.load() ? "Audio disponibile" : "Audio non disponibile",{w-352,22,160,25},12,palette::muted);
    label(g,"WORKSPACE",{20,99,140,18},10,palette::muted,true);
    g.setColour(palette::teal.withAlpha(0.10f)); g.fillRoundedRectangle(12,131,156,42,6);
    g.setColour(palette::teal); g.fillRoundedRectangle(12,140,3,24,1);
    label(g,"Cuelist",{28,138,125,28},14,palette::teal,true);
    label(g,"PROGETTO",{20,242,140,18},10,palette::muted,true);
    label(g,show.name,{20,269,143,26},16,palette::text,true);
    label(g,juce::String((int)show.rows.size())+" cue  /  "+juce::String((int)show.columns.size())+" colonne",{20,301,150,22},12,palette::muted);
    label(g,hasUnsavedChanges() ? "Modifiche non salvate" : "Nessuna modifica",{20,330,153,22},11,hasUnsavedChanges() ? palette::amber : palette::muted);
    g.setColour(palette::line); g.drawHorizontalLine(383,20,160);
    label(g,"MOTORI",{20,408,140,18},10,palette::muted,true);
    label(g,"Audio WAV / MP3",{20,430,140,24},13,palette::teal);
    label(g,"MIDI / note e piano roll",{20,455,150,24},12,palette::muted);
    label(g,"OSC / UDP",{20,480,150,24},12,palette::teal);
    label(g,nextCue<(int)show.rows.size() ? "Prossima cue: "+juce::String(nextCue+1) : "Fine sequenza",{20,h-315,150,22},12,palette::teal);
    label(g,"OUTPUT",{20,h-198,130,18},10,palette::muted,true);
    g.setColour(palette::raised); g.fillRoundedRectangle(20.0f,(float)h-164,138,7,3);
    g.setColour(displayPeak>.97f ? palette::red : palette::teal); g.fillRoundedRectangle(20.0f,(float)h-164,138*displayPeak,7,3);
    label(g,juce::String(engine.activeVoices.load())+" celle in riproduzione",{20,h-146,150,22},11,palette::muted);
    label(g,"SPAZIO   Cue successiva",{20,h-92,150,20},11,palette::muted);
    label(g,"ESC          Panic",{20,h-70,150,20},11,palette::muted);
    label(g,"Sessione",{204,90,170,30},25,palette::text,true);
    label(g,"Audio, MIDI e OSC. Un'unica lista di cue.",{205,124,400,19},12,palette::muted);

    auto panel=juce::Rectangle<int>(editorX,154,260,h-201);
    g.setColour(palette::panel); g.fillRoundedRectangle(panel.toFloat(),8);
    g.setColour(palette::line); g.drawRoundedRectangle(panel.toFloat(),8,1);
    int x=editorX+16;
    label(g,"INSPECTOR",{x,169,200,18},10,palette::muted,true);
    const auto& col=show.columns[selectedColumn];
    label(g,"CUE "+juce::String(selectedRow+1).paddedLeft('0',2),{x,198,120,25},18,palette::text,true);
    label(g,typeName(col.type),{x+143,201,84,20},11,palette::type((int)col.type),true,juce::Justification::centredRight);
    g.saveState();
    if (h<890) { const float factor=(float)(h-232)/658.0f; g.addTransform(juce::AffineTransform(1,0,0,0,factor,226*(1-factor))); }
    label(g,"Nome della cue",{x,235,220,18},11,palette::muted);
    label(g,"BPM  /  quarti in 4/4",{x,303,220,18},11,palette::muted);
    g.setColour(palette::line); g.drawHorizontalLine(405,(float)x,(float)x+228);
    if (!rowSettings) {
    label(g,"CELLA  /  "+col.name,{x,420,228,20},11,palette::muted,true);
    label(g,"Attivazione",{x,490,228,18},11,palette::muted);
    if (col.type==ColumnType::audio) label(g,"Livello",{x,621,70,20},11,palette::muted);
    if (col.type==ColumnType::osc)
        label(g,"Destinazione UDP nelle utility",{x,707,228,14},10,palette::teal);
    } else {
        label(g,"IMPOSTAZIONI INTERA CUE",{x,420,228,20},11,palette::teal,true);
        label(g,"Attivazione al GO della riga",{x,450,228,20},11,palette::muted);
        label(g,"Livello audio cue (dB)",{x,580,228,20},11,palette::muted);
        label(g,"Il livello agisce sull'audio; le note MIDI usano la velocity.",{x,652,228,40},11,palette::muted);
    }
    g.restoreState();
    label(g,status,{204,h-35,w-226,25},12,palette::muted);
}
void MainComponent::resized() {
    const int w=getWidth(),h=getHeight(); editorX=w-280;
    newButton.setBounds(w-190,23,68,29); openButton.setBounds(w-116,23,46,29); saveButton.setBounds(w-64,23,52,29);
    deviceButton.setBounds(20,188,140,33);
    addRowButton.setBounds(editorX-188,98,76,33); addColumnButton.setBounds(editorX-104,98,92,33);
    const bool docked=(clipEditor && !clipWindow) || (oscEditor && !oscWindow);
    const int dockHeight=docked ? (clipCollapsed ? 36 : 208) : 0;
    const int gridHeight=std::max(235,h-438-dockHeight);
    viewport.setBounds(204,154,editorX-220,gridHeight-116);
    utilities.setBounds(204,154+gridHeight-116,editorX-220,116);
    utilities.syncScroll(viewport.getViewPositionX());
    const int dockY=168+gridHeight;
    clipCollapseButton.setVisible(docked);
    clipCloseButton.setVisible(docked);
    if (docked) {
        clipCollapseButton.setBounds(522,dockY+4,86,26);
        clipCloseButton.setBounds(615,dockY+4,82,26);
        clipCollapseButton.toFront(false); clipCloseButton.toFront(false);
        if (clipEditor && !clipWindow) {
            clipEditor->setVisible(!clipCollapsed);
            if (!clipCollapsed) clipEditor->setBounds(204,dockY,editorX-220,dockHeight);
        }
        if (oscEditor && !oscWindow) {
            oscEditor->setVisible(!clipCollapsed);
            if (!clipCollapsed) oscEditor->setBounds(204,dockY,editorX-220,dockHeight);
        }
    }
    timeline.setBounds(204,dockY+dockHeight,editorX-220,std::max(75,h-gridHeight-dockHeight-215));
    int x=editorX+16;
    cellTab.setBounds(x,198,106,27); rowTab.setBounds(x+114,198,114,27);
    rowBehaviour.setBounds(x,479,228,30); rowMute.setBounds(x,527,228,24); rowLoop.setBounds(x,553,228,24); rowGain.setBounds(x,610,228,27);
    rowName.setBounds(x,258,228,31); bpm.setBounds(x,326,228,31);
    tempoButton.setBounds(x,368,228,27); cellName.setBounds(x,450,228,29);
    behaviour.setBounds(x,512,228,30);
    follow.setBounds(x-3,556,235,24); loop.setBounds(x-3,587,230,24); gain.setBounds(x+56,619,175,26);
    cellAutoStop.setBounds(x-3,647,235,24);
    midiEditButton.setBounds(x,673,228,32);
    oscEditButton.setBounds(x,673,228,32);
    importButton.setBounds(x,722,228,33); playCellButton.setBounds(x,765,142,30); stopCellButton.setBounds(x+150,765,78,30);
    clearButton.setBounds(x,804,105,28); applyButton.setBounds(x+113,804,115,28);
    // Compact inspector on smaller displays, keeping all actions reachable.
    if (h<890) {
        const float factor=(float)(h-232)/658.0f;
        for (juce::Component* c : std::initializer_list<juce::Component*>{&rowBehaviour,&rowMute,&rowLoop,&rowGain,&rowName,&bpm,&tempoButton,&cellName,&behaviour,&follow,&loop,&cellAutoStop,&gain,&importButton,&midiEditButton,&oscEditButton,&playCellButton,&stopCellButton,&clearButton,&applyButton}) {
            auto b=c->getBounds(); b.setY(226+(int)((b.getY()-226)*factor)); b.setHeight(std::max(22,(int)(b.getHeight()*factor))); c->setBounds(b);
        }
    }
    goButton.setBounds(20,h-282,140,34); panicButton.setBounds(20,h-239,140,34);
    grid.setSize(std::max(viewport.getWidth()-14,CueGrid::nameWidth+(int)show.columns.size()*CueGrid::columnWidth+280),
                 std::max(viewport.getHeight()-14,CueGrid::headerHeight+(int)show.rows.size()*CueGrid::rowHeight));
}
void MainComponent::refresh() {
    selectedRow=juce::jlimit(0,(int)show.rows.size()-1,selectedRow);
    selectedColumn=juce::jlimit(0,(int)show.columns.size()-1,selectedColumn);
    syncClipEditor(); syncOscEditor(); refreshInspector(); utilities.refresh(); resized();
    repaint(); grid.repaint(); timeline.repaint();
}
void MainComponent::refreshInspector() {
    const auto& row=show.rows[selectedRow]; const auto& c=row.cells[selectedColumn]; const auto& col=show.columns[selectedColumn];
    engine.monitoredRowId.store(row.id);
    rowName.setTextToShowWhenEmpty("Nome della cue",palette::muted);
    rowName.setText(row.name,false); cellName.setText(c.name,false); cellName.setTextToShowWhenEmpty("Nome della cella",palette::muted);
    bpm.setText(juce::String(row.tempo.front().bpm,1),false);
    behaviour.setSelectedId((int)c.behaviour+1,juce::dontSendNotification);
    follow.setToggleState(c.followsRow,juce::dontSendNotification); loop.setToggleState(c.loop,juce::dontSendNotification);
    cellAutoStop.setToggleState(c.stopOnNext,juce::dontSendNotification);
    cellAutoStop.setTooltip(row.stopOnNext ? "ON: partecipa ad Auto Stop. OFF: continua al GO successivo."
        : "Auto Stop della cue e OFF: l'opzione della cella diventa effettiva quando la cue torna ON.");
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
    oscEditButton.setButtonText(c.osc ? "Modifica clip OSC" : "Crea clip OSC");
    midiEditButton.setButtonText(c.midi ? "Modifica piano roll" : "Crea clip MIDI");
    importButton.setVisible(!rowSettings && col.type!=ColumnType::osc);
    importButton.setButtonText(midi ? "Importa MIDI" : "Importa WAV / MP3");
    importButton.setTooltip(midi ? "File .mid/.midi: importa le note al tempo della cue." : "WAV / MP3 mono o stereo.");
    clearButton.setButtonText(col.type==ColumnType::osc ? "Rimuovi OSC" : midi ? "Rimuovi MIDI" : "Rimuovi audio");
    const bool loaded=hasClip(selectedRow,selectedColumn);
    playCellButton.setEnabled(loaded); stopCellButton.setEnabled(loaded);
    clearButton.setEnabled(loaded || c.file.isNotEmpty());
}
bool MainComponent::select(int row,int column) {
    if (row<0 || row>=(int)show.rows.size() || column<0 || column>=(int)show.columns.size() || !applyInspector()) return false;
    nextCue=row; selectedRow=row; selectedColumn=column; syncClipEditor(); syncOscEditor(); refreshInspector();
    const int top=CueGrid::headerHeight+row*CueGrid::rowHeight, bottom=top+CueGrid::rowHeight;
    int viewY=viewport.getViewPositionY();
    if (top<viewY) viewY=top; else if (bottom>viewY+viewport.getViewHeight()) viewY=bottom-viewport.getViewHeight();
    viewport.setViewPosition(viewport.getViewPositionX(),std::max(0,viewY));
    repaint(); grid.repaint(); timeline.repaint(); return true;
}
void MainComponent::changed(const juce::String& text,bool stopPlayback) { if (stopPlayback) engine.panic(); engine.publish(makeRenderState(show,clips),!stopPlayback); dirty=true; status=text; refresh(); }
bool MainComponent::inspectorDiffers() const {
    const auto& row=show.rows[selectedRow]; const auto& cell=row.cells[selectedColumn];
    if (rowName.getText().trim()!=row.name || bpm.getText()!=juce::String(row.tempo.front().bpm,1)) return true;
    if (rowMute.getToggleState()!=row.muted || rowLoop.getToggleState()!=row.loop
        || rowBehaviour.getSelectedId()!=(row.overrideBehaviour ? (int)row.behaviour+2 : 1)
        || std::abs(rowGain.getValue()-juce::Decibels::gainToDecibels(row.gain,-60.0f))>0.051) return true;
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
    cell.name=cellName.getText().trim(); cell.behaviour=(Behaviour)(behaviour.getSelectedId()-1);
    cell.followsRow=follow.getToggleState(); cell.loop=loop.getToggleState();
    if (candidate.columns[selectedColumn].type!=ColumnType::osc)
        cell.gain=juce::Decibels::decibelsToGain((float)gain.getValue(),-60.0f);
    auto result=validateShow(candidate); if (result.failed()) { message(result.getErrorMessage()); return false; }
    show=std::move(candidate); changed("Parametri applicati. Il BPM non modifica la velocita audio.");
    return true;
}
void MainComponent::message(const juce::String& text) { status=text; repaint(); juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"The Cuelist",text,"OK",this); }
bool MainComponent::memoryFits(const ClipMap& collection) const {
    int64_t bytes=0; for (const auto& item : collection) bytes+=(int64_t)item.second->samples.getNumSamples()*item.second->samples.getNumChannels()*4;
    return bytes<=512LL*1024*1024;
}
void MainComponent::importDialog() {
    if (show.columns[selectedColumn].type==ColumnType::osc) return;
    if (!applyInspector()) return;
    const int cellId=show.rows[selectedRow].cells[selectedColumn].id;
    const bool midi=show.columns[selectedColumn].type==ColumnType::midi;
    chooser=std::make_unique<juce::FileChooser>(midi ? "Importa clip MIDI (note)" : "Importa WAV o MP3 mono o stereo",juce::File{},midi ? "*.mid;*.midi" : "*.wav;*.mp3");
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
        [safe=juce::Component::SafePointer<MainComponent>(this),cellId](const juce::FileChooser& fc) {
            if (!safe || fc.getResult()==juce::File{}) return;
            for (size_t r=0;r<safe->show.rows.size();++r) for (size_t c=0;c<safe->show.columns.size();++c)
                if (safe->show.rows[r].cells[c].id==cellId) { if (safe->select((int)r,(int)c)) safe->importFile(fc.getResult()); return; }
        });
}
void MainComponent::importFile(const juce::File& file) {
    if (show.columns[selectedColumn].type==ColumnType::midi) {
        if (!applyInspector()) return;
        MidiClip imported; juce::String warning;
        auto result=loadMidiClip(file,imported,warning);
        if (result.failed()) { message(result.getErrorMessage()); return; }
        auto& cell=show.rows[selectedRow].cells[selectedColumn]; cell.midi=std::move(imported);
        cell.file.clear(); cell.name=file.getFileNameWithoutExtension();
        changed("Clip MIDI importata. Doppio clic per aprire il piano roll.",true);
        if (warning.isNotEmpty()) message(warning);
        return;
    }
    if (show.columns[selectedColumn].type!=ColumnType::audio) { message("Importa audio in una colonna Audio."); return; }
    engine.panic(); std::shared_ptr<const AudioClip> clip; auto result=loadClip(file,clip);
    if (result.failed()) { message(result.getErrorMessage()); return; }
    auto updated=clips; auto& c=show.rows[selectedRow].cells[selectedColumn]; updated[c.id]=clip;
    if (!memoryFits(updated)) { message("Limite di precaricamento raggiunto: 512 MB PCM per show. Il file non e stato aggiunto."); return; }
    clips=std::move(updated); c.file=file.getFullPathName(); c.name=file.getFileNameWithoutExtension();
    changed("Importato "+file.getFileName()+". Doppio click sulla cella per avviare.");
}
bool MainComponent::send(int id,bool row,bool stop) {
    if (!engine.connected.load()) { message("Nessun dispositivo audio disponibile. Apri Dispositivi audio."); return false; }
    if (!engine.send(id,row,stop)) { engine.panic(); message("Coda comandi piena: riproduzioni fermate. Riprovare."); return false; }
    status=stop ? "Cella fermata." : "Comando inviato al trasporto audio / MIDI."; return true;
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
        if (identifier.isEmpty()) { message("Seleziona l'uscita MIDI della colonna "+juce::String((int)c+1)+" prima del GO."); return; }
        auto ready=engine.setMidiOutput(identifier); if (ready.failed()) { message(ready.getErrorMessage()); return; }
    }
    engine.publish(makeRenderState(show,clips));
    if (send(show.rows[row].id,true)) nextCue=row+1; repaint(); grid.repaint();
}
void MainComponent::goNext() { if (nextCue<(int)show.rows.size()) triggerRow(nextCue); else { status="Fine sequenza. Seleziona una cue per ripartire."; repaint(); } }
void MainComponent::triggerCell() {
    if (!applyInspector()) return;
    auto& c=show.rows[selectedRow].cells[selectedColumn];
    if (show.columns[selectedColumn].type==ColumnType::osc && !c.osc) { editOscClip(); return; }
    if (!hasClip(selectedRow,selectedColumn)) { if (show.columns[selectedColumn].type==ColumnType::midi) editMidiClip(); else if (show.columns[selectedColumn].type==ColumnType::osc) editOscClip(); else importDialog(); return; }
    const bool playing=engine.getCellPlayback(c.id).playing;
    const bool needsOutput=c.behaviour!=Behaviour::stop && !(playing && (c.behaviour==Behaviour::pauseResume
        || c.behaviour==Behaviour::pauseRestart || c.behaviour==Behaviour::continuePlaying));
    if (show.columns[selectedColumn].type==ColumnType::midi && needsOutput) {
        const auto& identifier=show.columns[selectedColumn].midiOutputId;
        if (identifier.isEmpty()) { message("Seleziona un'uscita MIDI nelle utility della colonna."); return; }
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
    if (send(show.rows[row].id,true,true)) status="Tutte le clip della cue "+juce::String(row+1)+" fermate.";
    repaint();
}
void MainComponent::setColumnUtilities(int column,float level,float pan,bool muted,bool inverted) {
    if (column<0 || column>=(int)show.columns.size() || !std::isfinite(level) || !std::isfinite(pan)) return;
    auto& col=show.columns[column];
    col.gain=juce::jlimit(0.0f,4.0f,level); col.pan=juce::jlimit(-1.0f,1.0f,pan);
    col.muted=muted; col.phaseInverted=inverted;
    dirty=true; engine.publish(makeRenderState(show,clips));
    status="Utility colonna aggiornate."; repaint();
}
void MainComponent::setColumnOutput(int column,int outputIndex) {
    if (column<0 || column>=(int)show.columns.size() || show.columns[column].type!=ColumnType::audio
        || outputIndex<0 || outputIndex>1022) return;
    show.columns[column].output=outputIndex; dirty=true;
    engine.publish(makeRenderState(show,clips));
    status="Uscita audio della colonna aggiornata."; repaint();
}
void MainComponent::setAutoStop(int row,bool enabled) {
    if (row<0 || row>=(int)show.rows.size()) return;
    show.rows[row].stopOnNext=enabled; dirty=true; engine.publish(makeRenderState(show,clips));
    status=enabled ? "La riga si fermera al GO di un'altra cue." : "La riga continuera anche al GO di un'altra cue.";
    grid.repaint(); repaint();
}
void MainComponent::setCellAutoStop(int row,int column,bool enabled) {
    if (row<0 || row>=(int)show.rows.size() || column<0 || column>=(int)show.columns.size()) return;
    auto& cell=show.rows[(size_t)row].cells[(size_t)column];
    if (cell.stopOnNext==enabled) return;
    cell.stopOnNext=enabled;
    dirty=true;
    engine.publish(makeRenderState(show,clips));
    if (row==selectedRow && column==selectedColumn) cellAutoStop.setToggleState(enabled,juce::dontSendNotification);
    status=enabled ? "Cella inclusa in Auto Stop della cue." : "Cella esclusa da Auto Stop: continua al prossimo GO.";
    grid.repaint(); repaint();
}
void MainComponent::stopAll() { engine.panic(); status="PANIC / riproduzioni audio, MIDI e OSC fermate."; repaint(); }
bool MainComponent::keyPressed(const juce::KeyPress& key) {
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
    if (++midiRefreshTicks>=25) { midiRefreshTicks=0; engine.refreshMidiOutputs(); }
    utilities.syncScroll(viewport.getViewPositionX()); utilities.refreshRouting(); grid.repaint();
    displayPeak=std::max(engine.peak.load(),displayPeak*.88f); repaint(); timeline.repaint();
}
void MainComponent::withDiscardConfirmation(std::function<void()> action) {
    if (!hasUnsavedChanges()) { action(); return; }
    juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon)
        .withTitle("Modifiche non salvate").withMessage("Continuando perderai le modifiche non salvate. Per salvarle, annulla e premi Salva.")
        .withButton("Scarta e continua").withButton("Annulla").withAssociatedComponent(this),
        [safe=juce::Component::SafePointer<MainComponent>(this),action](int result) { if (safe && result==1) action(); });
}
void MainComponent::requestQuit() { withDiscardConfirmation([this] { stopAll(); juce::JUCEApplication::getInstance()->quit(); }); }
void MainComponent::saveProject() {
    if (!applyInspector()) return;
    chooser=std::make_unique<juce::FileChooser>("Salva lo show",projectFile==juce::File{} ? juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("Show.cuelist") : projectFile,"*.cuelist");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,
        [safe=juce::Component::SafePointer<MainComponent>(this)](const juce::FileChooser& fc) {
            if (!safe || fc.getResult()==juce::File{}) return;
            auto file=fc.getResult(); auto result=saveShow(safe->show,file);
            if (result.failed()) { safe->message(result.getErrorMessage()); return; }
            Show portable; if (loadShow(file,portable).wasOk()) safe->show=std::move(portable);
            safe->projectFile=file; safe->dirty=false; safe->status="Salvato: "+file.getFullPathName(); safe->repaint();
        });
}
void MainComponent::openProject() {
    chooser=std::make_unique<juce::FileChooser>("Apri uno show",projectFile,"*.cuelist;*.json;*.bak");
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
        [safe=juce::Component::SafePointer<MainComponent>(this)](const juce::FileChooser& fc) {
            if (!safe || fc.getResult()==juce::File{}) return;
            const auto file=fc.getResult(); if (file.getSize()>32*1024*1024) { safe->message("File progetto troppo grande (massimo 32 MB)."); return; }
            Show candidate; auto result=loadShow(file,candidate);
            if (result.failed()) { safe->message(result.getErrorMessage()); return; }
            safe->engine.panic(); ClipMap loaded; juce::StringArray issues;
            for (const auto& row : candidate.rows) for (const auto& c : row.cells) if (c.file.isNotEmpty()) {
                std::shared_ptr<const AudioClip> clip; auto read=loadClip(juce::File(c.file),clip);
                if (read.failed()) { issues.add(read.getErrorMessage()); continue; }
                loaded[c.id]=clip; if (!safe->memoryFits(loaded)) { safe->message("Show oltre 512 MB PCM: apertura annullata. Il progetto corrente resta aperto."); return; }
            }
            for (const auto& column:candidate.columns) if (column.type==ColumnType::midi && column.midiOutputId.isNotEmpty()) {
                auto opened=safe->engine.setMidiOutput(column.midiOutputId); if (opened.failed()) issues.add(opened.getErrorMessage());
            }
            safe->closeMidiClip(); safe->closeOscClip(); safe->editHistory.clearUndoHistory();
            safe->show=std::move(candidate); safe->clips=std::move(loaded); safe->projectFile=file; safe->dirty=false;
            safe->selectedRow=safe->selectedColumn=safe->nextCue=0; safe->engine.publish(makeRenderState(safe->show,safe->clips)); safe->refresh();
            safe->status="Progetto aperto: "+file.getFileName();
            if (!issues.isEmpty()) safe->message(juce::String(issues.size())+" problemi di media o uscite MIDI. Ricollega i file o seleziona un dispositivo nelle utility.\n\n"+issues.joinIntoString("\n").substring(0,1500));
        });
}
void MainComponent::showDevices() {
    if (deviceDialog) { deviceDialog->toFront(true); return; }
    stopAll(); auto* selector=new juce::AudioDeviceSelectorComponent(engine.devices,0,0,0,64,false,false,true,false);
    selector->setSize(600,450);
    juce::DialogWindow::LaunchOptions options; options.content.setOwned(selector); options.dialogTitle="Dispositivi audio / riproduzione fermata";
    options.dialogBackgroundColour=palette::panel; options.escapeKeyTriggersCloseButton=true; options.useNativeTitleBar=true;
    options.componentToCentreAround=this; options.resizable=false; deviceDialog=options.launchAsync(); deviceDialog->setLookAndFeel(&theme);
}
void MainComponent::editTempo() {
    if (!applyInspector()) return;
    stopAll();
    const int rowId=show.rows[selectedRow].id; juce::String text;
    for (const auto& p : show.rows[selectedRow].tempo) text+=juce::String(p.beat)+" "+juce::String(p.bpm)+"\n";
    prompt(this,"Mappa tempo della cue","Una riga per cambio: beat BPM. Metrica 4/4; beat in quarti, partenza da 0. Non modifica la velocita dei WAV.",text,true,
        [safe=juce::Component::SafePointer<MainComponent>(this),rowId](juce::String input) {
            if (!safe) return;
            std::vector<TempoPoint> points;
            for (const auto& line : juce::StringArray::fromLines(input)) {
                if (line.trim().isEmpty()) continue;
                std::istringstream stream(line.toStdString()); TempoPoint p; std::string extra;
                if (!(stream>>p.beat>>p.bpm) || stream>>extra) { safe->message("Formato richiesto: 0 120. Una riga per ogni cambio."); return; }
                points.push_back(p);
            }
            auto candidate=safe->show; for (auto& row : candidate.rows) if (row.id==rowId) row.tempo=points;
            auto valid=validateShow(candidate); if (valid.failed()) { safe->message(valid.getErrorMessage()); return; }
            safe->show=std::move(candidate); safe->changed("Mappa tempo salvata per questa cue. Trasporto fermato.");
        });
}
void MainComponent::renameTitle(bool column,int index) {
    if (!applyInspector()) return;
    const int id=column ? show.columns[index].id : show.rows[index].id;
    prompt(this,column ? "Rinomina colonna" : "Rinomina cue","Invio per confermare; il nome puo essere vuoto.",
        column ? show.columns[index].name : show.rows[index].name,false,
        [safe=juce::Component::SafePointer<MainComponent>(this),column,id](juce::String name) {
            if (!safe) return;
            if (column) { for (auto& c : safe->show.columns) if (c.id==id) c.name=name.trim(); }
            else { for (auto& r : safe->show.rows) if (r.id==id) r.name=name.trim(); }
            safe->changed("Titolo aggiornato.");
        });
}
void MainComponent::reorderRow(int index,int delta) {
    int target=index+delta;
    if (index<0 || index>=(int)show.rows.size() || target<0 || target>=(int)show.rows.size() || !applyInspector()) return;
    std::swap(show.rows[index],show.rows[target]); selectedRow=nextCue=target; changed("Ordine cue aggiornato.",true);
}
void MainComponent::reorderColumn(int index,int delta) {
    int target=index+delta;
    if (index<0 || index>=(int)show.columns.size() || target<0 || target>=(int)show.columns.size() || !applyInspector()) return;
    std::swap(show.columns[index],show.columns[target]);
    for (auto& row : show.rows) std::swap(row.cells[index],row.cells[target]);
    selectedColumn=target; changed("Ordine colonne aggiornato.",true);
}
void MainComponent::deleteRow(int index) {
    if (index<0 || index>=(int)show.rows.size() || !applyInspector()) return;
    for (auto& c : show.rows[index].cells) clips.erase(c.id);
    show.rows.erase(show.rows.begin()+index); if (show.rows.empty()) addRow(show,{});
    selectedRow=nextCue=std::min(index,(int)show.rows.size()-1); changed("Cue eliminata. File originali conservati.",true);
}
void MainComponent::deleteColumn(int index) {
    if (index<0 || index>=(int)show.columns.size() || !applyInspector()) return;
    for (auto& row : show.rows) { clips.erase(row.cells[index].id); row.cells.erase(row.cells.begin()+index); }
    show.columns.erase(show.columns.begin()+index); if (show.columns.empty()) addColumn(show,ColumnType::audio,{});
    selectedColumn=std::min(index,(int)show.columns.size()-1); changed("Colonna eliminata. File originali conservati.",true);
}
void MainComponent::moveClip(int sr,int sc,int tr,int tc) {
    if (!applyInspector()) return;
    auto result=moveCell(show,sr,sc,tr,tc); if (result.failed()) { status=result.getErrorMessage(); repaint(); return; }
    selectedRow=tr; selectedColumn=tc; nextCue=tr; changed("Celle scambiate.",true);
}
void MainComponent::setClipStart(int column,double seconds) {
    if (column<0 || column>=(int)show.columns.size() || !std::isfinite(seconds)) return;
    auto& cell=show.rows[selectedRow].cells[column]; seconds=juce::jlimit(0.0,86400.0,std::round(seconds*100)/100);
    if (cell.startSeconds==seconds) return;
    cell.startSeconds=seconds; dirty=true; engine.publish(makeRenderState(show,clips));
    status="Inizio clip: "+juce::String(seconds,2)+" s"; timeline.repaint(); repaint();
}
void MainComponent::seekTimeline(double seconds) {
    if (!std::isfinite(seconds)) return;
    if (!engine.seekRow(show.rows[selectedRow].id,juce::jlimit(0.0,86400.0,seconds))) { status="Coda comandi piena."; }
    timeline.repaint();
}
}
