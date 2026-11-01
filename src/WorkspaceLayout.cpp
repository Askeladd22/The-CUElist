#include "WorkspaceLayout.h"
#include <algorithm>

namespace cuelist {
namespace {
constexpr int inspectorWidth=288;
constexpr int collapsedInspectorRail=26;
constexpr int inspectorInnerWidth=236;
constexpr int inspectorScaleOriginY=160;
constexpr int cueLeft=182;
constexpr int cueTop=74;
// These panels share a real boundary with the Inspector, not a blank gutter.
constexpr int workspaceSeparation=0;
// Content tail is part of CueGridGeometry: row-count, viewport and scroll
// must not calculate different content extents.
constexpr int cueGridTail=CueGridGeometry::gridTail;
constexpr int minimumGridHeight=310;
constexpr int minimumTimelineHeight=75;
constexpr int statusFooterGap=47;
// The dock/timeline begins exactly where the upper Utility panel ends.
constexpr int dockBaseY=154;
// Preserve the tested maximum dock heights when resizing a compact window.
constexpr int originalDockBudgetY=168;
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
    result.editorX=input.width-(input.inspectorVisible ? inspectorWidth : collapsedInspectorRail);
    result.inspectorLeft=result.editorX+16;
    result.inspectorInnerWidth=inspectorInnerWidth;
    result.inspectorScale=std::clamp((float)(input.height-232)/730.0f,0.72f,0.92f);

    // The resized dock is a user preference, not an unconditional demand for
    // screen space. At the minimum 820px window height a 400px editor would
    // otherwise push the entire Timeline below the usable window.
    // Fit it below the minimum cue-list region and above a 75px timeline.
    const int maximumDockHeight=std::max(0,input.height-
        (originalDockBudgetY+minimumGridHeight+minimumTimelineHeight+statusFooterGap));
    const int baseDockHeight=std::clamp(input.activeDockHeight,0,maximumDockHeight);
    const int baseGridHeight=std::max(minimumGridHeight,
        input.height-400-baseDockHeight);
    const int baseTimelineHeight=std::max(minimumTimelineHeight,
        input.height-baseGridHeight-baseDockHeight-201);
    // Growing the Timeline first borrows unused height from the cue viewport.
    // If the cue viewport is already at minimum, an expanded dock may yield
    // space down to 130px; the 36px collapsed dock remains unchanged.
    // Neither horizontal clipping nor the pinned header is affected.
    const int minimumDockHeight=baseDockHeight<=36 ? baseDockHeight : 130;
    const int gridBudget=baseGridHeight-minimumGridHeight;
    const int dockBudget=baseDockHeight-minimumDockHeight;
    result.effectiveTimelineAdjustment=std::clamp(input.timelineHeightAdjustment,
        -(baseTimelineHeight-minimumTimelineHeight),gridBudget+dockBudget);
    const int requestIncrease=std::max(0,result.effectiveTimelineAdjustment);
    const int gridDecrease=std::min(requestIncrease,gridBudget);
    result.effectiveDockHeight=baseDockHeight-(requestIncrease-gridDecrease);
    const int gridHeight=baseGridHeight-gridDecrease+
        std::max(0,-result.effectiveTimelineAdjustment);
    const int utilityHeight=input.height<800 ? 180 : 208;
    // The fixed BPM/Auto Stop band is OUTSIDE the scrollable column region.
    const int lowerWidth=result.editorX-workspaceSeparation-cueLeft;
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
        std::max(minimumTimelineHeight,input.height-gridHeight-result.effectiveDockHeight-201)};

    result.timelineResize={cueLeft+(lowerWidth-104)/2,
        result.timeline.getY()-7,104,12};
    result.metrics={result.viewport.getRight(),result.viewport.getY(),
        input.pinnedMetricsWidth,result.viewport.getHeight()};
    result.cueHeaders={result.viewport.getX(),result.viewport.getY(),
        input.pinnedHeaderWidth,result.viewport.getHeight()};

    result.inspectorPanel={result.editorX,76,inspectorWidth-20,input.height-124};
    result.inspectorCueCard={result.inspectorLeft-7,162,inspectorInnerWidth+14,245};
    result.inspectorCellCard={result.inspectorLeft-7,412,inspectorInnerWidth+14,337};

    // Compute both scrollbars from actual content size. The former unconditional
    // width-9 left an empty 9px lane beside BPM even when no vertical scrollbar
    // was needed. A visible vertical scrollbar now lives at the far LEFT of the
    // viewport, not between the last cue cell and the fixed BPM/Auto Stop band.
    constexpr int cueScrollbarThickness=9;
    const int scrollWidth=input.columnEndX+cueGridTail;
    const int scrollHeight=input.rowHeaderHeight+
                           input.rowCount*input.rowHeight+cueGridTail;
    const int viewWidth=result.viewport.getWidth();
    const int viewHeight=result.viewport.getHeight();
    bool horizontal=scrollWidth>viewWidth;
    bool vertical=scrollHeight>viewHeight-(horizontal?cueScrollbarThickness:0);
    horizontal=scrollWidth>viewWidth-(vertical?cueScrollbarThickness:0);
    vertical=scrollHeight>viewHeight-(horizontal?cueScrollbarThickness:0);
    result.gridSize={
        std::max(viewWidth-(vertical?cueScrollbarThickness:0),scrollWidth),
        std::max(viewHeight-(horizontal?cueScrollbarThickness:0),scrollHeight)};
    return result;
}
} // namespace cuelist
