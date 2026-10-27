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

int activeControlPresetForCue(const Show& show, int activeCueId) noexcept {
    if (activeCueId<=0) return 0;
    for (const auto& row:show.rows)
        if (row.id==activeCueId) return row.mappingPresetId;
    return 0;
}

} // namespace cuelist
