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
}
