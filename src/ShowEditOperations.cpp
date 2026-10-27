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

} // namespace cuelist
