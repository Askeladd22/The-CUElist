#include "ControlDispatchPolicy.h"
#include <cmath>

namespace cuelist {

bool PhysicalControlEdges::recordAndWasDown(const ControlEvent& event) {
    const auto physical=event.deviceId+"|"+juce::String((int)event.kind)+"|"+
        juce::String(event.channel)+"|"+juce::String(event.number);
    const bool wasDown=pressed[physical];
    const bool isDown=!event.isNoteOff && event.normalized()>=0.5f;
    pressed[physical]=isDown;
    return wasDown;
}

std::optional<float> mappedPickupTarget(const Show& show,
                                        const ControlBinding& binding) noexcept {
    const float span=binding.maximum-binding.minimum;
    if (!(span>0.0f)) return {};

    std::optional<float> actual;
    if (binding.target==ControlTarget::masterGain) actual=show.masterGain;
    else if (binding.target==ControlTarget::masterPan) actual=show.masterPan;
    else if (binding.target==ControlTarget::columnGain ||
             binding.target==ControlTarget::columnPan) {
        // As before, pickup uses stable column IDs, not their visual indices.
        for (const auto& column:show.columns)
            if (column.id==binding.targetColumnId) {
                actual=binding.target==ControlTarget::columnPan ? column.pan : column.gain;
                break;
            }
    }
    if (!actual) return {};
    float current=juce::jlimit(0.0f,1.0f,(*actual-binding.minimum)/span);
    current=inverseControlCurve(current,binding.curve);
    if (binding.invert) current=1.0f-current;
    return current;
}

int mappedTargetColumn(const Show& show, int stableColumnId, ColumnType type) noexcept {
    for (int index=0;index<(int)show.columns.size();++index)
        if (show.columns[(size_t)index].id==stableColumnId &&
            show.columns[(size_t)index].type==type)
            return index;
    return -1;
}

bool shouldApplyMappedValue(float oldValue,float requestedValue) noexcept {
    return std::abs(oldValue-requestedValue)>0.005f;
}

int mappedMidiCcValue(float scaled) noexcept {
    return juce::jlimit(0,127,(int)std::lround(scaled));
}

} // namespace cuelist
