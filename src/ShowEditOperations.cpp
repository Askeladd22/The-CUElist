#include "ShowEditOperations.h"
#include <algorithm>
#include <cstdint>
#include <utility>

namespace cuelist {

juce::Result swapCueRows(Show& show, int source, int destination) {
    if (source<0 || destination<0 || source>=(int)show.rows.size()
        || destination>=(int)show.rows.size())
        return juce::Result::fail("Cue reorder index outside the show.");
    std::swap(show.rows[(size_t)source],show.rows[(size_t)destination]);
    return juce::Result::ok();
}

juce::Result swapCueColumns(Show& show, int source, int destination) {
    if (source<0 || destination<0 || source>=(int)show.columns.size()
        || destination>=(int)show.columns.size())
        return juce::Result::fail("Column reorder index outside the show.");
    // Changing a rectangular show must never produce a ragged, type-misaligned
    // grid even if an external caller supplies malformed model data.
    for (const auto& row:show.rows)
        if (row.cells.size()!=show.columns.size())
            return juce::Result::fail("Cannot reorder nonrectangular cue cells.");
    std::swap(show.columns[(size_t)source],show.columns[(size_t)destination]);
    for (auto& row:show.rows)
        std::swap(row.cells[(size_t)source],row.cells[(size_t)destination]);
    return juce::Result::ok();
}

juce::Result prepareInsertedCueCopy(const Show& current, int afterIndex,
                                    const Row& source, Show& destination) {
    if (afterIndex<0 || afterIndex>=(int)current.rows.size()
        || source.cells.size()!=current.columns.size())
        return juce::Result::fail("Copied cue does not fit the current column layout.");
    if ((int64_t)current.nextId+1+(int64_t)source.cells.size()>=1000000000)
        return juce::Result::fail("Show identifier limit reached.");

    Show prepared=current;
    Row inserted=source;
    inserted.id=prepared.nextId++;
    for (auto& cell:inserted.cells)
        cell.id=prepared.nextId++;
    for (auto& cell:inserted.cells)
        if (cell.controlSet)
            for (auto& binding:cell.controlSet->bindings)
                binding.id=prepared.controls.nextId++;
    prepared.rows.insert(prepared.rows.begin()+afterIndex+1,std::move(inserted));
    if (auto result=validateShow(prepared);result.failed()) return result;

    destination=std::move(prepared);
    return juce::Result::ok();
}

namespace {
bool refersToColumn(const ControlBinding& binding, int columnId) {
    if (binding.targetColumnId!=columnId) return false;
    return binding.scope==ControlScope::column ||
        binding.target==ControlTarget::columnGain ||
        binding.target==ControlTarget::columnPan ||
        binding.target==ControlTarget::midiCc ||
        binding.target==ControlTarget::oscFloat;
}

void purgeColumnTargets(std::vector<ControlBinding>& bindings, int columnId) {
    bindings.erase(std::remove_if(bindings.begin(),bindings.end(),
        [columnId](const ControlBinding& binding) {
            return refersToColumn(binding,columnId);
        }),bindings.end());
}

void purgeColumnTargets(ControlSet& set, int columnId) {
    purgeColumnTargets(set.bindings,columnId);
}
}

juce::Result prepareDeletedCue(const Show& current, int index,
                               Show& destination, std::vector<int>& removedCellIds) {
    if (index<0 || index>=(int)current.rows.size())
        return juce::Result::fail("Cue delete index outside the show.");
    Show prepared=current;
    const int removedId=prepared.rows[(size_t)index].id;
    std::vector<int> removed;
    removed.reserve(prepared.rows[(size_t)index].cells.size());
    for (const auto& cell:prepared.rows[(size_t)index].cells) removed.push_back(cell.id);

    prepared.rows.erase(prepared.rows.begin()+index);
    if (prepared.rows.empty()) addRow(prepared,{});
    // A locked Control Column can remember which cue supplied its default
    // mapping. After that cue is deleted, unlocking already uses a safe
    // surviving-cue fallback; clear the stale source identity explicitly.
    for (auto& column:prepared.columns)
        if (column.type==ColumnType::control &&
            column.controlDefaultSourceRowId==removedId)
            column.controlDefaultSourceRowId=0;

    auto& bindings=prepared.controls.bindings;
    bindings.erase(std::remove_if(bindings.begin(),bindings.end(),
        [removedId](const ControlBinding& binding) {
            return binding.scope==ControlScope::cue && binding.scopeId==removedId;
        }),bindings.end());

    if (auto result=validateShow(prepared);result.failed()) return result;
    destination=std::move(prepared);
    removedCellIds=std::move(removed);
    return juce::Result::ok();
}

juce::Result prepareDeletedColumn(const Show& current, int index,
                                  Show& destination, std::vector<int>& removedCellIds) {
    if (index<0 || index>=(int)current.columns.size())
        return juce::Result::fail("Column delete index outside the show.");
    // Reject malformed input rather than accessing an absent cell or erasing
    // only part of a column. The caller receives no partial model/cache edits.
    for (const auto& row:current.rows)
        if (row.cells.size()!=current.columns.size())
            return juce::Result::fail("Cannot delete a column from a nonrectangular show.");

    Show prepared=current;
    const int removedId=prepared.columns[(size_t)index].id;
    std::vector<int> removed;
    removed.reserve(prepared.rows.size());
    for (auto& row:prepared.rows) {
        removed.push_back(row.cells[(size_t)index].id);
        row.cells.erase(row.cells.begin()+index);
    }

    prepared.columns.erase(prepared.columns.begin()+index);
    if (prepared.columns.empty()) addColumn(prepared,ColumnType::audio,{});

    purgeColumnTargets(prepared.controls.bindings,removedId);
    for (auto& column:prepared.columns)
        purgeColumnTargets(column.controlDefaultSet,removedId);
    for (auto& row:prepared.rows) for (auto& cell:row.cells)
        if (cell.controlSet) purgeColumnTargets(*cell.controlSet,removedId);
    for (auto& item:prepared.controls.templates)
        purgeColumnTargets(item.set,removedId);

    if (auto result=validateShow(prepared);result.failed()) return result;
    destination=std::move(prepared);
    removedCellIds=std::move(removed);
    return juce::Result::ok();
}

} // namespace cuelist
