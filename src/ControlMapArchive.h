#pragma once
#include "ShowModel.h"
#include "ControlPresetArchive.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cuelist {

// A reusable Control Map is a named, cue-independent archive item.
// Export files contain only this map's input assignments, never cue rows,
// connected devices, MIDI/OSC settings or project-specific function IDs.
inline juce::String exportControlMap(const Show& show,int mapId) {
    const std::vector<ControlBinding>* bindings=nullptr;
    juce::String name="Default Map";
    if (mapId==0) {
        bindings=&show.controls.bindings;
    } else {
        const auto map=std::find_if(show.controls.templates.begin(),
            show.controls.templates.end(),
            [mapId](const ControlSetTemplate& item){return item.id==mapId;});
        if (map==show.controls.templates.end()) return {};
        name=map->name;
        bindings=&map->set.bindings;
    }
    juce::Array<juce::var> rows;
    for(const auto& b:*bindings) {
        // A controller archive is not an export of cue/preset assignments.
        if (mapId==0 && b.scope!=ControlScope::global) continue;
        juce::String functionName;
        if (b.functionId>0) {
            const auto found=std::find_if(show.controls.functions.begin(),
                show.controls.functions.end(),
                [&](const ControlFunction& f){return f.id==b.functionId;});
            if (found==show.controls.functions.end()) return {};
            functionName=found->name;
        }
        rows.add(presetObject({
            {"function",functionName},{"deviceId",b.deviceId},
            {"kind",(int)b.kind},{"channel",b.channel},{"number",b.number},
            {"target",(int)b.target},{"targetColumnId",b.targetColumnId},
            {"outputChannel",b.outputChannel},{"outputNumber",b.outputNumber},
            {"oscHost",b.oscHost},{"oscPort",b.oscPort},
            {"oscAddress",b.oscAddress},{"minimum",b.minimum},{"maximum",b.maximum},
            {"takeover",(int)b.takeover},{"curve",(int)b.curve},
            {"invert",b.invert},{"enabled",b.enabled}
        }));
    }
    return juce::JSON::toString(presetObject({
        {"format","TheCuelistControlMap"},{"version",1},
        {"name",name},{"bindings",rows}
    }));
}

// Import always creates a NEW independent reusable map. Default Map can be
// exported as a template but is never silently overwritten on import.
// Function references resolve by NAME against the destination project's
// abstract functions; their stable IDs and destination routing are retained.
// The whole operation is transactional and validates against the show schema.
inline juce::Result importControlMap(const juce::String& text,Show& destination) {
    if (text.getNumBytesAsUTF8()>256*1024)
        return juce::Result::fail("Control Map exceeds the 256KB limit.");
    try {
        const auto root=juce::JSON::parse(text);
        if (!root.isObject() ||
            root.getProperty("format",{}).toString()!="TheCuelistControlMap" ||
            (int)root.getProperty("version",{})!=1)
            return juce::Result::fail("Unrecognized Control Map file.");
        const auto rows=root.getProperty("bindings",{});
        const auto name=root.getProperty("name",{}).toString().trim();
        if (!rows.isArray() || rows.getArray()->size()>256 ||
            name.isEmpty() || name.length()>120)
            return juce::Result::fail("Invalid or oversized Control Map.");
        Show next=destination;
        if (next.controls.templates.size()>=128)
            return juce::Result::fail("The Control Map archive is full.");
        for(const auto& item:next.controls.templates)
            if (item.name.equalsIgnoreCase(name))
                return juce::Result::fail("A map with this name already exists. Rename it first.");
        ControlSetTemplate map;
        map.id=next.controls.nextId++;
        map.name=name;
        for(const auto& entry:*rows.getArray()) {
            if(!entry.isObject()) throw std::runtime_error("Invalid mapping entry.");
            const auto field=[&](const char* key) {
                if(!entry.hasProperty(key)) throw std::runtime_error("Missing mapping field.");
                return entry.getProperty(key,{});
            };
            ControlBinding b;
            b.id=next.controls.nextId++;
            b.scope=ControlScope::global;b.scopeId=0;
            const auto fn=field("function").toString();
            if (fn.isNotEmpty()) {
                const auto found=std::find_if(next.controls.functions.begin(),
                    next.controls.functions.end(),
                    [&](const ControlFunction& f){return f.name==fn;});
                if (found==next.controls.functions.end())
                    return juce::Result::fail("Missing project function: "+fn);
                b.functionId=found->id;
            }
            b.deviceId=field("deviceId").toString();
            b.kind=(ControlKind)(int)field("kind");
            b.channel=(int)field("channel");
            b.number=(int)field("number");
            b.target=(ControlTarget)(int)field("target");
            b.targetColumnId=(int)field("targetColumnId");
            b.outputChannel=(int)field("outputChannel");
            b.outputNumber=(int)field("outputNumber");
            b.oscHost=field("oscHost").toString();
            b.oscPort=(int)field("oscPort");
            b.oscAddress=field("oscAddress").toString();
            b.minimum=(float)(double)field("minimum");
            b.maximum=(float)(double)field("maximum");
            b.takeover=(ControlTakeover)(int)field("takeover");
            b.curve=(ControlCurve)(int)field("curve");
            b.invert=(bool)field("invert");
            b.enabled=(bool)field("enabled");
            if (b.functionId>0) {
                const auto found=std::find_if(next.controls.functions.begin(),
                    next.controls.functions.end(),
                    [&](const ControlFunction& f){return f.id==b.functionId;});
                const auto source=b;
                b=materializeFunctionBinding(source,*found);
                b.id=source.id;
            }
            map.set.bindings.push_back(std::move(b));
        }
        next.controls.templates.push_back(std::move(map));
        const auto result=validateShow(next);
        if (result.failed()) return result;
        destination=std::move(next);
        return juce::Result::ok();
    } catch(const std::exception& ex) {
        return juce::Result::fail(ex.what());
    }
}
}
