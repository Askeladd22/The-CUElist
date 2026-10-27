#include "WorkspaceLayout.h"
#include <algorithm>

namespace cuelist {
namespace {
constexpr int inspectorWidth=288;
constexpr int inspectorInnerWidth=236;
constexpr int inspectorScaleOriginY=226;
constexpr int cueLeft=192;
constexpr int cueTop=86;
constexpr int cueRightGap=8;
constexpr int cueGridTail=44;
}

juce::AffineTransform WorkspaceLayout::inspectorTransform() const noexcept {
    return juce::AffineTransform(1,0,0,0,inspectorScale,
                                 inspectorScaleOriginY*(1-inspectorScale));
}

juce::Rectangle<int> WorkspaceLayout::scaleInspectorControl(
        juce::Rectangle<int> rect) const noexcept {
    // JUCE float -> int truncation and the 22px hit-target floor are part
    // of the pre-refactor Inspector behavior. Preserve both for now.
    rect.setY(inspectorScaleOriginY+
              (int)((rect.getY()-inspectorScaleOriginY)*inspectorScale));
    rect.setHeight(std::max(22,(int)(rect.getHeight()*inspectorScale)));
    return rect;
}

WorkspaceLayout calculateWorkspaceLayout(const WorkspaceLayoutInput& input) {
    WorkspaceLayout result;
    result.editorX=input.width-inspectorWidth;
    result.inspectorLeft=result.editorX+16;
    result.inspectorInnerWidth=inspectorInnerWidth;
    result.inspectorScale=std::clamp((float)(input.height-232)/730.0f,0.72f,0.92f);

    const int gridHeight=std::max(310,input.height-400-input.activeDockHeight);
    const int utilityHeight=input.height<800 ? 180 : 208;
    const int cueWidth=result.editorX-cueRightGap-cueLeft;
    const int utilityY=154+gridHeight-utilityHeight;
    result.viewport={cueLeft,cueTop,cueWidth,utilityY-cueTop};
    result.utilities={cueLeft,utilityY,cueWidth,utilityHeight};

    result.dockY=168+gridHeight;
    result.dockResize={std::max(700,result.editorX-245),result.dockY-8,94,11};
    result.dockCollapse={522,result.dockY+4,86,26};
    result.dockClose={615,result.dockY+4,82,26};
    result.dockEditor={cueLeft,result.dockY,cueWidth,input.activeDockHeight};
    result.timeline={cueLeft,result.dockY+input.activeDockHeight,cueWidth,
        std::max(75,input.height-gridHeight-input.activeDockHeight-215)};

    result.metrics={result.viewport.getRight()-input.pinnedMetricsWidth,
        result.viewport.getY(),input.pinnedMetricsWidth,result.viewport.getHeight()};

    result.inspectorPanel={result.editorX,154,inspectorWidth-20,input.height-201};
    result.inspectorCueCard={result.inspectorLeft-7,228,inspectorInnerWidth+14,245};
    result.inspectorCellCard={result.inspectorLeft-7,478,inspectorInnerWidth+14,337};

    // Reserve metrics clearance only when columns exceed the usable area.
    // A small show must not gain a fake horizontal scrollbar.
    const int cueContent=input.columnEndX+cueGridTail;
    const int clearWidth=result.viewport.getWidth()-input.pinnedMetricsWidth-14;
    const int scrollWidth=cueContent+(cueContent>clearWidth ? input.pinnedMetricsWidth : 0);
    result.gridSize={
        std::max(result.viewport.getWidth()-14,scrollWidth),
        std::max(result.viewport.getHeight()-14,
                 input.rowHeaderHeight+input.rowCount*input.rowHeight+cueGridTail)};
    return result;
}
} // namespace cuelist
