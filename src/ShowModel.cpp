#include "ShowModel.h"
#include <cmath>
#include <set>
#include <stdexcept>
namespace cuelist {
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
    addColumn(show, ColumnType::audio, "Voce"); addColumn(show, ColumnType::audio, "Ambiente");
    addColumn(show, ColumnType::audio, "Effetti"); addColumn(show, ColumnType::midi, "MIDI 01");
    addColumn(show, ColumnType::osc, "OSC 01");
    for (auto name : {"Intro", "Ingresso", "Scena 01", "Transizione", "Scena 02", "Finale"}) addRow(show,name);
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
        if (!unique(column.id) || column.name.trim().isEmpty() || column.output < 0 || column.output > 1022
            || (int)column.type < 0 || (int)column.type > 2) return fail("Colonna non valida.");
    }
    for (const auto& row : show.rows) {
        if (!unique(row.id) || row.name.trim().isEmpty() || row.cells.size()!=show.columns.size() || row.tempo.empty()) return fail("Riga non valida.");
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
            if (!unique(c.id) || !std::isfinite(c.gain) || c.gain < 0 || c.gain > 1 || (int)c.behaviour < 0 || (int)c.behaviour > 4)
                return fail("Cella non valida.");
            if (c.file.isNotEmpty() && (show.columns[i].type != ColumnType::audio || !juce::File::isAbsolutePath(c.file)))
                return fail("Il file audio deve avere un percorso assoluto e una colonna Audio.");
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
    for (const auto& c : show.columns) columns.add(object({{"id",c.id},{"name",c.name},{"type",(int)c.type},{"output",c.output}}));
    for (const auto& r : show.rows) {
        juce::Array<juce::var> tempo, cells;
        for (const auto& p : r.tempo) tempo.add(object({{"beat",p.beat},{"bpm",p.bpm},{"numerator",p.numerator},{"denominator",p.denominator}}));
        for (const auto& c : r.cells) cells.add(object({{"id",c.id},{"name",c.name},{"file",c.file},{"followsRow",c.followsRow},
            {"loop",c.loop},{"gain",c.gain},{"behaviour",(int)c.behaviour}}));
        rows.add(object({{"id",r.id},{"name",r.name},{"tempo",tempo},{"cells",cells}}));
    }
    return juce::JSON::toString(object({{"format","TheCuelist"},{"version",1},{"name",show.name},{"nextId",show.nextId},{"columns",columns},{"rows",rows}}));
}
juce::Result decodeShow(const juce::String& text, Show& destination) {
    if (text.length() > 32*1024*1024) return juce::Result::fail("Progetto troppo grande (massimo 32 MB). ");
    try {
        auto root = juce::JSON::parse(text);
        if (string(root,"format") != "TheCuelist" || integer(root,"version") != 1) return juce::Result::fail("Formato o versione progetto non supportati.");
        Show show; show.name = string(root,"name"); show.nextId = integer(root,"nextId");
        for (auto& c : array(root,"columns")) show.columns.push_back({integer(c,"id"),string(c,"name"),(ColumnType)integer(c,"type"),integer(c,"output")});
        for (auto& r : array(root,"rows")) {
            Row row; row.id = integer(r,"id"); row.name = string(r,"name"); row.tempo.clear();
            for (auto& p : array(r,"tempo")) row.tempo.push_back({number(p,"beat"),number(p,"bpm"),integer(p,"numerator"),integer(p,"denominator")});
            for (auto& c : array(r,"cells")) {
                Cell cell; cell.id=integer(c,"id"); cell.name=string(c,"name"); cell.file=string(c,"file");
                cell.followsRow=boolean(c,"followsRow"); cell.loop=boolean(c,"loop"); cell.gain=(float)number(c,"gain"); cell.behaviour=(Behaviour)integer(c,"behaviour");
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
    juce::TemporaryFile temp(file);
    if (!temp.getFile().replaceWithText(encodeShow(show))) return juce::Result::fail("Impossibile scrivere il file temporaneo.");
    if (file.existsAsFile() && !file.copyFileTo(file.getSiblingFile(file.getFileName()+".bak")))
        return juce::Result::fail("Backup non riuscito: il progetto originale non e stato modificato.");
    if (!temp.overwriteTargetFileWithTemporary()) return juce::Result::fail("Impossibile sostituire il progetto. Controllare permessi e spazio libero.");
    return juce::Result::ok();
}
}
