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
enum class ControlTarget { go, panic, masterGain, columnGain, midiCc, oscFloat, columnPan, masterPan };
enum class ControlTakeover { pickup, jump };
enum class ControlCurve { linear, easeIn, easeOut };
inline float applyControlCurve(float v,ControlCurve curve) {
    v=juce::jlimit(0.0f,1.0f,v);
    return curve==ControlCurve::easeIn ? v*v :
        curve==ControlCurve::easeOut ? 1.0f-(1.0f-v)*(1.0f-v) : v;
}
inline float inverseControlCurve(float v,ControlCurve curve) {
    v=juce::jlimit(0.0f,1.0f,v);
    return curve==ControlCurve::easeIn ? std::sqrt(v) :
        curve==ControlCurve::easeOut ? 1.0f-std::sqrt(1.0f-v) : v;
}

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
    // Stable function identity. Zero preserves projects created before function routing.
    int functionId=0;
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
    ControlCurve curve=ControlCurve::linear;
};
// A Control Set stores independent physical-to-target mappings. These are
// inactive until their owning Control Column applies to the current cue.
struct ControlSet { std::vector<ControlBinding> bindings; };
struct ControlLayer {
    int columnId=0, cueId=0, priority=1;
    bool locked=false;
    ControlSet set;
};
struct ControlPreset { int id=0; juce::String name; };
// Reusable content is distinct from the Control Column that determines when
// and with which priority the content becomes active.
struct ControlSetTemplate { int id=0; juce::String name; ControlSet set; };
// The destination belongs to a named function, never to its physical input.
// destination contains the output parameters; its device/channel/number are ignored.
struct ControlFunction {
    int id=0;
    juce::String name;
    ControlBinding destination;
};
struct ControlSettings {
    juce::String midiInputId;
    int nextId=1;
    std::vector<ControlPreset> presets;
    std::vector<ControlBinding> bindings;
    std::vector<ControlFunction> functions;
    std::vector<ControlSetTemplate> templates;
};
struct EffectiveControl {
    ControlBinding binding;
    int columnId=0,priority=1;
    bool inherited=false;
    bool overrides=false;
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
// Compose physical inputs into functions, and resolve the destination only
// after a winning assignment is known. The effective cache is rebuilt only on
// settings/layer/GO changes, not in the MIDI input callback.
class ControlMappingEngine {
public:
    void setSettings(ControlSettings next) {
        settings=std::move(next); pickup.clear(); rebuild();
    }
    void setLayers(std::vector<ControlLayer> next) {
        layers=std::move(next); pickup.clear(); rebuild();
    }
    void setActiveCue(int id,int preset) {
        if (id==activeCueId && preset==activePresetId) return;
        activeCueId=id; activePresetId=preset; pickup.clear(); rebuild();
    }
    int activeCue() const noexcept { return activeCueId; }
    // Selecting a row previews that cue; it does NOT change the active routing.
    std::vector<EffectiveControl> effectiveForCue(int cueId,int presetId=0) const {
        std::vector<EffectiveControl> result;
        const auto sameSource=[](const ControlBinding& a,const ControlBinding& b) {
            return a.deviceId==b.deviceId && a.kind==b.kind &&
                a.channel==b.channel && a.number==b.number;
        };
        // Keep the old explicit scopes readable in existing projects, while
        // treating the project's Default Map as the inherited foundation.
        for (const auto& raw:settings.bindings) if (raw.enabled &&
                scopeApplies(raw,cueId,presetId)) {
            const auto b=withFunctionDestination(raw);
            auto it=std::find_if(result.begin(),result.end(),
                [&](const EffectiveControl& v){return sameSource(v.binding,b);});
            if (it==result.end()) result.push_back({b,0,0,true,false});
            else if (priority(b.scope)>priority(it->binding.scope) ||
                     (priority(b.scope)==priority(it->binding.scope) &&
                      b.id<it->binding.id))
                *it={b,0,0,true,false};
        }
        // A local override replaces the *function* assignment even if its
        // physical input changed. It also wins source collisions. Other
        // inherited functions remain untouched.
        for (const auto& layer:layers) {
            if (!layer.locked && (cueId<=0 || layer.cueId!=cueId)) continue;
            for (const auto& raw:layer.set.bindings) {
                if (!raw.enabled) continue;
                const auto b=withFunctionDestination(raw);
                int highestConflict=-1;
                bool replaced=false;
                size_t insertion=result.size();
                for (size_t i=0;i<result.size();++i) {
                    const auto& current=result[i];
                    const bool sameFunction=b.functionId>0 &&
                        current.binding.functionId==b.functionId;
                    if (sameFunction || sameSource(current.binding,b)) {
                        highestConflict=std::max(highestConflict,current.priority);
                        insertion=std::min(insertion,i);
                        replaced=true;
                    }
                }
                if (highestConflict>layer.priority) continue;
                result.erase(std::remove_if(result.begin(),result.end(),
                    [&](const EffectiveControl& current) {
                        return sameSource(current.binding,b) ||
                            (b.functionId>0 && current.binding.functionId==b.functionId);
                    }),result.end());
                // Stable display order matters for inherited/override previews.
                result.insert(result.begin()+std::min(insertion,result.size()),
                              {b,layer.columnId,layer.priority,layer.locked,replaced});
            }
        }
        return result;
    }
    const ControlBinding* matched(const ControlEvent& e) const {
        for (const auto& control:activeEffective)
            if (samePhysicalInput(control.binding,e)) return &control.binding;
        return nullptr;
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
            const bool crossed=(from<=desired && raw>=desired) ||
                               (from>=desired && raw<=desired);
            if (!state.captured && !crossed && std::abs(raw-desired)>0.015f) {
                state.previous=raw; state.hasPrevious=true; return {};
            }
            state.captured=true;
        }
        state.previous=raw; state.hasPrevious=true;
        const float scaledRaw=best->invert ? 1.0f-raw : raw;
        const float value=best->minimum+(best->maximum-best->minimum)*
            applyControlCurve(scaledRaw,best->curve);
        return MappedControl{*best,raw,value,raw>=0.5f && !e.isNoteOff};
    }
private:
    ControlBinding withFunctionDestination(ControlBinding b) const {
        if (b.functionId<=0) return b;
        const auto it=std::find_if(settings.functions.begin(),settings.functions.end(),
            [&](const ControlFunction& f){return f.id==b.functionId;});
        if (it==settings.functions.end()) return b; // invalid references are rejected on load
        const auto& d=it->destination;
        b.target=d.target;
        b.targetColumnId=d.targetColumnId;
        b.outputChannel=d.outputChannel; b.outputNumber=d.outputNumber;
        b.outputDeviceId=d.outputDeviceId;
        b.oscHost=d.oscHost; b.oscPort=d.oscPort; b.oscAddress=d.oscAddress;
        b.minimum=d.minimum; b.maximum=d.maximum;
        return b;
    }
    void rebuild() { activeEffective=effectiveForCue(activeCueId,activePresetId); }
    struct Pickup { float previous=0; bool hasPrevious=false,captured=false; };
    ControlSettings settings;
    std::vector<ControlLayer> layers;
    std::vector<EffectiveControl> activeEffective;
    int activeCueId=0,activePresetId=0;
    std::map<int,Pickup> pickup;
};

