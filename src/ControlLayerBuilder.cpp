#include "ControlLayerBuilder.h"

namespace cuelist {

std::vector<ControlLayer> buildControlLayers(const Show& show) {
    std::vector<ControlLayer> layers;
    for (size_t column=0;column<show.columns.size();++column) {
        const auto& col=show.columns[column];
        if (col.type!=ColumnType::control) continue;
        if (col.controlLocked) {
            layers.push_back({col.id,0,1,true,col.controlDefaultSet});
        } else {
            for (const auto& row:show.rows) {
                // A corrupted/ragged in-memory model must not crash the
                // mapping refresh before it can be diagnosed/validated.
                if (column>=row.cells.size()) continue;
                if (row.cells[column].controlSet)
                    layers.push_back({col.id,row.id,col.controlPriority,false,
                                      *row.cells[column].controlSet});
            }
        }
    }
    return layers;
}

juce::Result migrateLegacyControlColumns(Show& show,bool& migrated) {
    migrated=false;
    const bool hasLegacy=std::any_of(show.columns.begin(),show.columns.end(),
        [](const Column& col){return col.type==ColumnType::control && col.controlLocked;});
    if (!hasLegacy) return juce::Result::ok();

    Show candidate=show; // No partial migration if an ID/budget check fails.
    const auto allocate=[&candidate]()->int {
        if(candidate.controls.nextId>=999999999) return 0;
        return candidate.controls.nextId++;
    };
    for (size_t column=0;column<candidate.columns.size();++column) {
        auto& col=candidate.columns[column];
        if (col.type!=ColumnType::control || !col.controlLocked) continue;
        // Each old globally locked source becomes a stable named Default Map
        // function, with its own fixed route and globally inherited input.
        // Its former priority-1 layer is no longer used by the live engine.
        int ordinal=0;
        for (const auto& old:col.controlDefaultSet.bindings) {
            if (candidate.controls.functions.size()>=512 ||
                candidate.controls.bindings.size()>=2048)
                return juce::Result::fail("Too many legacy mappings to migrate.");
            const int functionId=allocate(),bindingId=allocate();
            if(functionId==0 || bindingId==0)
                return juce::Result::fail("Legacy mapping identifiers exhausted.");
            ControlBinding output=old;
            output.id=0;output.functionId=0;output.deviceId={};
            output.scope=ControlScope::global;output.scopeId=0;
            const auto name=("Imported "+col.name+" / "+juce::String(++ordinal))
                .substring(0,120);
            candidate.controls.functions.push_back({functionId,name,output});
            ControlBinding input=old;
            input.id=bindingId;input.functionId=functionId;
            input.scope=ControlScope::global;input.scopeId=0;
            candidate.controls.bindings.push_back(std::move(input));
        }

        // Locked columns previously ignored the cue-specific cells. Preserve
        // those dormant edits as reusable templates, NOT automatically live
        // overrides that could unexpectedly fire on the next GO.
        auto archive=[&](std::vector<Row>& rows)->juce::Result {
            for(auto& row:rows) {
                if(column>=row.cells.size())
                    return juce::Result::fail("Legacy Control Column is not rectangular.");
                auto& cell=row.cells[column];
                if(!cell.controlSet || cell.controlSet->bindings.empty()) {
                    cell.controlSet.reset();continue;
                }
                if(candidate.controls.templates.size()>=128)
                    return juce::Result::fail("Too many dormant sets to archive.");
                ControlSetTemplate archived;
                archived.id=allocate();
                if(archived.id==0)
                    return juce::Result::fail("Legacy template identifiers exhausted.");
                archived.name=("Imported Q"+juce::String(row.id)+" / "+col.name)
                    .substring(0,120);
                archived.set=*cell.controlSet;
                for(auto& binding:archived.set.bindings) {
                    binding.id=allocate();
                    if(binding.id==0)
                        return juce::Result::fail("Legacy binding identifiers exhausted.");
                }
                candidate.controls.templates.push_back(std::move(archived));
                cell.controlSet.reset();
            }
            return juce::Result::ok();
        };
        if(auto result=archive(candidate.rows);result.failed()) return result;
        for(size_t list=0;list<candidate.cuelists.size();++list) {
            if((int)list==candidate.activeCuelist) continue;
            if(auto result=archive(candidate.cuelists[list].rows);result.failed())
                return result;
        }
        col.controlDefaultSet.bindings.clear();
        col.controlDefaultSourceRowId=0;
        col.controlLocked=false;
        col.controlPriority=2;
    }
    if(!candidate.cuelists.empty())
        candidate.cuelists[(size_t)candidate.activeCuelist].rows=candidate.rows;
    if(auto result=validateShow(candidate);result.failed())
        return juce::Result::fail("Legacy Control Column migration: "+
                                   result.getErrorMessage());
    show=std::move(candidate);
    migrated=true;
    return juce::Result::ok();
}

int activeControlPresetForCue(const Show& show, int activeCueId) noexcept {
    if (activeCueId<=0) return 0;
    for (const auto& row:show.rows)
        if (row.id==activeCueId) return row.mappingPresetId;
    return 0;
}

} // namespace cuelist
