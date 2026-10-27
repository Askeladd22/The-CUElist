#pragma once
#include "ShowModel.h"

namespace cuelist {

// Pure transformation of the stored show into runtime mapping layers.
// Layer order follows visible column order (the rightmost column wins ties).
// Locked Control Columns apply globally; unlocked sets apply to their own cue.
// No devices, audio engine, UI state or MIDI Learn state are accessed here.
std::vector<ControlLayer> buildControlLayers(const Show& show);

// Presets are tied to the acknowledged active cue, never the selected row.
// An unknown/disarmed cue has no active preset.
int activeControlPresetForCue(const Show& show, int activeCueId) noexcept;

} // namespace cuelist
