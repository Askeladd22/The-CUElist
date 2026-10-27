#pragma once
#include "ControlMapping.h"
#include "DeviceIdentity.h"
#include <optional>

namespace cuelist {

// A pure decision, not a device operation. Never select another MIDI port
// merely because its display name, list index, or message type matches.
// The caller owns actual disconnect/reconnect, logging and input-edge reset.
enum class MidiSessionAction { none, disconnectMissing, reconnectSelected };

template<typename DeviceInventory>
MidiSessionAction planMidiSession(const juce::String& selectedId,
                                  const juce::String& connectedId,
                                  bool connected,
                                  const DeviceInventory& devices) {
    if (connected)
        return hasExactMidiIdentifier(devices,connectedId) ?
            MidiSessionAction::none : MidiSessionAction::disconnectMissing;
    return hasExactMidiIdentifier(devices,selectedId) ?
        MidiSessionAction::reconnectSelected : MidiSessionAction::none;
}

// Consume exactly one MIDI event for an explicitly armed Learn operation,
// rather than applying that event to a live GO/PANIC or output mapping.
// Preserve the previous capture until the operator arms Learn again.
bool consumeMidiLearnEvent(bool& armed,
                           std::optional<ControlEvent>& captured,
                           const ControlEvent& event);

} // namespace cuelist
