#pragma once
#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <map>
#include <set>
#include <vector>

namespace cuelist {
enum class ControlKind { cc, note, pitchBend };
enum class ControlScope { global, column, preset, cue };
enum class ControlTarget { go, panic, masterGain, columnGain, midiCc, oscFloat };
enum class ControlTakeover { pickup, jump };

struct ControlEvent {
    juce::String deviceId;
    ControlKind kind=ControlKind::cc;
    int channel=1, number=0, value=0;
    bool isNoteOff=false;
    float normalized() const noexcept {
        return juce::jlimit(0.0f,1.0f,(float)value/
            (kind==ControlKind::pitchBend ? 16383.0f : 127.0f));
    }
};
struct ControlBinding {
    int id=0;
    juce::String deviceId;
    ControlKind kind=ControlKind::cc;
    int channel=1, number=0;
    ControlScope scope=ControlScope::global;
    int scopeId=0; // column ID, shared preset ID or cue row ID
    ControlTarget target=ControlTarget::masterGain;
    int targetColumnId=0; // stable column identity for gain or routing
    int outputChannel=1, outputNumber=1, oscPort=9000;
    juce::String outputDeviceId, oscHost="127.0.0.1", oscAddress="/control";
    float minimum=0,maximum=1;
    bool invert=false, enabled=true;
    ControlTakeover takeover=ControlTakeover::pickup;
};
struct ControlPreset { int id=0; juce::String name; };
struct ControlSettings {
    juce::String midiInputId;
    int nextId=1;
    std::vector<ControlPreset> presets;
    std::vector<ControlBinding> bindings;
};
struct MappedControl {
    ControlBinding binding;
    float normalized=0,scaled=0;
    bool pressed=false;
};

inline bool samePhysicalInput(const ControlBinding& b,const ControlEvent& e) {
    return b.enabled && b.deviceId==e.deviceId && b.kind==e.kind &&
        b.channel==e.channel && b.number==e.number;
}
inline bool scopeApplies(const ControlBinding& b,int activeCueId,int activePresetId) {
    if (b.scope==ControlScope::global) return true;
    if (b.scope==ControlScope::column) return true; // target column ID is validated
    if (b.scope==ControlScope::preset) return activeCueId>0 && b.scopeId==activePresetId;
    return activeCueId>0 && b.scopeId==activeCueId;
}
inline int priority(ControlScope s) { return (int)s; }

// Pure model. No MIDI port, audio callback, GUI or network operations.
class ControlMappingEngine {
public:
    void setSettings(ControlSettings next) { settings=std::move(next); pickup.clear(); }
    void setActiveCue(int id,int preset) {
        if (id==activeCueId && preset==activePresetId) return;
        activeCueId=id; activePresetId=preset; pickup.clear();
    }
    int activeCue() const noexcept { return activeCueId; }
    const ControlBinding* matched(const ControlEvent& e) const {
        const ControlBinding* best=nullptr;
        for (const auto& b:settings.bindings) if (samePhysicalInput(b,e)
            && scopeApplies(b,activeCueId,activePresetId)) {
            if (!best || priority(b.scope)>priority(best->scope) ||
                (priority(b.scope)==priority(best->scope) && b.id<best->id)) best=&b;
        }
        return best;
    }
    std::optional<MappedControl> resolve(const ControlEvent& e,
                                         std::optional<float> currentTarget={}) {
        const ControlBinding* best=matched(e);
        if (!best) return {};
        const float raw=e.normalized();
        auto& state=pickup[best->id];
        if (best->takeover==ControlTakeover::pickup &&
            best->kind!=ControlKind::note && currentTarget.has_value()) {
            const float desired=juce::jlimit(0.0f,1.0f,*currentTarget);
            const float from=state.hasPrevious ? state.previous : raw;
            const bool crossed=from<=desired && raw>=desired || from>=desired && raw<=desired;
            if (!state.captured && !crossed && std::abs(raw-desired)>0.015f) {
                state.previous=raw; state.hasPrevious=true; return {};
            }
            state.captured=true;
        }
        state.previous=raw; state.hasPrevious=true;
        const float scaledRaw=best->invert ? 1.0f-raw : raw;
        const float value=best->minimum+(best->maximum-best->minimum)*scaledRaw;
        return MappedControl{*best,raw,value,raw>=0.5f && !e.isNoteOff};
    }
private:
    struct Pickup { float previous=0; bool hasPrevious=false,captured=false; };
    ControlSettings settings;
    int activeCueId=0,activePresetId=0;
    std::map<int,Pickup> pickup;
};

inline juce::Result validateControls(const ControlSettings& s,
        const std::vector<int>& rowIds,const std::vector<int>& columnIds) {
    if (s.midiInputId.length()>255 || s.nextId<1 || s.nextId>=1000000000 ||
        s.bindings.size()>2048 || s.presets.size()>256)
        return juce::Result::fail("MIDI mapping limits exceeded.");
    std::set<int> ids;
    for (const auto& p:s.presets)
        if (p.id<1 || p.id>=s.nextId || !ids.insert(p.id).second ||
            p.name.trim().isEmpty() || p.name.length()>120)
            return juce::Result::fail("Invalid mapping preset.");
    for (size_t i=0;i<s.bindings.size();++i)
        for (size_t j=i+1;j<s.bindings.size();++j) {
            const auto& a=s.bindings[i],&b=s.bindings[j];
            if (!a.enabled || !b.enabled || a.deviceId!=b.deviceId ||
                a.kind!=b.kind || a.channel!=b.channel || a.number!=b.number ||
                a.scope!=b.scope) continue;
            if (a.scope==ControlScope::column || a.scopeId==b.scopeId)
                return juce::Result::fail("Two mappings share the same control and priority.");
        }
    for (const auto& b:s.bindings) {
        if (b.id<1 || b.id>=s.nextId || !ids.insert(b.id).second || b.deviceId.length()>255
            || (int)b.kind<0 || (int)b.kind>2 || (int)b.scope<0 || (int)b.scope>3
            || (int)b.target<0 || (int)b.target>5 || (int)b.takeover<0 || (int)b.takeover>1
            || b.channel<1 || b.channel>16 || b.number<0 || b.number>127
            || b.outputChannel<1 || b.outputChannel>16 || b.outputNumber<0 || b.outputNumber>127
            || b.oscPort<1 || b.oscPort>65535 || b.oscHost.isEmpty() || b.oscHost.length()>255
            || b.oscAddress.isEmpty() || !b.oscAddress.startsWithChar('/')
            || !std::isfinite(b.minimum) || !std::isfinite(b.maximum) || b.minimum>b.maximum)
            return juce::Result::fail("Invalid MIDI binding.");
        if (b.scope==ControlScope::cue &&
            std::find(rowIds.begin(),rowIds.end(),b.scopeId)==rowIds.end())
            return juce::Result::fail("Mapping refers to a missing cue.");
        if (b.scope==ControlScope::preset &&
            std::none_of(s.presets.begin(),s.presets.end(),[&](const auto& p){return p.id==b.scopeId;}))
            return juce::Result::fail("Mapping refers to a missing preset.");
        if ((b.scope==ControlScope::column || b.target==ControlTarget::columnGain ||
             b.target==ControlTarget::midiCc || b.target==ControlTarget::oscFloat) &&
            std::find(columnIds.begin(),columnIds.end(),b.targetColumnId)==columnIds.end())
            return juce::Result::fail("Mapping refers to a missing column.");
        if ((b.target==ControlTarget::masterGain || b.target==ControlTarget::columnGain)
            && (b.minimum<0 || b.maximum>4))
            return juce::Result::fail("Gain mapping out of range.");
    }
    return juce::Result::ok();
}
}
