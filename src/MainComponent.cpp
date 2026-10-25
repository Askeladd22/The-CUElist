#include "MainComponent.h"
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
    alert->addButton("Applica",1); alert->addButton("Annulla",0,juce::KeyPress(juce::KeyPress::escapeKey));
    alert->enterModalState(true,juce::ModalCallbackFunction::create([alert,result](int answer) {
        if (answer==1) result(alert->getTextEditorContents("value"));
    }),true);
}
}

MainComponent::MainComponent(bool startAudio) {
    setLookAndFeel(&theme); setWantsKeyboardFocus(true); setSize(1440,900);
    for (juce::Component* c : std::initializer_list<juce::Component*>{&openButton,&saveButton,&newButton,&addRowButton,&addColumnButton,
         &goButton,&panicButton,&deviceButton,&importButton,&playCellButton,&stopCellButton,&clearButton,&tempoButton,&applyButton,
         &rowName,&cellName,&bpm,&behaviour,&meter,&output,&follow,&loop,&gain,&viewport,&timeline}) addAndMakeVisible(c);
    for (auto* b : {&openButton,&saveButton,&newButton,&addRowButton,&addColumnButton,&goButton,&panicButton,&deviceButton,
                    &importButton,&playCellButton,&stopCellButton,&clearButton,&tempoButton,&applyButton}) b->setWantsKeyboardFocus(false);
    viewport.setViewedComponent(&grid,false); viewport.setScrollBarsShown(true,true);
    goButton.setColour(juce::TextButton::buttonColourId,palette::teal.darker(0.55f));
    panicButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff422133));
    panicButton.setColour(juce::TextButton::textColourOffId,palette::red);
    importButton.setColour(juce::TextButton::buttonColourId,palette::teal.darker(0.65f));
    behaviour.addItemList({"Retrigger","Restart","Pause / Resume","Continue","Stop"},1);
    meter.addItemList({"4/4","3/4","6/8","7/8","5/4"},1);
    gain.setSliderStyle(juce::Slider::LinearHorizontal); gain.setTextBoxStyle(juce::Slider::TextBoxRight,false,52,24);
    gain.setRange(0,100,0); gain.setNumDecimalPlacesToDisplay(1); gain.setTextValueSuffix(" %");
    rowName.setSelectAllWhenFocused(true); cellName.setSelectAllWhenFocused(true); bpm.setInputRestrictions(6,"0123456789.");
    rowName.setName("cueName"); cellName.setName("cellName"); bpm.setName("cueBpm");
    for (auto* editor : {&rowName,&cellName,&bpm}) editor->setEscapeAndReturnKeysConsumed(false);
    bpm.setSelectAllWhenFocused(true);
    newButton.onClick=[this] { withDiscardConfirmation([this] { stopAll(); show=makeShow(); clips.clear(); projectFile={}; selectedRow=selectedColumn=0;
        dirty=false; engine.publish(makeRenderState(show,clips)); refresh(); status="Nuovo show. Importa i file nelle celle Audio."; }); };
    openButton.onClick=[this] { withDiscardConfirmation([this] { openProject(); }); };
    saveButton.onClick=[this] { saveProject(); };
    addRowButton.onClick=[this] { if (!applyInspector()) return; addRow(show,"Cue "+juce::String((int)show.rows.size()+1)); selectedRow=(int)show.rows.size()-1; changed("Cue aggiunta. Trasporto fermato."); };
    addColumnButton.onClick=[this] {
        if (!applyInspector()) return;
        juce::PopupMenu menu; menu.addItem(1,"Audio"); menu.addItem(2,"MIDI (motore previsto)"); menu.addItem(3,"OSC (motore previsto)");
        auto safe=juce::Component::SafePointer<MainComponent>(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&addColumnButton),[safe](int choice) {
            if (!safe || choice==0) return; auto type=(ColumnType)(choice-1);
            addColumn(safe->show,type,typeName(type)+" "+juce::String((int)safe->show.columns.size()+1));
            safe->selectedColumn=(int)safe->show.columns.size()-1; safe->changed("Colonna aggiunta. Trasporto fermato.");
        });
    };
    goButton.onClick=[this] { triggerRow(selectedRow); }; panicButton.onClick=[this] { stopAll(); };
    deviceButton.onClick=[this] { showDevices(); }; importButton.onClick=[this] { importDialog(); };
    playCellButton.onClick=[this] { triggerCell(); }; stopCellButton.onClick=[this] { send(show.rows[selectedRow].cells[selectedColumn].id,false,true); };
    clearButton.onClick=[this] {
        auto& c=show.rows[selectedRow].cells[selectedColumn]; clips.erase(c.id); c.file.clear(); c.name.clear(); changed("Audio rimosso dalla cella. File originale conservato.");
    };
    applyButton.onClick=[this] { applyInspector(); }; tempoButton.onClick=[this] { editTempo(); };
    importButton.setTooltip("WAV mono/stereo. Importare ferma tutte le riproduzioni.");
    applyButton.setTooltip("Applica i parametri e ferma il trasporto. Anche Salva e il cambio selezione applicano i parametri modificati.");
    addColumnButton.setTooltip("Aggiungi colonne. Click sul titolo di una colonna per rinominarla o riordinarla.");
    output.setTooltip("Coppia numerata tra le uscite attive nel pannello Dispositivi audio.");
    if (startAudio) { auto error=engine.initialise(); if (error.isNotEmpty()) status="Audio non disponibile: "+error; }
    else status="Modalita offline: dispositivo audio non aperto.";
    engine.publish(makeRenderState(show,clips)); refresh(); startTimerHz(25);
}
MainComponent::~MainComponent() {
    stopTimer(); engine.panic(); chooser.reset();
    // The device selector holds a reference to engine.devices: destroy it first.
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
    label(g,"0.1  /  ANTEPRIMA",{334,25,160,22},11,palette::muted);
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
    label(g,"Audio WAV",{20,440,140,24},13,palette::teal);
    label(g,"MIDI / previsto",{20,475,150,24},12,palette::muted);
    label(g,"OSC / previsto",{20,510,150,24},12,palette::muted);
    label(g,"OUTPUT",{20,h-198,130,18},10,palette::muted,true);
    g.setColour(palette::raised); g.fillRoundedRectangle(20.0f,(float)h-164,138,7,3);
    g.setColour(displayPeak>.97f ? palette::red : palette::teal); g.fillRoundedRectangle(20.0f,(float)h-164,138*displayPeak,7,3);
    label(g,juce::String(engine.activeVoices.load())+" celle in riproduzione",{20,h-146,150,22},11,palette::muted);
    label(g,"SPAZIO   Avvia riga",{20,h-92,150,20},11,palette::muted);
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
    label(g,"BPM",{x,303,85,18},11,palette::muted); label(g,"Metrica",{x+112,303,100,18},11,palette::muted);
    g.setColour(palette::line); g.drawHorizontalLine(405,(float)x,(float)x+228);
    label(g,"CELLA  /  "+col.name,{x,420,228,20},11,palette::muted,true);
    label(g,"Attivazione",{x,490,228,18},11,palette::muted);
    label(g,"Uscite attive",{x,553,228,18},11,palette::muted);
    label(g,"Livello",{x,682,70,20},11,palette::muted);
    if (col.type!=ColumnType::audio)
        label(g,"Motore "+typeName(col.type)+" previsto",{x,707,228,14},10,palette::amber);
    g.restoreState();
    label(g,status,{204,h-35,w-226,25},12,palette::muted);
}
void MainComponent::resized() {
    const int w=getWidth(),h=getHeight(); editorX=w-280;
    newButton.setBounds(w-190,23,68,29); openButton.setBounds(w-116,23,46,29); saveButton.setBounds(w-64,23,52,29);
    deviceButton.setBounds(20,188,140,33);
    addRowButton.setBounds(editorX-188,98,76,33); addColumnButton.setBounds(editorX-104,98,92,33);
    const int gridHeight=std::max(235,h-438);
    viewport.setBounds(204,154,editorX-220,gridHeight);
    timeline.setBounds(204,168+gridHeight,editorX-220,h-gridHeight-215);
    int x=editorX+16;
    rowName.setBounds(x,258,228,31); bpm.setBounds(x,326,100,31); meter.setBounds(x+112,326,116,31);
    tempoButton.setBounds(x,368,228,27); cellName.setBounds(x,450,228,29);
    behaviour.setBounds(x,512,228,30); output.setBounds(x,575,228,30);
    follow.setBounds(x-3,619,235,24); loop.setBounds(x-3,650,230,24); gain.setBounds(x+56,680,175,26);
    importButton.setBounds(x,722,228,33); playCellButton.setBounds(x,765,142,30); stopCellButton.setBounds(x+150,765,78,30);
    clearButton.setBounds(x,804,105,28); applyButton.setBounds(x+113,804,115,28);
    // Compact inspector on smaller displays, keeping all actions reachable.
    if (h<890) {
        const float factor=(float)(h-232)/658.0f;
        for (juce::Component* c : std::initializer_list<juce::Component*>{&rowName,&bpm,&meter,&tempoButton,&cellName,&behaviour,&output,&follow,&loop,&gain,&importButton,&playCellButton,&stopCellButton,&clearButton,&applyButton}) {
            auto b=c->getBounds(); b.setY(226+(int)((b.getY()-226)*factor)); b.setHeight(std::max(22,(int)(b.getHeight()*factor))); c->setBounds(b);
        }
    }
    goButton.setBounds(20,h-282,140,34); panicButton.setBounds(20,h-239,140,34);
    grid.setSize(std::max(viewport.getWidth()-14,CueGrid::nameWidth+(int)show.columns.size()*CueGrid::columnWidth+180),
                 std::max(viewport.getHeight()-14,CueGrid::headerHeight+(int)show.rows.size()*CueGrid::rowHeight));
}
void MainComponent::refresh() { selectedRow=juce::jlimit(0,(int)show.rows.size()-1,selectedRow); selectedColumn=juce::jlimit(0,(int)show.columns.size()-1,selectedColumn); refreshInspector(); resized(); repaint(); grid.repaint(); timeline.repaint(); }
void MainComponent::refreshInspector() {
    const auto& row=show.rows[selectedRow]; const auto& c=row.cells[selectedColumn]; const auto& col=show.columns[selectedColumn];
    rowName.setText(row.name,false); cellName.setText(c.name,false); cellName.setTextToShowWhenEmpty("Nome della cella",palette::muted);
    bpm.setText(juce::String(row.tempo.front().bpm,1),false);
    auto signature=juce::String(row.tempo.front().numerator)+"/"+juce::String(row.tempo.front().denominator);
    int item=0; for (int i=0;i<meter.getNumItems();++i) if (meter.getItemText(i)==signature) item=meter.getItemId(i);
    if (!item) { item=meter.getNumItems()+1; meter.addItem(signature,item); } meter.setSelectedId(item,juce::dontSendNotification);
    behaviour.setSelectedId((int)c.behaviour+1,juce::dontSendNotification);
    follow.setToggleState(c.followsRow,juce::dontSendNotification); loop.setToggleState(c.loop,juce::dontSendNotification);
    gain.setValue(c.gain*100,juce::dontSendNotification);
    output.clear(juce::dontSendNotification); int channels=2;
    if (auto* device=engine.devices.getCurrentAudioDevice()) channels=std::max(2,device->getActiveOutputChannels().countNumberOfSetBits());
    for (int i=0;i<channels;i+=2) output.addItem(juce::String(i+1)+" / "+juce::String(i+2),i+1);
    if (col.output>=channels || col.output%2!=0) output.addItem(juce::String(col.output+1)+" / "+juce::String(col.output+2)+" (assente)",col.output+1);
    output.setSelectedId(col.output+1,juce::dontSendNotification);
    bool audio=col.type==ColumnType::audio;
    for (juce::Component* component : std::initializer_list<juce::Component*>{&importButton,&playCellButton,&stopCellButton,&clearButton,&output,&gain,&loop,&behaviour,&follow,&cellName}) component->setEnabled(audio);
    playCellButton.setEnabled(audio && clips.count(c.id)>0); stopCellButton.setEnabled(audio && clips.count(c.id)>0); clearButton.setEnabled(audio && c.file.isNotEmpty());
}
bool MainComponent::select(int row,int column) {
    if (!applyInspector()) return false;
    selectedRow=row; selectedColumn=column; refreshInspector(); repaint(); grid.repaint(); timeline.repaint(); return true;
}
void MainComponent::changed(const juce::String& text) { engine.panic(); engine.publish(makeRenderState(show,clips)); dirty=true; status=text; refresh(); }
bool MainComponent::inspectorDiffers() const {
    const auto& row=show.rows[selectedRow]; const auto& cell=row.cells[selectedColumn];
    auto signature=juce::String(row.tempo.front().numerator)+"/"+juce::String(row.tempo.front().denominator);
    if (rowName.getText().trim()!=row.name || bpm.getText()!=juce::String(row.tempo.front().bpm,1) || meter.getText()!=signature) return true;
    if (show.columns[selectedColumn].type!=ColumnType::audio) return false;
    return cellName.getText().trim()!=cell.name || behaviour.getSelectedId()!=(int)cell.behaviour+1 || follow.getToggleState()!=cell.followsRow
        || loop.getToggleState()!=cell.loop || std::abs(gain.getValue()-cell.gain*100)>0.01 || output.getSelectedId()!=show.columns[selectedColumn].output+1;
}
bool MainComponent::applyInspector() {
    if (!inspectorDiffers()) return true;
    auto candidate=show; auto& row=candidate.rows[selectedRow]; auto& cell=row.cells[selectedColumn];
    row.name=rowName.getText().trim();
    if (bpm.getText()!=juce::String(row.tempo.front().bpm,1)) row.tempo.front().bpm=bpm.getText().getDoubleValue();
    auto sig=juce::StringArray::fromTokens(meter.getText(),"/","");
    row.tempo.front().numerator=sig[0].getIntValue(); row.tempo.front().denominator=sig[1].getIntValue();
    if (candidate.columns[selectedColumn].type==ColumnType::audio) {
        cell.name=cellName.getText().trim(); cell.behaviour=(Behaviour)(behaviour.getSelectedId()-1);
        cell.followsRow=follow.getToggleState(); cell.loop=loop.getToggleState(); cell.gain=(float)gain.getValue()/100;
        candidate.columns[selectedColumn].output=output.getSelectedId()-1;
    }
    auto result=validateShow(candidate); if (result.failed()) { message(result.getErrorMessage()); return false; }
    show=std::move(candidate); changed("Parametri applicati. Trasporto fermato; il BPM non modifica la velocita audio.");
    return true;
}
void MainComponent::message(const juce::String& text) { status=text; repaint(); juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"The Cuelist",text,"OK",this); }
bool MainComponent::memoryFits(const ClipMap& collection) const {
    int64_t bytes=0; for (const auto& item : collection) bytes+=(int64_t)item.second->samples.getNumSamples()*item.second->samples.getNumChannels()*4;
    return bytes<=512LL*1024*1024;
}
void MainComponent::importDialog() {
    if (show.columns[selectedColumn].type!=ColumnType::audio) return;
    if (!applyInspector()) return;
    const int cellId=show.rows[selectedRow].cells[selectedColumn].id;
    chooser=std::make_unique<juce::FileChooser>("Importa un WAV mono o stereo",juce::File{},"*.wav");
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
        [safe=juce::Component::SafePointer<MainComponent>(this),cellId](const juce::FileChooser& fc) {
            if (!safe || fc.getResult()==juce::File{}) return;
            for (size_t r=0;r<safe->show.rows.size();++r) for (size_t c=0;c<safe->show.columns.size();++c)
                if (safe->show.rows[r].cells[c].id==cellId) { if (safe->select((int)r,(int)c)) safe->importFile(fc.getResult()); return; }
        });
}
void MainComponent::importFile(const juce::File& file) {
    if (show.columns[selectedColumn].type!=ColumnType::audio) { message("Importa i WAV in una colonna Audio."); return; }
    engine.panic(); std::shared_ptr<const AudioClip> clip; auto result=loadClip(file,clip);
    if (result.failed()) { message(result.getErrorMessage()); return; }
    auto updated=clips; auto& c=show.rows[selectedRow].cells[selectedColumn]; updated[c.id]=clip;
    if (!memoryFits(updated)) { message("Limite di precaricamento raggiunto: 512 MB PCM per show. Il file non e stato aggiunto."); return; }
    clips=std::move(updated); c.file=file.getFullPathName(); c.name=file.getFileNameWithoutExtension();
    changed("Importato "+file.getFileName()+". Doppio click sulla cella per avviare.");
}
void MainComponent::send(int id,bool row,bool stop) {
    if (!engine.connected.load()) { message("Nessun dispositivo audio disponibile. Apri Dispositivi audio."); return; }
    if (!engine.send(id,row,stop)) { engine.panic(); message("Coda comandi piena: riproduzioni fermate. Riprovare."); return; }
    status=stop ? "Cella fermata." : "Comando inviato al motore audio.";
}
void MainComponent::triggerRow(int row) { if (!select(row,selectedColumn)) return; send(show.rows[row].id,true); }
void MainComponent::triggerCell() {
    if (!applyInspector()) return;
    auto& c=show.rows[selectedRow].cells[selectedColumn];
    if (show.columns[selectedColumn].type!=ColumnType::audio) { message("Il motore MIDI/OSC e previsto in un incremento successivo."); return; }
    if (!clips.count(c.id)) { importDialog(); return; } send(c.id,false);
}
void MainComponent::stopAll() { engine.panic(); status="PANIC / tutte le riproduzioni audio fermate."; repaint(); }
bool MainComponent::keyPressed(const juce::KeyPress& key) {
    if (key==juce::KeyPress::escapeKey) { stopAll(); return true; }
    if (key.getModifiers().isCommandDown() && key.getKeyCode()=='S') { saveProject(); return true; }
    if (key==juce::KeyPress::spaceKey && !dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent())) { triggerRow(selectedRow); return true; }
    if (key==juce::KeyPress::downKey) { select(std::min(selectedRow+1,(int)show.rows.size()-1),selectedColumn); return true; }
    if (key==juce::KeyPress::upKey) { select(std::max(0,selectedRow-1),selectedColumn); return true; } return false;
}
void MainComponent::timerCallback() { engine.collectRetired(); displayPeak=std::max(engine.peak.load(),displayPeak*.88f); repaint(); }
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
    chooser=std::make_unique<juce::FileChooser>("Salva lo show",projectFile==juce::File{} ? juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Show_001.cuelist") : projectFile,"*.cuelist");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,
        [safe=juce::Component::SafePointer<MainComponent>(this)](const juce::FileChooser& fc) {
            if (!safe || fc.getResult()==juce::File{}) return;
            auto file=fc.getResult(); auto result=saveShow(safe->show,file);
            if (result.failed()) { safe->message(result.getErrorMessage()); return; }
            safe->projectFile=file; safe->dirty=false; safe->status="Salvato: "+file.getFullPathName(); safe->repaint();
        });
}
void MainComponent::openProject() {
    chooser=std::make_unique<juce::FileChooser>("Apri uno show",projectFile,"*.cuelist;*.json");
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
        [safe=juce::Component::SafePointer<MainComponent>(this)](const juce::FileChooser& fc) {
            if (!safe || fc.getResult()==juce::File{}) return;
            const auto file=fc.getResult(); if (file.getSize()>32*1024*1024) { safe->message("File progetto troppo grande (massimo 32 MB)."); return; }
            Show candidate; auto result=decodeShow(file.loadFileAsString(),candidate);
            if (result.failed()) { safe->message(result.getErrorMessage()); return; }
            safe->engine.panic(); ClipMap loaded; juce::StringArray issues;
            for (const auto& row : candidate.rows) for (const auto& c : row.cells) if (c.file.isNotEmpty()) {
                std::shared_ptr<const AudioClip> clip; auto read=loadClip(juce::File(c.file),clip);
                if (read.failed()) { issues.add(read.getErrorMessage()); continue; }
                loaded[c.id]=clip; if (!safe->memoryFits(loaded)) { safe->message("Show oltre 512 MB PCM: apertura annullata. Il progetto corrente resta aperto."); return; }
            }
            safe->show=std::move(candidate); safe->clips=std::move(loaded); safe->projectFile=file; safe->dirty=false;
            safe->selectedRow=safe->selectedColumn=0; safe->engine.publish(makeRenderState(safe->show,safe->clips)); safe->refresh();
            safe->status="Progetto aperto: "+file.getFileName();
            if (!issues.isEmpty()) safe->message(juce::String(issues.size())+" file non caricati. Le celle sono evidenziate in rosso: usa Importa WAV per ricollegarle.\n\n"+issues.joinIntoString("\n").substring(0,1500));
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
    for (const auto& p : show.rows[selectedRow].tempo) text+=juce::String(p.beat)+" "+juce::String(p.bpm)+" "+juce::String(p.numerator)+"/"+juce::String(p.denominator)+"\n";
    prompt(this,"Mappa tempo della cue","Una riga per cambio: beat BPM metrica. Beat in quarti, partenza da 0. Non modifica la velocita dei WAV.",text,true,
        [safe=juce::Component::SafePointer<MainComponent>(this),rowId](juce::String input) {
            if (!safe) return;
            std::vector<TempoPoint> points;
            for (const auto& line : juce::StringArray::fromLines(input)) {
                if (line.trim().isEmpty()) continue;
                std::istringstream stream(line.toStdString()); TempoPoint p; char slash=0; std::string extra;
                if (!(stream>>p.beat>>p.bpm>>p.numerator>>slash>>p.denominator) || slash!='/' || stream>>extra) { safe->message("Formato richiesto: 0 120 4/4. Una riga per ogni cambio."); return; }
                points.push_back(p);
            }
            auto candidate=safe->show; for (auto& row : candidate.rows) if (row.id==rowId) row.tempo=points;
            auto valid=validateShow(candidate); if (valid.failed()) { safe->message(valid.getErrorMessage()); return; }
            safe->show=std::move(candidate); safe->changed("Mappa tempo salvata per questa cue. Trasporto fermato.");
        });
}
void MainComponent::columnMenu(int column) {
    if (!select(selectedRow,column)) return; juce::PopupMenu menu;
    menu.addSectionHeader(show.columns[column].name); menu.addItem(1,"Rinomina"); menu.addItem(2,"Sposta a sinistra",column>0);
    menu.addItem(3,"Sposta a destra",column+1<(int)show.columns.size()); menu.addSeparator(); menu.addItem(4,"Elimina colonna...",show.columns.size()>1);
    const int id=show.columns[column].id;
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&grid),[safe=juce::Component::SafePointer<MainComponent>(this),id](int choice) {
        if (!safe || !choice) return;
        int index=-1; for (int i=0;i<(int)safe->show.columns.size();++i) if (safe->show.columns[i].id==id) index=i; if (index<0) return;
        if (choice==1) {
            safe->stopAll();
            prompt(safe,"Rinomina colonna","Nome della colonna",safe->show.columns[index].name,false,[safe,id](juce::String name) {
                if (!safe || name.trim().isEmpty()) return; for (auto& c : safe->show.columns) if (c.id==id) c.name=name.trim(); safe->changed("Colonna rinominata.");
            });
        } else if (choice==2 || choice==3) {
            int target=index+(choice==2 ? -1 : 1); if (target<0 || target>=(int)safe->show.columns.size()) return;
            std::swap(safe->show.columns[index],safe->show.columns[target]); for (auto& row : safe->show.rows) std::swap(row.cells[index],row.cells[target]);
            safe->selectedColumn=target; safe->changed("Ordine colonne aggiornato. Trasporto fermato.");
        } else {
            juce::AlertWindow::showAsync(juce::MessageBoxOptions().withTitle("Elimina colonna").withMessage("Eliminare la colonna e le sue celle? I file originali vengono conservati.").withButton("Elimina").withButton("Annulla").withAssociatedComponent(safe),
                [safe,id](int answer) { if (!safe || answer!=1 || safe->show.columns.size()<=1) return;
                    for (size_t i=0;i<safe->show.columns.size();++i) if (safe->show.columns[i].id==id) {
                        for (auto& row : safe->show.rows) { safe->clips.erase(row.cells[i].id); row.cells.erase(row.cells.begin()+i); }
                        safe->show.columns.erase(safe->show.columns.begin()+i); safe->changed("Colonna eliminata."); break;
                    }
                });
        }
    });
}
void MainComponent::rowMenu(int row) {
    if (!select(row,selectedColumn)) return; juce::PopupMenu menu; menu.addItem(1,"Sposta su",row>0); menu.addItem(2,"Sposta giu",row+1<(int)show.rows.size());
    menu.addItem(3,"Elimina cue...",show.rows.size()>1); const int id=show.rows[row].id;
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&grid),[safe=juce::Component::SafePointer<MainComponent>(this),id](int choice) {
        if (!safe || !choice) return; int index=-1;
        for (int i=0;i<(int)safe->show.rows.size();++i) if (safe->show.rows[i].id==id) index=i; if (index<0) return;
        if (choice==1 || choice==2) {
            int target=index+(choice==1 ? -1 : 1); if (target<0 || target>=(int)safe->show.rows.size()) return;
            std::swap(safe->show.rows[index],safe->show.rows[target]); safe->selectedRow=target; safe->changed("Ordine cue aggiornato.");
        } else {
            juce::AlertWindow::showAsync(juce::MessageBoxOptions().withTitle("Elimina cue").withMessage("Eliminare questa cue e le sue celle? I file originali vengono conservati.").withButton("Elimina").withButton("Annulla").withAssociatedComponent(safe),
                [safe,id](int answer) { if (!safe || answer!=1 || safe->show.rows.size()<=1) return;
                    for (size_t i=0;i<safe->show.rows.size();++i) if (safe->show.rows[i].id==id) {
                        for (auto& c : safe->show.rows[i].cells) safe->clips.erase(c.id);
                        safe->show.rows.erase(safe->show.rows.begin()+i); safe->changed("Cue eliminata."); break;
                    }
                });
        }
    });
}

