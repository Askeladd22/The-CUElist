#include "MidiSessionPolicy.h"

namespace cuelist {

bool consumeMidiLearnEvent(bool& armed,
                           std::optional<ControlEvent>& captured,
                           const ControlEvent& event) {
    if (!armed) return false;
    captured=event;
    armed=false;
    return true;
}

} // namespace cuelist
