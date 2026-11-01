#pragma once
#include <algorithm>
#include <cstddef>

namespace cuelist {

// Native N3.1 coordinate contract for CueGrid, pinned header overlays and
// the fixed BPM/Auto Stop lane. These are the VERIFIED ORIGINAL dimensions,
// not the smaller untested HTML concept. All hitboxes remain unchanged.
// Crucially, Master Audio is sized from the original nameWidth (318px).
struct CueGridGeometry final {
    static constexpr int rowHeight=62;
    static constexpr int headerHeight=72;
    static constexpr int nameWidth=318;
    static constexpr int columnWidth=152;
    static constexpr int compressedWidth=48;
    static constexpr int pinnedMetricsWidth=184;
    static constexpr int rowContentInset=4;
    static constexpr int gridTail=44;

    // Columns are model values (with .collapsed). Geometry never owns or
    // modifies a model, transport state, scrolling controller or callbacks.
    template<class Columns>
    static int widthAt(const Columns& columns,int index) noexcept {
        if (index<0 || index>=(int)columns.size()) return 0;
        return columns[(size_t)index].collapsed ? compressedWidth : columnWidth;
    }
    template<class Columns>
    static int xAt(const Columns& columns,int index) noexcept {
        int x=nameWidth;
        for (int i=0;i<index && i<(int)columns.size();++i)
            x+=widthAt(columns,i);
        return x;
    }
    template<class Columns>
    static int columnAt(const Columns& columns,int gridX) noexcept {
        if (gridX<nameWidth) return -1;
        int x=nameWidth;
        for (int i=0;i<(int)columns.size();++i) {
            const int next=x+widthAt(columns,i);
            if (gridX>=x && gridX<next) return i;
            x=next;
        }
        return -1;
    }

    static constexpr int rowTop(int row) noexcept {
        return headerHeight+row*rowHeight;
    }
    static constexpr int rowAt(int gridY) noexcept {
        return gridY<headerHeight ? -1 : (gridY-headerHeight)/rowHeight;
    }
    static constexpr int rowLocalY(int gridY) noexcept {
        return gridY<headerHeight ? -1 : (gridY-headerHeight)%rowHeight;
    }
    static constexpr int overlayRowAt(int localY,int verticalScroll) noexcept {
        return rowAt(localY+verticalScroll);
    }
    static constexpr int overlayRowTop(int row,int verticalScroll) noexcept {
        return rowTop(row)-verticalScroll;
    }
    static constexpr int pinnedColumnGridX(int localX,int horizontalScroll) noexcept {
        return nameWidth+horizontalScroll+localX;
    }
    static constexpr int pinnedColumnLocalX(int gridX,int horizontalScroll) noexcept {
        return gridX-nameWidth-horizontalScroll;
    }
    static constexpr int scrollContentHeight(int rows) noexcept {
        return rowTop(rows)+gridTail;
    }
    template<class Columns>
    static int scrollContentWidth(const Columns& columns) noexcept {
        return xAt(columns,(int)columns.size())+gridTail;
    }
};
} // namespace cuelist
