#include "WorkspaceLayout.h"
#include <algorithm>

namespace cuelist {
namespace {
constexpr int inspectorWidth=288;
constexpr int inspectorInnerWidth=236;
constexpr int inspectorScaleOriginY=160;
constexpr int cueLeft=182;
constexpr int cueTop=74;
constexpr int cueRightGap=8;
constexpr int cueGridTail=44;
constexpr int minimumGridHeight=310;
constexpr int minimumTimelineHeight=75;
constexpr int statusFooterGap=47;
constexpr int dockBaseY=168;
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

    // The resized dock is a user preference, not an unconditional demand for
    // screen space. At the minimum 820px window height a 400px editor would
    // otherwise push the entire Timeline below the usable window.
    // Fit it below the minimum cue-list region and above a 75px timeline.
    const int maximumDockHeight=std::max(0,input.height-
        (dockBaseY+minimumGridHeight+minimumTimelineHeight+statusFooterGap));
    result.effectiveDockHeight=std::clamp(input.activeDockHeight,0,maximumDockHeight);
    const int baseGridHeight=std::max(minimumGridHeight,
        input.height-400-result.effectiveDockHeight);
    const int baseTimelineHeight=std::max(minimumTimelineHeight,
        input.height-baseGridHeight-result.effectiveDockHeight-215);
    // The upper workspace and the lower Timeline have separate resize
    // budgets. Dragging the Timeline redistributes height without changing
    // the cue header, scrolling behaviour, or audio editor's dock height.
    result.effectiveTimelineAdjustment=std::clamp(input.timelineHeightAdjustment,
        -(baseTimelineHeight-minimumTimelineHeight),
        baseGridHeight-minimumGridHeight);
    const int gridHeight=baseGridHeight-result.effectiveTimelineAdjustment;
    const int utilityHeight=input.height<800 ? 180 : 208;
    // The fixed BPM/Auto Stop band is OUTSIDE the scrollable column region.
    const int lowerWidth=result.editorX-cueRightGap-cueLeft;
    const int cueWidth=lowerWidth-input.pinnedMetricsWidth;
    const int utilityY=154+gridHeight-utilityHeight;
    result.viewport={cueLeft,cueTop,cueWidth,utilityY-cueTop};
    result.utilities={cueLeft,utilityY,cueWidth,utilityHeight};

    result.dockY=dockBaseY+gridHeight;
    result.dockResize={std::max(700,result.editorX-245),result.dockY-8,94,11};
    result.dockCollapse={522,result.dockY+4,86,26};
    result.dockClose={615,result.dockY+4,82,26};
    result.dockEditor={cueLeft,result.dockY,lowerWidth,result.effectiveDockHeight};
    result.timeline={cueLeft,result.dockY+result.effectiveDockHeight,lowerWidth,
        std::max(minimumTimelineHeight,input.height-gridHeight-result.effectiveDockHeight-215)};

    result.timelineResize={cueLeft+(lowerWidth-104)/2,
        result.timeline.getY()-7,104,12};
    result.metrics={result.viewport.getRight(),result.viewport.getY(),
        input.pinnedMetricsWidth,result.viewport.getHeight()};
    result.cueHeaders={result.viewport.getX(),result.viewport.getY(),
        input.pinnedHeaderWidth,result.viewport.getHeight()};

    result.inspectorPanel={result.editorX,76,inspectorWidth-20,input.height-124};
    result.inspectorCueCard={result.inspectorLeft-7,162,inspectorInnerWidth+14,245};
    result.inspectorCellCard={result.inspectorLeft-7,412,inspectorInnerWidth+14,337};

    // Reserve metrics clearance only when columns exceed the usable area.
    // A small show must not gain a fake horizontal scrollbar.
    const int cueContent=input.columnEndX+cueGridTail;
    const int scrollWidth=cueContent; // The fixed right band never needs scroll clearance.
    result.gridSize={
        std::max(result.viewport.getWidth()-14,scrollWidth),
        std::max(result.viewport.getHeight()-14,
                 input.rowHeaderHeight+input.rowCount*input.rowHeight+cueGridTail)};
    return result;
}
} // namespace cuelist
