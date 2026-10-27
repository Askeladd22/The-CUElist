#pragma once
#include "ShowModel.h"
#include <map>
#include <optional>

namespace cuelist {

// UI-thread physical-edge state, kept separate from binding and preset IDs.
// Record *all* events including unmatched releases, so a cue change cannot
// interpret an already-held GO/PANIC button as a fresh press.
class PhysicalControlEdges final {
public:
    bool recordAndWasDown(const ControlEvent& event);
    void clear() noexcept { pressed.clear(); }
private:
    std::map<juce::String,bool> pressed;
};

// Model-only dispatch decisions. These neither enqueue output nor modify the
// project, and do not depend on the currently selected Inspector cell.
std::optional<float> mappedPickupTarget(const Show&, const ControlBinding&) noexcept;
int mappedTargetColumn(const Show&, int stableColumnId, ColumnType type) noexcept;
bool shouldApplyMappedValue(float oldValue, float requestedValue) noexcept;
int mappedMidiCcValue(float scaled) noexcept;

} // namespace cuelist
