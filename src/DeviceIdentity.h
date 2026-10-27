#pragma once
#include <juce_core/juce_core.h>

namespace cuelist {
// The configured ID must match exactly; product names or ordinal positions are
// not safe substitutes for a live show's routings. Shared by MIDI input/output
// discovery and exercised with fake device inventories in automated tests.
template<typename DeviceInventory>
bool hasExactMidiIdentifier(const DeviceInventory& devices,const juce::String& identifier) {
    if(identifier.isEmpty()) return false;
    for(const auto& device:devices)
        if(device.identifier==identifier) return true;
    return false;
}
}
