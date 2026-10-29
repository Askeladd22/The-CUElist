#pragma once
#include "ShowModel.h"
#include <algorithm>
#include <stdexcept>

namespace cuelist {
// Controller presets intentionally exclude PROJECT-SPECIFIC FUNCTION NAMES.
// Functions are matched by ordinal; existing project labels/IDs and all
// cue-local overrides are preserved. No partial import on validation errors.
inline juce::var presetObject(std::initializer_list<std::pair<const char*,juce::var>> fields) {
    auto* obj=new juce::DynamicObject();
    for(const auto& p:fields) obj->setProperty(p.first,p.second);
    return obj;
}
inline juce::String exportControllerPreset(const Show& show) {
    juce::Array<juce::var> records;
    for(const auto& f:show.controls.functions) {
        const auto& route=f.destination;
        int columnType=-1,ordinal=-1;
        juce::String midiOutput,oscHost;
        int oscPort=9000;
        if(route.target==ControlTarget::columnGain ||
           route.target==ControlTarget::columnPan ||
           route.target==ControlTarget::midiCc ||
           route.target==ControlTarget::oscFloat) {
            int count=0;
            for(const auto& col:show.columns) {
                if(col.id==route.targetColumnId) {
                    columnType=(int)col.type;ordinal=count;
                    midiOutput=col.midiOutputId;
                    oscHost=col.oscHost;oscPort=col.oscPort;
                    break;
                }
                if(col.type==(route.target==ControlTarget::midiCc ? ColumnType::midi :
                   route.target==ControlTarget::oscFloat ? ColumnType::osc : ColumnType::audio))
                    ++count;
            }
        }
        juce::var input;
        for(const auto& binding:show.controls.bindings)
            if(binding.functionId==f.id && binding.scope==ControlScope::global) {
                input=presetObject({{"deviceId",binding.deviceId},
                    {"kind",(int)binding.kind},{"channel",binding.channel},
                    {"number",binding.number},{"invert",binding.invert},
                    {"takeover",(int)binding.takeover},{"curve",(int)binding.curve},
                    {"enabled",binding.enabled}});
                break;
            }
        records.add(presetObject({
            {"target",(int)route.target},{"columnType",columnType},
            {"ordinal",ordinal},{"midiOutputId",midiOutput},
            {"oscHost",oscHost},{"oscPort",oscPort},
            {"outputChannel",route.outputChannel},{"outputNumber",route.outputNumber},
            {"address",route.oscAddress},{"minimum",route.minimum},
            {"maximum",route.maximum},{"input",input}
        }));
    }
    return juce::JSON::toString(presetObject({
        {"format","TheCuelistControllerPreset"},{"version",1},
        {"functions",records}}));
}

inline juce::Result importControllerPreset(const juce::String& text,Show& destination) {
    if(text.getNumBytesAsUTF8()>256*1024)
        return juce::Result::fail("Controller preset exceeds the 256KB limit.");
    try {
        const auto root=juce::JSON::parse(text);
        if(!root.isObject() || root.getProperty("format",{}).toString()!="TheCuelistControllerPreset" ||
            (int)root.getProperty("version",{})!=1)
            return juce::Result::fail("Unrecognized controller preset.");
        const auto functions=root.getProperty("functions",{});
        if(!functions.isArray() || functions.getArray()->size()>512)
            return juce::Result::fail("Invalid or oversized preset function list.");
        Show proposed=destination;
        const auto& records=*functions.getArray();
        for(int i=0;i<records.size();++i) {
            const auto& record=records.getReference(i);
            if(!record.isObject()) throw std::runtime_error("Malformed function record.");
            const auto target=(int)record.getProperty("target",{});
            const auto type=(int)record.getProperty("columnType",{});
            const auto ordinal=(int)record.getProperty("ordinal",{});
            if(target<0 || target>7) throw std::runtime_error("Unsupported destination type.");
            ControlBinding route;
            route.target=(ControlTarget)target;
            route.outputChannel=(int)record.getProperty("outputChannel",{});
            route.outputNumber=(int)record.getProperty("outputNumber",{});
            route.oscAddress=record.getProperty("address",{}).toString();
            route.minimum=(float)(double)record.getProperty("minimum",{});
            route.maximum=(float)(double)record.getProperty("maximum",{});
            const bool needsColumn=route.target==ControlTarget::midiCc ||
                route.target==ControlTarget::oscFloat ||
                route.target==ControlTarget::columnGain ||
                route.target==ControlTarget::columnPan;
            if(needsColumn) {
                const int wanted=(int)(route.target==ControlTarget::midiCc ? ColumnType::midi :
                    route.target==ControlTarget::oscFloat ? ColumnType::osc : ColumnType::audio);
                if(type!=wanted || ordinal<0)
                    throw std::runtime_error("Preset has an invalid destination column.");
                int count=0;
                Column* selected=nullptr;
                for(auto& col:proposed.columns)
                    if((int)col.type==wanted && count++==ordinal) {
                        selected=&col;break;
                    }
                if(!selected) throw std::runtime_error(
                    "Project lacks a compatible output column for this preset.");
                route.targetColumnId=selected->id;
                if(route.target==ControlTarget::midiCc)
                    selected->midiOutputId=record.getProperty("midiOutputId",{}).toString();
                else if(route.target==ControlTarget::oscFloat) {
                    selected->oscHost=record.getProperty("oscHost",{}).toString();
                    selected->oscPort=(int)record.getProperty("oscPort",{});
                }
            }
            int functionId=0;
            if(i<(int)proposed.controls.functions.size()) {
                auto& f=proposed.controls.functions[(size_t)i];
                f.destination=route;
                functionId=f.id; // Keep the project's own function label.
            } else {
                functionId=proposed.controls.nextId++;
                proposed.controls.functions.push_back({
                    functionId,"Function "+juce::String(i+1),route});
            }
            auto existing=std::find_if(proposed.controls.bindings.begin(),
                proposed.controls.bindings.end(),
                [&](const ControlBinding& b){
                    return b.functionId==functionId &&
                           b.scope==ControlScope::global;
                });
            const auto in=record.getProperty("input",{});
            if(in.isVoid()) {
                if(existing!=proposed.controls.bindings.end())
                    proposed.controls.bindings.erase(existing);
            } else {
                if(!in.isObject()) throw std::runtime_error("Invalid preset input.");
                ControlBinding binding;
                if(existing!=proposed.controls.bindings.end()) binding=*existing;
                else binding.id=proposed.controls.nextId++;
                binding.functionId=functionId;
                binding.scope=ControlScope::global;binding.scopeId=0;
                binding.deviceId=in.getProperty("deviceId",{}).toString();
                binding.kind=(ControlKind)(int)in.getProperty("kind",{});
                binding.channel=(int)in.getProperty("channel",{});
                binding.number=(int)in.getProperty("number",{});
                binding.invert=(bool)in.getProperty("invert",{});
                binding.enabled=(bool)in.getProperty("enabled",{});
                binding.curve=(ControlCurve)(int)in.getProperty("curve",{});
                binding.takeover=(ControlTakeover)(int)in.getProperty("takeover",{});
                // Keep legacy schema validation consistent with its target.
                binding.target=route.target;
                binding.targetColumnId=route.targetColumnId;
                binding.outputChannel=route.outputChannel;
                binding.outputNumber=route.outputNumber;
                binding.oscAddress=route.oscAddress;
                binding.minimum=route.minimum;binding.maximum=route.maximum;
                if(existing!=proposed.controls.bindings.end())
                    *existing=std::move(binding);
                else proposed.controls.bindings.push_back(std::move(binding));
            }
        }
        const auto validation=validateShow(proposed);
        if(validation.failed()) return validation;
        destination=std::move(proposed);
        return juce::Result::ok();
    } catch(const std::exception& e) {
        return juce::Result::fail(e.what());
    }
}
} // namespace cuelist