void CueGrid::paint(juce::Graphics& g) {
    g.fillAll(palette::panel); g.setColour(palette::raised); g.fillRect(0,0,getWidth(),headerHeight);
    label(g,"CUE",{18,10,100,19},10,palette::muted,true); label(g,"Nome / scena",{56,35,145,22},12,palette::text);
    const auto& show=owner.show;
    for (int c=0;c<(int)show.columns.size();++c) {
        int x=nameWidth+c*columnWidth; auto colour=palette::type((int)show.columns[c].type);
        g.setColour(colour); g.fillRect(x,0,columnWidth-1,3);
        label(g,typeName(show.columns[c].type),{x+12,12,columnWidth-24,18},10,colour,true);
        label(g,show.columns[c].name,{x+12,35,columnWidth-24,22},13,palette::text,true);
        g.setColour(palette::line); g.drawVerticalLine(x,0,(float)getHeight());
    }
    int tail=nameWidth+(int)show.columns.size()*columnWidth;
    label(g,"BPM",{tail+12,31,70,24},11,palette::muted,true); label(g,"METRICA",{tail+88,31,80,24},11,palette::muted,true);
    auto visible=g.getClipBounds(); int first=std::max(0,(visible.getY()-headerHeight)/rowHeight);
    int last=std::min((int)show.rows.size(),(visible.getBottom()-headerHeight)/rowHeight+1);
    for (int r=first;r<last;++r) {
        int y=headerHeight+r*rowHeight; bool selected=r==owner.selectedRow;
        g.setColour(selected ? palette::teal.withAlpha(.06f) : r%2 ? palette::background.withAlpha(.4f) : palette::panel);
        g.fillRect(0,y,getWidth(),rowHeight);
        if (selected) { g.setColour(palette::teal); g.fillRect(0,y,3,rowHeight); }
        juce::Path triangle; triangle.addTriangle(15,(float)y+20,15,(float)y+33,25,(float)y+26.5f);
        g.setColour(selected ? palette::teal : palette::muted); g.fillPath(triangle);
        label(g,juce::String(r+1).paddedLeft('0',2),{36,y+17,28,22},11,palette::muted);
        label(g,show.rows[r].name,{70,y+10,nameWidth-78,34},13,selected ? palette::text : palette::muted,selected);
        for (int c=0;c<(int)show.columns.size();++c) {
            int x=nameWidth+c*columnWidth; const auto& cell=show.rows[r].cells[c]; const auto type=show.columns[c].type;
            const bool loaded=owner.clips.count(cell.id)>0, missing=cell.file.isNotEmpty() && !loaded;
            auto colour=missing ? palette::red : palette::type((int)type);
            juce::Rectangle<float> box((float)x+5,(float)y+6,(float)columnWidth-10,(float)rowHeight-12);
            g.setColour(loaded ? colour.withAlpha(.16f) : palette::background.withAlpha(.5f)); g.fillRoundedRectangle(box,5);
            g.setColour(selected && c==owner.selectedColumn ? colour : palette::line.withAlpha(.65f)); g.drawRoundedRectangle(box,5,selected && c==owner.selectedColumn ? 1.5f : .6f);
            if (loaded) {
                for (int n=0;n<6;++n) { float bar=6.0f+(float)((n*7)%15); g.setColour(colour); g.fillRect((float)x+14+n*3,(float)y+27-bar/2,1.5f,bar); }
                label(g,cell.name.isEmpty() ? "WAV" : cell.name,{x+39,y+11,columnWidth-52,22},12,palette::text);
                label(g,cell.loop ? "LOOP" : duration(owner.clips.at(cell.id)->samples.getNumSamples()/owner.clips.at(cell.id)->sampleRate),{x+39,y+30,columnWidth-52,13},9,colour);
                if (!cell.followsRow) label(g,"IND",{x+columnWidth-35,y+29,28,14},8,palette::amber);
            } else label(g,missing ? "! File non disponibile" : type==ColumnType::audio ? "+  WAV" : "previsto",{x+12,y+15,columnWidth-24,24},11,missing ? palette::red : palette::muted,false,juce::Justification::centred);
        }
        auto tempo=show.rows[r].tempo.front();
        label(g,juce::String(tempo.bpm,1),{tail+12,y+14,70,24},12,palette::muted);
        label(g,juce::String(tempo.numerator)+"/"+juce::String(tempo.denominator),{tail+88,y+14,70,24},12,palette::muted);
        g.setColour(palette::line.withAlpha(.6f)); g.drawHorizontalLine(y+rowHeight-1,0,(float)getWidth());
    }
}
void CueGrid::mouseDown(const juce::MouseEvent& e) {
    owner.grabKeyboardFocus(); int column=(e.x-nameWidth)/columnWidth;
    if (e.y<headerHeight) { if (e.x>=nameWidth && column<(int)owner.show.columns.size()) owner.columnMenu(column); return; }
    int row=(e.y-headerHeight)/rowHeight; if (row<0 || row>=(int)owner.show.rows.size()) return;
    if (e.x<nameWidth) { if (e.mods.isPopupMenu()) owner.rowMenu(row); else if (e.x<32) owner.triggerRow(row); else owner.select(row,owner.selectedColumn); }
    else if (column>=0 && column<(int)owner.show.columns.size()) owner.select(row,column);
}
void CueGrid::mouseDoubleClick(const juce::MouseEvent& e) { if (e.y>=headerHeight && e.x>=nameWidth && e.x<nameWidth+(int)owner.show.columns.size()*columnWidth && (e.y-headerHeight)/rowHeight<(int)owner.show.rows.size()) owner.triggerCell(); }
bool CueGrid::isInterestedInFileDrag(const juce::StringArray& files) { return files.size()==1 && files[0].endsWithIgnoreCase(".wav"); }
void CueGrid::filesDropped(const juce::StringArray& files,int x,int y) {
    if (x<nameWidth || y<headerHeight) return; int row=(y-headerHeight)/rowHeight,column=(x-nameWidth)/columnWidth;
    if (row>=(int)owner.show.rows.size() || column>=(int)owner.show.columns.size()) return;
    if (owner.select(row,column)) owner.importFile(juce::File(files[0]));
}
void Timeline::paint(juce::Graphics& g) {
    g.fillAll(palette::panel); const auto& row=owner.show.rows[owner.selectedRow];
    label(g,"TIMELINE",{16,11,100,23},11,palette::teal,true);
    label(g,row.name+"  /  durate audio",{123,11,getWidth()-140,23},12,palette::muted);
    g.setColour(palette::line); g.drawHorizontalLine(45,0,(float)getWidth());
    double length=10; int count=0;
    for (const auto& cell : row.cells) { auto it=owner.clips.find(cell.id); if (it!=owner.clips.end()) { length=std::max(length,it->second->samples.getNumSamples()/it->second->sampleRate); ++count; } }
    const int start=140,area=getWidth()-start-20;
    for (int i=0;i<=4;++i) {
        int x=start+area*i/4; label(g,duration(length*i/4),{x-10,53,55,18},10,palette::muted);
        g.setColour(palette::line.withAlpha(.6f)); g.drawVerticalLine(x,76,(float)getHeight()-12);
    }
    int track=0;
    for (size_t c=0;c<row.cells.size();++c) {
        if (owner.show.columns[c].type!=ColumnType::audio) continue;
        int y=82+track++*42; if (y+32>getHeight()) break;
        label(g,owner.show.columns[c].name,{16,y,115,30},12,palette::muted);
        auto it=owner.clips.find(row.cells[c].id); if (it==owner.clips.end()) continue;
        const auto& audio=it->second->samples; double seconds=audio.getNumSamples()/it->second->sampleRate;
        const int width=std::max(4,(int)(area*seconds/length));
        g.setColour(palette::teal.withAlpha(.12f)); g.fillRoundedRectangle((float)start,(float)y,(float)width,31,4);
        g.setColour(palette::teal.withAlpha(.35f));
        for (int px=0;px<width;px+=3) {
            int sample=juce::jlimit(0,audio.getNumSamples()-1,(int)((double)px/width*audio.getNumSamples()));
            float amp=std::min(1.0f,std::abs(audio.getSample(0,sample)))*24;
            g.fillRect((float)start+px,(float)y+15-amp/2,1.0f,std::max(1.0f,amp));
        }
        label(g,row.cells[c].name,{start+9,y+5,width-16,20},11,palette::text);
    }
    if (count==0) label(g,"Importa un WAV per visualizzare la durata della cue.",{140,83,getWidth()-160,35},12,palette::muted);
    label(g,"TEMPO DELLA CUE   "+juce::String(row.tempo.front().bpm,1)+" BPM     /     "+juce::String((int)row.tempo.size())+" punti tempo     /     Audio a velocita originale",
          {16,getHeight()-28,getWidth()-30,20},10,palette::muted);
}
}
