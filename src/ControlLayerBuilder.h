#pragma once
#include "ShowModel.h"

namespace cuelist {

// Pure transformation of the stored show into runtime mapping layers.
// Layer order follows visible column order (the rightmost column wins ties).
// Locked Control Columns apply globally; unlocked sets apply to their own cue.
// No devices, audio engine, UI state or MIDI Learn state are accessed here.
std::vector<ControlLayer> buildControlLayers(const Show& show);

// Convert legacy globally locked Control Columns into named Default Map
// functions. Dormant per-cue sets are archived as reusable templates instead
// of being silently activated. Transactional: on failure the Show is intact.
// Only the project activation path invokes this; decoder stays compatible.
juce::Result migrateLegacyControlColumns(Show& show,bool& migrated);

// Presets are tied to the acknowledged active cue, never the selected row.
// An unknown/disarmed cue has no active preset.
int activeControlPresetForCue(const Show& show, int activeCueId) noexcept;

} // namespace cuelist
