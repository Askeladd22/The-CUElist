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
    // 0 means a legacy direct binding. Other values refer to one stable
    // project-level function; the function owns the destination.
    int functionId=0;
};
struct ControlFunction {
    int id=0;
    juce::String name;
    ControlBinding destination; // no physical input; holds target, route and value scale
};
inline ControlBinding materializeFunctionBinding(
        const ControlBinding& input,const ControlFunction& function) {
    ControlBinding b=function.destination;
    b.id=input.id; b.functionId=function.id;
    b.deviceId=input.deviceId; b.kind=input.kind;
    b.channel=input.channel; b.number=input.number;
    b.scope=input.scope; b.scopeId=input.scopeId;
    b.enabled=input.enabled; b.invert=input.invert;
    b.takeover=input.takeover; b.curve=input.curve;
    return b;
}
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
struct ControlSettings {
    juce::String midiInputId;
    int nextId=1;
    std::vector<ControlPreset> presets;
    std::vector<ControlBinding> bindings;
    std::vector<ControlSetTemplate> templates;
    // Functions belong to the project, not reusable controller presets.
    std::vector<ControlFunction> functions;
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
class ControlMappingEngine {
public:
    void setSettings(ControlSettings next) {
        settings=std::move(next); pickup.clear(); refreshAbstractCache();
    }
    void setLayers(std::vector<ControlLayer> next) {
        layers=std::move(next); pickup.clear(); refreshAbstractCache();
    }
    void setActiveCue(int id,int preset) {
        if (id==activeCueId && preset==activePresetId) return;
        activeCueId=id; activePresetId=preset; pickup.clear();
        refreshAbstractCache();
    }
    int activeCue() const noexcept { return activeCueId; }
    // Compose the visible effective mapping for a selected cue. No live state
    // is altered: selecting a row must never activate hardware mappings.
    std::vector<EffectiveControl> effectiveForCue(int cueId,int presetId=0) const {
        std::vector<EffectiveControl> result;
        // Preserve legacy global/column/preset/cue mappings as the lowest
        // fallback layer, matching the behavior of matched().
        for (const auto& b:settings.bindings) if (b.enabled && b.functionId==0 &&
            scopeApplies(b,cueId,presetId)) {
            const auto it=std::find_if(result.begin(),result.end(),
                [&](const EffectiveControl& v) {
                    const auto& a=v.binding;
                    return a.deviceId==b.deviceId && a.kind==b.kind &&
                        a.channel==b.channel && a.number==b.number;
                });
            if (it==result.end()) result.push_back({b,0,0,true});
            else if (priority(b.scope)>priority(it->binding.scope) ||
                (priority(b.scope)==priority(it->binding.scope) &&
                 b.id<it->binding.id))
                *it={b,0,0,true};
        }
        for (const auto& layer:layers) {
            if (!layer.locked && (cueId==0 || layer.cueId!=cueId)) continue;
            for (const auto& b:layer.set.bindings) {
                if (!b.enabled || b.functionId!=0) continue;
                auto it=std::find_if(result.begin(),result.end(),
                    [&](const EffectiveControl& v) {
                        const auto& a=v.binding;
                        return a.deviceId==b.deviceId && a.kind==b.kind &&
                            a.channel==b.channel && a.number==b.number;
                    });
                if (it==result.end()) result.push_back({b,layer.columnId,layer.priority,layer.locked});
                else if (layer.priority>=it->priority)
                    *it={b,layer.columnId,layer.priority,layer.locked,true};
            }
        }
        // Function-based assignments are layered by function identity,
        // never by hardware input. Overriding a function on Q2 suppresses
        // its old Q1/default hardware input and preserves its destination.
        std::vector<EffectiveControl> abstract;
        auto addFunction=[&](const ControlBinding& input,int source,int weight) {
            const auto f=std::find_if(settings.functions.begin(),settings.functions.end(),
                [&](const ControlFunction& item){return item.id==input.functionId;});
            if (f==settings.functions.end()) return;
            auto b=materializeFunctionBinding(input,*f);
            auto it=std::find_if(abstract.begin(),abstract.end(),
                [&](const EffectiveControl& item) {
                    return item.binding.functionId==input.functionId;
                });
            const EffectiveControl entry{b,source,weight,source==0,source!=0};
            if (it==abstract.end()) abstract.push_back(entry);
            else if (weight>=it->priority) *it=entry;
        };
        for (const auto& b:settings.bindings)
            if (b.functionId>0 && b.enabled && scopeApplies(b,cueId,presetId))
                addFunction(b,0,0);
        for (const auto& layer:layers) {
            if (layer.locked || cueId<=0 || layer.cueId!=cueId) continue;
            for (const auto& b:layer.set.bindings)
                if (b.functionId>0 && b.enabled)
                    addFunction(b,layer.columnId,layer.priority);
        }
        result.insert(result.end(),abstract.begin(),abstract.end());
        return result;
    }
    const ControlBinding* matched(const ControlEvent& e) const {
        const ControlBinding* best=nullptr;
        for (const auto& b:settings.bindings) if (b.functionId==0 && samePhysicalInput(b,e)
            && scopeApplies(b,activeCueId,activePresetId)) {
            if (!best || priority(b.scope)>priority(best->scope) ||
                (priority(b.scope)==priority(best->scope) && b.id<best->id)) best=&b;
        }
        // Control Column precedence: Maximum > High > Medium > Low.
        // For the SAME priority the RIGHTMOST Control Column in show order
        // wins. refreshControlSettings() constructs layers in that order;
        // the >= comparison below is therefore deliberate and deterministic.
        // Other physical inputs remain inherited from lower-priority layers
        // or the pre-existing global/preset mapping system.
        int winningPriority=0;
        for (const auto& layer:layers) {
            if (!layer.locked && (activeCueId<=0 || layer.cueId!=activeCueId)) continue;
            for (const auto& b:layer.set.bindings) if (b.functionId==0 && samePhysicalInput(b,e)) {
                if (layer.priority>=winningPriority) {
                    best=&b; winningPriority=layer.priority;
                }
            }
        }
        // Re-evaluate once on cue activation; no local input survives when
        // the next cue has no override for its abstract function.
        int weight=-1;
        for (const auto& item:abstractCache) {
            if (samePhysicalInput(item.binding,e) && item.priority>=weight) {
                best=&item.binding;weight=item.priority;
            }
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
        const float value=best->minimum+(best->maximum-best->minimum)*
            applyControlCurve(scaledRaw,best->curve);
        return MappedControl{*best,raw,value,raw>=0.5f && !e.isNoteOff};
    }
private:
    struct Pickup { float previous=0; bool hasPrevious=false,captured=false; };
    ControlSettings settings;
    std::vector<ControlLayer> layers;
    std::vector<EffectiveControl> abstractCache;
    void refreshAbstractCache() {
        abstractCache.clear();
        for (auto& item:effectiveForCue(activeCueId,activePresetId))
            if (item.binding.functionId>0) abstractCache.push_back(std::move(item));
    }
    int activeCueId=0,activePresetId=0;
    std::map<int,Pickup> pickup;
};

inline juce::Result validateControls(const ControlSettings& s,
        const std::vector<int>& rowIds,const std::vector<int>& columnIds) {
    if (s.midiInputId.length()>255 || s.nextId<1 || s.nextId>=1000000000 ||
        s.bindings.size()>2048 || s.presets.size()>256 ||
        s.templates.size()>128 || s.functions.size()>512)
        return juce::Result::fail("MIDI mapping limits exceeded.");
    std::set<int> ids;
    for (const auto& p:s.presets)
        if (p.id<1 || p.id>=s.nextId || !ids.insert(p.id).second ||
            p.name.trim().isEmpty() || p.name.length()>120)
            return juce::Result::fail("Invalid mapping preset.");
    for (const auto& t:s.templates)
        if (t.id<1 || t.id>=s.nextId || !ids.insert(t.id).second ||
            t.name.trim().isEmpty() || t.name.length()>120)
            return juce::Result::fail("Invalid Control Set template.");
    for (const auto& f:s.functions) {
        if (f.id<1 || f.id>=s.nextId || !ids.insert(f.id).second ||
            f.name.trim().isEmpty() || f.name.length()>120 ||
            (int)f.destination.target<0 || (int)f.destination.target>7 ||
            f.destination.outputChannel<1 || f.destination.outputChannel>16 ||
            f.destination.outputNumber<0 || f.destination.outputNumber>127 ||
            f.destination.oscPort<1 || f.destination.oscPort>65535 ||
            f.destination.oscHost.isEmpty() ||
            !f.destination.oscAddress.startsWithChar('/') ||
            !std::isfinite(f.destination.minimum) ||
            !std::isfinite(f.destination.maximum) ||
            f.destination.minimum>f.destination.maximum)
            return juce::Result::fail("Invalid abstract control function.");
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
        if (b.id<1 || b.id>=s.nextId || !ids.insert(b.id).second || b.deviceId.length()>255
            || (b.functionId>0 && std::none_of(s.functions.begin(),s.functions.end(),
                [&](const ControlFunction& f){return f.id==b.functionId;}))
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
