#include "MainComponent.h"
#include <cmath>

namespace cuelist {

void MainComponent::editControlSet(int row,int column) {
    if (!select(row,column) || show.columns[(size_t)column].type!=ColumnType::control) return;
    openControlPanel(true);
}

class ControlPanel final : public juce::Component, private juce::Timer,
                           private juce::ListBoxModel {
public:
    explicit ControlPanel(MainComponent& main,bool useSet):owner(&main),
        editSet(useSet),sourceRow(main.selectedRow),sourceColumn(main.selectedColumn),
        sourceRowId(main.show.rows[(size_t)main.selectedRow].id),
        sourceColumnId(main.show.columns[(size_t)main.selectedColumn].id) {
        setName("controlMappingPanel");
        for (juce::Component* c : std::initializer_list<juce::Component*>{
            &device,&connect,&learn,&add,&remove,&newPreset,&assignPreset,
            &presetName,&presets,&scope,&target,&takeover,&columns,&bindingList,&state,
            &outputCc,&oscAddress,&invert,&updateSelected,&curve,&minimum,&maximum,
             &effectivePreview,&templateName,&templates,&saveTemplate,&loadTemplate})
            addAndMakeVisible(c);
        device.setName("controlInputDevice");
        device.setTooltip("Select a device explicitly. No automatic fallback on disconnect.");
        connect.setButtonText("Refresh ports");
        learn.setButtonText("MIDI Learn");
        add.setButtonText("Add mapping");
        updateSelected.setButtonText("Update selected");
        updateSelected.setName("mappingUpdateSelected");
        updateSelected.setEnabled(false);
        updateSelected.onClick=[this] { createBinding(true); };
        curve.setName("mappingResponseCurve");
        curve.addItemList({"Linear","Ease in","Ease out"},1);
        curve.setSelectedId(1);
        minimum.setName("mappingMinimum");
        maximum.setName("mappingMaximum");
        minimum.setText("0"); maximum.setText("1");
        effectivePreview.setName("effectiveControlPreview");
        effectivePreview.setMultiLine(true);
        effectivePreview.setReadOnly(true);
        effectivePreview.setScrollbarsShown(true);
        effectivePreview.setVisible(editSet);
        templateName.setVisible(editSet); templates.setVisible(editSet);
        saveTemplate.setVisible(editSet); loadTemplate.setVisible(editSet);
        templateName.setTextToShowWhenEmpty("Reusable Set name",palette::muted);
        templateName.setName("controlTemplateName");
        templates.setName("controlTemplates");
        saveTemplate.setName("saveControlTemplate");
        loadTemplate.setName("loadControlTemplate");
        saveTemplate.setButtonText("Save Set");
        loadTemplate.setButtonText("Apply Set");
        saveTemplate.onClick=[this] {
            if (!owner) return;
            int row=-1,col=-1;
            if (!resolveSource(row,col)) return;
            const int id=owner->saveControlSetTemplate(row,col,templateName.getText());
            if (id) {
                refreshTemplates();templates.setSelectedId(id+1,juce::dontSendNotification);
                state.setText("Control Set saved for reuse.",juce::dontSendNotification);
            } else state.setText(owner->status,juce::dontSendNotification);
        };
        loadTemplate.onClick=[this] {
            if (!owner || templates.getSelectedId()<2) return;
            int row=-1,col=-1;
            if (!resolveSource(row,col)) return;
            if (owner->applyControlSetTemplate(row,col,templates.getSelectedId()-1)) {
                selected=-1;bindingList.deselectAllRows();
                bindingList.updateContent();bindingList.repaint();
                updateSelected.setEnabled(false);
                updateEffectivePreview();
                state.setText("Reusable Set applied.",juce::dontSendNotification);
            } else state.setText(owner->status,juce::dontSendNotification);
        };
        effectivePreview.setColour(juce::TextEditor::outlineColourId,palette::line);
        remove.setButtonText("Remove selected");
        newPreset.setButtonText("New preset");
        assignPreset.setButtonText("Assign to cue");
        presetName.setTextToShowWhenEmpty("New preset name",palette::muted);
        state.setName("mappingStatus");
        state.setJustificationType(juce::Justification::centredLeft);
        scope.addItemList({"Global","Column","Preset","Single cue"},1);
        scope.setSelectedId(1);
        scope.setVisible(!editSet);
        presets.setVisible(!editSet); newPreset.setVisible(!editSet);
        assignPreset.setVisible(!editSet); presetName.setVisible(!editSet);
        target.addItemList({"Master Audio","Audio column gain","GO next cue","PANIC",
                            "MIDI CC","OSC Float","Audio column pan","Master pan"},1);
        target.setSelectedId(1);
        target.onChange=[this] {
            refreshColumns();
            const int t=target.getSelectedId();
            minimum.setText(t==7 || t==8 ? "-1" : "0",false);
            maximum.setText(t==5 ? "127" : "1",false);
        };
        outputCc.setName("mappingOutputCc");
        outputCc.setText("20"); outputCc.setInputRestrictions(3,"0123456789");
        oscAddress.setName("mappingOscAddress");
        oscAddress.setText("/control/value");
        invert.setName("mappingInvert");
        takeover.addItemList({"Pickup","Jump"},1);
        takeover.setSelectedId(1);
        bindingList.setName("mappingBindings");
        bindingList.setModel(this); bindingList.setRowHeight(28);
        device.onChange=[this] {
            if (!owner) return;
            auto id=juce::String();
            int index=device.getSelectedId()-2;
            if (index>=0 && index<(int)deviceIds.size()) id=deviceIds[(size_t)index];
            auto result=owner->selectControlInput(id);
            state.setText(result.wasOk() ? "Device selected." : result.getErrorMessage(),
                juce::dontSendNotification);
        };
        connect.onClick=[this]{
            refreshDevices();
            if (owner && owner->show.controls.midiInputId.isNotEmpty())
                owner->selectControlInput(owner->show.controls.midiInputId);
        };
        learn.onClick=[this]{
            if (!owner || !owner->midiInput.connected()) {
                state.setText("Connect a MIDI input.",juce::dontSendNotification); return;
            }
            captured.reset(); owner->armControlLearn();
            state.setText(selected>=0 && currentBindings() && selected<(int)currentBindings()->size() ?
                "MIDI Learn for mapping #"+juce::String((*currentBindings())[(size_t)selected].id)+
                    " — move its hardware control, then click Update selected." :
                "MIDI Learn for NEW mapping — move a controller control.",juce::dontSendNotification);
        };
        add.onClick=[this]{ createBinding(false); };
        remove.onClick=[this]{
            const auto* bindings=currentBindings();
            if (!owner || !bindings || selected<0 || selected>=(int)bindings->size()) return;
            const int id=(*bindings)[(size_t)selected].id;
            int row=-1,column=-1;
            const bool removed=editSet ?
                (resolveSource(row,column) && owner->deleteControlSetBinding(row,column,id)) :
                owner->deleteMappingBinding(id);
            if (removed) {
                selected=-1; bindingList.updateContent(); bindingList.repaint();
            }
        };
        newPreset.onClick=[this]{
            if (!owner) return;
            const int id=owner->createMappingPreset(presetName.getText());
            if (id==0) {state.setText("Invalid preset name.",juce::dontSendNotification);return;}
            refreshPresets(); presets.setSelectedId(id+1);
            presetName.clear();
        };
        assignPreset.onClick=[this]{
            if (!owner) return;
            if (owner->assignMappingPreset(owner->show.rows[(size_t)owner->selectedRow].id,
                                           std::max(0,presets.getSelectedId()-1)))
                state.setText("Preset assigned to selected cue.",juce::dontSendNotification);
        };
        refreshDevices(); refreshPresets(); refreshColumns();refreshTemplates();
        updateEffectivePreview();
        setSize(780,editSet ? 718 : 610); startTimerHz(10);
    }
    ~ControlPanel() override {stopTimer();bindingList.setModel(nullptr);}
    void paint(juce::Graphics& g) override {
        g.fillAll(palette::panel);
        label(g,editSet ? "CONTROL SET / Q"+juce::String(sourceRow+1) :
              "CONTROL & MAPPING",{17,12,350,26},20,palette::teal,true);
        label(g,"MIDI input",{17,55,250,19},11,palette::muted,true);
        label(g,"MIDI Learn",{17,120,150,19},11,palette::muted,true);
        label(g,editSet ? "Column-level priority (set in utilities)" : "Scope",
              {17,205,195,17},11,palette::muted,true);
        label(g,"Destination",{213,205,120,17},11,palette::muted,true);
        label(g,"Takeover",{410,205,120,17},11,palette::muted,true);
        label(g,"Output CC",{17,263,95,16},10,palette::muted,true);
        label(g,"OSC address",{112,263,140,16},10,palette::muted,true);
        const auto currentTitle=editSet ? juce::String("Control Column / ") +
              juce::String(sourceColumnId) : juce::String("Cue preset");
        label(g,currentTitle,{17,324,300,17},11,palette::muted,true);
        if (editSet) label(g,"REUSABLE CONTROL SETS",{17,421,290,18},10,palette::teal,true);
        label(g,editSet ? "Mappings in this Control Set" : "Saved mappings",
              {17,editSet ? 475 : 402,350,20},12,palette::teal,true);
        label(g,"Curve",{440,263,100,16},10,palette::muted,true);
        label(g,"Min",{572,263,75,16},10,palette::muted,true);
        label(g,"Max",{671,263,75,16},10,palette::muted,true);
        label(g,editSet ? "Control Set takes effect on GO; locked sets apply to every cue." :
              "Mapping chooses its active destination after GO, not when selecting a row.",
              {17,editSet ? 686 : 579,getWidth()-32,19},10,palette::muted);
    }
    void resized() override {
        const int w=getWidth();
        device.setBounds(17,78,w-157,30);
        connect.setBounds(w-132,78,115,30);
        learn.setBounds(17,143,120,33);
        state.setBounds(147,142,w-164,34);
        scope.setBounds(17,227,178,29);
        target.setBounds(213,227,180,29);
        takeover.setBounds(410,227,133,29);
        columns.setBounds(555,227,w-571,29);
        outputCc.setBounds(17,281,83,29);
        oscAddress.setBounds(112,281,210,29);
        invert.setBounds(345,281,88,29);
        curve.setBounds(440,281,116,29);
        minimum.setBounds(572,281,86,29);
        maximum.setBounds(671,281,86,29);
        effectivePreview.setBounds(17,344,w-34,66);
        templateName.setBounds(17,440,170,27);
        templates.setBounds(193,440,232,27);
        saveTemplate.setBounds(432,440,140,27);
        loadTemplate.setBounds(580,440,w-597,27);
        presets.setBounds(17,344,200,30);
        assignPreset.setBounds(226,344,150,30);
        presetName.setBounds(388,344,w-536,30);
        newPreset.setBounds(w-140,344,123,30);
        bindingList.setBounds(17,editSet ? 501 : 425,w-230,editSet ? 164 : 139);
        add.setBounds(w-202,editSet ? 501 : 426,184,37);
        updateSelected.setBounds(w-202,editSet ? 547 : 472,184,37);
        remove.setBounds(w-202,editSet ? 593 : 518,184,37);
    }
private:
    juce::Component::SafePointer<MainComponent> owner;
    bool editSet=false;
    int sourceRow=0,sourceColumn=0;
    int sourceRowId=0,sourceColumnId=0;
    bool resolveSource(int& row,int& column) const {
        if (!owner) return false;
        for (int i=0;i<(int)owner->show.rows.size();++i)
            if (owner->show.rows[(size_t)i].id==sourceRowId) {row=i;break;}
        for (int i=0;i<(int)owner->show.columns.size();++i)
            if (owner->show.columns[(size_t)i].id==sourceColumnId) {column=i;break;}
        return row>=0 && row<(int)owner->show.rows.size() &&
            column>=0 && column<(int)owner->show.columns.size() &&
            owner->show.rows[(size_t)row].id==sourceRowId &&
            owner->show.columns[(size_t)column].id==sourceColumnId;
    }
    const std::vector<ControlBinding>* currentBindings() const {
        if (!owner) return nullptr;
        if (!editSet) return &owner->show.controls.bindings;
        int row=-1,columnIndex=-1;
        if (!resolveSource(row,columnIndex)) return nullptr;
        const auto& column=owner->show.columns[(size_t)columnIndex];
        if (column.type!=ColumnType::control) return nullptr;
        if (column.controlLocked) return &column.controlDefaultSet.bindings;
        const auto& cell=owner->show.rows[(size_t)row].cells[(size_t)columnIndex];
        return cell.controlSet ? &cell.controlSet->bindings : nullptr;
    }
    juce::ComboBox device,presets,scope,target,takeover,columns,curve,templates;
    juce::TextButton connect,learn,add,remove,updateSelected,newPreset,assignPreset,saveTemplate,loadTemplate;
    juce::TextEditor presetName,outputCc,oscAddress,minimum,maximum,effectivePreview,templateName;
    juce::ToggleButton invert{"Invert"};
    juce::Label state;
    juce::ListBox bindingList;
    std::vector<juce::String> deviceIds;
    std::vector<int> columnIds;
    std::optional<ControlEvent> captured;
    int selected=-1;
    void timerCallback() override {
        if (!owner) return;
        if (auto event=owner->consumeLearnedInput()) {
            captured=event;
            state.setText("Learned "+event->deviceId+" ch"+juce::String(event->channel)+
                " / "+juce::String(event->number)+" value "+juce::String(event->value),
                juce::dontSendNotification);
        }
        if (editSet) updateEffectivePreview();
    }
    void refreshDevices() {
        if (!owner) return;
        device.clear(juce::dontSendNotification);
        deviceIds.clear(); device.addItem("Disconnected",1);
        int selectedId=1;
        for (const auto& d:juce::MidiInput::getAvailableDevices()) {
            deviceIds.push_back(d.identifier);
            const int id=(int)deviceIds.size()+1;
            device.addItem(d.name,id);
            if (owner->show.controls.midiInputId==d.identifier) selectedId=id;
        }
        device.setSelectedId(selectedId,juce::dontSendNotification);
    }
    void refreshTemplates() {
        if (!owner) return;
        const int old=templates.getSelectedId();
        templates.clear(juce::dontSendNotification);
        templates.addItem("Choose saved set",1);
        for (const auto& item:owner->show.controls.templates)
            templates.addItem(item.name,item.id+1);
        templates.setSelectedId(old>0 && templates.indexOfItemId(old)>=0 ? old : 1,
                                juce::dontSendNotification);
    }
    void refreshPresets() {
        if (!owner) return;
        const int old=presets.getSelectedId();
        presets.clear(juce::dontSendNotification);
        presets.addItem("No preset",1);
        for (const auto& p:owner->show.controls.presets) presets.addItem(p.name,p.id+1);
        int current=owner->show.rows[(size_t)owner->selectedRow].mappingPresetId+1;
        presets.setSelectedId(old>0 && presets.indexOfItemId(old)>=0 ? old : current,
                              juce::dontSendNotification);
    }
    void refreshColumns() {
        if (!owner) return;
        columns.clear(juce::dontSendNotification);
        columnIds.clear();
        const int targetId=target.getSelectedId();
        for (const auto& c:owner->show.columns) {
            const bool eligible=(targetId==2 || targetId==7) ? c.type==ColumnType::audio :
                targetId==5 ? c.type==ColumnType::midi :
                targetId==6 ? c.type==ColumnType::osc : true;
            if (!eligible) continue;
            columnIds.push_back(c.id);
            columns.addItem(c.name,(int)columnIds.size());
        }
        if (!columnIds.empty()) columns.setSelectedId(1);
    }
    void createBinding(bool update) {
        const auto* existing=currentBindings();
        if (!owner || (update && (!existing || selected<0 || selected>=(int)existing->size())) ||
            (!update && !captured)) {
            state.setText("Use MIDI Learn first or select a mapping to edit.",
                juce::dontSendNotification); return;
        }
        ControlBinding b=update ? (*existing)[(size_t)selected] : ControlBinding{};
        if (captured) {
            b.deviceId=captured->deviceId;
            b.kind=captured->kind; b.channel=captured->channel; b.number=captured->number;
        }
        b.scope=(ControlScope)(scope.getSelectedId()-1);
        switch (target.getSelectedId()) {
            case 1: b.target=ControlTarget::masterGain; break;
            case 2: b.target=ControlTarget::columnGain; break;
            case 3: b.target=ControlTarget::go; break;
            case 4: b.target=ControlTarget::panic; break;
            case 5: b.target=ControlTarget::midiCc; break;
            case 6: b.target=ControlTarget::oscFloat; break;
            case 7: b.target=ControlTarget::columnPan; break;
            case 8: b.target=ControlTarget::masterPan; break;
            default: return;
        }
        b.takeover=(ControlTakeover)(takeover.getSelectedId()-1);
        b.curve=(ControlCurve)(curve.getSelectedId()-1);
        b.scopeId=0; b.targetColumnId=0;
        if (b.scope==ControlScope::cue)
            b.scopeId=owner->show.rows[(size_t)owner->selectedRow].id;
        else if (b.scope==ControlScope::preset)
            b.scopeId=std::max(0,presets.getSelectedId()-1);
        if (b.scope==ControlScope::column || b.target==ControlTarget::columnGain ||
            b.target==ControlTarget::columnPan ||
            b.target==ControlTarget::midiCc || b.target==ControlTarget::oscFloat) {
            const int i=columns.getSelectedId()-1;
            if (i<0 || i>=(int)columnIds.size()) {
                state.setText("Select a destination column.",juce::dontSendNotification);return;
            }
            b.targetColumnId=columnIds[(size_t)i];
            if (b.scope==ControlScope::column) b.scopeId=b.targetColumnId;
        }
        if (b.kind==ControlKind::note || b.target==ControlTarget::go ||
            b.target==ControlTarget::panic) b.takeover=ControlTakeover::jump;
        b.invert=invert.getToggleState();
        if (b.target==ControlTarget::midiCc) {
            b.outputChannel=b.channel;
            b.outputNumber=outputCc.getText().getIntValue();
        }
        if (b.target==ControlTarget::oscFloat)
            b.oscAddress=oscAddress.getText().trim();
        const auto low=minimum.getText().trim(),high=maximum.getText().trim();
        const auto numeric=[](const juce::String& value) {
            return value.isNotEmpty() && value.containsAnyOf("0123456789") &&
                value.retainCharacters("0123456789.-+")==value;
        };
        if (!numeric(low) || !numeric(high)) {
            state.setText("Enter valid minimum and maximum values.",juce::dontSendNotification); return;
        }
        b.minimum=low.getFloatValue(); b.maximum=high.getFloatValue();
        int row=-1,column=-1;
        const bool resolved=!editSet || resolveSource(row,column);
        const bool saved=resolved && (update ?
            owner->updateControlBinding(row,column,b.id,b,editSet) :
            editSet ? owner->addControlSetBinding(row,column,b) :
                      owner->addMappingBinding(b));
        if (saved) {
            bindingList.updateContent(); bindingList.repaint();
            selected=-1;updateSelected.setEnabled(false);bindingList.deselectAllRows();
            captured.reset();updateEffectivePreview();
            state.setText(update ? "Mapping updated." : "Mapping saved to the show.",
                juce::dontSendNotification);
        } else state.setText(owner->status,juce::dontSendNotification);
    }
    void updateEffectivePreview() {
        if (!owner || !editSet) return;
        int row=-1,column=-1;
        if (!resolveSource(row,column)) {
            effectivePreview.setText("Control Column no longer exists.",false);return;
        }
        const int cue=owner->show.rows[(size_t)row].id;
        const bool live=owner->activeCueMappingId()==cue;
        const auto resolved=owner->effectiveControlSet(cue);
        juce::String summary=live ? "LIVE Q"+juce::String(row+1) :
            "PREVIEW Q"+juce::String(row+1)+" — changes take effect after GO";
        if (resolved.empty()) summary+="\nNo active Control Column mappings.";
        for (const auto& item:resolved) {
            juce::String source="Global / legacy";
            for (const auto& col:owner->show.columns)
                if (col.id==item.columnId) {source=col.name;break;}
            const auto& b=item.binding;
            const bool local=item.columnId==sourceColumnId;
            summary+="\n"+juce::String(local ? (item.overrides ? "OVERRIDE / " : "LOCAL / ") :
                (item.overrides ? "INHERITED OVERRIDE / " : "INHERITED / "))+
                source+" ("+(item.priority==0 ? juce::String("legacy") :
                      "P"+juce::String(item.priority))+")  "+
                (b.kind==ControlKind::cc ? "CC " :
                 b.kind==ControlKind::note ? "NOTE " : "BEND ")+
                juce::String(b.channel)+":"+juce::String(b.number)+
                " -> "+(b.target==ControlTarget::masterGain ? "MASTER GAIN" :
                  b.target==ControlTarget::masterPan ? "MASTER PAN" :
                  b.target==ControlTarget::columnGain ? "CHANNEL GAIN" :
                  b.target==ControlTarget::columnPan ? "CHANNEL PAN" :
                  b.target==ControlTarget::midiCc ? "MIDI CC" :
                  b.target==ControlTarget::oscFloat ? "OSC" :
                  b.target==ControlTarget::go ? "GO" : "PANIC");
        }
        if (effectivePreview.getText()!=summary)
            effectivePreview.setText(summary,false);
    }
    int getNumRows() override {
        const auto* entries=currentBindings();
        return entries ? (int)entries->size():0;
    }
    void selectedRowsChanged(int row) override {
        selected=row;
        const auto* entries=currentBindings();
        updateSelected.setEnabled(entries && row>=0 && row<(int)entries->size());
        if (!entries || row<0 || row>=(int)entries->size()) return;
        const auto& b=(*entries)[(size_t)row];
        captured.reset();
        scope.setSelectedId((int)b.scope+1,juce::dontSendNotification);
        target.setSelectedId((int)b.target+1,juce::dontSendNotification);
        refreshColumns();
        for (size_t i=0;i<columnIds.size();++i)
            if (columnIds[i]==b.targetColumnId)
                columns.setSelectedId((int)i+1,juce::dontSendNotification);
        takeover.setSelectedId((int)b.takeover+1,juce::dontSendNotification);
        curve.setSelectedId((int)b.curve+1,juce::dontSendNotification);
        minimum.setText(juce::String(b.minimum,3),false);
        maximum.setText(juce::String(b.maximum,3),false);
        outputCc.setText(juce::String(b.outputNumber),false);
        oscAddress.setText(b.oscAddress,false);
        invert.setToggleState(b.invert,juce::dontSendNotification);
        state.setText("Editing #"+juce::String(b.id)+" — input "+b.deviceId+
            " ch"+juce::String(b.channel)+" / "+juce::String(b.number),
            juce::dontSendNotification);
    }
    void paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool active) override {
        if (!owner || row<0 || row>=getNumRows()) return;
        const auto& b=(*currentBindings())[(size_t)row];
        if (active) g.fillAll(palette::teal.withAlpha(.20f));
        juce::String targetName=b.target==ControlTarget::go ? "GO" :
            b.target==ControlTarget::panic ? "PANIC" :
            b.target==ControlTarget::masterGain ? "MASTER GAIN" :
            b.target==ControlTarget::masterPan ? "MASTER PAN" :
            b.target==ControlTarget::columnPan ? "PAN" :
            b.target==ControlTarget::oscFloat ? "OSC" :
            b.target==ControlTarget::midiCc ? "MIDI CC" : "GAIN";
        label(g,"#"+juce::String(b.id)+"   ch"+juce::String(b.channel)+
            " "+(b.kind==ControlKind::cc?"CC ":b.kind==ControlKind::note?"NOTE ":"BEND ")+
            juce::String(b.number)+" → "+targetName+
            "  ["+juce::String((int)b.scope)+"]",
            {7,0,width-14,height},12,active?palette::text:palette::muted);
    }
};
void MainComponent::openControlPanel(bool editSet) {
    if (controlDialog) {
        auto* previous=controlDialog.getComponent();
        controlDialog=nullptr;
        delete previous;
    }
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(new ControlPanel(*this,editSet));
    options.dialogTitle=editSet ? "The Cuelist — Control Set" :
        "The Cuelist — Control & Mapping";
    options.dialogBackgroundColour=palette::panel;
    options.escapeKeyTriggersCloseButton=true;
    options.useNativeTitleBar=true;
    options.resizable=true;
    controlDialog=options.launchAsync();
}
}
