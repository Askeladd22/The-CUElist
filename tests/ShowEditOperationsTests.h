#pragma once
#include "ShowEditOperations.h"

void showEditOperationsTests() {
    test("pure row and column moves preserve IDs and typed cell associations",[] {
        auto show=makeShow();
        addColumn(show,ColumnType::midi,"MIDI");
        addRow(show,"Second");
        const int originalRow=show.rows[0].id;
        const int audioColumn=show.columns[0].id;
        const int audioCell=show.rows[0].cells[0].id;
        const int midiCell=show.rows[0].cells[1].id;
        show.rows[0].cells[1].midi=MidiClip{8,{{0,2,60,100,1}}};
        check(swapCueColumns(show,0,1).wasOk(),"Cannot swap model columns");
        check(show.columns[1].id==audioColumn &&
              show.rows[0].cells[0].id==midiCell &&
              show.rows[0].cells[1].id==audioCell &&
              show.rows[0].cells[0].midi.has_value(),
              "Column move broke the cell/column type relationship");
        check(swapCueRows(show,0,1).wasOk() &&
              show.rows[1].id==originalRow &&
              show.rows[1].cells[0].id==midiCell,
              "Row move did not preserve cue identity");
        check(validateShow(show).wasOk(),"Pure reordering created invalid show");
    });
    test("invalid pure edits leave the input show unchanged",[] {
        auto show=makeShow();
        addRow(show,"Another");
        const auto original=encodeShow(show);
        check(swapCueRows(show,0,100).failed() &&
              swapCueColumns(show,-1,0).failed() &&
              encodeShow(show)==original,
              "Out-of-range reorder changed the model");
        show.rows[1].cells.clear(); // deliberate ragged fixture
        const auto ragged=encodeShow(show);
        check(swapCueColumns(show,0,0).failed() && encodeShow(show)==ragged,
              "Malformed grid was partially moved");
    });
    test("copied cue reassigns row, cell and private control binding identities",[] {
        auto show=makeShow();
        addColumn(show,ColumnType::control,"Control");
        ControlBinding binding;
        binding.id=show.controls.nextId++;
        binding.deviceId="test-midi";
        binding.number=23;
        ControlSet set; set.bindings.push_back(binding);
        show.rows[0].cells[1].controlSet=set;
        check(validateShow(show).wasOk(),"Source Control Set fixture invalid");
        const auto old=show.rows[0];

        Show output=makeShow();
        check(prepareInsertedCueCopy(show,0,show.rows[0],output).wasOk(),
              "Cannot prepare copied model cue");
        check(show.rows.size()==1 && output.rows.size()==2,
              "Preparing a copy mutated the source model");
        const auto& inserted=output.rows[1];
        check(inserted.id!=old.id && inserted.cells.size()==2 &&
              inserted.cells[0].id!=old.cells[0].id &&
              inserted.cells[1].id!=old.cells[1].id,
              "Copied row/cell stable identifiers reused");
        check(inserted.cells[1].controlSet &&
              inserted.cells[1].controlSet->bindings.size()==1 &&
              inserted.cells[1].controlSet->bindings[0].id!=binding.id,
              "Copied Control Set shares physical mapping identifiers");
        check(validateShow(output).wasOk(),"Prepared copied model fails validation");
    });
    test("failed cue copy preserves destination and does not consume IDs",[] {
        auto show=makeShow();
        Show output=makeShow(); output.name="Unchanged destination";
        const auto original=encodeShow(output);
        Row mismatched=show.rows[0];
        mismatched.cells.push_back(Cell{});
        check(prepareInsertedCueCopy(show,0,mismatched,output).failed() &&
              encodeShow(output)==original,
              "Wrong-column clipboard modified destination");
        check(prepareInsertedCueCopy(show,9,show.rows[0],output).failed() &&
              encodeShow(output)==original,
              "Bad cue index modified destination");
        show.nextId=999999998;
        check(prepareInsertedCueCopy(show,0,show.rows[0],output).failed() &&
              encodeShow(output)==original,
              "Identifier exhaustion partially inserted copied cue");
    });
    test("transactional cue deletion removes cue-scoped mappings but retains other mappings",[] {
        auto source=makeShow();
        addRow(source,"Surviving cue");
        const int removedCueId=source.rows[0].id;
        const int survivingCueId=source.rows[1].id;
        const int removedCellId=source.rows[0].cells[0].id;
        const int survivingCellId=source.rows[1].cells[0].id;
        ControlBinding obsolete;
        obsolete.id=source.controls.nextId++;
        obsolete.deviceId="test-input";
        obsolete.number=22;
        obsolete.scope=ControlScope::cue;
        obsolete.scopeId=removedCueId;
        obsolete.target=ControlTarget::go;
        ControlBinding retained=obsolete;
        retained.id=source.controls.nextId++;
        retained.number=23;
        retained.scopeId=survivingCueId;
        source.controls.bindings={obsolete,retained};
        check(validateShow(source).wasOk(),"Cue deletion source fixture is invalid");

        Show output=makeShow();
        std::vector<int> removed{999};
        check(prepareDeletedCue(source,0,output,removed).wasOk(),
              "Cannot prepare deletion of a cue with legacy mappings");
        check(source.rows.size()==2 && source.controls.bindings.size()==2,
              "Deleting cue modified the source show");
        check(removed.size()==1 && removed[0]==removedCellId,
              "Deleted cue did not report its cached media identity");
        check(output.rows.size()==1 && output.rows[0].id==survivingCueId &&
              output.rows[0].cells[0].id==survivingCellId,
              "Deleting cue lost surviving identity");
        check(output.controls.bindings.size()==1 &&
              output.controls.bindings[0].id==retained.id,
              "Cue deletion retained a stale mapping or removed a valid one");
        check(validateShow(output).wasOk(),"Deleted cue candidate fails validation");
    });
    test("deleting last cue or column leaves valid default project geometry",[] {
        auto source=makeShow();
        const int oldCellId=source.rows[0].cells[0].id;
        const int oldColumnId=source.columns[0].id;
        Show rowResult;
        std::vector<int> removed;
        check(prepareDeletedCue(source,0,rowResult,removed).wasOk() &&
              rowResult.rows.size()==1 && rowResult.rows[0].cells.size()==1 &&
              rowResult.rows[0].cells[0].id!=oldCellId &&
              removed.size()==1 && removed[0]==oldCellId,
              "Removing the final cue failed to create a blank replacement");
        check(validateShow(rowResult).wasOk(),"Final-cue fallback is invalid");

        Show columnResult;
        check(prepareDeletedColumn(source,0,columnResult,removed).wasOk() &&
              columnResult.columns.size()==1 && columnResult.columns[0].id!=oldColumnId &&
              columnResult.columns[0].name=="Audio 1" &&
              columnResult.rows[0].cells.size()==1 &&
              columnResult.rows[0].cells[0].id!=oldCellId &&
              removed.size()==1 && removed[0]==oldCellId,
              "Removing the final column failed to create a usable Audio column");
        check(validateShow(columnResult).wasOk(),"Final-column fallback is invalid");
    });
    test("deleted audio column cleans global gain/pan mappings and every Control Set layer",[] {
        auto source=makeShow();
        addColumn(source,ColumnType::audio,"Remaining audio");
        addColumn(source,ColumnType::control,"Control");
        addRow(source,"Second cue");
        const int removedAudioId=source.columns[0].id;
        const int retainedAudioId=source.columns[1].id;
        const int controlId=source.columns[2].id;
        const int removedFirstCellId=source.rows[0].cells[0].id;
        const int removedSecondCellId=source.rows[1].cells[0].id;
        auto mapping=[&](int number, ControlTarget target, int targetId) {
            ControlBinding b;
            b.id=source.controls.nextId++;
            b.deviceId="controller";
            b.number=number;
            b.target=target;
            b.targetColumnId=targetId;
            if (target==ControlTarget::columnPan) { b.minimum=-1; b.maximum=1; }
            return b;
        };
        auto stalePan=mapping(1,ControlTarget::columnPan,removedAudioId);
        auto keptGain=mapping(2,ControlTarget::columnGain,retainedAudioId);
        auto staleScope=mapping(3,ControlTarget::masterGain,removedAudioId);
        staleScope.scope=ControlScope::column;
        source.controls.bindings={stalePan,keptGain,staleScope};

        auto& controlColumn=source.columns[2];
        controlColumn.controlLocked=true;
        controlColumn.controlPriority=1;
        controlColumn.controlDefaultSet.bindings.push_back(
            mapping(4,ControlTarget::columnPan,removedAudioId));

        ControlSet cueSet;
        cueSet.bindings.push_back(mapping(5,ControlTarget::columnGain,removedAudioId));
        source.rows[0].cells[2].controlSet=cueSet;

        ControlSetTemplate saved;
        saved.id=source.controls.nextId++;
        saved.name="Legacy target";
        saved.set.bindings.push_back(mapping(6,ControlTarget::columnPan,removedAudioId));
        source.controls.templates.push_back(saved);
        check(validateShow(source).wasOk(),"Column deletion mapping fixture invalid");

        Show output;
        std::vector<int> removed;
        check(prepareDeletedColumn(source,0,output,removed).wasOk(),
              "Column with dependent mappings could not be deleted");
        check(removed.size()==2 && removed[0]==removedFirstCellId &&
              removed[1]==removedSecondCellId,
              "Deleted column failed to enumerate audio cache keys");
        check(output.columns.size()==2 && output.columns[0].id==retainedAudioId &&
              output.columns[1].id==controlId &&
              output.rows[0].cells[0].id==source.rows[0].cells[1].id,
              "Deleting a column damaged retained cell/column identities");
        check(output.controls.bindings.size()==1 &&
              output.controls.bindings[0].id==keptGain.id &&
              output.controls.bindings[0].targetColumnId==retainedAudioId,
              "Deleted column left global pan/column-scoped references behind");
        check(output.columns[1].controlDefaultSet.bindings.empty() &&
              output.rows[0].cells[1].controlSet &&
              output.rows[0].cells[1].controlSet->bindings.empty() &&
              output.controls.templates.size()==1 &&
              output.controls.templates[0].set.bindings.empty(),
              "Deleted column left references in locked, cell or saved Control Sets");
        check(source.controls.bindings.size()==3 &&
              source.columns[2].controlDefaultSet.bindings.size()==1,
              "Column deletion mutated original mappings");
        check(validateShow(output).wasOk(),"Column deletion produced dangling mappings");
    });
    test("rejected model deletions preserve both outputs and media ID lists",[] {
        auto source=makeShow();
        Show destination=makeShow();
        destination.name="Untouched output";
        std::vector<int> removed={777};
        const auto before=encodeShow(destination);
        check(prepareDeletedCue(source,5,destination,removed).failed() &&
              prepareDeletedColumn(source,-1,destination,removed).failed() &&
              encodeShow(destination)==before &&
              removed==std::vector<int>{777},
              "Out-of-range deletion mutated committed result or media IDs");

        source.rows[0].cells.clear();
        check(prepareDeletedColumn(source,0,destination,removed).failed() &&
              encodeShow(destination)==before && removed==std::vector<int>{777},
              "Ragged model caused a partial deletion");
    });

}
