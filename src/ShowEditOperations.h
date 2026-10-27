#pragma once
#include "ShowModel.h"

namespace cuelist {
// Pure show-model mutations. These never access GUI, audio, MIDI/OSC or media.
//
// Adjacent move operations preserve stable row, column and cell identities.
// Bounds failures leave the original Show unchanged.
juce::Result swapCueRows(Show& show, int source, int destination);
juce::Result swapCueColumns(Show& show, int source, int destination);

// Construct a new show with a copy of a cue inserted immediately after
// afterIndex. Copies row/cell properties but reassigns stable identities and
// private Control Set binding IDs. Only assign destination on success; no
// playback or PCM memory ownership is changed here.
juce::Result prepareInsertedCueCopy(const Show& current, int afterIndex,
                                    const Row& source, Show& destination);
// Transactional deletion: prepare a valid replacement Show and collect every
// removed cell identity so the UI can evict only that media from its cache.
// On failure, neither destination nor removedCellIds is changed.
// A deleted final row/column is replaced by an empty usable one.
juce::Result prepareDeletedCue(const Show& current, int index,
                               Show& destination, std::vector<int>& removedCellIds);
juce::Result prepareDeletedColumn(const Show& current, int index,
                                  Show& destination, std::vector<int>& removedCellIds);

} // namespace cuelist
