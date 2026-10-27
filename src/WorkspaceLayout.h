#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace cuelist {

// Input facts, not UI components: the calculation does not mutate state or
// access a renderer, an audio engine or an operating-system window.
struct WorkspaceLayoutInput {
    int width=1440;
    int height=900;
    int activeDockHeight=0;
    int columnEndX=467;   // grid.columnX(number of columns)
    int rowCount=1;
    int rowHeaderHeight=72;
    int rowHeight=62;
    int pinnedMetricsWidth=184;
};

struct WorkspaceLayout {
    int editorX=0;
    int inspectorLeft=0;
    int inspectorInnerWidth=236;
    int dockY=0;
    float inspectorScale=1.0f;

    juce::Rectangle<int> viewport;
    juce::Rectangle<int> utilities;
    juce::Rectangle<int> timeline;
    juce::Rectangle<int> dockEditor;
    juce::Rectangle<int> dockResize;
    juce::Rectangle<int> dockCollapse;
    juce::Rectangle<int> dockClose;
    juce::Rectangle<int> metrics;
    juce::Rectangle<int> inspectorPanel;

    // The Inspector cards are in *unscaled* painting coordinates; the same
    // vertical transformation is applied to labels and component bounds.
    juce::Rectangle<int> inspectorCueCard;
    juce::Rectangle<int> inspectorCellCard;
    juce::Point<int> gridSize;

    juce::AffineTransform inspectorTransform() const noexcept;
    juce::Rectangle<int> scaleInspectorControl(juce::Rectangle<int>) const noexcept;
};

// Keeps the 0.9 screenshot-approved geometry unchanged at the supported
// desktop sizes. Dock height is already resolved to 0 / 36 / dockHeight by
// MainComponent; geometry itself has no editor lifetime/visibility concerns.
WorkspaceLayout calculateWorkspaceLayout(const WorkspaceLayoutInput& input);

} // namespace cuelist
