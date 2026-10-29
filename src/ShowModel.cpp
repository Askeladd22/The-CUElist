#include "ShowModel.h"
#include <cmath>
#include <set>
#include <stdexcept>
#include <filesystem>
#include <map>
namespace cuelist {
namespace {
bool safeRelative(const juce::String& path) {
    if (path.isEmpty() || juce::File::isAbsolutePath(path) || path.containsChar(':') || path.startsWithChar('/') || path.startsWithChar('\\')) return false;
    for (const auto& part : juce::StringArray::fromTokens(path.replaceCharacter('\\', '/'), "/", ""))
        if (part.isEmpty() || part == "." || part == "..") return false;
    return true;
}
bool contained(const juce::File& child, const juce::File& parent) {
    std::error_code ec;
    auto base = std::filesystem::weakly_canonical(std::filesystem::u8path(parent.getFullPathName().toStdString()), ec);
    if (ec) return false;
    auto path = std::filesystem::weakly_canonical(std::filesystem::u8path(child.getFullPathName().toStdString()), ec);
    if (ec) return false;
    auto p = path.begin();
    for (auto b = base.begin(); b != base.end(); ++b, ++p) if (p == path.end() || *p != *b) return false;
    return p != path.end();
}
}
void addRow(Show& show, const juce::String& name) {
    Row row; row.id = show.nextId++; row.name = name;
    for (size_t i = 0; i < show.columns.size(); ++i) { Cell c; c.id = show.nextId++; row.cells.push_back(c); }
    show.rows.push_back(std::move(row));
}
void addColumn(Show& show, ColumnType type, const juce::String& name) {
    juce::String label=name;
    if (label.trim().isEmpty()) {
        const juce::String prefix=type==ColumnType::audio ? "Audio" : type==ColumnType::midi ? "MIDI" : type==ColumnType::osc ? "OSC" : "Control";
        int ordinal=1;
        for (const auto& col:show.columns) if (col.type==type) ++ordinal;
        label=prefix+" "+juce::String(ordinal);
        // Existing projects may contain user-defined names matching generated
        // labels. Never assign the same automatic label twice.
        const auto taken=[&](const juce::String& candidate) {
            for (const auto& col:show.columns) if (col.name.equalsIgnoreCase(candidate)) return true;
            return false;
        };
        while (taken(label)) label=prefix+" "+juce::String(++ordinal);
    }
    Column created{show.nextId++, label, type, 0};
    created.masterSend=(type==ColumnType::audio);
    show.columns.push_back(std::move(created));
    for (auto& row : show.rows) { Cell c; c.id = show.nextId++; row.cells.push_back(c); }
    // An operator can add columns at any moment, including with multiple
    // Cuelists. Keep inactive cue matrices rectangular as well.
    for (size_t n=0;n<show.cuelists.size();++n)
        if ((int)n!=show.activeCuelist)
            for (auto& row:show.cuelists[n].rows) {
                Cell c; c.id=show.nextId++; row.cells.push_back(c);
            }
}
Show makeShow() {
    Show show;
    addColumn(show, ColumnType::audio, {});
    addRow(show, {});
    show.cuelists.push_back({"Cuelist 1",show.rows});
    return show;
}
double secondsAtBeat(const std::vector<TempoPoint>& points, double beat) {
    if (!std::isfinite(beat) || beat < 0 || points.empty()) return 0;
    double seconds = 0;
    for (size_t i = 0; i < points.size(); ++i) {
        const auto end = i + 1 < points.size() ? std::min(beat, points[i+1].beat) : beat;
        if (points[i].bpm <= 0 || !std::isfinite(points[i].bpm)) return 0;
        if (end > points[i].beat) seconds += (end - points[i].beat) * 60.0 / points[i].bpm;
        if (end >= beat) break;
    }
    return seconds;
}
juce::Result validateShow(const Show& show) {
    const auto fail = [](const char* s) { return juce::Result::fail(s); };
    if (show.name.trim().isEmpty() || show.columns.empty() || show.rows.empty()) return fail("Missing name, rows or columns.");
    if (!std::isfinite(show.masterGain) || show.masterGain<0 || show.masterGain>4) return fail("Invalid master audio gain.");
    if (!std::isfinite(show.masterPan) || show.masterPan < -1 || show.masterPan > 1)
        return fail("Invalid master pan.");
    if (show.masterOutput<0 || show.masterOutput>1022 || show.masterOutput%2!=0)
        return fail("Invalid stereo Master main output.");
    if (show.columns.size() > 10000 || show.rows.size() > 10000 || show.rows.size()*show.columns.size() > 1000000)
        return fail("Project exceeds the safety limit of one million cells.");
    std::set<int> ids;
    auto unique = [&](int id) { return id > 0 && id < 1000000000 && ids.insert(id).second; };
    for (const auto& column : show.columns) {
        if (!unique(column.id) || column.output < 0 || column.output > 1022
            || (int)column.type < 0 || (int)column.type > 3
            || column.oscPort<1 || column.oscPort>65535 || column.oscHost.isEmpty()
            || column.oscHost.length()>255 || column.oscHost.containsChar(' ')
            || !std::isfinite(column.gain) || column.gain < 0 || column.gain > 4
            || !std::isfinite(column.pan) || column.pan < -1 || column.pan > 1
            || (column.type==ColumnType::control && (column.controlPriority<1 || column.controlPriority>4 ||
                (column.controlLocked && column.controlPriority!=1) ||
                (!column.controlLocked && column.controlPriority==1)))) return fail("Invalid column.");
    }
    for (const auto& row : show.rows) {
        if (row.notes.length()>8192) return fail("Cue notes must not exceed 8192 characters.");
        if (!unique(row.id) || row.cells.size()!=show.columns.size() || row.tempo.empty()
            || row.colour<0 || row.colour>7 || !std::isfinite(row.gain) || row.gain < 0 || row.gain > 4 || (int)row.behaviour < 0 || (int)row.behaviour > 3) return fail("Invalid row.");
        double previous = -1;
        for (const auto& p : row.tempo) {
            if (!std::isfinite(p.beat) || p.beat < 0 || p.beat <= previous || (previous < 0 && p.beat != 0)
                || !std::isfinite(p.bpm) || p.bpm < 1 || p.bpm > 999 || p.numerator < 1 || p.numerator > 32
                || (p.denominator != 1 && p.denominator != 2 && p.denominator != 4 && p.denominator != 8 && p.denominator != 16 && p.denominator != 32))
                return fail("Invalid tempo map: first point must be at beat 0, BPM 1–999, increasing positions.");
            previous = p.beat;
        }
        for (size_t i=0; i<row.cells.size(); ++i) {
            const auto& c = row.cells[i];
            if (c.midi) {
                if (show.columns[i].type!=ColumnType::midi || c.file.isNotEmpty() || c.osc) return fail("MIDI clip in a non-MIDI column.");
                auto result=validateMidiClip(*c.midi); if (result.failed()) return result;
            }
            if (c.osc) {
                if (show.columns[i].type!=ColumnType::osc || c.file.isNotEmpty() || c.midi) return fail("OSC clip in a non-OSC column.");
                const auto result=validateOscClip(*c.osc); if (result.failed()) return result;
            }
            if (show.columns[i].type==ColumnType::control) {
                if (c.midi || c.osc || c.file.isNotEmpty())
                    return fail("Invalid Control Column cell.");
            } else if (c.controlSet) return fail("Control Set in a non-Control column.");
            if (!unique(c.id) || !std::isfinite(c.gain) || c.gain < 0 || c.gain > 4 || (int)c.behaviour < 0 || (int)c.behaviour > 3
                || !std::isfinite(c.startSeconds) || c.startSeconds < 0 || c.startSeconds > 86400)
                return fail("Invalid cell.");
            if (c.file.isNotEmpty() && (show.columns[i].type != ColumnType::audio || (!juce::File::isAbsolutePath(c.file) && !safeRelative(c.file))))
                return fail("Invalid audio path or non-Audio column.");
        }
    }
    // Validate dormant Cuelists without asking the transport or current
    // cue mapping to interpret a different list's row IDs.
    if (!show.cuelists.empty()) {
        if (show.cuelists.size()>12 || show.activeCuelist<0 ||
            show.activeCuelist>=(int)show.cuelists.size())
            return fail("Invalid Cuelist selection or count.");
        int totalRows=(int)show.rows.size();
        for (size_t n=0;n<show.cuelists.size();++n) {
            const auto& list=show.cuelists[n];
            if (list.name.trim().isEmpty() || list.name.length()>64)
                return fail("Invalid Cuelist name.");
            if ((int)n==show.activeCuelist) continue;
            totalRows+=(int)list.rows.size();
            if (totalRows>10000) return fail("Too many cues across Cuelists.");
            Show independent=show;
            independent.cuelists.clear();
            independent.activeCuelist=0;
            independent.rows=list.rows;
            // Scoped cue mappings belong to the active list, so checking
            // dormant media/cell geometry must not resolve them against it.
            independent.controls.bindings.clear();
            if (auto validation=validateShow(independent);validation.failed())
                return validation;
            for (const auto& dormantRow:list.rows) {
                if (!unique(dormantRow.id)) return fail("Duplicate cue ID across Cuelists.");
                for (const auto& cell:dormantRow.cells)
                    if (!unique(cell.id)) return fail("Duplicate cell ID across Cuelists.");
            }
        }
    }
    if (show.nextId <= *ids.rbegin() || show.nextId >= 1000000000) return fail("Invalid identifier counter.");
    std::vector<int> rowIds,columnIds;
    for (const auto& row:show.rows) {
        rowIds.push_back(row.id);
        if (row.mappingPresetId!=0 &&
            std::none_of(show.controls.presets.begin(),show.controls.presets.end(),
                [&](const auto& p){ return p.id==row.mappingPresetId; }))
            return fail("Cue references a missing mapping preset.");
    }
    // Legacy cue-scoped mappings may refer to a dormant Cuelist. Accept
    // those stable row IDs in the document while the runtime still dispatches
    // only the currently selected Cuelist's GO and Control Set.
    for (size_t n=0;n<show.cuelists.size();++n)
        if ((int)n!=show.activeCuelist)
            for (const auto& row:show.cuelists[n].rows)
                rowIds.push_back(row.id);
    for (const auto& column:show.columns) columnIds.push_back(column.id);
    if (auto controlResult=validateControls(show.controls,rowIds,columnIds); controlResult.failed())
        return controlResult;
    for (const auto& function:show.controls.functions) {
        const auto& route=function.destination;
        // ID zero means this newly created output is not configured yet.
        // The function remains editable and inert until an output is chosen.
        if (route.targetColumnId!=0 &&
            (route.target==ControlTarget::midiCc ||
             route.target==ControlTarget::oscFloat ||
             route.target==ControlTarget::columnGain ||
             route.target==ControlTarget::columnPan)) {
            const auto it=std::find_if(show.columns.begin(),show.columns.end(),
                [&](const Column& col){return col.id==route.targetColumnId;});
            if (it==show.columns.end() ||
                (route.target==ControlTarget::midiCc && it->type!=ColumnType::midi) ||
                (route.target==ControlTarget::oscFloat && it->type!=ColumnType::osc) ||
                ((route.target==ControlTarget::columnGain ||
                  route.target==ControlTarget::columnPan) && it->type!=ColumnType::audio))
                return fail("Abstract function has a missing or incompatible destination.");
        }
        if (((route.target==ControlTarget::masterGain ||
              route.target==ControlTarget::columnGain) &&
             (route.minimum<0 || route.maximum>4)) ||
            ((route.target==ControlTarget::masterPan ||
              route.target==ControlTarget::columnPan) &&
             (route.minimum< -1 || route.maximum>1)))
            return fail("Abstract function value range is invalid.");
    }
    for (const auto& binding:show.controls.bindings) {
        if (binding.functionId==0 && (binding.target==ControlTarget::midiCc ||
            binding.target==ControlTarget::oscFloat ||
            binding.target==ControlTarget::columnGain ||
            binding.target==ControlTarget::columnPan)) {
            const auto it=std::find_if(show.columns.begin(),show.columns.end(),
                [&](const Column& col){return col.id==binding.targetColumnId;});
            if (it==show.columns.end()) return fail("Control Mapping destination does not exist.");
            if (binding.target==ControlTarget::midiCc && it->type!=ColumnType::midi)
                return fail("MIDI CC requires a MIDI column.");
            if (binding.target==ControlTarget::oscFloat && it->type!=ColumnType::osc)
                return fail("OSC requires an OSC column.");
            if ((binding.target==ControlTarget::columnGain ||
                 binding.target==ControlTarget::columnPan) && it->type!=ColumnType::audio)
                return fail("Gain mapping requires an Audio column.");
        }
    }
    // Validate all stored sets, including dormant sets on locked columns.
    // IDs must remain unique across the whole show (pickup uses binding IDs).
    std::set<int> allControlBindingIds;
    for (const auto& binding:show.controls.bindings)
        allControlBindingIds.insert(binding.id);
    for (const auto& preset:show.controls.presets)
        allControlBindingIds.insert(preset.id);
    for (const auto& function:show.controls.functions)
        allControlBindingIds.insert(function.id);
    for (const auto& item:show.controls.templates)
        allControlBindingIds.insert(item.id);
    // Source-column priority conflicts are resolved deterministically at runtime.
    const auto validateSet=[&](const ControlSet& set)->juce::Result {
        if (set.bindings.size()>256) return fail("Control Set exceeds 256 mappings.");
        std::set<int> overriddenFunctions;
        for (const auto& input:set.bindings)
            if (input.functionId>0 &&
                !overriddenFunctions.insert(input.functionId).second)
                return fail("A Control Set overrides one function more than once.");
        ControlSettings probe; probe.nextId=show.controls.nextId;
        probe.bindings=set.bindings; probe.functions=show.controls.functions;
        auto result=validateControls(probe,rowIds,columnIds);
        if (result.failed()) return result;
        for (const auto& binding:set.bindings) {
            if (!allControlBindingIds.insert(binding.id).second)
                return fail("Duplicate Control Set binding ID.");
            if (binding.scope!=ControlScope::global)
                return fail("Control Set bindings must be independent of legacy scopes.");
            if (binding.functionId==0 &&
                (binding.target==ControlTarget::columnGain ||
                binding.target==ControlTarget::midiCc ||
                binding.target==ControlTarget::oscFloat ||
                binding.target==ControlTarget::columnPan)) {
                const auto destination=std::find_if(show.columns.begin(),show.columns.end(),
                    [&](const Column& col){return col.id==binding.targetColumnId;});
                if (destination==show.columns.end() ||
                    ((binding.target==ControlTarget::columnGain ||
                      binding.target==ControlTarget::columnPan) && destination->type!=ColumnType::audio) ||
                    (binding.target==ControlTarget::midiCc && destination->type!=ColumnType::midi) ||
                    (binding.target==ControlTarget::oscFloat && destination->type!=ColumnType::osc))
                    return fail("Invalid Control Set target column type.");
            }
        }
        return juce::Result::ok();
    };
    for (const auto& item:show.controls.templates)
        if (auto result=validateSet(item.set);result.failed()) return result;
    for (size_t i=0;i<show.columns.size();++i) {
        const auto& column=show.columns[i];
        if (column.type!=ColumnType::control) {
            if (column.controlLocked || !column.controlDefaultSet.bindings.empty())
                return fail("Control settings in a non-Control column.");
            continue;
        }
        if (!column.controlLocked && !column.controlDefaultSet.bindings.empty())
            return fail("Unlocked Control Column has a default set.");
        if (auto result=validateSet(column.controlDefaultSet);result.failed()) return result;
        for (const auto& row:show.rows) if (row.cells[i].controlSet)
            if (auto result=validateSet(*row.cells[i].controlSet);result.failed()) return result;
    }
    return juce::Result::ok();
}
namespace {
juce::var object(std::initializer_list<std::pair<const char*,juce::var>> properties) {
    auto* o = new juce::DynamicObject(); for (const auto& p : properties) o->setProperty(p.first,p.second); return o;
}
juce::var required(const juce::var& o, const char* key) {
    if (!o.isObject() || !o.hasProperty(key)) throw std::runtime_error("Missing field in project.");
    return o[key];
}
double number(const juce::var& o, const char* key) {
    auto v = required(o,key); if (!v.isDouble() && !v.isInt() && !v.isInt64()) throw std::runtime_error("Invalid number.");
    double n = v; if (!std::isfinite(n)) throw std::runtime_error("Non-finite number."); return n;
}
int integer(const juce::var& o, const char* key) {
    const auto n = number(o,key); if (std::floor(n)!=n || n < -2147483647.0 || n > 2147483647.0) throw std::runtime_error("Invalid integer."); return (int)n;
}
juce::String string(const juce::var& o,const char* key) {
    auto v = required(o,key); if (!v.isString()) throw std::runtime_error("Invalid text."); return v.toString();
}
bool boolean(const juce::var& o,const char* key) {
    auto v = required(o,key); if (!v.isBool()) throw std::runtime_error("Invalid flag."); return (bool)v;
}
juce::Array<juce::var> array(const juce::var& o,const char* key) {
    auto v = required(o,key); if (!v.isArray()) throw std::runtime_error("Invalid list."); return *v.getArray();
}
}
// A Control Set stores the same binding schema as legacy Control Mapping,
// but without a competing cue/preset scope. Optional v10 extension.
juce::var controlSetObject(const ControlSet& set) {
    juce::Array<juce::var> bindings;
    for (const auto& b:set.bindings)
        bindings.add(object({{"id",b.id},{"deviceId",b.deviceId},
            {"kind",(int)b.kind},{"channel",b.channel},{"number",b.number},
            {"scope",(int)b.scope},{"scopeId",b.scopeId},{"target",(int)b.target},
            {"targetColumnId",b.targetColumnId},{"outputChannel",b.outputChannel},
            {"outputNumber",b.outputNumber},{"outputDeviceId",b.outputDeviceId},
            {"oscHost",b.oscHost},{"oscPort",b.oscPort},{"oscAddress",b.oscAddress},
            {"minimum",b.minimum},{"maximum",b.maximum},{"invert",b.invert},
            {"enabled",b.enabled},{"takeover",(int)b.takeover},{"curve",(int)b.curve},
            {"functionId",b.functionId}}));
    return object({{"bindings",bindings}});
}
ControlSet parseControlSet(const juce::var& data) {
    ControlSet set;
    for (const auto& item:array(data,"bindings")) {
        ControlBinding b;
        b.id=integer(item,"id"); b.deviceId=string(item,"deviceId");
        b.kind=(ControlKind)integer(item,"kind");
        b.channel=integer(item,"channel"); b.number=integer(item,"number");
        b.scope=(ControlScope)integer(item,"scope");
        b.scopeId=integer(item,"scopeId"); b.target=(ControlTarget)integer(item,"target");
        b.targetColumnId=integer(item,"targetColumnId");
        b.outputChannel=integer(item,"outputChannel");
        b.outputNumber=integer(item,"outputNumber"); b.outputDeviceId=string(item,"outputDeviceId");
        b.oscHost=string(item,"oscHost"); b.oscPort=integer(item,"oscPort");
        b.oscAddress=string(item,"oscAddress");
        b.minimum=(float)number(item,"minimum"); b.maximum=(float)number(item,"maximum");
        b.invert=boolean(item,"invert"); b.enabled=boolean(item,"enabled");
        b.takeover=(ControlTakeover)integer(item,"takeover");
        if (item.hasProperty("curve")) b.curve=(ControlCurve)integer(item,"curve");
        if (item.hasProperty("functionId")) b.functionId=integer(item,"functionId");
        set.bindings.push_back(std::move(b));
    }
    return set;
}
juce::String encodeShow(const Show& show) {
    juce::Array<juce::var> columns, rows;
    for (const auto& c : show.columns) columns.add(object({{"id",c.id},{"name",c.name},{"type",(int)c.type},{"output",c.output},
        {"gain",c.gain},{"pan",c.pan},{"muted",c.muted},{"phaseInverted",c.phaseInverted},{"midiOutputId",c.midiOutputId},{"oscHost",c.oscHost},{"oscPort",c.oscPort},
        {"collapsed",c.collapsed},{"controlLocked",c.controlLocked},{"controlPriority",c.controlPriority},
        {"masterSend",c.masterSend},{"directOutputEnabled",c.directOutputEnabled},
        {"directPostFader",c.directPostFader},
        {"controlDefaultSourceRowId",c.controlDefaultSourceRowId},
        {"controlDefaultSet",controlSetObject(c.controlDefaultSet)}}));
    for (const auto& r : show.rows) {
        juce::Array<juce::var> tempo, cells;
        for (const auto& p : r.tempo) tempo.add(object({{"beat",p.beat},{"bpm",p.bpm},{"numerator",p.numerator},{"denominator",p.denominator}}));
        for (const auto& c : r.cells) {
            juce::var midi;
            if (c.midi) {
                juce::Array<juce::var> notes;
                for (const auto& n:c.midi->notes) notes.add(object({{"beat",n.beat},{"length",n.length},{"pitch",n.pitch},{"velocity",n.velocity},{"channel",n.channel}}));
                midi=object({{"lengthBeats",c.midi->lengthBeats},{"notes",notes}});
            }
            juce::var osc;
            if (c.osc) {
                juce::Array<juce::var> events;
                for (const auto& e:c.osc->events) {
                    juce::Array<juce::var> args;
                    for (const auto& a:e.arguments) args.add(object({{"type",(int)a.type},{"integer",a.integer},
                        {"decimal",(double)a.decimal},{"text",a.text},{"boolean",a.boolean}}));
                    events.add(object({{"time",e.time},{"address",e.address},{"args",args}}));
                }
                juce::Array<juce::var> automations;
                for (const auto& a:c.osc->automations) {
                    juce::Array<juce::var> args,points;
                    for (const auto& v:a.message.arguments)
                        args.add(object({{"type",(int)v.type},{"integer",v.integer},
                            {"decimal",(double)v.decimal},{"text",v.text},{"boolean",v.boolean}}));
                    for (const auto& point:a.points)
                        points.add(object({{"time",point.time},{"value",point.value}}));
                    automations.add(object({{"address",a.message.address},{"args",args},
                        {"argumentIndex",a.argumentIndex},{"time",a.time},
                        {"duration",a.duration},{"points",points}}));
                }
                juce::Array<juce::var> tracks;
                for (const auto& track:c.osc->tracks) {
                    juce::Array<juce::var> args;
                    for (const auto& v:track.message.arguments)
                        args.add(object({{"type",(int)v.type},{"integer",v.integer},
                            {"decimal",(double)v.decimal},{"text",v.text},{"boolean",v.boolean}}));
                    tracks.add(object({{"address",track.message.address},{"args",args},
                        {"argumentIndex",track.argumentIndex},{"rangeMin",track.rangeMin},
                        {"rangeMax",track.rangeMax}}));
                }
                osc=object({{"useBeats",c.osc->useBeats},{"length",c.osc->length},
                    {"events",events},{"automations",automations},{"tracks",tracks}});
            }
            cells.add(object({{"id",c.id},{"name",c.name},{"file",c.file},{"followsRow",c.followsRow},
                {"loop",c.loop},{"stopOnNext",c.stopOnNext},{"gain",c.gain},{"startSeconds",c.startSeconds},{"behaviour",(int)c.behaviour},{"midi",midi},{"osc",osc},
                {"controlSet",c.controlSet ? controlSetObject(*c.controlSet) : juce::var()}}));
        }
        rows.add(object({{"id",r.id},{"name",r.name},{"notes",r.notes},{"tempo",tempo},{"cells",cells},{"gain",r.gain},
            {"muted",r.muted},{"loop",r.loop},{"overrideBehaviour",r.overrideBehaviour},{"behaviour",(int)r.behaviour},{"stopOnNext",r.stopOnNext},
            {"mappingPresetId",r.mappingPresetId},{"colour",r.colour}}));
    }
    juce::Array<juce::var> presets,bindings;
    for (const auto& preset:show.controls.presets)
        presets.add(object({{"id",preset.id},{"name",preset.name}}));
    for (const auto& binding:show.controls.bindings)
        bindings.add(object({{"id",binding.id},{"deviceId",binding.deviceId},
            {"kind",(int)binding.kind},{"channel",binding.channel},{"number",binding.number},
            {"scope",(int)binding.scope},{"scopeId",binding.scopeId},{"target",(int)binding.target},
            {"targetColumnId",binding.targetColumnId},{"outputChannel",binding.outputChannel},
            {"outputNumber",binding.outputNumber},{"outputDeviceId",binding.outputDeviceId},
            {"oscHost",binding.oscHost},{"oscPort",binding.oscPort},{"oscAddress",binding.oscAddress},
            {"minimum",binding.minimum},{"maximum",binding.maximum},{"invert",binding.invert},
            {"enabled",binding.enabled},{"takeover",(int)binding.takeover},
            {"curve",(int)binding.curve},{"functionId",binding.functionId}}));
    juce::Array<juce::var> templates;
    for (const auto& item:show.controls.templates)
        templates.add(object({{"id",item.id},{"name",item.name},
                              {"set",controlSetObject(item.set)}}));
    juce::Array<juce::var> functions;
    for (const auto& f:show.controls.functions) {
        ControlSet route;route.bindings.push_back(f.destination);
        functions.add(object({{"id",f.id},{"name",f.name},
                              {"route",controlSetObject(route)}}));
    }
    const auto controls=object({{"midiInputId",show.controls.midiInputId},
        {"nextId",show.controls.nextId},{"presets",presets},
        {"bindings",bindings},{"templates",templates},{"functions",functions},
        {"oscInputPort",show.controls.oscInputPort}});
    juce::Array<juce::var> cuelistObjects;
    for (size_t n=0;n<show.cuelists.size();++n) {
        // The active Cuelist already lives at root.rows. Never duplicate
        // its clips/notes: large v10 projects must remain within the 32MB
        // document size budget even after this extension.
        if ((int)n==show.activeCuelist) {
            cuelistObjects.add(object({{"name",show.cuelists[n].name}}));
            continue;
        }
        Show inactive=show;
        inactive.cuelists.clear();
        inactive.activeCuelist=0;
        inactive.rows=show.cuelists[n].rows;
        const auto document=juce::JSON::parse(encodeShow(inactive));
        cuelistObjects.add(object({{"name",show.cuelists[n].name},
                                 {"rows",document.getProperty("rows",{})}}));
    }
    return juce::JSON::toString(object({{"format","TheCuelist"},{"version",10},
        {"name",show.name},{"masterGain",show.masterGain},{"masterPan",show.masterPan},
        {"masterMuted",show.masterMuted},{"masterOutput",show.masterOutput},
        {"safetyLimiterEnabled",show.safetyLimiterEnabled},{"nextId",show.nextId},
        {"columns",columns},{"rows",rows},{"controls",controls},
        {"cuelists",cuelistObjects},{"activeCuelist",show.activeCuelist}}));
}
juce::Result decodeShow(const juce::String& text, Show& destination) {
    if (text.length() > 32*1024*1024) return juce::Result::fail("Project too large (maximum 32 MB). ");
    try {
        auto root = juce::JSON::parse(text);
        const auto version = integer(root,"version");
        if (string(root,"format") != "TheCuelist" || version < 1 || version > 10) return juce::Result::fail("Unsupported project format or version.");
        const auto activation = [version](int value) {
            if (version < 3 && (value == 1 || value == 4)) return Behaviour::retrigger;
            return (Behaviour)value;
        };
        Show show; show.name = string(root,"name"); show.nextId = integer(root,"nextId");
        if (version>=10) {
            const auto data=required(root,"controls");
            show.controls.midiInputId=string(data,"midiInputId");
            show.controls.nextId=integer(data,"nextId");
            if (data.hasProperty("oscInputPort"))
                show.controls.oscInputPort=integer(data,"oscInputPort");
            for (const auto& p:array(data,"presets"))
                show.controls.presets.push_back({integer(p,"id"),string(p,"name")});
            // Optional v10 extension; absence is normal for older projects.
            if (data.hasProperty("templates"))
                for (const auto& item:array(data,"templates"))
                    show.controls.templates.push_back({
                        integer(item,"id"),string(item,"name"),
                        parseControlSet(required(item,"set"))});
            if (data.hasProperty("functions"))
                for (const auto& item:array(data,"functions")) {
                    auto route=parseControlSet(required(item,"route"));
                    if (route.bindings.size()!=1)
                        throw std::runtime_error("Invalid control function route.");
                    show.controls.functions.push_back({
                        integer(item,"id"),string(item,"name"),
                        std::move(route.bindings.front())});
                }
            for (const auto& item:array(data,"bindings")) {
                ControlBinding b;
                b.id=integer(item,"id"); b.deviceId=string(item,"deviceId");
                b.kind=(ControlKind)integer(item,"kind");
                b.channel=integer(item,"channel"); b.number=integer(item,"number");
                b.scope=(ControlScope)integer(item,"scope"); b.scopeId=integer(item,"scopeId");
                b.target=(ControlTarget)integer(item,"target");
                b.targetColumnId=integer(item,"targetColumnId");
                b.outputChannel=integer(item,"outputChannel");
                b.outputNumber=integer(item,"outputNumber");
                b.outputDeviceId=string(item,"outputDeviceId");
                b.oscHost=string(item,"oscHost"); b.oscPort=integer(item,"oscPort");
                b.oscAddress=string(item,"oscAddress");
                b.minimum=(float)number(item,"minimum"); b.maximum=(float)number(item,"maximum");
                b.invert=boolean(item,"invert"); b.enabled=boolean(item,"enabled");
                b.takeover=(ControlTakeover)integer(item,"takeover");
                if (item.hasProperty("curve")) b.curve=(ControlCurve)integer(item,"curve");
                if (item.hasProperty("functionId")) b.functionId=integer(item,"functionId");
                show.controls.bindings.push_back(std::move(b));
            }
        }
        if (version>=8) show.masterGain=(float)number(root,"masterGain");
        // Optional fields keep every existing v10 document backwards-compatible.
        if (auto* obj=root.getDynamicObject()) {
            if (obj->hasProperty("masterPan")) show.masterPan=(float)number(root,"masterPan");
            if (obj->hasProperty("masterMuted")) show.masterMuted=boolean(root,"masterMuted");
            if (obj->hasProperty("masterOutput")) show.masterOutput=integer(root,"masterOutput");
            if (obj->hasProperty("safetyLimiterEnabled"))
                show.safetyLimiterEnabled=boolean(root,"safetyLimiterEnabled");
        }
        for (auto& c : array(root,"columns")) {
            Column column {integer(c,"id"),string(c,"name"),(ColumnType)integer(c,"type"),integer(c,"output")};
            // v1-v10 legacy audio projects had no separate Master Send.
            // Default outputs 1/2 are sent to Master; explicitly routed
            // higher pairs remain direct. Preserve their audible destination.
            column.masterSend=(column.type==ColumnType::audio && column.output==0);
            column.directOutputEnabled=(column.output!=0);
            if (auto* fields=c.getDynamicObject()) {
                if (fields->hasProperty("masterSend"))
                    column.masterSend=boolean(c,"masterSend");
                if (fields->hasProperty("directOutputEnabled"))
                    column.directOutputEnabled=boolean(c,"directOutputEnabled");
                if (fields->hasProperty("directPostFader"))
                    column.directPostFader=boolean(c,"directPostFader");
            }
            if (version>=5) column.midiOutputId=string(c,"midiOutputId");
            if (version>=6) { column.oscHost=string(c,"oscHost"); column.oscPort=integer(c,"oscPort"); }
            if (version >= 3) {
                column.gain=(float)number(c,"gain"); column.pan=(float)number(c,"pan");
                column.muted=boolean(c,"muted"); column.phaseInverted=boolean(c,"phaseInverted");
            }
            if (auto* obj=c.getDynamicObject()) {
                if (obj->hasProperty("collapsed")) column.collapsed=boolean(c,"collapsed");
                if (obj->hasProperty("controlLocked")) column.controlLocked=boolean(c,"controlLocked");
                if (obj->hasProperty("controlPriority")) column.controlPriority=integer(c,"controlPriority");
                if (obj->hasProperty("controlDefaultSourceRowId"))
                    column.controlDefaultSourceRowId=integer(c,"controlDefaultSourceRowId");
                if (obj->hasProperty("controlDefaultSet")) {
                    auto value=c.getProperty("controlDefaultSet",{});
                    if (!value.isVoid()) column.controlDefaultSet=parseControlSet(value);
                }
            }
            show.columns.push_back(column);
        }
        for (auto& r : array(root,"rows")) {
            Row row; row.id = integer(r,"id"); row.name = string(r,"name");
            if (r.hasProperty("notes")) row.notes=string(r,"notes");
            row.tempo.clear();
            if (version>=10) row.mappingPresetId=integer(r,"mappingPresetId");
            if (r.hasProperty("colour")) row.colour=integer(r,"colour");
            if (version >= 4) row.stopOnNext=boolean(r,"stopOnNext");
            if (version >= 2) {
                row.gain=(float)number(r,"gain"); row.muted=boolean(r,"muted"); row.loop=boolean(r,"loop");
                row.overrideBehaviour=boolean(r,"overrideBehaviour"); row.behaviour=activation(integer(r,"behaviour"));
            }
            for (auto& p : array(r,"tempo")) row.tempo.push_back({number(p,"beat"),number(p,"bpm"),integer(p,"numerator"),integer(p,"denominator")});
            for (auto& c : array(r,"cells")) {
                Cell cell; cell.id=integer(c,"id"); cell.name=string(c,"name"); cell.file=string(c,"file");
                cell.followsRow=boolean(c,"followsRow"); cell.loop=boolean(c,"loop"); cell.gain=(float)number(c,"gain"); cell.behaviour=activation(integer(c,"behaviour"));
                if (version >= 2) cell.startSeconds=number(c,"startSeconds");
                if (auto* obj=c.getDynamicObject())
                    if (obj->hasProperty("controlSet")) {
                        auto value=c.getProperty("controlSet",{});
                        if (!value.isVoid()) cell.controlSet=parseControlSet(value);
                    }
                if (version >= 7) cell.stopOnNext=boolean(c,"stopOnNext");
                if (version>=5) {
                    const auto m=required(c,"midi");
                    if (!m.isVoid()) {
                        MidiClip clip; clip.lengthBeats=number(m,"lengthBeats");
                        const auto notes=array(m,"notes");
                        if (notes.size()>10000) throw std::runtime_error("Too many MIDI notes.");
                        for (const auto& n:notes) clip.notes.push_back({number(n,"beat"),number(n,"length"),integer(n,"pitch"),integer(n,"velocity"),integer(n,"channel")});
                        cell.midi=std::move(clip);
                    }
                }
                if (version>=6) {
                    const auto o=required(c,"osc");
                    if (!o.isVoid()) {
                        OscClip clip; clip.useBeats=boolean(o,"useBeats"); clip.length=number(o,"length");
                        const auto events=array(o,"events");
                        if (events.size()>256) throw std::runtime_error("Too many OSC events.");
                        for (const auto& e:events) {
                            OscEvent ev; ev.time=number(e,"time"); ev.address=string(e,"address");
                            const auto args=array(e,"args");
                            if (args.size()>16) throw std::runtime_error("Too many OSC arguments.");
                            for (const auto& a:args) {
                                OscValue value; value.type=(OscValueType)integer(a,"type");
                                value.integer=integer(a,"integer"); value.decimal=(float)number(a,"decimal");
                                value.text=string(a,"text");
                                if (version>=9) value.boolean=boolean(a,"boolean");
                                ev.arguments.push_back(value);
                            }
                            clip.events.push_back(std::move(ev));
                        }
                        if (version>=9) {
                            const auto automations=array(o,"automations");
                            if (automations.size()>16) throw std::runtime_error("Too many OSC automations.");
                            for (const auto& a:automations) {
                                OscAutomation au; au.message.address=string(a,"address");
                                au.argumentIndex=integer(a,"argumentIndex");
                                au.time=number(a,"time"); au.duration=number(a,"duration");
                                const auto args=array(a,"args");
                                if (args.size()>16) throw std::runtime_error("Too many OSC arguments.");
                                for (const auto& v:args) {
                                    OscValue arg; arg.type=(OscValueType)integer(v,"type");
                                    arg.integer=integer(v,"integer"); arg.decimal=(float)number(v,"decimal");
                                    arg.text=string(v,"text"); arg.boolean=boolean(v,"boolean");
                                    au.message.arguments.push_back(arg);
                                }
                                const auto points=array(a,"points");
                                if (points.size()>128) throw std::runtime_error("Too many OSC points.");
                                for (const auto& point:points)
                                    au.points.push_back({number(point,"time"),number(point,"value")});
                                clip.automations.push_back(std::move(au));
                            }
                        }
                        // v10-compatible optional lane metadata. Older v10 and v1-v9
                        // files have no tracks property: their ramps are imported unchanged.
                        if (o.hasProperty("tracks")) {
                            const auto items=array(o,"tracks");
                            if (items.size()>16) throw std::runtime_error("Too many OSC tracks.");
                            for (const auto& item:items) {
                                OscTrack track; track.message.address=string(item,"address");
                                track.argumentIndex=integer(item,"argumentIndex");
                                track.rangeMin=number(item,"rangeMin");
                                track.rangeMax=number(item,"rangeMax");
                                const auto args=array(item,"args");
                                if (args.size()>16) throw std::runtime_error("Too many OSC arguments.");
                                for (const auto& v:args) {
                                    OscValue arg; arg.type=(OscValueType)integer(v,"type");
                                    arg.integer=integer(v,"integer"); arg.decimal=(float)number(v,"decimal");
                                    arg.text=string(v,"text"); arg.boolean=boolean(v,"boolean");
                                    track.message.arguments.push_back(arg);
                                }
                                clip.tracks.push_back(std::move(track));
                            }
                        }
                        cell.osc=std::move(clip);
                    }
                }
                if (version == 1 && cell.file.isNotEmpty() && !juce::File::isAbsolutePath(cell.file)) return juce::Result::fail("v1 path is not absolute.");
                row.cells.push_back(cell);
            }
            show.rows.push_back(std::move(row));
        }
        if (root.hasProperty("cuelists")) {
            const auto lists=array(root,"cuelists");
            if (lists.isEmpty() || lists.size()>12)
                return juce::Result::fail("Invalid Cuelist count.");
            show.activeCuelist=integer(root,"activeCuelist");
            if (show.activeCuelist<0 || show.activeCuelist>=lists.size())
                return juce::Result::fail("Invalid active Cuelist.");
            for (int n=0;n<lists.size();++n) {
                const auto& list=lists.getReference(n);
                // Active cue data is root.rows. Its lightweight tab record
                // intentionally has no second copy of the row array.
                if (n==show.activeCuelist) {
                    show.cuelists.push_back({string(list,"name"),show.rows});
                    continue;
                }
                // Parse dormant rows through the same cell/MIDI/OSC/control
                // validation as regular v10 rows, never a second parser.
                auto isolated=juce::JSON::parse(juce::JSON::toString(root));
                isolated.getDynamicObject()->removeProperty("cuelists");
                isolated.getDynamicObject()->removeProperty("activeCuelist");
                isolated.getDynamicObject()->setProperty("rows",required(list,"rows"));
                auto controlsCopy=isolated.getProperty("controls",{});
                if (auto* fields=controlsCopy.getDynamicObject())
                    fields->setProperty("bindings",juce::Array<juce::var>{});
                Show dormant;
                if (auto result=decodeShow(juce::JSON::toString(isolated),dormant);
                    result.failed()) return result;
                show.cuelists.push_back({string(list,"name"),std::move(dormant.rows)});
            }
            // Root rows remain authoritative for the active list.
            show.cuelists[(size_t)show.activeCuelist].rows=show.rows;
        } else {
            // All historical v1-v10 files represent one Cuelist.
            show.cuelists.push_back({"Cuelist 1",show.rows});
            show.activeCuelist=0;
        }
        auto valid = validateShow(show); if (valid.failed()) return valid;
        destination=std::move(show); return juce::Result::ok();
    } catch (const std::exception& e) { return juce::Result::fail(juce::String("Invalid project: ")+e.what()); }
}
juce::Result saveShow(const Show& show,const juce::File& file) {
    auto valid = validateShow(show); if (valid.failed()) return valid;
    const auto parent = file.getParentDirectory();
    if (!parent.isDirectory() || file.isDirectory()) return juce::Result::fail("Invalid destination folder.");
    // Each save owns a new media generation. Older generations remain available to backups.
    const auto media = parent.getChildFile(file.getFileNameWithoutExtension()+"_media");
    const auto generation = media.getChildFile(juce::Uuid().toString());
    if (!contained(media, parent) || generation.exists()) return juce::Result::fail("Unsafe media folder.");
    Show portable = show;
    std::map<juce::String, juce::String> copied;
    bool createdGeneration = false;
    struct Cleanup {
        juce::File folder; bool& created; bool committed = false;
        ~Cleanup() { if (created && !committed) folder.deleteRecursively(); }
    } cleanup { generation, createdGeneration };
    const auto copyMedia=[&](Cell& cell)->juce::Result {
        if (cell.file.isEmpty()) return juce::Result::ok();
        if (!juce::File::isAbsolutePath(cell.file)) return juce::Result::fail("Load the project before saving it: relative media path.");
        auto found = copied.find(cell.file);
        if (found != copied.end()) { cell.file = found->second; return juce::Result::ok(); }
        const juce::File source(cell.file);
        if (!source.existsAsFile()) return juce::Result::fail("Missing media: " + cell.file);
        if (!createdGeneration) {
            if (generation.createDirectory().failed()) return juce::Result::fail("Could not create media folder.");
            createdGeneration = true;
        }
        const auto target = generation.getChildFile(juce::String((int)copied.size()) + source.getFileExtension());
        if (target.exists() || !source.copyFileTo(target)) return juce::Result::fail("Could not copy media.");
        const auto relative = target.getRelativePathFrom(parent).replaceCharacter('\\','/');
        copied.emplace(cell.file, relative); cell.file = relative;
        return juce::Result::ok();
    };
    for (auto& row:portable.rows) for (auto& cell:row.cells)
        if (auto result=copyMedia(cell);result.failed()) return result;
    for (size_t n=0;n<portable.cuelists.size();++n)
        if ((int)n!=portable.activeCuelist)
            for (auto& row:portable.cuelists[n].rows)
                for (auto& cell:row.cells)
                    if (auto result=copyMedia(cell);result.failed()) return result;
    juce::TemporaryFile temp(file);
    if (!temp.getFile().replaceWithText(encodeShow(portable))) return juce::Result::fail("Could not write temporary file.");
    const auto backup = file.getSiblingFile(file.getFileName()+".bak");
    juce::TemporaryFile stagedBackup(backup), oldBackup(backup);
    const bool existing = file.existsAsFile(), hadBackup = backup.existsAsFile();
    if (existing) {
        if (backup.isDirectory() || (hadBackup && !backup.copyFileTo(oldBackup.getFile()))
            || !file.copyFileTo(stagedBackup.getFile()) || !stagedBackup.overwriteTargetFileWithTemporary())
            return juce::Result::fail("Backup failed: the original project was not changed.");
    }
    if (!temp.overwriteTargetFileWithTemporary()) {
        if (existing) { if (hadBackup) oldBackup.overwriteTargetFileWithTemporary(); else backup.deleteFile(); }
        return juce::Result::fail("Could not replace project. Check permissions and free disk space.");
    }
    cleanup.committed = true;
    return juce::Result::ok();
}
juce::Result loadShow(const juce::File& file, Show& destination) {
    if (!file.existsAsFile() || file.getSize() > 32*1024*1024) return juce::Result::fail("Project missing or too large.");
    Show loaded;
    auto result = decodeShow(file.loadFileAsString(), loaded); if (result.failed()) return result;
    const auto resolveMedia=[&](Cell& cell)->juce::Result {
        if (cell.file.isEmpty() || juce::File::isAbsolutePath(cell.file))
            return juce::Result::ok();
        if (!safeRelative(cell.file)) return juce::Result::fail("Unsafe relative path.");
        const auto resolved = file.getParentDirectory().getChildFile(cell.file);
        if (!contained(resolved, file.getParentDirectory())) return juce::Result::fail("Media path escapes the project folder.");
        cell.file = resolved.getFullPathName();
        return juce::Result::ok();
    };
    for (auto& row:loaded.rows) for (auto& cell:row.cells)
        if (auto resolved=resolveMedia(cell);resolved.failed()) return resolved;
    for (size_t n=0;n<loaded.cuelists.size();++n)
        if ((int)n!=loaded.activeCuelist)
            for (auto& row:loaded.cuelists[n].rows)
                for (auto& cell:row.cells)
                    if (auto resolved=resolveMedia(cell);resolved.failed()) return resolved;
    loaded.cuelists[(size_t)loaded.activeCuelist].rows=loaded.rows;
    destination = std::move(loaded);
    return juce::Result::ok();
}
juce::Result moveCell(Show& show, int sr, int sc, int tr, int tc) {
    const auto valid = [&](int r, int c) { return r >= 0 && c >= 0 && (size_t)r < show.rows.size()
        && (size_t)c < show.columns.size() && (size_t)c < show.rows[(size_t)r].cells.size(); };
    if (!valid(sr,sc) || !valid(tr,tc)) return juce::Result::fail("Invalid cell position.");
    if (show.columns[(size_t)sc].type != show.columns[(size_t)tc].type) return juce::Result::fail("Columns have different types.");
    std::swap(show.rows[(size_t)sr].cells[(size_t)sc], show.rows[(size_t)tr].cells[(size_t)tc]);
    return juce::Result::ok();
}
}
