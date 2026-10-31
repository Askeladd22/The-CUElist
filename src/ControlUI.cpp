#include "MainComponent.h"
#include "OperatorTextPrompt.h"
#include "ControlMapArchive.h"
#include <cmath>

namespace cuelist {

void MainComponent::editControlSet(int row,int column) {
    if (!select(row,column) || show.columns[(size_t)column].type!=ColumnType::control) return;
    const auto mapId=show.rows[(size_t)row].cells[(size_t)column].assignedMapId;
    if (mapId>0) openControlPanel(false,mapId);
    else openControlPanel(true);
}

bool MainComponent::createEmptyControlSet(int row,int column) {
    if (row<0 || row>=(int)show.rows.size() ||
        column<0 || column>=(int)show.columns.size() ||
        show.columns[(size_t)column].type!=ColumnType::control) return false;
    if (show.columns[(size_t)column].controlLocked ||
        hasControlMapAssignment(show.rows[(size_t)row].cells[(size_t)column])) return true;
    auto candidate=show;
    candidate.rows[(size_t)row].cells[(size_t)column].controlSet.emplace();
    candidate.rows[(size_t)row].cells[(size_t)column].assignedMapId=0;
    if (const auto result=validateShow(candidate);result.failed()) {
        status=result.getErrorMessage();return false;
    }
    show=std::move(candidate);refreshControlSettings();dirty=true;
    status="Empty override created; Default Map remains available.";
    refresh();return true;
}

bool MainComponent::duplicateControlSet(int sr,int sc,int tr,int tc) {
    if (tr<0 || tr>=(int)show.rows.size() || tc<0 ||
        tc>=(int)show.columns.size() ||
        show.columns[(size_t)tc].type!=ColumnType::control ||
        show.columns[(size_t)tc].controlLocked ||
        hasControlMapAssignment(show.rows[(size_t)tr].cells[(size_t)tc])) return false;
    ControlSet clone;
    if (sr<0) {
        for (const auto& b:show.controls.bindings)
            if (b.functionId>0 && b.scope==ControlScope::global)
                clone.bindings.push_back(b);
    } else {
        if (sr>=(int)show.rows.size() || sc<0 || sc>=(int)show.columns.size())
            return false;
        const auto& source=show.rows[(size_t)sr].cells[(size_t)sc].controlSet;
        if (!source) return false;
        clone=*source;
        if (show.rows[(size_t)sr].cells[(size_t)sc].assignedMapId>0)
            for (const auto& saved:show.controls.templates)
                if (saved.id==show.rows[(size_t)sr].cells[(size_t)sc].assignedMapId)
                    {clone=saved.set;break;}
    }
    auto next=show;
    for (auto& b:clone.bindings) {
        b.id=next.controls.nextId++;
        b.scope=ControlScope::global;b.scopeId=0;
    }
    next.rows[(size_t)tr].cells[(size_t)tc].controlSet=std::move(clone);
    next.rows[(size_t)tr].cells[(size_t)tc].assignedMapId=0;
    if (auto valid=validateShow(next);valid.failed()) {
        status=valid.getErrorMessage();return false;
    }
    show=std::move(next);dirty=true;refreshControlSettings();
    status="Control Map duplicated.";refresh();return true;
}

void MainComponent::showControlCellMenu(int row,int column,
                                         juce::Point<int> screenPosition) {
    if (row<0 || row>=(int)show.rows.size() || column<0 ||
        column>=(int)show.columns.size() ||
        show.columns[(size_t)column].type!=ColumnType::control ||
        show.columns[(size_t)column].controlLocked) return;
    const int rowId=show.rows[(size_t)row].id;
    const int columnId=show.columns[(size_t)column].id;
    const auto& selectedCell=show.rows[(size_t)row].cells[(size_t)column];
    const bool present=hasControlMapAssignment(selectedCell);
    juce::PopupMenu menu;
    menu.addItem(1,present ?
        (selectedCell.assignedMapId>0 ? "Edit Control Map (shared)" : "Edit Control Map") :
        "Create empty Control Map");
    juce::PopupMenu saved;
    for (const auto& item:show.controls.templates)
        saved.addItem(1000000+item.id,item.name);
    menu.addSubMenu(present?"Change map assignment":"Assign saved map",saved,
                    !show.controls.templates.empty());
    if (present) {
        menu.addItem(2,"Save map for reuse...");
        menu.addItem(3,"Remove map assignment (inherit Default Map)");
    }
    auto safe=juce::Component::SafePointer<MainComponent>(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(
        {screenPosition.x,screenPosition.y,1,1}),
        [safe,rowId,columnId](int choice) {
            if (!safe || choice<=0) return;
            int r=-1,c=-1;
            for (int i=0;i<(int)safe->show.rows.size();++i)
                if (safe->show.rows[(size_t)i].id==rowId) r=i;
            for (int i=0;i<(int)safe->show.columns.size();++i)
                if (safe->show.columns[(size_t)i].id==columnId) c=i;
            if (r<0 || c<0 || safe->show.columns[(size_t)c].type!=ColumnType::control ||
                safe->show.columns[(size_t)c].controlLocked) return;
            if (choice==1) {
                if (safe->createEmptyControlSet(r,c)) safe->editControlSet(r,c);
            } else if (choice==2) {
                if (!safe->show.rows[(size_t)r].cells[(size_t)c].controlSet) return;
                showOperatorTextPrompt(*safe,safe->theme,"Save reusable Control Map",
                    "Save an independent copy. Editing it later will not rewrite existing cue overrides.",
                    "Map name","Map "+juce::String(safe->show.controls.templates.size()+1),
                    false,"Save",[safe,rowId,columnId](juce::String name) {
                        if (!safe) return;
                        for (int row=0;row<(int)safe->show.rows.size();++row)
                            if (safe->show.rows[(size_t)row].id==rowId)
                                for (int col=0;col<(int)safe->show.columns.size();++col)
                                    if (safe->show.columns[(size_t)col].id==columnId) {
                                        const auto& cell=safe->show.rows[(size_t)row].cells[(size_t)col];
                                        if (cell.assignedMapId>0) {
                                            const auto found=std::find_if(safe->show.controls.templates.begin(),
                                                safe->show.controls.templates.end(),
                                                [&cell](const ControlSetTemplate& map){
                                                    return map.id==cell.assignedMapId;
                                                });
                                            if (found!=safe->show.controls.templates.end())
                                                safe->createReusableControlMap(name,found->set);
                                        } else if (cell.controlSet)
                                            safe->createReusableControlMap(name,*cell.controlSet);
                                        return;
                                    }
                    });
            } else if (choice==3) {
                safe->clearControlCellMap(r,c);
            } else {
                const int chosenMap=choice-1000000;
                const auto exists=choice>=1000000 &&
                    std::any_of(safe->show.controls.templates.begin(),
                    safe->show.controls.templates.end(),
                    [chosenMap](const ControlSetTemplate& t){return t.id==chosenMap;});
                if (exists) safe->applyControlSetTemplate(r,c,chosenMap);
            }
        });
}

class ControlPanel final : public juce::Component, private juce::Timer,
                           private juce::ListBoxModel {
public:
    explicit ControlPanel(MainComponent& main,bool useSet,int templateId=0):owner(&main),
        editSet(useSet && templateId==0),editTemplateId(templateId),
        sourceRow(main.selectedRow),sourceColumn(main.selectedColumn),
        sourceRowId(main.show.rows[(size_t)main.selectedRow].id),
        sourceColumnId(main.show.columns[(size_t)main.selectedColumn].id) {
        setName("controlMappingPanel");
        for (juce::Component* c : std::initializer_list<juce::Component*>{
            &device,&connect,&learn,&oscLearn,&oscInputPort,&add,&remove,&newPreset,&assignPreset,
            &presetName,&presets,&scope,&target,&takeover,&columns,&bindingList,&state,
            &outputCc,&outputChannel,&oscAddress,&invert,&updateSelected,&curve,&minimum,&maximum,
             &functionChoice,&functionName,&newFunction,&saveRoute,
             &saveControllerPreset,&loadControllerPreset,&effectivePreview,&templateName,&templates,&saveTemplate,&loadTemplate,
             &emptyBindings,&mapChooser,&newMap,&cloneMap,&renameMap,&deleteMap,
            &mapMore,&inputKind,&inputChannel,&inputNumber,&inputAddress,&cancelLearn})
            addAndMakeVisible(c);
        mapChooser.setName("mappingMapSelector");
        newMap.setName("mappingNewMap");
        cloneMap.setName("mappingDuplicateMap");
        newMap.setButtonText("New Map");
        cloneMap.setButtonText("Duplicate");
        mapMore.setName("mappingMoreMenu");
        mapMore.setButtonText("...");
        mapMore.setTooltip("Rename, delete, import or export the selected map");
        inputKind.setName("mappingInputKind");
        inputKind.addItemList({"MIDI CC","MIDI Note","MIDI Pitch Bend","OSC"},1);
        inputKind.setSelectedId(1);
        inputChannel.setName("mappingInputChannel");
        inputChannel.setText("1",false);
        inputChannel.setInputRestrictions(2,"0123456789");
        inputNumber.setName("mappingInputNumber");
        inputNumber.setText("0",false);
        inputNumber.setInputRestrictions(5,"0123456789");
        inputAddress.setName("mappingInputAddress");
        inputAddress.setText("/control",false);
        cancelLearn.setName("mappingCancelLearn");
        cancelLearn.setButtonText("Stop Learn");
        cancelLearn.onClick=[this] { if (owner) {
            continuousLearn=0;
            owner->controlLearnArmed=false;owner->oscLearnArmed=false;
            owner->learnedInput.reset();
            state.setText("Learn stopped.",juce::dontSendNotification);
        }};
        inputKind.onChange=[this] {updateInputVisibility();};
        renameMap.setName("mappingRenameMap");
        renameMap.setButtonText("Rename Map");
        deleteMap.setName("mappingDeleteMap");
        deleteMap.setButtonText("Delete unused Map");
        mapChooser.addItem("Default Map (fallback)",1);
        int initial=1;
        // The main editor is the map archive; individual cue overrides
        // appear here only when invoked from that actual Control Column cell.
        if (useSet && templateId==0 &&
            sourceRow>=0 && sourceRow<(int)owner->show.rows.size() &&
            sourceColumn>=0 && sourceColumn<(int)owner->show.columns.size()) {
            availableMaps.push_back({sourceRow,sourceColumn});
            mapChooser.addItem("Cue-local override: Q"+juce::String(sourceRow+1)+
                " / "+owner->show.columns[(size_t)sourceColumn].name,2);
            initial=2;
        }
        for (const auto& map:owner->show.controls.templates) {
            availableTemplateIds.push_back(map.id);
            const int index=(int)availableMaps.size()+(int)availableTemplateIds.size()+1;
            mapChooser.addItem("Saved: "+map.name,index);
            if (editTemplateId==map.id) initial=index;
        }
        mapChooser.setSelectedId(initial,juce::dontSendNotification);
        mapChooser.onChange=[this] {
            if (!owner) return;
            const int index=mapChooser.getSelectedId()-2;
            const auto maps=availableMaps;
            const auto templates=availableTemplateIds;
            auto safe=juce::Component::SafePointer<MainComponent>(owner.getComponent());
            juce::MessageManager::callAsync([safe,maps,templates,index] {
                if (!safe) return;
                if (index<0) safe->openControlPanel(false);
                else if (index<(int)maps.size())
                    safe->editControlSet(maps[(size_t)index].first,maps[(size_t)index].second);
                else if (index<(int)maps.size()+(int)templates.size())
                    safe->openControlPanel(false,templates[(size_t)(index-(int)maps.size())]);
            });
        };
        newMap.onClick=[this]{createMap(false);};
        cloneMap.onClick=[this]{createMap(true);};
        renameMap.onClick=[this]{renameSelectedMap();};
        deleteMap.onClick=[this]{deleteSelectedMap();};
        renameMap.setEnabled(editTemplateId>0);
        deleteMap.setEnabled(editTemplateId>0);
        renameMap.setVisible(false);
        deleteMap.setVisible(false);
        mapMore.onClick=[this] {showMapMenu();};
        functionChoice.setName("mappingAbstractFunction");
        functionName.setName("mappingAbstractFunctionName");
        functionName.setTextToShowWhenEmpty("Function name (e.g. Resolume Brightness)",palette::muted);
        newFunction.setButtonText("Manage functions");
        saveRoute.setButtonText("Save route");
        newFunction.setName("createControlFunction");
        saveRoute.setName("saveControlFunctionRoute");
        saveControllerPreset.setName("exportControllerPreset");
        loadControllerPreset.setName("importControllerPreset");
        saveControllerPreset.setButtonText("Export Default Map");
        loadControllerPreset.setButtonText("Import Default Map");
        saveControllerPreset.setVisible(false);
        loadControllerPreset.setVisible(false);
        saveControllerPreset.setTooltip("Export hardware inputs and destinations without project-specific function names.");
        loadControllerPreset.setTooltip("Import routing by function order; preserve project names, other functions and cue-local overrides.");
        saveControllerPreset.onClick=[this] {
            if (!owner) return;
            const auto preset=owner->exportControlPresetText();
            presetFileChooser=std::make_unique<juce::FileChooser>(
                "Export Default Map",juce::File::getSpecialLocation(
                    juce::File::userDocumentsDirectory)
                    .getChildFile("TheCuelist-DefaultMap.json"),"*.json");
            auto safe=juce::Component::SafePointer<ControlPanel>(this);
            presetFileChooser->launchAsync(
                juce::FileBrowserComponent::saveMode |
                juce::FileBrowserComponent::canSelectFiles,
                [safe,preset](const juce::FileChooser& chooser) {
                    if (!safe) return;
                    const auto path=chooser.getResult();
                    if (path==juce::File()) return;
                    safe->state.setText(path.replaceWithText(preset) ?
                        "Default Map preset exported without function labels." :
                        "Could not save mapping preset.",juce::dontSendNotification);
                });
        };
        loadControllerPreset.onClick=[this] {
            if (!owner) return;
            presetFileChooser=std::make_unique<juce::FileChooser>(
                "Import Default Map",juce::File::getSpecialLocation(
                    juce::File::userDocumentsDirectory),"*.json");
            auto safe=juce::Component::SafePointer<ControlPanel>(this);
            presetFileChooser->launchAsync(
                juce::FileBrowserComponent::openMode |
                juce::FileBrowserComponent::canSelectFiles,
                [safe](const juce::FileChooser& chooser) {
                    if (!safe || !safe->owner) return;
                    const auto file=chooser.getResult();
                    if (file==juce::File()) return;
                    if (file.getSize()>256*1024) {
                        safe->state.setText("Mapping preset exceeds 256KB.",
                                            juce::dontSendNotification);
                        return;
                    }
                    const auto result=safe->owner->importControlPresetText(
                        file.loadFileAsString());
                    if (result.wasOk()) {
                        safe->refreshFunctions();
                        safe->bindingList.updateContent();
                        safe->bindingList.repaint();
                        safe->selected=-1;
                        safe->remove.setEnabled(false);
                        safe->updateSelected.setEnabled(false);
                        safe->refreshEmptyState();
                    }
                    safe->state.setText(result.wasOk() ?
                        "Imported routes and inputs; project labels retained." :
                        result.getErrorMessage(),juce::dontSendNotification);
                });
        };
        newFunction.setVisible(!editSet);
        saveRoute.setVisible(!editSet);
        newFunction.setTooltip("Add, delete or reorder MIDI/OSC functions. Their names remain project-specific.");
        saveRoute.setTooltip("Save the selected function destination globally; physical input assignments stay unchanged.");
        functionChoice.onChange=[this] {
            if (!owner) return;
            const int id=functionChoice.getSelectedId()-1;
            scope.setSelectedId(1,juce::dontSendNotification);
            scope.setEnabled(id==0 && !editSet);
            if (id==0 && !editSet) updateEditorVisibility();
            for (const auto& item:owner->show.controls.functions)
                if (item.id==id) {
                    functionName.setText(item.name,false);
                    showRoute(item.destination);
                    if (!editSet) updateEditorVisibility();
                    return;
                }
        };
        // Advanced function lifecycle (not part of the instant Learn flow).
        newFunction.onClick=[this] {
            if (!owner || editSet) return;
            juce::PopupMenu menu;
            menu.addItem(1,"+ Add MIDI Function");
            menu.addItem(2,"+ Add OSC Function");
            menu.addSeparator();
            const bool hasFunction=functionChoice.getSelectedId()>1;
            menu.addItem(3,"Move function up",hasFunction);
            menu.addItem(4,"Move function down",hasFunction);
            menu.addItem(5,"Delete function and its bindings",hasFunction);
            auto safe=juce::Component::SafePointer<ControlPanel>(this);
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&newFunction),
                [safe](int choice) {
                    if (!safe || !safe->owner || choice<=0) return;
                    auto& panel=*safe.getComponent();
                    auto& main=*panel.owner.getComponent();
                    const int selectedId=panel.functionChoice.getSelectedId()-1;
                    if (choice==1 || choice==2) {
                        ControlBinding destination;
                        destination.target=choice==1 ? ControlTarget::midiCc : ControlTarget::oscFloat;
                        destination.maximum=choice==1 ? 127.0f : 1.0f;
                        for (const auto& col:main.show.columns)
                            if ((choice==1 && col.type==ColumnType::midi) ||
                                (choice==2 && col.type==ColumnType::osc)) {
                                destination.targetColumnId=col.id;break;
                            }
                        const juce::String base=choice==1 ? "MIDI Function " : "OSC Function ";
                        // This field shows the selected function's current name.
                        // Never accidentally copy it when creating a new slot.
                        const auto name=base+juce::String((int)main.show.controls.functions.size()+1);
                        const int id=main.createAbstractControlFunction(name,destination);
                        if (id>0) {
                            panel.refreshFunctions();
                            panel.functionChoice.setSelectedId(id+1);
                            panel.state.setText("Function created. Use Learn to assign an input.",
                                juce::dontSendNotification);
                        } else panel.state.setText(main.status,juce::dontSendNotification);
                    } else if (choice==3 || choice==4) {
                        if (main.moveAbstractControlFunction(selectedId,choice==3 ? -1 : 1)) {
                            panel.refreshFunctions();
                            panel.functionChoice.setSelectedId(selectedId+1,
                                juce::dontSendNotification);
                            panel.state.setText("Function reordered.",juce::dontSendNotification);
                        }
                    } else if (choice==5) {
                        if (main.deleteAbstractControlFunction(selectedId)) {
                            panel.selected=-1;panel.bindingList.deselectAllRows();
                            panel.refreshFunctions();
                            panel.bindingList.updateContent();panel.bindingList.repaint();
                            panel.refreshEmptyState();
                            panel.state.setText("Function and its mappings deleted.",
                                juce::dontSendNotification);
                        } else panel.state.setText(main.status,juce::dontSendNotification);
                    }
                });
        };
        saveRoute.onClick=[this] {
            if (!owner || functionChoice.getSelectedId()<2) return;
            const auto route=readRoute();
            if (!route) return;
            const bool ok=owner->updateAbstractControlFunction(
                functionChoice.getSelectedId()-1,functionName.getText(),*route);
            state.setText(ok ? "Function output updated; input assignment unchanged." :
                           owner->status,juce::dontSendNotification);
            if (ok) refreshFunctions();
        };
        device.setName("controlInputDevice");
        device.setTooltip("Select a device explicitly. No automatic fallback on disconnect.");
        connect.setButtonText("Refresh ports");
        learn.setButtonText("MIDI Learn");
        oscLearn.setButtonText("OSC Learn");
        oscLearn.setName("mappingOscLearn");
        oscInputPort.setName("mappingOscInputPort");
        oscInputPort.setInputRestrictions(5,"0123456789");
        oscInputPort.setText(juce::String(main.show.controls.oscInputPort==0 ?
                                    9011 : main.show.controls.oscInputPort),false);
        oscInputPort.setTooltip("UDP input port; OSC listener receives single numeric/boolean OSC 1.0 messages.");
        oscLearn.setTooltip("Listen for an OSC input address. Only the selected mapping scope is edited.");
        oscLearn.onClick=[this] {
            if (!owner) return;
            if (owner->oscInput.port()==0) {
                state.setText("Enable an OSC UDP input in Settings first.",
                              juce::dontSendNotification);return;
            }
            continuousLearn=2;captured.reset();
            owner->armOscControlLearn();
            state.setText("Capture OSC armed on configured UDP ports. Send a supported message.",
                          juce::dontSendNotification);
        };
        add.setButtonText("+ Add Mapping");
        updateSelected.setButtonText("Update selected");
        updateSelected.setName("mappingUpdateSelected");
        updateSelected.setEnabled(false);
        remove.setEnabled(false);
        emptyBindings.setName("mappingEmptyState");
        emptyBindings.setText(editSet ?
             "No local overrides. Functions inherit the Default Map." :
             "This map has no inputs. Add a mapping or use MIDI/OSC Learn.",
             juce::dontSendNotification);
        emptyBindings.setColour(juce::Label::textColourId,palette::muted);
        emptyBindings.setFont(font(13.5f));
        emptyBindings.setJustificationType(juce::Justification::centred);
        emptyBindings.setInterceptsMouseClicks(false,false);
        updateSelected.setButtonText("Save Mapping");
        updateSelected.onClick=[this] { createBinding(!addingMapping); };
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
        templateName.setVisible(editSet && editTemplateId==0);
        templates.setVisible(editSet && editTemplateId==0);
        saveTemplate.setVisible(editSet && editTemplateId==0);
        loadTemplate.setVisible(editSet && editTemplateId==0);
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
        // Preserve legacy Cue Presets in serialized shows, but do not
        // manage cue assignments from the map archive window.
        presets.setVisible(false);newPreset.setVisible(false);
        assignPreset.setVisible(false);presetName.setVisible(false);
        target.addItemList({"Master Audio","Audio column gain","GO next cue","PANIC",
                            "MIDI CC","OSC Float","Audio column pan","Master pan",
                             "Audio Mute","Phase / Polarity","L/R Send","Limiter On/Off"},1);
        target.setSelectedId(1);
        target.onChange=[this] {
            refreshColumns();
            refreshDestinationFields();
            const int t=target.getSelectedId();
            minimum.setText(t==7 || t==8 ? "-1" : "0",false);
            maximum.setText(t==5 ? "127" : "1",false);
        };
        outputChannel.setName("mappingOutputMidiChannel");
        outputChannel.setText("1");outputChannel.setInputRestrictions(2,"0123456789");
        outputChannel.setTooltip("Outgoing MIDI channel, independent of the source controller.");
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
            if (!owner) return;
            const bool connected=owner->midiInput.connected() ||
                std::any_of(owner->additionalMidiInputs.begin(),owner->additionalMidiInputs.end(),
                    [](const auto& entry){return entry.second->connected();});
            if (!connected) {
                state.setText("Enable a MIDI input in Settings first.",
                              juce::dontSendNotification);return;
            }
            continuousLearn=1;captured.reset(); owner->armControlLearn();
            state.setText(selected>=0 && currentBindings() && selected<(int)currentBindings()->size() ?
                "MIDI Learn for mapping #"+juce::String((*currentBindings())[(size_t)selected].id)+
                    " — move its hardware control, then click Update selected." :
                "MIDI Learn for NEW mapping — move a controller control.",juce::dontSendNotification);
        };
        add.onClick=[this] {
            if (editSet) { createBinding(false);return; }
            selected=-1;addingMapping=true;editingMapping=true;captured.reset();
            bindingList.deselectAllRows();
            inputKind.setSelectedId(1,juce::dontSendNotification);
            inputChannel.setText("1",false);inputNumber.setText("0",false);
            inputAddress.setText("/control",false);
            updateEditorVisibility();
            state.setText("New mapping: choose input/function, then Save Mapping or Learn.",
                          juce::dontSendNotification);
        };
        remove.onClick=[this]{
            const auto* bindings=currentBindings();
            if (!owner || !bindings || selected<0 || selected>=(int)bindings->size()) return;
            const int id=(*bindings)[(size_t)selected].id;
            int row=-1,column=-1;
            const bool removed=editTemplateId>0 ?
                owner->modifyReusableControlBinding(editTemplateId,id,{},2) :
                editSet ? (resolveSource(row,column) &&
                    owner->deleteControlSetBinding(row,column,id)) :
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
        // Device enablement and UDP sockets belong to Settings; these buttons
        // only capture a source input for the selected map/assignment.
        device.setVisible(false);connect.setVisible(false);oscInputPort.setVisible(false);
        learn.setVisible(true);oscLearn.setVisible(true);
        learn.setButtonText("MIDI Learn");
        oscLearn.setButtonText("OSC Learn");
        learn.setName("mappingCaptureMidi");
        oscLearn.setName("mappingCaptureOsc");
        refreshDevices(); refreshPresets(); refreshColumns();refreshTemplates();
        refreshFunctions();
        if (!owner->show.controls.functions.empty()) {
            const auto& f=owner->show.controls.functions.front();
            functionName.setText(f.name,false);
            showRoute(f.destination);
        }
        if (editSet) {
            for (juce::Component* widget : std::initializer_list<juce::Component*>{
                    &target,&outputChannel,&outputCc,&oscAddress,&columns,
                    &minimum,&maximum,&functionName})
                widget->setEnabled(false);
        }
        refreshDestinationFields();
        refreshEmptyState();
        updateEffectivePreview();
        if (!editSet) { editingMapping=false; updateEditorVisibility(); }
        setSize(860,editSet ? 764 : 790); startTimerHz(10);
    }
    ~ControlPanel() override {
        stopTimer();
        if (owner) {
            owner->controlLearnArmed=false;owner->oscLearnArmed=false;
            owner->learnedInput.reset();
        }
        bindingList.setModel(nullptr);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(palette::panel);
        if (!editSet) {
            const int w=getWidth();
            label(g,"CONTROL MAPPINGS",{22,12,w-44,36},22,palette::teal,true);
            label(g,"MAP SELECTION",{22,61,420,20},12,palette::muted,true);
            g.setColour(palette::line);
            g.drawHorizontalLine(119,20,(float)w-20);
            label(g,"MAPPING LIST",{22,200,w-44,24},12,palette::teal,true);
            const int split1=(w-44)*38/100;
            const int split2=(w-44)*66/100;
            label(g,"FUNCTION",{29,235,split1-10,22},11,palette::muted,true);
            label(g,"INPUT",{29+split1,235,split2-split1-10,22},11,palette::muted,true);
            label(g,"DESTINATION",{29+split2,235,w-split2-70,22},11,palette::muted,true);
            g.setColour(palette::line);
            g.drawHorizontalLine(259,22,(float)w-22);
            if (editingMapping) {
                g.drawHorizontalLine(477,22,(float)w-22);
                label(g,addingMapping?"ADD MAPPING":"EDIT MAPPING",
                      {22,483,380,26},13,palette::teal,true);
                label(g,"Function",{22,520,195,18},11,palette::muted,true);
                label(g,"Name",{232,520,170,18},11,palette::muted,true);
                label(g,"INPUT",{22,577,185,18},11,palette::teal,true);
                label(g,"Kind",{22,600,100,18},10,palette::muted,true);
                label(g,"Channel",{156,600,70,18},10,palette::muted,true);
                label(g,"CC / Note",{235,600,100,18},10,palette::muted,true);
                label(g,"OSC address",{346,600,200,18},10,palette::muted,true);
                label(g,"DESTINATION / BEHAVIOUR",{22,654,300,18},11,palette::teal,true);
                label(g,"Target",{22,675,150,17},10,palette::muted,true);
                label(g,"Column",{215,675,160,17},10,palette::muted,true);
                label(g,"Takeover",{410,675,110,17},10,palette::muted,true);
                label(g,"Curve",{531,675,110,17},10,palette::muted,true);
                label(g,"Output",{22,726,140,17},10,palette::muted,true);
                label(g,"Range",{415,726,140,17},10,palette::muted,true);
            } else {
                label(g,"Select a mapping to edit its input, function and destination.",
                    {22,493,w-44,28},13,palette::muted);
            }
            return;
        }
        label(g,editTemplateId>0 ? "SAVED CONTROL MAP" :
              editSet ? "CONTROL SET / Q"+juce::String(sourceRow+1) :
              "DEFAULT MAP / CONTROL MAPPINGS",{20,15,620,31},21,palette::teal,true);
        g.setColour(palette::line);
        g.drawHorizontalLine(52,19,(float)getWidth()-19);
        label(g,"MAP SELECTION",{20,60,350,19},12,palette::teal,true);
        label(g,"Saved maps live here; Control Column cells assign them to cues.",
              {320,136,getWidth()-340,24},12,palette::muted);

        g.setColour(palette::line);
        g.drawHorizontalLine(191,20,(float)getWidth()-20);
        // Function selection occupies the 198-224px band; do not draw
        // a section heading over the active combo box.
        label(g,editSet?"Control Set scope":"Scope",{20,228,179,18},11,palette::muted,true);
        label(g,"Target",{214,228,178,18},11,palette::muted,true);
        label(g,"Takeover",{411,228,132,18},11,palette::muted,true);
        label(g,"Destination column",{556,228,207,18},11,palette::muted,true);
        label(g,"OUT CH",{20,285,48,18},10,palette::muted,true);
        label(g,"CC",{78,285,52,18},10,palette::muted,true);
        label(g,"OSC address",{142,285,184,18},11,palette::muted,true);
        label(g,"Curve",{441,285,114,18},11,palette::muted,true);
        label(g,"Min",{573,285,84,18},11,palette::muted,true);
        label(g,"Max",{672,285,84,18},11,palette::muted,true);

        if (editSet) {
            label(g,"EFFECTIVE MAPPING",{20,347,330,18},12,palette::teal,true);
            if (editTemplateId==0)
                label(g,"REUSABLE CONTROL MAPS",{20,456,330,18},12,palette::teal,true);
        } else {
            label(g,"CUE PRESETS",{20,347,250,18},12,palette::teal,true);
            label(g,"New preset name",{388,347,220,18},11,palette::muted,true);
        }
        label(g,editTemplateId>0 ? "MAPPINGS IN SAVED MAP" :
              editSet?"MAPPINGS IN THIS CONTROL SET":"DEFAULT MAP ASSIGNMENTS",
              {20,editSet?534:434,380,15},11,palette::teal,true);
        g.setColour(palette::line);
        g.drawHorizontalLine(getHeight()-48,20,(float)getWidth()-20);
        label(g,editTemplateId>0 ?
            "Named maps are shared by assigned cues; edit with care. Deletion requires no assignments." :
            editSet ? "Control overrides apply after GO; removing one restores Default Map." :
            "Default Map always remains active as the lowest-priority fallback.",
            {20,getHeight()-40,getWidth()-40,23},11,palette::muted);
    }
    void resized() override {
        const int w=getWidth();
        if (!editSet) {
            mapChooser.setBounds(22,86,w-352,30);
            newMap.setBounds(w-318,86,112,30);
            cloneMap.setBounds(w-198,86,115,30);
            mapMore.setBounds(w-75,86,52,30);
            learn.setBounds(22,136,133,31);
            oscLearn.setBounds(164,136,133,31);
            cancelLearn.setBounds(305,136,110,31);
            add.setBounds(423,136,151,31);
            newFunction.setBounds(582,136,w-604,31);
            state.setBounds(22,170,w-44,21);
            const int listHeight=editingMapping?205:410;
            bindingList.setBounds(22,264,w-44,listHeight);
            emptyBindings.setBounds(34,286,w-68,48);
            functionChoice.setBounds(22,539,198,29);
            functionName.setBounds(232,539,215,29);
            saveRoute.setBounds(456,539,144,29);
            updateSelected.setBounds(w-185,539,163,32);
            remove.setBounds(w-185,578,163,31);
            inputKind.setBounds(22,620,127,31);
            inputChannel.setBounds(156,620,72,31);
            inputNumber.setBounds(235,620,101,31);
            inputAddress.setBounds(346,620,194,31);
            target.setBounds(22,693,185,29);
            columns.setBounds(215,693,187,29);
            takeover.setBounds(410,693,114,29);
            curve.setBounds(531,693,130,29);
            scope.setBounds(668,693,w-690,29);
            outputChannel.setBounds(22,743,54,28);
            outputCc.setBounds(83,743,60,28);
            oscAddress.setBounds(151,743,240,28);
            minimum.setBounds(415,743,92,28);
            maximum.setBounds(516,743,92,28);
            invert.setBounds(620,742,110,31);
            return;
        }
        mapChooser.setBounds(20,97,w-314,31);
        newMap.setBounds(w-285,97,125,31);
        cloneMap.setBounds(w-150,97,130,31);
        renameMap.setBounds(20,132,139,28);
        deleteMap.setBounds(168,132,141,28);
        device.setBounds(20,105,w-177,31);
        connect.setBounds(w-143,105,123,31);
        learn.setBounds(20,163,148,32);
        oscLearn.setBounds(176,163,148,32);
        oscInputPort.setBounds(269,163,68,32);
        state.setBounds(337,162,w-357,33);
        functionChoice.setBounds(20,198,185,26);
        functionName.setBounds(213,198,228,26);
        newFunction.setBounds(449,198,140,26);
        saveRoute.setBounds(597,198,w-617,26);

        scope.setBounds(20,250,176,31);
        target.setBounds(214,250,178,31);
        takeover.setBounds(411,250,133,31);
        columns.setBounds(556,250,w-576,31);
        outputChannel.setBounds(20,307,48,31);
        outputCc.setBounds(78,307,54,31);
        oscAddress.setBounds(142,307,181,31);
        invert.setBounds(336,307,96,31);
        curve.setBounds(441,307,115,31);
        minimum.setBounds(573,307,85,31);
        maximum.setBounds(672,307,85,31);

        effectivePreview.setBounds(20,372,w-40,72);
        templateName.setBounds(20,479,169,31);
        templates.setBounds(198,479,229,31);
        saveTemplate.setBounds(437,479,137,31);
        loadTemplate.setBounds(584,479,w-604,31);
        presets.setBounds(20,370,195,31);
        assignPreset.setBounds(223,370,153,31);
        presetName.setBounds(388,370,w-548,31);
        newPreset.setBounds(w-149,370,129,31);

        saveControllerPreset.setBounds(20,403,173,29);
        loadControllerPreset.setBounds(201,403,173,29);
        const int listY=editSet?558:449;
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
    int editTemplateId=0;
    int sourceRow=0,sourceColumn=0;
    int sourceRowId=0,sourceColumnId=0;
    int continuousLearn=0; // 0=off, 1=MIDI, 2=OSC
    void updateInputVisibility() {
        if (editSet) return;
        const bool osc=inputKind.getSelectedId()==4;
        inputAddress.setVisible(editingMapping && osc);
        inputChannel.setVisible(editingMapping && !osc);
        inputNumber.setVisible(editingMapping && !osc &&
                               inputKind.getSelectedId()!=3);
    }
    void updateEditorVisibility() {
        if (editSet) return; // Historical per-cue override editor is separate.
        for (juce::Component* widget:std::initializer_list<juce::Component*>{
            &functionChoice,&functionName,&saveRoute,&scope,&target,
            &takeover,&columns,&outputChannel,&outputCc,&oscAddress,
            &invert,&curve,&minimum,&maximum,&inputKind,&inputChannel,
            &inputNumber,&inputAddress,&updateSelected,&remove})
            widget->setVisible(editingMapping);
        functionName.setVisible(editingMapping &&
                                functionChoice.getSelectedId()>1);
        saveRoute.setVisible(editingMapping &&
                             functionChoice.getSelectedId()>1);
        updateSelected.setEnabled(editingMapping &&
                                  (addingMapping || selected>=0));
        remove.setEnabled(editingMapping && !addingMapping && selected>=0);
        updateInputVisibility();
        if (editingMapping) refreshDestinationFields();
        resized();
        repaint();
    }
    void showMapMenu() {
        if (!owner || editSet) return;
        juce::PopupMenu menu;
        const bool saved=editTemplateId>0;
        menu.addItem(1,"Rename Map",saved);
        menu.addItem(2,"Delete Map",saved);
        menu.addSeparator();
        menu.addItem(3,"Export Map...");
        menu.addItem(4,"Import Map...");
        if (!saved) {
            menu.addSeparator();
            menu.addItem(5,"Export legacy controller preset...");
            menu.addItem(6,"Import legacy controller preset...");
        }
        auto safe=juce::Component::SafePointer<ControlPanel>(this);
        menu.showMenuAsync(
            juce::PopupMenu::Options().withTargetComponent(&mapMore),
            [safe](int choice) {
                if (!safe || !safe->owner) return;
                auto* panel=safe.getComponent();
                if (choice==1) {panel->renameSelectedMap();return;}
                if (choice==2) {panel->deleteSelectedMap();return;}
                // Preserve the original controller preset interchange format
                // for existing operator files, separately from map-only JSON.
                if (choice==5) {panel->saveControllerPreset.triggerClick();return;}
                if (choice==6) {panel->loadControllerPreset.triggerClick();return;}
                if (choice==3) {
                    const auto json=exportControlMap(panel->owner->show,
                                                       panel->editTemplateId);
                    if (json.isEmpty()) {
                        panel->state.setText("Cannot export the selected map.",
                                             juce::dontSendNotification);return;
                    }
                    panel->presetFileChooser=std::make_unique<juce::FileChooser>(
                        "Export Control Map",
                        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                            .getChildFile("TheCuelist-ControlMap.json"),"*.json");
                    panel->presetFileChooser->launchAsync(
                        juce::FileBrowserComponent::saveMode |
                        juce::FileBrowserComponent::canSelectFiles,
                        [safe,json](const juce::FileChooser& chooser) {
                            if (!safe) return;
                            const auto file=chooser.getResult();
                            if (file==juce::File()) return;
                            safe->state.setText(file.replaceWithText(json) ?
                                "Selected map exported." : "Map export failed.",
                                juce::dontSendNotification);
                        });
                } else if (choice==4) {
                    panel->presetFileChooser=std::make_unique<juce::FileChooser>(
                        "Import Control Map",
                        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                        "*.json");
                    panel->presetFileChooser->launchAsync(
                        juce::FileBrowserComponent::openMode |
                        juce::FileBrowserComponent::canSelectFiles,
                        [safe](const juce::FileChooser& chooser) {
                            if (!safe || !safe->owner) return;
                            const auto file=chooser.getResult();
                            if (file==juce::File()) return;
                            if (file.getSize()>256*1024) {
                                safe->state.setText("Map file exceeds 256KB.",
                                    juce::dontSendNotification);return;
                            }
                            auto* main=safe->owner.getComponent();
                            auto candidate=main->show;
                            const auto result=importControlMap(file.loadFileAsString(),
                                                               candidate);
                            if (result.failed()) {
                                safe->state.setText(result.getErrorMessage(),
                                                    juce::dontSendNotification);
                                return;
                            }
                            const int id=candidate.controls.templates.back().id;
                            main->show=std::move(candidate);
                            main->dirty=true;
                            main->refreshControlSettings();main->refresh();
                            juce::Component::SafePointer<MainComponent> safeMain(main);
                            juce::MessageManager::callAsync([safeMain,id] {
                                if (safeMain) safeMain->openControlPanel(false,id);
                            });
                        });
                }
            });
    }
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
        if (editTemplateId>0) {
            for (const auto& map:owner->show.controls.templates)
                if (map.id==editTemplateId) return &map.set.bindings;
            return nullptr;
        }
        if (!editSet) return &owner->show.controls.bindings;
        int row=-1,columnIndex=-1;
        if (!resolveSource(row,columnIndex)) return nullptr;
        const auto& column=owner->show.columns[(size_t)columnIndex];
        if (column.type!=ColumnType::control) return nullptr;
        if (column.controlLocked) return &column.controlDefaultSet.bindings;
        const auto& cell=owner->show.rows[(size_t)row].cells[(size_t)columnIndex];
        return cell.controlSet ? &cell.controlSet->bindings : nullptr;
    }
    juce::ComboBox device,presets,scope,target,takeover,columns,curve,templates,functionChoice,mapChooser;
    juce::TextButton newMap,cloneMap,renameMap,deleteMap,mapMore,cancelLearn;
    juce::ComboBox inputKind;
    juce::TextEditor inputChannel,inputNumber,inputAddress;
    bool editingMapping=false,addingMapping=false;
    std::vector<std::pair<int,int>> availableMaps;
    std::vector<int> availableTemplateIds;
    void renameSelectedMap() {
        if (!owner || editTemplateId<=0) return;
        const int id=editTemplateId;
        auto found=std::find_if(owner->show.controls.templates.begin(),
            owner->show.controls.templates.end(),
            [id](const ControlSetTemplate& m){return m.id==id;});
        if (found==owner->show.controls.templates.end()) return;
        auto safe=juce::Component::SafePointer<MainComponent>(owner.getComponent());
        showOperatorTextPrompt(*owner,owner->theme,"Rename Control Map",
            "Rename the saved map. All cue references retain the same stable ID.",
            "Map name",found->name,false,"Rename",
            [safe,id](juce::String value){
                if (!safe) return;
                if (safe->renameReusableControlMap(id,value))
                    juce::MessageManager::callAsync([safe,id]{
                        if (safe) safe->openControlPanel(false,id);
                    });
            });
    }
    void deleteSelectedMap() {
        if (!owner || editTemplateId<=0) return;
        const int id=editTemplateId;
        const int used=owner->reusableControlMapUsage(id);
        if (used>0) {
            state.setText("Map assigned to "+juce::String(used)+
                " cue(s). Unassign or replace those references first.",
                juce::dontSendNotification);
            return;
        }
        auto safe=juce::Component::SafePointer<MainComponent>(owner.getComponent());
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::WarningIcon)
                .withTitle("Delete saved Control Map?")
                .withMessage("This removes the unused map from the archive. This cannot be undone.")
                .withButton("Delete Map").withButton("Cancel"),
            [safe,id](int result) {
                if (!safe || result!=1) return;
                if (safe->deleteReusableControlMap(id))
                    juce::MessageManager::callAsync([safe] {
                        if (safe) safe->openControlPanel(false);
                    });
            });
    }
    void createMap(bool duplicate) {
        if (!owner) return;
        ControlSet source;
        if (duplicate) {
            const auto* bindings=currentBindings();
            if (!bindings) {
                state.setText("No map selected to duplicate.",juce::dontSendNotification);
                return;
            }
            source.bindings=*bindings;
        }
        // Reusable maps are independent from Control Columns until assigned.
        const auto& saved=owner->show.controls.templates;
        juce::String title;
        int number=1;
        for (;;) {
            title=(duplicate ? "Copy of Map " : "Map ")+juce::String(number++);
            const bool exists=std::any_of(saved.begin(),saved.end(),
                [&title](const ControlSetTemplate& t){return t.name.equalsIgnoreCase(title);});
            if (!exists) break;
        }
        const int newId=owner->createReusableControlMap(title,source);
        if (newId==0) {
            state.setText(owner->status,juce::dontSendNotification);return;
        }
        auto safe=juce::Component::SafePointer<MainComponent>(owner.getComponent());
        juce::MessageManager::callAsync([safe,newId] {
            if (safe) safe->openControlPanel(false,newId);
        });
    }
    juce::TextButton connect,learn,oscLearn,add,remove,updateSelected,newPreset,assignPreset,saveTemplate,loadTemplate,
                     newFunction,saveRoute,saveControllerPreset,loadControllerPreset;
    juce::TextEditor presetName,outputCc,outputChannel,oscAddress,minimum,maximum,effectivePreview,templateName,functionName,oscInputPort;
    juce::ToggleButton invert{"Invert"};
    juce::Label state;
    juce::ListBox bindingList;
    juce::Label emptyBindings;
    std::vector<juce::String> deviceIds;
    std::vector<int> columnIds;
    std::optional<ControlEvent> captured;
    std::unique_ptr<juce::FileChooser> presetFileChooser;
    int selected=-1;
    void refreshFunctions() {
        if (!owner) return;
        const int old=functionChoice.getSelectedId();
        functionChoice.clear(juce::dontSendNotification);
        functionChoice.addItem("Legacy direct mapping",1);
        for (const auto& f:owner->show.controls.functions)
            functionChoice.addItem(f.name,f.id+1);
        functionChoice.setSelectedId(old>0 && functionChoice.indexOfItemId(old)>=0 ?
            old : owner->show.controls.functions.empty() ? 1 :
                  owner->show.controls.functions.front().id+1,juce::dontSendNotification);
        // Scope belongs to legacy bindings only. Named-function source
        // assignments in Default Map always inherit globally.
        scope.setEnabled(!editSet && functionChoice.getSelectedId()==1);
    }
    void showRoute(const ControlBinding& b) {
        const int targetIndex=b.target==ControlTarget::columnMute ? 9 :
             b.target==ControlTarget::columnPolarity ? 10 :
             b.target==ControlTarget::columnSend ? 11 :
             b.target==ControlTarget::limiterToggle ? 12 :
             b.target==ControlTarget::masterGain ? 1 :
            b.target==ControlTarget::columnGain ? 2 :
            b.target==ControlTarget::go ? 3 :
            b.target==ControlTarget::panic ? 4 :
            b.target==ControlTarget::midiCc ? 5 :
            b.target==ControlTarget::oscFloat ? 6 :
            b.target==ControlTarget::columnPan ? 7 : 8;
        target.setSelectedId(targetIndex,juce::dontSendNotification);
        refreshColumns();
        for (size_t i=0;i<columnIds.size();++i)
            if (columnIds[i]==b.targetColumnId)
                columns.setSelectedId((int)i+1,juce::dontSendNotification);
        outputChannel.setText(juce::String(b.outputChannel),false);
        outputCc.setText(juce::String(b.outputNumber),false);
        oscAddress.setText(b.oscAddress,false);
        minimum.setText(juce::String(b.minimum,3),false);
        maximum.setText(juce::String(b.maximum,3),false);
        refreshDestinationFields();
    }
    std::optional<ControlBinding> readRoute() {
        ControlBinding b;
        switch (target.getSelectedId()) {
            case 1: b.target=ControlTarget::masterGain;break;
            case 2: b.target=ControlTarget::columnGain;break;
            case 3: b.target=ControlTarget::go;break;
            case 4: b.target=ControlTarget::panic;break;
            case 5: b.target=ControlTarget::midiCc;break;
            case 6: b.target=ControlTarget::oscFloat;break;
            case 7: b.target=ControlTarget::columnPan;break;
            case 8: b.target=ControlTarget::masterPan;break;
            case 9: b.target=ControlTarget::columnMute;break;
            case 10: b.target=ControlTarget::columnPolarity;break;
            case 11: b.target=ControlTarget::columnSend;break;
            case 12: b.target=ControlTarget::limiterToggle;break;
            default:return {};
        }
        if (b.target==ControlTarget::columnGain ||
            (b.target==ControlTarget::columnPan || b.target==ControlTarget::columnMute || b.target==ControlTarget::columnPolarity || b.target==ControlTarget::columnSend) || b.target==ControlTarget::midiCc || b.target==ControlTarget::oscFloat) {
            const int index=columns.getSelectedId()-1;
            if (index<0 || index>=(int)columnIds.size()) {
                state.setText("Choose a valid output column.",juce::dontSendNotification);
                return {};
            }
            b.targetColumnId=columnIds[(size_t)index];
        }
        if (b.target==ControlTarget::midiCc) {
            b.outputNumber=outputCc.getText().getIntValue();
            b.outputChannel=outputChannel.getText().getIntValue(); // Not the incoming channel.
        }
        if (b.target==ControlTarget::oscFloat && b.functionId==0)
            b.oscAddress=oscAddress.getText().trim();
        const auto low=minimum.getText().trim(), high=maximum.getText().trim();
        const auto valid=[](const juce::String& v) {
            return v.isNotEmpty() && v.containsAnyOf("0123456789") &&
                v.retainCharacters("0123456789.-+")==v;
        };
        if (!valid(low) || !valid(high)) {
            state.setText("Enter valid minimum and maximum.",juce::dontSendNotification);
            return {};
        }
        b.minimum=low.getFloatValue();b.maximum=high.getFloatValue();
        return b;
    }
    void refreshDestinationFields() {
        const int t=target.getSelectedId();
        const bool needsColumn=scope.getSelectedId()==2 || t==2 || t==5 ||
             t==6 || t==7 || t==9 || t==10 || t==11;
        outputChannel.setEnabled(t==5 && !editSet);
        outputCc.setEnabled(t==5 && !editSet);
        oscAddress.setEnabled(!editSet && t==6);
        columns.setEnabled(!editSet && needsColumn);
        if (!editSet) {
            outputChannel.setVisible(editingMapping && t==5);
            outputCc.setVisible(editingMapping && t==5);
            oscAddress.setVisible(editingMapping && t==6);
            columns.setVisible(editingMapping && needsColumn);
            scope.setVisible(editingMapping &&
                             functionChoice.getSelectedId()==1);
        }
    }
    void refreshEmptyState() {
        emptyBindings.setVisible(getNumRows()==0);
        remove.setEnabled(selected>=0 && selected<getNumRows());
    }
    void timerCallback() override {
        if (!owner) return;
        // Armed Learn is an operator-critical state, not a transient tooltip.
        learn.setColour(juce::TextButton::buttonColourId,
            owner->controlLearnArmed ? palette::amber.withAlpha(.58f) : palette::raised);
        oscLearn.setColour(juce::TextButton::buttonColourId,
            owner->oscLearnArmed ? palette::amber.withAlpha(.58f) : palette::raised);
        if (auto event=owner->consumeLearnedInput()) {
            // Learn persists for consecutive assignments to the SELECTED
            // map. Once an input is received, its mapping is saved, while
            // the operator may choose another function without reopening
            // the hardware input listener.
            captured=event;
            if (!editSet) {
                editingMapping=true;
                updateEditorVisibility();
            }
            const auto* entries=currentBindings();
            createBinding(entries && selected>=0 && selected<(int)entries->size());
            if (continuousLearn==1) owner->armControlLearn();
            else if (continuousLearn==2) owner->armOscControlLearn();
        }
        if (editSet && editTemplateId==0) updateEffectivePreview();
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
            const bool eligible=(targetId==2 || targetId==7 || targetId==9 || targetId==10 || targetId==11) ? c.type==ColumnType::audio :
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
        if (!owner || (update && (!existing || selected<0 || selected>=(int)existing->size()))) {
            state.setText("Select a mapping or use + Add Mapping.",
                juce::dontSendNotification);return;
        }
        if (editSet && !update && !captured) {
            state.setText("Capture MIDI/OSC input for this cue override.",
                          juce::dontSendNotification);return;
        }
        ControlBinding b=update ? (*existing)[(size_t)selected] : ControlBinding{};
        // A Control Set overrides only which physical input invokes the
        // function. Target/destination remain owned by the project function.
        b.functionId=std::max(0,functionChoice.getSelectedId()-1);
        if (!update && b.functionId==0) {
            state.setText("Create or select a named function in Default Map first.",
                          juce::dontSendNotification);
            return;
        }
        if (captured) {
            b.deviceId=captured->deviceId;
            b.kind=captured->kind; b.channel=captured->channel; b.number=captured->number;
        } else if (!editSet) {
            const int k=inputKind.getSelectedId()-1;
            if (k<0 || k>3) return;
            b.kind=(ControlKind)k;
            b.channel=inputChannel.getText().getIntValue();
            b.number=inputNumber.getText().getIntValue();
            if (b.kind==ControlKind::osc) {
                b.deviceId=inputAddress.getText().trim();
                b.channel=1;b.number=0;
                if (!b.deviceId.startsWithChar('/')) {
                    state.setText("Input OSC address must begin with /.",
                                  juce::dontSendNotification);return;
                }
            } else {
                if (b.kind==ControlKind::pitchBend) b.number=0;
                if (b.deviceId.isEmpty() || b.deviceId.startsWithChar('/'))
                    b.deviceId=owner->show.controls.midiInputId;
                if (b.deviceId.isEmpty()) {
                    state.setText("Select MIDI input in Settings > MIDI Devices first.",
                                  juce::dontSendNotification);return;
                }
            }
        }
        // Named functions in the Default Map are global. Only the
        // selected cue's Control Set can override their physical inputs.
        b.scope=b.functionId>0 ? ControlScope::global :
            (ControlScope)(scope.getSelectedId()-1);
        switch (target.getSelectedId()) {
            case 1: b.target=ControlTarget::masterGain; break;
            case 2: b.target=ControlTarget::columnGain; break;
            case 3: b.target=ControlTarget::go; break;
            case 4: b.target=ControlTarget::panic; break;
            case 5: b.target=ControlTarget::midiCc; break;
            case 6: b.target=ControlTarget::oscFloat; break;
            case 7: b.target=ControlTarget::columnPan; break;
            case 8: b.target=ControlTarget::masterPan; break;
            case 9: b.target=ControlTarget::columnMute; break;
            case 10: b.target=ControlTarget::columnPolarity; break;
            case 11: b.target=ControlTarget::columnSend; break;
            case 12: b.target=ControlTarget::limiterToggle; break;
            default: return;
        }
        b.takeover=(ControlTakeover)(takeover.getSelectedId()-1);
        b.curve=(ControlCurve)(curve.getSelectedId()-1);
        b.scopeId=0; b.targetColumnId=0;
        if (b.scope==ControlScope::cue)
            b.scopeId=owner->show.rows[(size_t)owner->selectedRow].id;
        else if (b.scope==ControlScope::preset)
            b.scopeId=std::max(0,presets.getSelectedId()-1);
        if (b.functionId==0 && (b.scope==ControlScope::column ||
            b.target==ControlTarget::columnGain ||
            (b.target==ControlTarget::columnPan || b.target==ControlTarget::columnMute || b.target==ControlTarget::columnPolarity || b.target==ControlTarget::columnSend) || b.target==ControlTarget::midiCc || b.target==ControlTarget::oscFloat)) {
            const int i=columns.getSelectedId()-1;
            if (i<0 || i>=(int)columnIds.size()) {
                state.setText("Select a destination column.",juce::dontSendNotification);return;
            }
            b.targetColumnId=columnIds[(size_t)i];
            if (b.scope==ControlScope::column) b.scopeId=b.targetColumnId;
        }
        if (b.functionId>0) {
            const auto it=std::find_if(owner->show.controls.functions.begin(),
                owner->show.controls.functions.end(),
                [&](const ControlFunction& f){return f.id==b.functionId;});
            if (it==owner->show.controls.functions.end()) {
                state.setText("Select an existing function.",juce::dontSendNotification);
                return;
            }
            const auto input=b;
            b=materializeFunctionBinding(input,*it);
        }
        if (b.kind==ControlKind::osc) b.takeover=ControlTakeover::jump;
        if (b.kind==ControlKind::note || b.target==ControlTarget::go ||
            b.target==ControlTarget::panic ||
            b.target==ControlTarget::columnMute ||
            b.target==ControlTarget::columnPolarity ||
            b.target==ControlTarget::columnSend ||
            b.target==ControlTarget::limiterToggle)
            b.takeover=ControlTakeover::jump;
        b.invert=invert.getToggleState();
        if (b.target==ControlTarget::midiCc && b.functionId==0) {
            // Legacy direct mappings preserve historical behavior.
            b.outputChannel=b.channel;
            b.outputNumber=outputCc.getText().getIntValue();
        }
        if (b.target==ControlTarget::oscFloat && b.functionId==0)
            b.oscAddress=oscAddress.getText().trim();
        const auto low=minimum.getText().trim(),high=maximum.getText().trim();
        const auto numeric=[](const juce::String& value) {
            return value.isNotEmpty() && value.containsAnyOf("0123456789") &&
                value.retainCharacters("0123456789.-+")==value;
        };
        if (!numeric(low) || !numeric(high)) {
            state.setText("Enter valid minimum and maximum values.",juce::dontSendNotification); return;
        }
        // Named functions exclusively own output value ranges. Learn and
        // source updates cannot rewrite the destination's scaling or route.
        if (b.functionId==0) {
            b.minimum=low.getFloatValue();
            b.maximum=high.getFloatValue();
        }
        int row=-1,column=-1;
        const bool resolved=editTemplateId>0 || !editSet || resolveSource(row,column);
        const bool saved=resolved && (editTemplateId>0 ?
            owner->modifyReusableControlBinding(editTemplateId,b.id,b,update ? 1 : 0) :
            update ? owner->updateControlBinding(row,column,b.id,b,editSet) :
            editSet ? owner->addControlSetBinding(row,column,b) :
                      owner->addMappingBinding(b));
        if (saved) {
            bindingList.updateContent();bindingList.repaint();
            selected=-1;addingMapping=false;
            bindingList.deselectAllRows();
            if (!editSet) {
                editingMapping=false;
                updateEditorVisibility();
            } else {
                updateSelected.setEnabled(false);remove.setEnabled(false);
            }
            refreshEmptyState();captured.reset();updateEffectivePreview();
            state.setText(update ? "Mapping updated." :
                editTemplateId>0 ? "Mapping saved to selected reusable map." :
                    "Mapping saved to Default Map.",
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
            juce::String source=item.binding.functionId>0 ?
                "Default Map" : "Global / legacy";
            for (const auto& col:owner->show.columns)
                if (col.id==item.columnId) {source=col.name;break;}
            const auto& b=item.binding;
            const bool local=item.columnId==sourceColumnId;
            juce::String abstractName;
            for (const auto& function:owner->show.controls.functions)
                if (function.id==b.functionId) {
                    abstractName="FUNCTION "+function.name+" / ";break;
                }
            summary+="\n"+abstractName+juce::String(local ? (item.overrides ? "OVERRIDE / " : "LOCAL / ") :
                (item.overrides ? "INHERITED OVERRIDE / " : "INHERITED / "))+
                source+" ("+(item.priority==0 ? juce::String("legacy") :
                      "P"+juce::String(item.priority))+")  "+
                (b.kind==ControlKind::cc ? "CC " :
                 b.kind==ControlKind::note ? "NOTE " : b.kind==ControlKind::osc ? "OSC " : "BEND ")+
                juce::String(b.channel)+":"+juce::String(b.number)+
                " -> "+(b.target==ControlTarget::masterGain ? "MASTER GAIN" :
                  b.target==ControlTarget::masterPan ? "MASTER PAN" :
                  b.target==ControlTarget::columnGain ? "CHANNEL GAIN" :
                  b.target==ControlTarget::columnPan ? "CHANNEL PAN" :
                   b.target==ControlTarget::columnMute ? "MUTE" :
                   b.target==ControlTarget::columnPolarity ? "PHASE" :
                   b.target==ControlTarget::columnSend ? "L/R SEND" :
                   b.target==ControlTarget::limiterToggle ? "LIMITER" :
                  b.target==ControlTarget::midiCc ?
                    "MIDI CC "+juce::String(b.outputChannel)+":"+
                      juce::String(b.outputNumber) :
                  b.target==ControlTarget::oscFloat ?
                    "OSC "+b.oscAddress :
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
        if (!editSet && row>=0) {
            editingMapping=true;addingMapping=false;
            updateEditorVisibility();
        }
        const auto* entries=currentBindings();
        updateSelected.setEnabled(entries && row>=0 && row<(int)entries->size());
        remove.setEnabled(entries && row>=0 && row<(int)entries->size());
        if (!entries || row<0 || row>=(int)entries->size()) return;
        const auto& b=(*entries)[(size_t)row];
        captured.reset();
        inputKind.setSelectedId((int)b.kind+1,juce::dontSendNotification);
        inputChannel.setText(juce::String(b.channel),false);
        inputNumber.setText(juce::String(b.number),false);
        inputAddress.setText(b.kind==ControlKind::osc?b.deviceId:"/control",false);
        updateInputVisibility();
        scope.setSelectedId((int)b.scope+1,juce::dontSendNotification);
        scope.setEnabled(!editSet && b.functionId==0);
        functionChoice.setSelectedId(b.functionId>0 ? b.functionId+1 : 1,
                                     juce::dontSendNotification);
        // Combo target item order differs from ControlTarget enumeration.
        const ControlBinding* route=&b;
        for (const auto& f:owner->show.controls.functions)
            if (f.id==b.functionId) {route=&f.destination;break;}
        showRoute(*route);
        takeover.setSelectedId((int)b.takeover+1,juce::dontSendNotification);
        curve.setSelectedId((int)b.curve+1,juce::dontSendNotification);
        minimum.setText(juce::String(b.minimum,3),false);
        maximum.setText(juce::String(b.maximum,3),false);
        if (b.functionId==0) {
            outputCc.setText(juce::String(b.outputNumber),false);
            oscAddress.setText(b.oscAddress,false);
        }
        invert.setToggleState(b.invert,juce::dontSendNotification);
        state.setText("Editing #"+juce::String(b.id)+" — input "+b.deviceId+
            " ch"+juce::String(b.channel)+" / "+juce::String(b.number),
            juce::dontSendNotification);
    }
    void paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool active) override {
        if (!owner || row<0 || row>=getNumRows()) return;
        const auto& b=(*currentBindings())[(size_t)row];
        if (active) g.fillAll(palette::teal.withAlpha(.20f));
        if (!editSet) {
            juce::String function="Legacy direct";
            for(const auto& f:owner->show.controls.functions)
                if (f.id==b.functionId) {function=f.name;break;}
            const auto input=b.kind==ControlKind::osc ? "OSC "+b.deviceId :
                (b.kind==ControlKind::cc ? "MIDI CC " :
                 b.kind==ControlKind::note ? "MIDI Note " : "Pitch Bend ")+
                juce::String(b.kind==ControlKind::pitchBend ?
                             b.channel:b.number);
            const auto dest=b.target==ControlTarget::masterGain?"Master Volume" :
                b.target==ControlTarget::masterPan?"Master Pan" :
                b.target==ControlTarget::columnGain?"Channel Gain" :
                b.target==ControlTarget::columnPan?"Channel Pan" :
                b.target==ControlTarget::columnMute?"Channel Mute" :
                b.target==ControlTarget::columnPolarity?"Phase" :
                b.target==ControlTarget::columnSend?"L/R Send" :
                b.target==ControlTarget::limiterToggle?"Limiter On/Off" :
                b.target==ControlTarget::go?"GO" :
                b.target==ControlTarget::panic?"PANIC" :
                b.target==ControlTarget::midiCc ?
                    "MIDI CC "+juce::String(b.outputNumber) :
                    "OSC "+b.oscAddress;
            const int first=width*38/100,second=width*66/100;
            label(g,function,{8,0,first-16,height},12,
                  active?palette::text:palette::muted);
            label(g,input,{first+8,0,second-first-16,height},12,
                  active?palette::text:palette::muted);
            label(g,dest,{second+8,0,width-second-16,height},12,
                  active?palette::text:palette::muted);
            return;
        }
        juce::String targetName=b.target==ControlTarget::go ? "GO" :
            b.target==ControlTarget::panic ? "PANIC" :
            b.target==ControlTarget::masterGain ? "MASTER GAIN" :
            b.target==ControlTarget::masterPan ? "MASTER PAN" :
            b.target==ControlTarget::columnPan ? "PAN" :
            b.target==ControlTarget::columnMute ? "MUTE" :
            b.target==ControlTarget::columnPolarity ? "PHASE" :
            b.target==ControlTarget::columnSend ? "L/R SEND" :
            b.target==ControlTarget::limiterToggle ? "LIMITER" :
            b.target==ControlTarget::oscFloat ? "OSC" :
            b.target==ControlTarget::midiCc ? "MIDI CC" : "GAIN";
        juce::String displayFunctionName=targetName;
        for (const auto& function:owner->show.controls.functions)
            if (function.id==b.functionId) {
                displayFunctionName=function.name+" / "+targetName;break;
            }
        label(g,"#"+juce::String(b.id)+"   ch"+juce::String(b.channel)+
            " "+(b.kind==ControlKind::cc?"CC ":b.kind==ControlKind::note?"NOTE ":"BEND ")+
            juce::String(b.number)+" → "+displayFunctionName+
            "  ["+juce::String((int)b.scope)+"]",
            {7,0,width-14,height},12,active?palette::text:palette::muted);
    }
};
void MainComponent::openControlPanel(bool editSet,int templateId) {
    if (quickLearnMode!=QuickLearnMode::off) finishQuickLearn();
    if (!editSet) ensureStarterControlFunctions();
    if (controlDialog) {
        auto* previous=controlDialog.getComponent();
        controlDialog=nullptr;
        delete previous;
    }
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(new ControlPanel(*this,editSet,templateId));
    options.dialogTitle=templateId>0 ? "The Cuelist — Reusable Map" :
        editSet ? "The Cuelist — Control Set" : "The Cuelist — Control Mappings";
    options.dialogBackgroundColour=palette::panel;
    options.escapeKeyTriggersCloseButton=true;
    options.useNativeTitleBar=false;
    options.resizable=true;
    controlDialog=options.launchAsync();
    if (controlDialog)
        controlDialog->setLookAndFeel(&theme); // Match Exo/Hind and Midnight Slate
}
}
