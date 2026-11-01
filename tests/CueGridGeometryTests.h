#pragma once
#include "CueGridGeometry.h"
#include "WorkspaceLayout.h"
#include <array>
#include <vector>

void cueGridGeometryTests() {
    using namespace cuelist;
    test("N3.1 original cue geometry is shared by paint, input and Master",[] {
        check(CueGrid::rowHeight==CueGridGeometry::rowHeight &&
              CueGrid::headerHeight==CueGridGeometry::headerHeight &&
              CueGrid::nameWidth==CueGridGeometry::nameWidth &&
              CueGrid::columnWidth==CueGridGeometry::columnWidth &&
              CueGrid::compressedWidth==CueGridGeometry::compressedWidth &&
              CueGrid::rowContentInset==CueGridGeometry::rowContentInset &&
              CueMetricsOverlay::width==CueGridGeometry::pinnedMetricsWidth,
              "Centralized dimensions drifted from original JUCE controls");
        check(CueGridGeometry::rowHeight==62 &&
              CueGridGeometry::headerHeight==72 &&
              CueGridGeometry::nameWidth==318 &&
              CueGridGeometry::columnWidth==152 &&
              CueGridGeometry::compressedWidth==48 &&
              CueGridGeometry::pinnedMetricsWidth==184,
              "N3.1 must not shrink native hitboxes or original Master Audio");
        check(CueGridGeometry::rowAt(71)==-1 &&
              CueGridGeometry::rowAt(72)==0 &&
              CueGridGeometry::rowAt(133)==0 &&
              CueGridGeometry::rowAt(134)==1 &&
              CueGridGeometry::rowLocalY(134)==0,
              "Row identification no longer matches painted pixel boundaries");
    });

    test("N3.1 13 cue / 11 mixed column stress geometry with collapses",[] {
        struct Entry {bool collapsed=false;};
        std::vector<Entry> columns(11);
        columns[1].collapsed=true;
        columns[4].collapsed=true;
        columns[8].collapsed=true;
        const auto& geo=CueGridGeometry{};
        const int total=geo.xAt(columns,11);
        check(total==geo.nameWidth+8*geo.columnWidth+3*geo.compressedWidth,
              "Column width sum does not include collapsed lanes");
        check(geo.columnAt(columns,geo.nameWidth-1)==-1 &&
              geo.columnAt(columns,total)==-1 &&
              geo.widthAt(columns,-1)==0 &&
              geo.widthAt(columns,11)==0,
              "Column hit test crosses the name lane or trailing workspace");
        for (int i=0;i<11;++i) {
            const int x=geo.xAt(columns,i),w=geo.widthAt(columns,i);
            check(geo.columnAt(columns,x)==i &&
                  geo.columnAt(columns,x+w-1)==i &&
                  geo.columnAt(columns,x+w)!=i,
                  "Rendering and hit test disagree at a column edge");
        }
        for (int scrollX:{0,25,169,475}) {
            for (int i=0;i<11;++i) {
                const int gridX=geo.xAt(columns,i)+2;
                const int pinnedLocal=geo.pinnedColumnLocalX(gridX,scrollX);
                check(geo.pinnedColumnGridX(pinnedLocal,scrollX)==gridX,
                      "Pinned header X and scrolled cell X have diverged");
            }
        }
        for (int scrollY:{0,24,62,155,530}) {
            for (int row=0;row<13;++row) {
                const int localTop=geo.overlayRowTop(row,scrollY);
                const int sampleY=localTop+20;
                // When overlaid rows are behind the fixed header, a click
                // must be clipped by the pinned header before rowAt.
                if(sampleY>=geo.headerHeight) {
                    check(geo.overlayRowAt(sampleY,scrollY)==row &&
                          geo.rowLocalY(sampleY+scrollY)==20,
                          "Pinned cue / BPM row Y disagrees with hit testing");
                }
            }
        }
        check(geo.scrollContentWidth(columns)==total+geo.gridTail &&
              geo.scrollContentHeight(13)==geo.rowTop(13)+geo.gridTail,
              "Content area for scrollbars differs from row/column geometry");
    });

    test("N3.1 fixed BPM band and viewport match dense window contracts",[] {
        struct Entry { bool collapsed=false; };
        std::vector<Entry> columns(11);
        for (int i=0;i<11;++i)
            columns[(std::size_t)i].collapsed=(i==2 || i==8);
        for (const auto size:std::initializer_list<juce::Point<int>>{
             {1280,720},{1647,895},{1920,1080},{3840,1080}}) {
            WorkspaceLayoutInput input;
            input.width=size.x; input.height=size.y;
            input.rowCount=13;
            input.rowHeaderHeight=CueGridGeometry::headerHeight;
            input.rowHeight=CueGridGeometry::rowHeight;
            input.pinnedHeaderWidth=CueGridGeometry::nameWidth;
            input.pinnedMetricsWidth=CueGridGeometry::pinnedMetricsWidth;
            input.columnEndX=CueGridGeometry::xAt(columns,11);
            const auto frame=calculateWorkspaceLayout(input);
            check(frame.metrics.getX()==frame.viewport.getRight() &&
                  frame.metrics.getWidth()==CueGridGeometry::pinnedMetricsWidth &&
                  frame.metrics.getRight()==frame.editorX &&
                  frame.cueHeaders.getX()==frame.viewport.getX() &&
                  frame.cueHeaders.getWidth()==CueGridGeometry::nameWidth &&
                  frame.utilities.getRight()==frame.metrics.getX(),
                  "Pinned BPM / header and mixer no longer have common boundaries");
            check(frame.gridSize.x>=CueGridGeometry::scrollContentWidth(columns) &&
                  frame.gridSize.y>=CueGridGeometry::scrollContentHeight(13),
                  "Viewport no longer contains all 13 rows / 11 columns");
        }
    });
}