inline juce::Result validateControls(const ControlSettings& s,
        const std::vector<int>& rowIds,const std::vector<int>& columnIds) {
    if (s.midiInputId.length()>255 || s.nextId<1 || s.nextId>=1000000000 ||
        s.bindings.size()>2048 || s.presets.size()>256 || s.templates.size()>128)
        return juce::Result::fail("MIDI mapping limits exceeded.");
    std::set<int> ids;
    for (const auto& p:s.presets)
        if (p.id<1 || p.id>=s.nextId || !ids.insert(p.id).second ||
            p.name.trim().isEmpty() || p.name.length()>120)
            return juce::Result::fail("Invalid mapping preset.");
    for (const auto& fn:s.functions) {
        const auto& d=fn.destination;
        if (fn.id<1 || fn.id>=s.nextId || !ids.insert(fn.id).second ||
            fn.name.trim().isEmpty() || fn.name.length()>120 ||
            (int)d.target<0 || (int)d.target>7 ||
            d.outputChannel<1 || d.outputChannel>16 ||
            d.outputNumber<0 || d.outputNumber>127 ||
            d.oscPort<1 || d.oscPort>65535 || d.oscHost.isEmpty() ||
            !d.oscAddress.startsWithChar('/') ||
            !std::isfinite(d.minimum) || !std::isfinite(d.maximum) || d.minimum>d.maximum ||
            ((d.target==ControlTarget::masterGain || d.target==ControlTarget::columnGain) &&
             (d.minimum<0 || d.maximum>4)) ||
            ((d.target==ControlTarget::masterPan || d.target==ControlTarget::columnPan) &&
             (d.minimum< -1 || d.maximum>1)))
            return juce::Result::fail("Invalid named control function.");
    }
    for (const auto& t:s.templates)
        if (t.id<1 || t.id>=s.nextId || !ids.insert(t.id).second ||
            t.name.trim().isEmpty() || t.name.length()>120)
            return juce::Result::fail("Invalid Control Set template.");
    for (size_t i=0;i<s.bindings.size();++i)
        for (size_t j=i+1;j<s.bindings.size();++j) {
            const auto& a=s.bindings[i],&b=s.bindings[j];
            if (a.enabled && b.enabled && a.functionId>0 &&
                a.functionId==b.functionId && a.scope==b.scope &&
                (a.scope==ControlScope::column || a.scopeId==b.scopeId))
                return juce::Result::fail("Function is assigned twice in the same mapping scope.");
        }
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
        if (b.functionId>0 && std::none_of(s.functions.begin(),s.functions.end(),
            [&](const ControlFunction& f){return f.id==b.functionId;}))
            return juce::Result::fail("Mapping references a missing function.");
        if (b.id<1 || b.id>=s.nextId || !ids.insert(b.id).second || b.deviceId.length()>255
            || (int)b.kind<0 || (int)b.kind>2 || (int)b.scope<0 || (int)b.scope>3
            || (int)b.target<0 || (int)b.target>7 || (int)b.takeover<0 || (int)b.takeover>1
            || (int)b.curve<0 || (int)b.curve>2
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
             b.target==ControlTarget::midiCc || b.target==ControlTarget::oscFloat ||
             b.target==ControlTarget::columnPan) &&
            std::find(columnIds.begin(),columnIds.end(),b.targetColumnId)==columnIds.end())
            return juce::Result::fail("Mapping refers to a missing column.");
        if ((b.target==ControlTarget::masterGain || b.target==ControlTarget::columnGain)
            && (b.minimum<0 || b.maximum>4))
            return juce::Result::fail("Gain mapping out of range.");
        if ((b.target==ControlTarget::masterPan || b.target==ControlTarget::columnPan)
            && (b.minimum< -1 || b.maximum>1))
            return juce::Result::fail("Pan mapping outside [-1, 1].");
    }
    return juce::Result::ok();
}
}
