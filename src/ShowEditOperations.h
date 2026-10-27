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
} // namespace cuelist
