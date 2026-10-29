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
             &effectivePreview,&templateName,&templates,&saveTemplate,&loadTemplate,
             &emptyBindings,&functionChoice,&functionName,&renameFunction})
            addAndMakeVisible(c);
        functionChoice.setName("controlFunctionChoice");
        functionName.setName("controlFunctionName");
        functionName.setTextToShowWhenEmpty("Function name",palette::muted);
        functionName.setReadOnly(editSet);
        renameFunction.setButtonText("Rename"); renameFunction.setVisible(!editSet);
        functionChoice.onChange=[this] { loadFunctionDestination(); };
        renameFunction.onClick=[this] {
            if (!owner) return;
            const int id=functionChoice.getSelectedId()-1;
            if (id>0 && owner->renameControlFunction(id,functionName.getText())) {
                refreshFunctions(); functionChoice.setSelectedId(id+1,juce::dontSendNotification);
                state.setText("Function renamed.",juce::dontSendNotification);
                bindingList.repaint();
            } else state.setText("Select a function and enter a valid name.",juce::dontSendNotification);
        };
        device.setName("controlInputDevice");
        device.setTooltip("Select a device explicitly. No automatic fallback on disconnect.");
        connect.setButtonText("Refresh ports");
        learn.setButtonText("MIDI Learn");
        add.setButtonText("Add mapping");
        updateSelected.setButtonText("Update selected");
        updateSelected.setName("mappingUpdateSelected");
        updateSelected.setEnabled(false);
        remove.setEnabled(false);
        emptyBindings.setName("mappingEmptyState");
        emptyBindings.setText("No mappings yet. Use MIDI Learn, then Add mapping.",
                              juce::dontSendNotification);
        emptyBindings.setColour(juce::Label::textColourId,palette::muted);
        emptyBindings.setFont(font(13.5f));
        emptyBindings.setJustificationType(juce::Justification::centred);
        emptyBindings.setInterceptsMouseClicks(false,false);
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
                remove.setEnabled(false);
                refreshEmptyState();
                updateEffectivePreview();
                state.setText("Reusable Set applied.",juce::dontSendNotification);
            } else state.setText(owner->status,juce::dontSendNotification);
        };
        effectivePreview.setColour(juce::TextEditor::outlineColourId,palette::line);
        remove.setButtonText("Remove selected");
        remove.setName("mappingRemoveSelected");
        newPreset.setButtonText("New preset");
        assignPreset.setButtonText("Assign to cue");
        presetName.setTextToShowWhenEmpty("New preset name",palette::muted);
        state.setName("mappingStatus");
        state.setJustificationType(juce::Justification::centredLeft);
        scope.setName("mappingScope");
        columns.setName("mappingDestinationColumn");
        target.setName("mappingTarget");
        scope.addItemList({"Global","Column","Preset","Single cue"},1);
        scope.setSelectedId(1);
        scope.onChange=[this] { refreshDestinationFields(); };
        scope.setVisible(!editSet);
        presets.setVisible(!editSet); newPreset.setVisible(!editSet);
        assignPreset.setVisible(!editSet); presetName.setVisible(!editSet);
        target.addItemList({"Master Audio","Audio column gain","GO next cue","PANIC",
                            "MIDI CC","OSC Float","Audio column pan","Master pan"},1);
        target.setSelectedId(1);
        target.onChange=[this] {
            refreshColumns();
            refreshDestinationFields();
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
                updateSelected.setEnabled(false);
                remove.setEnabled(false);
                refreshEmptyState();
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
        refreshDevices(); refreshPresets(); refreshColumns();refreshTemplates(); refreshFunctions();
        refreshDestinationFields();
        refreshEmptyState();
        updateEffectivePreview();
        setSize(780,editSet ? 816 : 710); startTimerHz(10);
    }
    ~ControlPanel() override {stopTimer();bindingList.setModel(nullptr);}
    void paint(juce::Graphics& g) override {
        g.fillAll(palette::panel);
        label(g,editSet ? "CONTROL SET / Q"+juce::String(sourceRow+1) :
              "CONTROL & MAPPING",{20,15,520,31},21,palette::teal,true);
        g.setColour(palette::line);
        g.drawHorizontalLine(52,19,(float)getWidth()-19);
        label(g,"MIDI INPUT",{20,60,350,19},12,palette::teal,true);
        label(g,"Input device",{20,83,200,18},11,palette::muted,true);
        label(g,"MIDI Learn",{20,143,200,18},11,palette::muted,true);

        g.setColour(palette::line);
        g.drawHorizontalLine(191,20,(float)getWidth()-20);
        label(g,"FUNCTION / DESTINATION ROUTING",{20,199,430,19},12,palette::teal,true);
        g.drawHorizontalLine(242,20,(float)getWidth()-20);
        label(g,"MAPPING CONFIGURATION",{20,246,350,19},12,palette::teal,true);
        label(g,editSet?"Control Set scope":"Scope",{20,276,179,18},11,palette::muted,true);
        label(g,"Target",{214,276,178,18},11,palette::muted,true);
        label(g,"Takeover",{411,276,132,18},11,palette::muted,true);
        label(g,"Destination column",{556,276,207,18},11,palette::muted,true);
        label(g,"Output CC",{20,333,85,18},11,palette::muted,true);
        label(g,"OSC address",{113,333,210,18},11,palette::muted,true);
        label(g,"Curve",{441,333,114,18},11,palette::muted,true);
        label(g,"Min",{573,333,84,18},11,palette::muted,true);
        label(g,"Max",{672,333,84,18},11,palette::muted,true);

        if (editSet) {
            label(g,"EFFECTIVE MAPPING",{20,395,330,18},12,palette::teal,true);
            label(g,"REUSABLE CONTROL SETS",{20,504,330,18},12,palette::teal,true);
        } else {
            label(g,"CUE PRESETS",{20,395,250,18},12,palette::teal,true);
            label(g,"New preset name",{388,395,220,18},11,palette::muted,true);
        }
        label(g,editSet?"MAPPINGS IN THIS CONTROL SET":"SAVED MAPPINGS",
              {20,editSet?582:473,380,18},12,palette::teal,true);
        g.setColour(palette::line);
        g.drawHorizontalLine(getHeight()-48,20,(float)getWidth()-20);
        label(g,editSet ?
            "Control Sets override the Default Map for this cue only." :
            "Cue mappings become active after GO, not when selecting a cue.",
            {20,getHeight()-40,getWidth()-40,23},11,palette::muted);
    }
    void resized() override {
        const int w=getWidth();
        device.setBounds(20,105,w-177,31);
        connect.setBounds(w-143,105,123,31);
        learn.setBounds(20,163,128,32);
        state.setBounds(160,162,w-180,33);

        functionChoice.setBounds(20,213,280,29);
        functionName.setBounds(308,213,275,29);
        renameFunction.setBounds(592,213,168,29);
        scope.setBounds(20,298,176,31);
        target.setBounds(214,298,178,31);
        takeover.setBounds(411,298,133,31);
        columns.setBounds(556,298,w-576,31);
        outputCc.setBounds(20,355,84,31);
        oscAddress.setBounds(113,355,210,31);
        invert.setBounds(336,355,96,31);
        curve.setBounds(441,355,115,31);
        minimum.setBounds(573,355,85,31);
        maximum.setBounds(672,355,85,31);

        effectivePreview.setBounds(20,420,w-40,72);
        templateName.setBounds(20,527,169,31);
        templates.setBounds(198,527,229,31);
        saveTemplate.setBounds(437,527,137,31);
        loadTemplate.setBounds(584,527,w-604,31);
        presets.setBounds(20,418,195,31);
        assignPreset.setBounds(223,418,153,31);
        presetName.setBounds(388,418,w-548,31);
        newPreset.setBounds(w-149,418,129,31);

        const int listY=editSet?606:497;
        const int listHeight=editSet?136:157;
        bindingList.setBounds(20,listY,w-239,listHeight);
        emptyBindings.setBounds(30,listY+19,w-259,42);
        add.setBounds(w-209,listY,189,38);
        updateSelected.setBounds(w-209,listY+47,189,38);
        remove.setBounds(w-209,listY+94,189,38);
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
    juce::ComboBox device,presets,scope,target,takeover,columns,curve,templates,functionChoice;
    juce::TextButton connect,learn,add,remove,updateSelected,newPreset,assignPreset,saveTemplate,loadTemplate,renameFunction;
    juce::TextEditor presetName,outputCc,oscAddress,minimum,maximum,effectivePreview,templateName,functionName;
    juce::ToggleButton invert{"Invert"};
    juce::Label state;
    juce::ListBox bindingList;
    juce::Label emptyBindings;
    std::vector<juce::String> deviceIds;
    std::vector<int> columnIds;
    std::optional<ControlEvent> captured;
    int selected=-1;
    void refreshDestinationFields() {
        const int t=target.getSelectedId();
        outputCc.setEnabled(t==5);
        oscAddress.setEnabled(t==6);
        columns.setEnabled(editSet || scope.getSelectedId()==2 ||
                           t==2 || t==5 || t==6 || t==7);
    }
    void refreshEmptyState() {
        emptyBindings.setVisible(getNumRows()==0);
        remove.setEnabled(selected>=0 && selected<getNumRows());
    }
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
    void refreshFunctions() {
        if (!owner) return;
        const int old=functionChoice.getSelectedId();
        functionChoice.clear(juce::dontSendNotification);
        functionChoice.addItem(editSet ? "Choose a Default Map function" : "New function",1);
        for (const auto& f:owner->show.controls.functions)
            functionChoice.addItem(f.name,f.id+1);
        functionChoice.setSelectedId(old>0 && functionChoice.indexOfItemId(old)>=0 ? old : 1,juce::dontSendNotification);
        loadFunctionDestination();
    }
    void loadFunctionDestination() {
        if (!owner) return;
        const int id=functionChoice.getSelectedId()-1;
        const auto& functions=owner->show.controls.functions;
        auto it=std::find_if(functions.begin(),functions.end(),
            [id](const ControlFunction& f){return f.id==id;});
        if (it==functions.end()) {if (editSet) functionName.clear();return;}
        functionName.setText(it->name,false);
        const auto& b=it->destination;
        target.setSelectedId((int)b.target+1,juce::dontSendNotification);
        refreshColumns();
        for (size_t i=0;i<columnIds.size();++i)
            if (columnIds[i]==b.targetColumnId)
                columns.setSelectedId((int)i+1,juce::dontSendNotification);
        outputCc.setText(juce::String(b.outputNumber),false);
        oscAddress.setText(b.oscAddress,false);
        minimum.setText(juce::String(b.minimum,3),false);
        maximum.setText(juce::String(b.maximum,3),false);
        refreshDestinationFields();
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
        const int functionId=functionChoice.getSelectedId()-1;
        if (editSet && functionId<=0) {
            state.setText("Choose a Default Map function.",juce::dontSendNotification);return;
        }
        if (update && !editSet && functionId<=0) {
            state.setText("Choose a function to update.",juce::dontSendNotification);return;
        }
        b.functionId=std::max(0,functionId);
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
                      owner->addMappingBinding(b,functionName.getText()));
        if (saved) {
            bindingList.updateContent(); bindingList.repaint();
            selected=-1;updateSelected.setEnabled(false);bindingList.deselectAllRows();
            remove.setEnabled(false);
            refreshEmptyState();
            captured.reset();refreshFunctions();updateEffectivePreview();
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
            juce::String functionLabel;
            for (const auto& fn:owner->show.controls.functions)
                if (fn.id==b.functionId) {functionLabel=fn.name+" / ";break;}
            summary+="\n"+juce::String(local ? (item.overrides ? "OVERRIDE / " : "LOCAL / ") :
                (item.overrides ? "INHERITED OVERRIDE / " : "INHERITED / "))+
                source+" ("+(item.priority==0 ? juce::String("default") :
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
        remove.setEnabled(entries && row>=0 && row<(int)entries->size());
        if (!entries || row<0 || row>=(int)entries->size()) return;
        const auto& b=(*entries)[(size_t)row];
        functionChoice.setSelectedId(b.functionId>0 ? b.functionId+1 : 1,juce::dontSendNotification);
        if (b.functionId>0) loadFunctionDestination();
        captured.reset();
        scope.setSelectedId((int)b.scope+1,juce::dontSendNotification);
        target.setSelectedId((int)b.target+1,juce::dontSendNotification);
        refreshColumns();
        refreshDestinationFields();
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
        juce::String functionLabel;
        for (const auto& fn:owner->show.controls.functions)
            if (fn.id==b.functionId) {functionLabel=fn.name+" / ";break;}
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
            "  ["+juce::String((int)b.scope)+"]  "+functionLabel,
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
    options.useNativeTitleBar=false;
    options.resizable=true;
    controlDialog=options.launchAsync();
    if (controlDialog)
        controlDialog->setLookAndFeel(&theme); // Match Exo/Hind and Midnight Slate
}
}
