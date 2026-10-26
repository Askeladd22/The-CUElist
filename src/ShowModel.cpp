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
    show.columns.push_back({show.nextId++, name, type, 0});
    for (auto& row : show.rows) { Cell c; c.id = show.nextId++; row.cells.push_back(c); }
}
Show makeShow() {
    Show show;
    addColumn(show, ColumnType::audio, {});
    addRow(show, {});
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
    if (show.name.trim().isEmpty() || show.columns.empty() || show.rows.empty()) return fail("Nome, righe o colonne mancanti.");
    if (show.columns.size() > 10000 || show.rows.size() > 10000 || show.rows.size()*show.columns.size() > 1000000)
        return fail("Progetto oltre il limite di sicurezza di un milione di celle.");
    std::set<int> ids;
    auto unique = [&](int id) { return id > 0 && id < 1000000000 && ids.insert(id).second; };
    for (const auto& column : show.columns) {
        if (!unique(column.id) || column.output < 0 || column.output > 1022
            || (int)column.type < 0 || (int)column.type > 2
            || column.oscPort<1 || column.oscPort>65535 || column.oscHost.isEmpty()
            || column.oscHost.length()>255 || column.oscHost.containsChar(' ')
            || !std::isfinite(column.gain) || column.gain < 0 || column.gain > 4
            || !std::isfinite(column.pan) || column.pan < -1 || column.pan > 1) return fail("Colonna non valida.");
    }
    for (const auto& row : show.rows) {
        if (!unique(row.id) || row.cells.size()!=show.columns.size() || row.tempo.empty()
            || !std::isfinite(row.gain) || row.gain < 0 || row.gain > 4 || (int)row.behaviour < 0 || (int)row.behaviour > 3) return fail("Riga non valida.");
        double previous = -1;
        for (const auto& p : row.tempo) {
            if (!std::isfinite(p.beat) || p.beat < 0 || p.beat <= previous || (previous < 0 && p.beat != 0)
                || !std::isfinite(p.bpm) || p.bpm < 1 || p.bpm > 999 || p.numerator < 1 || p.numerator > 32
                || (p.denominator != 1 && p.denominator != 2 && p.denominator != 4 && p.denominator != 8 && p.denominator != 16 && p.denominator != 32))
                return fail("Mappa tempo non valida: primo punto a beat 0, BPM 1-999, posizioni crescenti.");
            previous = p.beat;
        }
        for (size_t i=0; i<row.cells.size(); ++i) {
            const auto& c = row.cells[i];
            if (c.midi) {
                if (show.columns[i].type!=ColumnType::midi || c.file.isNotEmpty() || c.osc) return fail("Clip MIDI in colonna non MIDI.");
                auto result=validateMidiClip(*c.midi); if (result.failed()) return result;
            }
            if (c.osc) {
                if (show.columns[i].type!=ColumnType::osc || c.file.isNotEmpty() || c.midi) return fail("Clip OSC in colonna non OSC.");
                const auto result=validateOscClip(*c.osc); if (result.failed()) return result;
            }
            if (!unique(c.id) || !std::isfinite(c.gain) || c.gain < 0 || c.gain > 4 || (int)c.behaviour < 0 || (int)c.behaviour > 3
                || !std::isfinite(c.startSeconds) || c.startSeconds < 0 || c.startSeconds > 86400)
                return fail("Cella non valida.");
            if (c.file.isNotEmpty() && (show.columns[i].type != ColumnType::audio || (!juce::File::isAbsolutePath(c.file) && !safeRelative(c.file))))
                return fail("Percorso audio non valido o colonna non Audio.");
        }
    }
    if (show.nextId <= *ids.rbegin() || show.nextId >= 1000000000) return fail("Contatore identificativi non valido.");
    return juce::Result::ok();
}
namespace {
juce::var object(std::initializer_list<std::pair<const char*,juce::var>> properties) {
    auto* o = new juce::DynamicObject(); for (const auto& p : properties) o->setProperty(p.first,p.second); return o;
}
juce::var required(const juce::var& o, const char* key) {
    if (!o.isObject() || !o.hasProperty(key)) throw std::runtime_error("Campo mancante nel progetto.");
    return o[key];
}
double number(const juce::var& o, const char* key) {
    auto v = required(o,key); if (!v.isDouble() && !v.isInt() && !v.isInt64()) throw std::runtime_error("Numero non valido.");
    double n = v; if (!std::isfinite(n)) throw std::runtime_error("Numero non finito."); return n;
}
int integer(const juce::var& o, const char* key) {
    const auto n = number(o,key); if (std::floor(n)!=n || n < -2147483647.0 || n > 2147483647.0) throw std::runtime_error("Intero non valido."); return (int)n;
}
juce::String string(const juce::var& o,const char* key) {
    auto v = required(o,key); if (!v.isString()) throw std::runtime_error("Testo non valido."); return v.toString();
}
bool boolean(const juce::var& o,const char* key) {
    auto v = required(o,key); if (!v.isBool()) throw std::runtime_error("Flag non valido."); return (bool)v;
}
juce::Array<juce::var> array(const juce::var& o,const char* key) {
    auto v = required(o,key); if (!v.isArray()) throw std::runtime_error("Lista non valida."); return *v.getArray();
}
}
juce::String encodeShow(const Show& show) {
    juce::Array<juce::var> columns, rows;
    for (const auto& c : show.columns) columns.add(object({{"id",c.id},{"name",c.name},{"type",(int)c.type},{"output",c.output},
        {"gain",c.gain},{"pan",c.pan},{"muted",c.muted},{"phaseInverted",c.phaseInverted},{"midiOutputId",c.midiOutputId},{"oscHost",c.oscHost},{"oscPort",c.oscPort}}));
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
                        {"decimal",(double)a.decimal},{"text",a.text}}));
                    events.add(object({{"time",e.time},{"address",e.address},{"args",args}}));
                }
                osc=object({{"useBeats",c.osc->useBeats},{"length",c.osc->length},{"events",events}});
            }
            cells.add(object({{"id",c.id},{"name",c.name},{"file",c.file},{"followsRow",c.followsRow},
                {"loop",c.loop},{"stopOnNext",c.stopOnNext},{"gain",c.gain},{"startSeconds",c.startSeconds},{"behaviour",(int)c.behaviour},{"midi",midi},{"osc",osc}}));
        }
        rows.add(object({{"id",r.id},{"name",r.name},{"tempo",tempo},{"cells",cells},{"gain",r.gain},
            {"muted",r.muted},{"loop",r.loop},{"overrideBehaviour",r.overrideBehaviour},{"behaviour",(int)r.behaviour},{"stopOnNext",r.stopOnNext}}));
    }
    return juce::JSON::toString(object({{"format","TheCuelist"},{"version",7},{"name",show.name},{"nextId",show.nextId},{"columns",columns},{"rows",rows}}));
}
juce::Result decodeShow(const juce::String& text, Show& destination) {
    if (text.length() > 32*1024*1024) return juce::Result::fail("Progetto troppo grande (massimo 32 MB). ");
    try {
        auto root = juce::JSON::parse(text);
        const auto version = integer(root,"version");
        if (string(root,"format") != "TheCuelist" || version < 1 || version > 7) return juce::Result::fail("Formato o versione progetto non supportati.");
        const auto activation = [version](int value) {
            if (version < 3 && (value == 1 || value == 4)) return Behaviour::retrigger;
            return (Behaviour)value;
        };
        Show show; show.name = string(root,"name"); show.nextId = integer(root,"nextId");
        for (auto& c : array(root,"columns")) {
            Column column {integer(c,"id"),string(c,"name"),(ColumnType)integer(c,"type"),integer(c,"output")};
            if (version>=5) column.midiOutputId=string(c,"midiOutputId");
            if (version>=6) { column.oscHost=string(c,"oscHost"); column.oscPort=integer(c,"oscPort"); }
            if (version >= 3) {
                column.gain=(float)number(c,"gain"); column.pan=(float)number(c,"pan");
                column.muted=boolean(c,"muted"); column.phaseInverted=boolean(c,"phaseInverted");
            }
            show.columns.push_back(column);
        }
        for (auto& r : array(root,"rows")) {
            Row row; row.id = integer(r,"id"); row.name = string(r,"name"); row.tempo.clear();
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
                if (version >= 7) cell.stopOnNext=boolean(c,"stopOnNext");
                if (version>=5) {
                    const auto m=required(c,"midi");
                    if (!m.isVoid()) {
                        MidiClip clip; clip.lengthBeats=number(m,"lengthBeats");
                        const auto notes=array(m,"notes");
                        if (notes.size()>10000) throw std::runtime_error("Troppe note MIDI.");
                        for (const auto& n:notes) clip.notes.push_back({number(n,"beat"),number(n,"length"),integer(n,"pitch"),integer(n,"velocity"),integer(n,"channel")});
                        cell.midi=std::move(clip);
                    }
                }
                if (version>=6) {
                    const auto o=required(c,"osc");
                    if (!o.isVoid()) {
                        OscClip clip; clip.useBeats=boolean(o,"useBeats"); clip.length=number(o,"length");
                        const auto events=array(o,"events");
                        if (events.size()>256) throw std::runtime_error("Troppi eventi OSC.");
                        for (const auto& e:events) {
                            OscEvent ev; ev.time=number(e,"time"); ev.address=string(e,"address");
                            const auto args=array(e,"args");
                            if (args.size()>16) throw std::runtime_error("Troppi argomenti OSC.");
                            for (const auto& a:args) {
                                OscValue value; value.type=(OscValueType)integer(a,"type");
                                value.integer=integer(a,"integer"); value.decimal=(float)number(a,"decimal");
                                value.text=string(a,"text"); ev.arguments.push_back(value);
                            }
                            clip.events.push_back(std::move(ev));
                        }
                        cell.osc=std::move(clip);
                    }
                }
                if (version == 1 && cell.file.isNotEmpty() && !juce::File::isAbsolutePath(cell.file)) return juce::Result::fail("Percorso v1 non assoluto.");
                row.cells.push_back(cell);
            }
            show.rows.push_back(std::move(row));
        }
        auto valid = validateShow(show); if (valid.failed()) return valid;
        destination=std::move(show); return juce::Result::ok();
    } catch (const std::exception& e) { return juce::Result::fail(juce::String("Progetto non valido: ")+e.what()); }
}
juce::Result saveShow(const Show& show,const juce::File& file) {
    auto valid = validateShow(show); if (valid.failed()) return valid;
    const auto parent = file.getParentDirectory();
    if (!parent.isDirectory() || file.isDirectory()) return juce::Result::fail("Cartella destinazione non valida.");
    // Each save owns a new media generation. Older generations remain available to backups.
    const auto media = parent.getChildFile(file.getFileNameWithoutExtension()+"_media");
    const auto generation = media.getChildFile(juce::Uuid().toString());
    if (!contained(media, parent) || generation.exists()) return juce::Result::fail("Cartella media non sicura.");
    Show portable = show;
    std::map<juce::String, juce::String> copied;
    bool createdGeneration = false;
    struct Cleanup {
        juce::File folder; bool& created; bool committed = false;
        ~Cleanup() { if (created && !committed) folder.deleteRecursively(); }
    } cleanup { generation, createdGeneration };
    for (auto& row : portable.rows) for (auto& cell : row.cells) {
        if (cell.file.isEmpty()) continue;
        if (!juce::File::isAbsolutePath(cell.file)) return juce::Result::fail("Caricare il progetto prima di salvarlo: percorso media relativo.");
        auto found = copied.find(cell.file);
        if (found != copied.end()) { cell.file = found->second; continue; }
        const juce::File source(cell.file);
        if (!source.existsAsFile()) return juce::Result::fail("Media mancante: " + cell.file);
        if (!createdGeneration) {
            if (generation.createDirectory().failed()) return juce::Result::fail("Impossibile creare la cartella media.");
            createdGeneration = true;
        }
        const auto target = generation.getChildFile(juce::String((int)copied.size()) + source.getFileExtension());
        if (target.exists() || !source.copyFileTo(target)) return juce::Result::fail("Copia media non riuscita.");
        const auto relative = target.getRelativePathFrom(parent).replaceCharacter('\\','/');
        copied.emplace(cell.file, relative); cell.file = relative;
    }
    juce::TemporaryFile temp(file);
    if (!temp.getFile().replaceWithText(encodeShow(portable))) return juce::Result::fail("Impossibile scrivere il file temporaneo.");
    const auto backup = file.getSiblingFile(file.getFileName()+".bak");
    juce::TemporaryFile stagedBackup(backup), oldBackup(backup);
    const bool existing = file.existsAsFile(), hadBackup = backup.existsAsFile();
    if (existing) {
        if (backup.isDirectory() || (hadBackup && !backup.copyFileTo(oldBackup.getFile()))
            || !file.copyFileTo(stagedBackup.getFile()) || !stagedBackup.overwriteTargetFileWithTemporary())
            return juce::Result::fail("Backup non riuscito: il progetto originale non e stato modificato.");
    }
    if (!temp.overwriteTargetFileWithTemporary()) {
        if (existing) { if (hadBackup) oldBackup.overwriteTargetFileWithTemporary(); else backup.deleteFile(); }
        return juce::Result::fail("Impossibile sostituire il progetto. Controllare permessi e spazio libero.");
    }
    cleanup.committed = true;
    return juce::Result::ok();
}
juce::Result loadShow(const juce::File& file, Show& destination) {
    if (!file.existsAsFile() || file.getSize() > 32*1024*1024) return juce::Result::fail("Progetto assente o troppo grande.");
    Show loaded;
    auto result = decodeShow(file.loadFileAsString(), loaded); if (result.failed()) return result;
    for (auto& row : loaded.rows) for (auto& cell : row.cells) {
        if (cell.file.isEmpty() || juce::File::isAbsolutePath(cell.file)) continue;
        if (!safeRelative(cell.file)) return juce::Result::fail("Percorso relativo non sicuro.");
        const auto resolved = file.getParentDirectory().getChildFile(cell.file);
        if (!contained(resolved, file.getParentDirectory())) return juce::Result::fail("Il media esce dalla cartella progetto.");
        cell.file = resolved.getFullPathName();
    }
    destination = std::move(loaded);
    return juce::Result::ok();
}
juce::Result moveCell(Show& show, int sr, int sc, int tr, int tc) {
    const auto valid = [&](int r, int c) { return r >= 0 && c >= 0 && (size_t)r < show.rows.size()
        && (size_t)c < show.columns.size() && (size_t)c < show.rows[(size_t)r].cells.size(); };
    if (!valid(sr,sc) || !valid(tr,tc)) return juce::Result::fail("Posizione cella non valida.");
    if (show.columns[(size_t)sc].type != show.columns[(size_t)tc].type) return juce::Result::fail("Colonne di tipo diverso.");
    std::swap(show.rows[(size_t)sr].cells[(size_t)sc], show.rows[(size_t)tr].cells[(size_t)tc]);
    return juce::Result::ok();
}
}
