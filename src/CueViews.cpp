#include "MainComponent.h"
#include <cmath>

namespace cuelist {
namespace {
juce::String columnTypeName(ColumnType type) { return type==ColumnType::audio ? "AUDIO" : type==ColumnType::midi ? "MIDI" : type==ColumnType::osc ? "OSC" : "CONTROL"; }
juce::String timeText(double seconds) {
    const auto whole=(int)seconds;
    return juce::String(whole/60).paddedLeft('0',2)+":"+juce::String(whole%60).paddedLeft('0',2);
}
void smallButton(juce::Graphics& g, const juce::String& text,
                 juce::Rectangle<int> box, bool enabled, bool danger=false,
                 bool understated=false) {
    const auto colour=danger ? palette::red : palette::muted;
    const IconButtonState state{enabled,false,false,false,false};
    // Secondary grid controls are visually quiet, but retain the exact
    // paint rectangles and original independent mouse hit targets.
    if (understated) {
        g.setColour((danger ? palette::red : palette::raised)
                       .withAlpha(enabled ? .22f : .10f));
        g.fillRoundedRectangle(box.toFloat().reduced(.75f),3.0f);
    } else {
        drawIconButtonSurface(g,box.toFloat(),colour,state);
    }
    const auto ink=iconButtonInk(colour,state);
    if (text=="^" || text=="v" || text=="<" || text==">") {
        // Unicode directional symbols are rendered in embedded Noto Sans
        // Symbols 2, rather than text-shaped ^, v, < or > placeholders.
        const auto cp=(juce::juce_wchar)(
            text=="^" ? 0x25B2 : text=="v" ? 0x25BC :
            text=="<" ? 0x25C0 : 0x25B6);
        g.setColour(ink);
        if (!drawNotoSymbol(g,cp,box,12.0f,ink)) {
            const auto c=box.toFloat().getCentre();
            juce::Path triangle;
            if (text=="^") triangle.addTriangle(c.x,c.y-4.2f,c.x-5.f,c.y+3.5f,c.x+5.f,c.y+3.5f);
            if (text=="v") triangle.addTriangle(c.x,c.y+4.2f,c.x-5.f,c.y-3.5f,c.x+5.f,c.y-3.5f);
            if (text=="<") triangle.addTriangle(c.x-4.2f,c.y,c.x+3.5f,c.y-5.f,c.x+3.5f,c.y+5.f);
            if (text==">") triangle.addTriangle(c.x+4.2f,c.y,c.x-3.5f,c.y-5.f,c.x-3.5f,c.y+5.f);
            g.fillPath(triangle);
        }
    } else if (text=="...") {
        // The operational menu uses an actual midline ellipsis, not three
        // punctuation characters from the ordinary label font.
        if (!drawNotoSymbol(g,(juce::juce_wchar)0x22EF,box,15.5f,ink)) {
            const auto c=box.toFloat().getCentre();
            g.setColour(ink);
            for (int offset=-1;offset<=1;++offset)
                g.fillEllipse(c.x+offset*4.0f-1.1f,c.y-1.1f,2.2f,2.2f);
        }
    } else if (text=="x") {
        // Deletion must remain legible regardless of Unicode coverage.
        const auto c=box.toFloat().getCentre();
        juce::Path cross;
        cross.startNewSubPath(c.x-3.8f,c.y-3.8f);
        cross.lineTo(c.x+3.8f,c.y+3.8f);
        cross.startNewSubPath(c.x+3.8f,c.y-3.8f);
        cross.lineTo(c.x-3.8f,c.y+3.8f);
        g.setColour(ink);
        g.strokePath(cross,juce::PathStrokeType(1.9f,juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    } else {
        label(g,text,box,12,ink,true,juce::Justification::centred);
    }
}
// Draw the loop glyph as vector geometry: never rely on a missing Unicode glyph.
void drawLoopIcon(juce::Graphics& g,juce::Rectangle<int> bounds,juce::Colour colour) {
    // JUCE arc angles are clockwise from 12 o'clock, not the conventional
    // cos(x)/sin(y) coordinate system. The arrow must use the same convention
    // and follow the tangent at the arc endpoint to remain aligned at 13-15 px.
    const auto centre=bounds.toFloat().getCentre();
    const float side=(float)std::min(bounds.getWidth(),bounds.getHeight());
    const float radius=side*0.33f;
    constexpr float from=1.92f;
    const float to=juce::MathConstants<float>::twoPi+0.64f;
    juce::Path circle;
    circle.addCentredArc(centre.x,centre.y,radius,radius,0.0f,from,to,true);
    g.setColour(colour);
    g.strokePath(circle,juce::PathStrokeType(1.55f,juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));

    const juce::Point<float> end{centre.x+radius*std::sin(to),
                                 centre.y-radius*std::cos(to)};
    const juce::Point<float> tangent{std::cos(to),std::sin(to)};
    const juce::Point<float> normal{-tangent.y,tangent.x};
    const float headLength=side*0.26f;
    const float halfWidth=side*0.14f;
    const auto tip=end+tangent*(headLength*0.48f);
    const auto base=end-tangent*(headLength*0.52f);
    juce::Path arrow;
    const auto left=base+normal*halfWidth;
    const auto right=base-normal*halfWidth;
    arrow.addTriangle(tip.x,tip.y,left.x,left.y,right.x,right.y);
    g.fillPath(arrow);
}
void playPauseIcon(juce::Graphics& g,juce::Rectangle<int> box,bool pause) {
    const IconButtonState state{true,false,false,pause,false};
    drawIconButtonSurface(g,box.toFloat(),palette::teal,state);
    const auto ink=iconButtonInk(palette::teal,state);
    g.setColour(ink);
    if (drawNotoSymbol(g,(juce::juce_wchar)(pause ? 0x23F8 : 0x25B6),
                       box,17.5f,ink))
        return;
    // Keep the previous vector drawing as a reliable fallback.
    const float cx=(float)box.getCentreX(),cy=(float)box.getCentreY();
    if (pause) {
        g.fillRoundedRectangle(cx-5.5f,cy-5.f,3.5f,10.f,0.7f);
        g.fillRoundedRectangle(cx+2.f,cy-5.f,3.5f,10.f,0.7f);
    } else {
        juce::Path p; p.addTriangle(cx-3.5f,cy-5.5f,cx-3.5f,cy+5.5f,cx+5.f,cy);
        g.fillPath(p);
    }
}
void transportButton(juce::Graphics& g,juce::Rectangle<int> box,bool stop,
                     bool enabled,float iconSize=17.5f) {
    const auto accent=stop ? palette::red : palette::teal;
    const IconButtonState state{enabled,false,false,false,false};
    drawIconButtonSurface(g,box.toFloat(),accent,state);
    const auto ink=iconButtonInk(accent,state);
    g.setColour(ink);
    if (drawNotoSymbol(g,(juce::juce_wchar)(stop ? 0x23F9 : 0x25B6),
                       box,iconSize,ink))
        return;
    const float x=(float)box.getCentreX(),y=(float)box.getCentreY();
    if (stop) g.fillRect(x-4,y-4,8.0f,8.0f);
    else { juce::Path triangle; triangle.addTriangle(x-3,y-5,x-3,y+5,x+5,y); g.fillPath(triangle); }
}
juce::String clippedCueName(const Show& show,int row) {
    if (row<0 || row>=(int)show.rows.size()) return {};
    const auto& name=show.rows[(size_t)row].name;
    const auto visible=name.isEmpty()?juce::String("Untitled cue"):name;
    return juce::GlyphArrangement::getStringWidth(font(15.0f,true),visible) >
        (float)CueGrid::rowNameBounds(0).getWidth() ? visible : juce::String{};
}
}


int CueGrid::columnWidthAt(int index) const {
    return CueGridGeometry::widthAt(owner.show.columns,index);
}
int CueGrid::columnX(int index) const {
    return CueGridGeometry::xAt(owner.show.columns,index);
}
int CueGrid::totalColumnsWidth() const {
    return columnX((int)owner.show.columns.size())-nameWidth;
}
int CueGrid::columnAt(int x) const {
    return CueGridGeometry::columnAt(owner.show.columns,x);
}
juce::Colour CueGrid::cueColour(int id) {
    switch (id) {
        case 1:return palette::teal;
        case 2:return palette::amber;
        case 3:return palette::red;
        case 4:return palette::purple;
        case 5:return juce::Colour(0xff69aeea);
        case 6:return juce::Colour(0xff73d89b);
        case 7:return juce::Colour(0xffffa669);
        default:return juce::Colour();
    }
}
void CueGrid::paint(juce::Graphics& g) {
    g.fillAll(palette::background);
    // A continuous 72px pinned header, preserving the N3.1 coordinates.
    g.setColour(palette::panel);g.fillRect(0,0,getWidth(),headerHeight);
    g.setColour(palette::line.withAlpha(.72f));
    g.drawHorizontalLine(headerHeight-1,0,(float)getWidth());
    g.setColour(palette::teal);
    g.fillRect(0,0,nameWidth-1,3);
    // Light vertical identity marker; the real Cuelist tab already
    // indicates which list is active and is never duplicated as a button.
    g.setColour(palette::teal.withAlpha(.70f));
    g.fillRoundedRectangle(12.f,13.f,2.f,40.f,1.f);
    const auto& show=owner.show;
    const auto activeName=show.cuelists.empty()
        ? juce::String("Cuelist 1")
        : show.cuelists[(size_t)show.activeCuelist].name;
    label(g,activeName,{24,9,nameWidth-173,24},
          15,palette::text,true);
    // Count is useful operator telemetry, unlike the duplicate
    // "CUE / TRANSPORT" section label which consumed a third text row.
    label(g,juce::String((int)show.rows.size())+" cues  /  "+
          juce::String(owner.visibleClipCount)+" clips",
          {24,36,nameWidth-48,20},11,palette::muted,true);
    g.setColour(palette::line.withAlpha(.28f));
    g.fillRoundedRectangle(24.f,63.f,(float)nameWidth-48.f,1.f,.5f);
    for (int c=0;c<(int)show.columns.size();++c) {
        const auto& col=show.columns[(size_t)c];
        const int x=columnX(c),w=columnWidthAt(c);
        const auto tone=palette::type((int)col.type);
        // Type stays identifiable by colour AND label without a bright
        // full-column block, including when a column is collapsed.
        g.setColour(palette::raised.withAlpha(.25f));
        g.fillRect(x+1,3,w-2,headerHeight-4);
        g.setColour(tone.withAlpha(.94f));
        g.fillRect(x+1,0,w-2,3);
        if (col.collapsed) {
            label(g,"+",{x+7,9,w-14,23},17,tone,true,juce::Justification::centred);
            const auto shortName=col.type==ColumnType::audio ? "A" :
                col.type==ColumnType::midi ? "M" : col.type==ColumnType::osc ? "O" : "C";
            label(g,shortName,{x+7,39,w-14,20},12,palette::muted,true,juce::Justification::centred);
        } else {
            label(g,columnTypeName(col.type),{x+7,9,w-68,20},11,tone,true);
            smallButton(g,"<",{x+w-61,10,18,19},c>0,false,true);
            smallButton(g,">",{x+w-41,10,18,19},
                        c+1<(int)show.columns.size(),false,true);
            smallButton(g,"x",{x+w-21,10,18,19},true,true,true);
            int ordinal=0;
            for (int i=0;i<=c;++i) if (show.columns[(size_t)i].type==col.type) ++ordinal;
            const auto defaultName=col.type==ColumnType::audio ? "Audio" :
                col.type==ColumnType::midi ? "MIDI" : col.type==ColumnType::osc ? "OSC" : "Control";
            const auto name=col.name.isEmpty()? juce::String(defaultName)+" "+juce::String(ordinal) : col.name;
            label(g,name,{x+8,35,w-38,25},14,palette::text,true);
            smallButton(g,"-",{x+w-26,38,19,20},true,false,true);
        }
        g.setColour(palette::line.withAlpha(.54f));
        g.drawVerticalLine(x,0,(float)getHeight());
    }
    // Contextual add-column control follows the rightmost column, including
    // after reordering or when all preceding columns are compressed.
    smallButton(g,"+",{columnX((int)show.columns.size())+6,16,31,31},true);
    const auto clip=g.getClipBounds();
    const int first=std::max(0,(clip.getY()-headerHeight)/rowHeight);
    const int last=std::min((int)show.rows.size(),(clip.getBottom()-headerHeight)/rowHeight+1);
    for (int r=first;r<last;++r) {
        const int y=headerHeight+r*rowHeight;
        const auto& row=show.rows[(size_t)r];
        const bool selected=r==owner.selectedRow,next=r==owner.nextCueIndex();
        bool playing=false,paused=false;
        if (owner.engine.connected.load()) for (const auto& cell:row.cells) {
            const auto state=owner.engine.getCellPlayback(cell.id);
            playing=playing||state.playing;paused=paused||state.paused;
        }
        const auto custom=cueColour(row.colour);
        // One continuous row surface; subtle alternating luminance and
        // independent selection, NEXT and actual playback signals.
        const auto base=(r&1)==0 ? palette::panel :
            palette::panel.interpolatedWith(palette::raised,.10f);
        g.setColour(selected ? palette::raised.interpolatedWith(palette::panel,.20f) : base);
        g.fillRect(0,y,getWidth(),rowHeight);
        if (playing) {
            g.setColour(palette::teal.withAlpha(selected?.08f:.10f));
            g.fillRect(0,y,getWidth(),rowHeight);
        }
        if (next) {
            g.setColour(palette::amber.withAlpha(selected?.08f:.10f));
            g.fillRect(0,y,getWidth(),rowHeight);
        }
        if (row.colour>0) {
            g.setColour(custom.withAlpha(.13f));
            g.fillRect(0,y,getWidth(),rowHeight);
        }
        // Parallel status rails remain unambiguous for selected NEXT cues.
        if (selected) {
            g.setColour(palette::text.withAlpha(.78f));
            g.fillRect(0,y,2,rowHeight);
        }
        if (next) {
            g.setColour(palette::amber);
            g.fillRect(selected?2:0,y,3,rowHeight);
        }
        if (playing) {
            g.setColour(palette::teal);
            g.fillRect(selected?(next?5:2):(next?3:0),y,2,rowHeight);
        }
        if (row.colour>0 && !next && !playing) {
            g.setColour(custom.withAlpha(.88f));
            g.fillRect(selected?2:0,y,2,rowHeight);
        }
        g.saveState();
        g.addTransform(juce::AffineTransform::translation(0.f,(float)rowContentInset));
        smallButton(g,"^",{5,y+6,18,18},r>0,false,true);
        smallButton(g,"v",{5,y+27,18,18},
                    r+1<(int)show.rows.size(),false,true);
        smallButton(g,"x",{27,y+13,18,20},true,true,true);
        g.setColour(next ? palette::amber : palette::teal);
        g.fillRoundedRectangle(rowGoBounds(y).toFloat(),5.f);
        label(g,"GO",rowGoBounds(y),13,palette::background,true,
              juce::Justification::centred);
        // The title occupies its own first tier, with the three compact
        // transport targets below it. Both tiers remain inside the 62px row.
        label(g,row.name.isEmpty()?"Untitled cue":row.name,
              rowNameBounds(y),15,palette::text,true);
        const auto state=playing?"PLAYING":paused?"PAUSED":
                         next?"NEXT CUE":row.muted?"MUTED":"STANDBY";
        const auto stateInk=playing ? palette::teal :
                            paused || next ? palette::amber :
                            row.muted ? palette::red : palette::muted;
        label(g,juce::String(r+1).paddedLeft('0',2)+" / "+state,
              rowStatusBounds(y),10,stateInk,true);
        playPauseIcon(g,rowPlayBounds(y),playing);
        transportButton(g,rowStopBounds(y),true,true);
        smallButton(g,"...",rowMenuBounds(y),true,false,true);
        for (int c=0;c<(int)show.columns.size();++c) {
            const auto& column=show.columns[(size_t)c];
            const auto& cell=row.cells[(size_t)c];
            const int x=columnX(c),w=columnWidthAt(c);
            const bool assigned=column.type==ColumnType::control &&
                (column.controlLocked || hasControlMapAssignment(cell));
            const bool loaded=owner.hasClip(r,c) || assigned;
            const bool missing=cell.file.isNotEmpty()&&!loaded;
            const auto tone=missing?palette::red:palette::type((int)column.type);
            const bool selectedCell=selected && c==owner.selectedColumn;
            const bool hovered=hoverRow==r && hoverColumn==c;
            const bool drop=dragging && dropRow==r && dropColumn==c;
            if (column.collapsed) {
                // Empty collapsed lanes are not false clip buttons.
                if (loaded) {
                    g.setColour(tone.withAlpha(.13f));
                    g.fillRoundedRectangle((float)x+5,(float)y+7,(float)w-10,39.f,4.f);
                    g.setColour(tone);
                    g.fillEllipse((float)x+w/2-4,(float)y+21,8.f,8.f);
                } else {
                    label(g,"-",{x+8,y+13,w-16,20},12,
                          palette::muted.withAlpha(.72f),true,juce::Justification::centred);
                }
                if(!cell.followsRow) {g.setColour(palette::red);g.fillEllipse((float)x+6,(float)y+8,3.f,3.f);}
                if(cell.loop) drawLoopIcon(g,{x+w-19,y+8,13,15},palette::amber);
                // Compressed columns cannot fit text, but a muted amber
                // marker preserves visibility of the Auto Stop exception.
                if (!cell.stopOnNext) {
                    g.setColour(palette::amber);
                    g.fillRoundedRectangle((float)x+w-8,(float)y+29,3.f,15.f,1.5f);
                }
                continue;
            }
            const auto box=juce::Rectangle<float>((float)x+4,(float)y+3,(float)w-8,49.f);
            // Empty slots share the row surface. Only actual content,
            // missing media, hover, selection and drag require a surface.
            if (loaded) {
                g.setColour(tone.withAlpha(.11f));
                g.fillRoundedRectangle(box,3.f);
                g.setColour(tone.withAlpha(.72f));
                g.fillRect(x+5,y+9,2,34);
            } else if (missing) {
                g.setColour(palette::red.withAlpha(.12f));
                g.fillRoundedRectangle(box,3.f);
                g.setColour(palette::red);
                g.fillRect(x+5,y+9,2,34);
            } else if (hovered) {
                g.setColour(palette::raised.withAlpha(.32f));
                g.fillRoundedRectangle(box,3.f);
            }
            if (selectedCell || drop) {
                const bool focused=owner.hasKeyboardFocus(true);
                g.setColour(drop ? palette::amber :
                    tone.withAlpha(focused ? .92f : .63f));
                g.drawRoundedRectangle(box.reduced(1.f),3.f,
                                       drop || focused ? 1.65f : 1.1f);
            } else if (loaded) {
                // Keep real media recognizable without outlining every slot.
                g.setColour(tone.withAlpha(.25f));
                g.drawRoundedRectangle(box,3.f,.65f);
            }
            if (column.type==ColumnType::control) {
                const ControlSet* set=column.controlLocked?&column.controlDefaultSet:
                    (cell.controlSet?&*cell.controlSet:nullptr);
                juce::String mapTitle;
                if (cell.assignedMapId>0)
                    for (const auto& map:show.controls.templates)
                        if (map.id==cell.assignedMapId) {
                            mapTitle=map.name;
                            set=&map.set;break;
                        }
                const int count=set?(int)set->bindings.size():0;
                label(g,column.controlLocked?"DEFAULT / LOCKED":
                        mapTitle.isNotEmpty()?mapTitle:
                        assigned?"CONTROL MAP":" + Control Map",
                      {x+8,y+6,w-(cell.stopOnNext?16:67),18},
                      11,assigned?tone:palette::muted,true);
                if (assigned)
                    label(g,count>0 ? juce::String(count)+" mappings" : "Empty map",
                          {x+8,y+26,w-16,16},10, count>0?palette::text:palette::muted);
            } else if (loaded) {
                auto playback=owner.engine.getCellPlayback(cell.id);
                if (!owner.engine.connected.load())
                    playback.playing=playback.paused=playback.waiting=false;
                const auto name=cell.name.isNotEmpty()?cell.name:cell.midi?"MIDI":cell.osc?"OSC":
                    juce::File(cell.file).getFileNameWithoutExtension();
                label(g,name,{x+9,y+4,w-(cell.loop&&!cell.stopOnNext?86:70),21},
                      12,palette::text,true);
                const auto seconds=owner.clipDuration(r,c);
                // Runtime-driven dot; never infer playback from loaded media.
                const bool active=playback.playing || playback.paused || playback.waiting;
                if (active) {
                    g.setColour(playback.playing?palette::teal:palette::amber);
                    g.fillEllipse((float)x+10,(float)y+31,5.f,5.f);
                }
                // Preserve numeric position/duration while paused; the
                // amber activity dot and row status already say PAUSED.
                label(g,playback.waiting?"WAITING":
                        timeText(playback.positionSeconds)+" / "+timeText(seconds),
                     {x+(active?19:9),y+26,w-(active?84:74),16},10,
                     playback.playing?palette::teal:
                     playback.paused||playback.waiting?palette::amber:tone);
                const auto progress=seconds>0?juce::jlimit(0.,1.,playback.positionSeconds/seconds):0.;
                g.setColour(palette::line);g.fillRect(x+7,y+45,w-14,3);
                g.setColour(playback.waiting||playback.paused?palette::amber:tone);
                g.fillRect((float)x+7,(float)y+45,(float)((w-14)*progress),3.f);
            } else label(g,missing?"MEDIA MISSING":
                           column.type==ColumnType::audio?"+ WAV/MP3":
                           column.type==ColumnType::midi?"+ MIDI":
                           column.type==ColumnType::osc?"+ OSC":"+ CONTROL",
                           {x+8,y+11,w-(cell.stopOnNext?16:60),20},11,
                           missing?palette::red:palette::muted.withAlpha(hovered?.96f:.78f),
                           false,juce::Justification::centred);
            if(!cell.followsRow) {
                g.setColour(palette::red);g.fillEllipse((float)x+7,(float)y+6,3.f,3.f);
            }
            if(cell.loop)
                drawLoopIcon(g,{x+w-(cell.stopOnNext?19:72),y+5,15,15},palette::amber);
            if (!cell.stopOnNext) {
                // Read-only exception: editing happens through the cell menu
                // or the existing Inspector. The indicator must never act as
                // a transport or per-row disclosure control.
                const auto badge=cellContinueBadgeBounds(x,w,y);
                g.setColour(palette::amber.withAlpha(.19f));
                g.fillRoundedRectangle(badge.toFloat(),3.f);
                g.setColour(palette::amber.withAlpha(.85f));
                g.drawRoundedRectangle(badge.toFloat(),3.f,.7f);
                label(g,"CONT.",badge,10,palette::amber,true,
                      juce::Justification::centred);
            }
            if (column.type!=ColumnType::control && loaded) {
                // Transport remains identical for usable clips. Disabled
                // buttons are not painted over empty or missing-media slots.
                transportButton(g,cellButtonVisualBounds(cellPlayBounds(x,w,y)),
                                false,true,15.0f);
                transportButton(g,cellButtonVisualBounds(cellStopBounds(x,w,y)),
                                true,true,15.0f);
            }
        }
        g.restoreState();
        // Thin dividers use the same collapsed-column positions as hit tests.
        g.setColour(palette::line.withAlpha(.25f));
        for (int c=0;c<(int)show.columns.size();++c)
            g.drawVerticalLine(columnX(c),(float)y,(float)(y+rowHeight));
        g.setColour(palette::line.withAlpha(.43f));
        g.drawHorizontalLine(y+rowHeight-1,0,(float)getWidth());
    }
    // The add-cue button is part of the scrolled grid and is always last.
    smallButton(g,"+",{nameWidth/2-18,headerHeight+(int)show.rows.size()*rowHeight+7,36,29},true);
}

void CueGrid::showCellMenu(int row,int column,juce::Point<int> screenPoint) {
    if(row<0 || row>=(int)owner.show.rows.size() ||
       column<0 || column>=(int)owner.show.columns.size() ||
       owner.show.columns[(size_t)column].collapsed) return;
    const auto& col=owner.show.columns[(size_t)column];
    const bool locked=col.type==ColumnType::control && col.controlLocked;
    const auto& cell=owner.show.rows[(size_t)row].cells[(size_t)column];
    const int rowId=owner.show.rows[(size_t)row].id;
    const int columnId=col.id;
    juce::PopupMenu menu;
    menu.setLookAndFeel(&getLookAndFeel());
    menu.addItem(1,"Delete cell  Backspace",!locked);
    menu.addItem(2,"Cut  Ctrl+X",!locked);
    menu.addItem(3,"Copy  Ctrl+C",!locked);
    menu.addItem(4,"Paste  Ctrl+V",!locked && owner.copiedCell.has_value() &&
                 owner.copiedType==col.type);
    menu.addSeparator();
    menu.addItem(5,"Loop Playback",!locked,cell.loop);
    menu.addItem(6,"Include in Row GO",!locked,cell.followsRow);
    menu.addSeparator();
    menu.addItem(7,"Includi nell'Auto Stop",true,cell.stopOnNext);
    auto safe=juce::Component::SafePointer<MainComponent>(&owner);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(
        {screenPoint.x,screenPoint.y,1,1}),[safe,rowId,columnId](int choice) {
        if(!safe || choice==0) return;
        int r=-1,c=-1;
        for(int i=0;i<(int)safe->show.rows.size();++i)
            if(safe->show.rows[(size_t)i].id==rowId) r=i;
        for(int i=0;i<(int)safe->show.columns.size();++i)
            if(safe->show.columns[(size_t)i].id==columnId) c=i;
        if(r<0 || c<0 || !safe->select(r,c)) return;
        switch(choice) {
            case 1:safe->deleteSelectedCell();break;
            case 2:safe->cutSelectedCell();break;
            case 3:safe->copySelectedCell();break;
            case 4:safe->pasteSelectedCell();break;
            case 5:safe->setCellLoop(r,c,!safe->show.rows[(size_t)r].cells[(size_t)c].loop);break;
            case 6:safe->setCellFollowsRow(r,c,!safe->show.rows[(size_t)r].cells[(size_t)c].followsRow);break;
            case 7:safe->setCellAutoStop(r,c,!safe->show.rows[(size_t)r].cells[(size_t)c].stopOnNext);break;
        }
    });
}
void CueGrid::showRowMenu(int row,juce::Point<int> screenPoint,int /*cellColumn*/) {
    if (row<0 || row>=(int)owner.show.rows.size()) return;
    const int rowId=owner.show.rows[(size_t)row].id;
    const auto& model=owner.show;

    juce::PopupMenu menu;
    menu.setLookAndFeel(&getLookAndFeel());
    menu.addItem(1,"Copy cue");
    menu.addItem(2,"Paste cue below",owner.copiedRow.has_value() &&
        owner.copiedRow->cells.size()==model.columns.size());
    menu.addItem(3,"Duplicate cue");
    menu.addSeparator();
    juce::PopupMenu colours;
    colours.setLookAndFeel(&getLookAndFeel());
    const char* labels[]={"Default","Turquoise","Amber","Red","Purple","Blue","Green","Orange"};
    for (int i=0;i<8;++i)
        colours.addItem(100+i,labels[i],true,model.rows[(size_t)row].colour==i);
    menu.addSubMenu("Cue colour",colours);
    menu.addSeparator();
    menu.addItem(4,"Delete cue");
    auto safe=juce::Component::SafePointer<MainComponent>(&owner);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(
        {screenPoint.x,screenPoint.y,1,1}),[safe,rowId](int item) {
        if (!safe || item==0) return;
        int r=-1;
        for (int i=0;i<(int)safe->show.rows.size();++i)
            if (safe->show.rows[(size_t)i].id==rowId) r=i;
        if (r<0) return;
        if (item==1) safe->copyRow(r);
        else if (item==2) safe->pasteRowAfter(r);
        else if (item==3) safe->duplicateRow(r);
        else if (item==4) safe->deleteRow(r);
        else if (item>=100 && item<=107) safe->setRowColour(r,item-100);
    });
}
void CueGrid::clickRowHeader(int row,int x,int localY,int clicks,juce::Point<int> screen) {
    if (row<0 || row>=(int)owner.show.rows.size()) return;
        if (x<50 && clicks>1) return;
        // The exact rectangles used for painting also own the hit tests.
        if (rowMenuBounds(0).contains(x,localY)) {
            if (clicks==1) showRowMenu(row,screen);
            return;
        }
        if (rowStopBounds(0).contains(x,localY)) {
            if (clicks==1) owner.stopRow(row);
            return;
        }
        if (rowPlayBounds(0).contains(x,localY)) {
            if (clicks==1) owner.toggleRowPlayPause(row);
            return;
        }
        if (localY>=6 && localY<24 && x>=5 && x<23) owner.reorderRow(row,-1);
        else if (localY>=27 && localY<45 && x>=5 && x<23) owner.reorderRow(row,1);
        else if (localY>=13 && localY<33 && x>=27 && x<45) owner.deleteRow(row);
        else if (rowGoBounds(0).contains(x,localY)) owner.triggerRow(row);
        else owner.select(row,owner.selectedColumn);
        return;
}
void CueGrid::clickColumnHeader(int gridX,int y,int clicks,
                                juce::Point<int> screen) {
    if (gridX<nameWidth || y<0 || y>=headerHeight) return;
    const int addColumnX=columnX((int)owner.show.columns.size())+6;
    if (y>=16 && y<47 && gridX>=addColumnX && gridX<addColumnX+31) {
        if (clicks==1) owner.showColumnMenu(screen);
        return;
    }
    const int column=columnAt(gridX);
    if (column<0) return;
    const auto& col=owner.show.columns[(size_t)column];
    const int width=columnWidthAt(column), local=gridX-columnX(column);
    if (col.collapsed) {
        if (clicks==1) owner.toggleColumnCollapsed(column);
        return;
    }
    if (clicks>1) return;
    if (y>=10 && y<29) {
        if (local>=width-61 && local<width-43) owner.reorderColumn(column,-1);
        else if (local>=width-41 && local<width-23) owner.reorderColumn(column,1);
        else if (local>=width-21 && local<width-3) owner.deleteColumn(column);
        else owner.select(owner.selectedRow,column);
    } else if (y>=38 && y<59 && local>=width-26)
        owner.toggleColumnCollapsed(column);
    else owner.select(owner.selectedRow,column);
}
void CueGrid::doubleClickColumnHeader(int gridX,int y) {
    if (y<34 || y>=headerHeight || gridX<nameWidth) return;
    const int column=columnAt(gridX);
    if (column>=0 && !owner.show.columns[(size_t)column].collapsed &&
        gridX-columnX(column)<columnWidthAt(column)-28)
        owner.renameTitle(true,column);
}
void CueGrid::mouseMove(const juce::MouseEvent& e) {
    const int row=CueGridGeometry::rowAt(e.y);
    const int column=columnAt(e.x);
    const bool valid=row>=0 && row<(int)owner.show.rows.size() &&
                     column>=0;
    const bool collapsed=valid && owner.show.columns[(size_t)column].collapsed;
    const int nextRow=valid && !collapsed ? row : -1;
    const int nextColumn=nextRow>=0 ? column : -1;
    juce::String tooltip;
    if (valid && !owner.show.rows[(size_t)row].cells[(size_t)column].stopOnNext) {
        const int x=columnX(column),w=columnWidthAt(column);
        const int y=CueGridGeometry::rowTop(row)+rowContentInset;
        const bool overMarker=collapsed ?
            juce::Rectangle<int>(x+w-12,y+26,12,21).contains(e.getPosition()) :
            cellContinueBadgeBounds(x,w,y).contains(e.getPosition());
        if (overMarker)
            tooltip="Esclusa dall'Auto Stop: continua dopo il prossimo GO quando l'Auto Stop della cue è attivo. Tasto destro per includerla.";
    }
    setTooltip(tooltip);
    if (hoverRow==nextRow && hoverColumn==nextColumn) return;
    const auto repaintCell=[this](int r,int c) {
        if (r>=0 && c>=0)
            repaint(columnX(c),CueGridGeometry::rowTop(r),
                    columnWidthAt(c),rowHeight);
    };
    repaintCell(hoverRow,hoverColumn);
    hoverRow=nextRow;
    hoverColumn=nextColumn;
    repaintCell(hoverRow,hoverColumn);
}
void CueGrid::mouseExit(const juce::MouseEvent&) {
    if (hoverRow>=0 && hoverColumn>=0)
        repaint(columnX(hoverColumn),CueGridGeometry::rowTop(hoverRow),
                columnWidthAt(hoverColumn),rowHeight);
    hoverRow=hoverColumn=-1;
    setTooltip({});
}
void CueGrid::mouseDown(const juce::MouseEvent& e) {
    dragRow=dragColumn=dropRow=dropColumn=-1;dragging=false;
    const int row=CueGridGeometry::rowAt(e.y);
    const int column=columnAt(e.x);
    if (e.mods.isPopupMenu()) {
        if (row>=0 && row<(int)owner.show.rows.size()) {
            if (column>=0 && owner.show.columns[(size_t)column].type==ColumnType::control &&
                !owner.show.columns[(size_t)column].controlLocked)
                owner.showControlCellMenu(row,column,e.getScreenPosition());
            else if(column>=0) showCellMenu(row,column,e.getScreenPosition());
            else showRowMenu(row,e.getScreenPosition());
        }
        return;
    }
    if (!e.mods.isLeftButtonDown()) return;
    owner.grabKeyboardFocus();
    if (e.x<0 || e.y<0) return;
    const int addCueY=CueGridGeometry::rowTop(
        (int)owner.show.rows.size())+7;
    if (e.y>=addCueY && e.y<addCueY+29 &&
        e.x>=nameWidth/2-18 && e.x<nameWidth/2+18) {
        owner.addCue();
        return;
    }
    if (e.y<headerHeight) {
        clickColumnHeader(e.x,e.y,e.getNumberOfClicks(),e.getScreenPosition());
        return;
    }
    if (row<0 || row>=(int)owner.show.rows.size()) return;
    const int localY=(e.y-headerHeight)%rowHeight-rowContentInset;
    if (e.x<nameWidth) {
        clickRowHeader(row,e.x,localY,e.getNumberOfClicks(),e.getScreenPosition());
        return;
    }
    if (column<0 || owner.show.columns[(size_t)column].collapsed) return;
    const int localX=e.x-columnX(column),w=columnWidthAt(column);
    const auto& col=owner.show.columns[(size_t)column];
    if (col.type!=ColumnType::control && owner.hasClip(row,column)) {
        // Invisible disabled transport targets must not block selection of
        // an empty cell. Loaded clips retain their exact safe hitboxes.
        if (cellPlayBounds(0,w,0).contains(localX,localY)) {
            if (e.getNumberOfClicks()==1) owner.triggerCellAt(row,column);
            return;
        }
        if (cellStopBounds(0,w,0).contains(localX,localY)) {
            if (e.getNumberOfClicks()==1) owner.stopCellAt(row,column);
            return;
        }
    }
    if (!owner.select(row,column)) return;
    if (col.type==ColumnType::control) {
        const auto& cell=owner.show.rows[(size_t)row].cells[(size_t)column];
        if (!col.controlLocked && cell.controlSet)
            owner.showControlCellMenu(row,column,e.getScreenPosition());
        else if (col.controlLocked) owner.editControlSet(row,column);
    } else if (owner.hasClip(row,column)) {
        dragRow=row; dragColumn=column;
    }
}
void CueGrid::mouseDrag(const juce::MouseEvent& e) {
    if (dragRow<0 || e.getDistanceFromDragStart()<5) return;
    dragging=true;dropRow=dropColumn=-1;
    const int row=CueGridGeometry::rowAt(e.y);
    const int column=columnAt(e.x);
    if (row>=0 && row<(int)owner.show.rows.size() &&
        column>=0 && !owner.show.columns[(size_t)column].collapsed &&
        owner.show.columns[(size_t)column].type==owner.show.columns[(size_t)dragColumn].type) {
        dropRow=row;dropColumn=column;
    }
    setMouseCursor(dropRow>=0?juce::MouseCursor::DraggingHandCursor:juce::MouseCursor::NormalCursor);
    repaint();
}
void CueGrid::mouseUp(const juce::MouseEvent&) {
    if (dragging && dropRow>=0 && (dragRow!=dropRow || dragColumn!=dropColumn))
        owner.moveClip(dragRow,dragColumn,dropRow,dropColumn);
    dragRow=dragColumn=dropRow=dropColumn=-1;dragging=false;
    setMouseCursor(juce::MouseCursor::NormalCursor);repaint();
}
void CueGrid::mouseDoubleClick(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu() || e.x<0 || e.y<0) return;
    const int column=columnAt(e.x);
    if (e.y<headerHeight) {
        if (column<0 && e.x>=18 && e.x<nameWidth-114 && e.y<33)
            owner.renameActiveCuelist();
        else doubleClickColumnHeader(e.x,e.y);
        return;
    }
    const int row=CueGridGeometry::rowAt(e.y);
    if (row<0 || row>=(int)owner.show.rows.size()) return;
    const int localY=CueGridGeometry::rowLocalY(e.y)-rowContentInset;
    if (e.x<nameWidth && rowTitleHit(e.x,localY))
        owner.renameTitle(false,row);
    else if (column>=0 && !owner.show.columns[(size_t)column].collapsed) {
        const int localX=e.x-columnX(column);
        if (cellPlayBounds(0,columnWidthAt(column),0).contains(localX,localY) ||
            cellStopBounds(0,columnWidthAt(column),0).contains(localX,localY))
            return;
        if (owner.select(row,column)) {
            const auto type=owner.show.columns[(size_t)column].type;
            if (type==ColumnType::midi) owner.editMidiClip();
            else if (type==ColumnType::osc) owner.editOscClip();
            else if (type==ColumnType::control) {
                // An empty cell creates the override without opening a dialog.
                owner.createEmptyControlSet(row,column);
            }
            else owner.triggerCell();
        }
    }
}
bool CueGrid::isInterestedInFileDrag(const juce::StringArray& files) {
    return files.size()==1 && (files[0].endsWithIgnoreCase(".wav") ||
        files[0].endsWithIgnoreCase(".mp3") || files[0].endsWithIgnoreCase(".mid") ||
        files[0].endsWithIgnoreCase(".midi"));
}
void CueGrid::filesDropped(const juce::StringArray& files,int x,int y) {
    if (!isInterestedInFileDrag(files) || y<headerHeight) return;
    const int row=CueGridGeometry::rowAt(y),column=columnAt(x);
    if (row<0 || row>=(int)owner.show.rows.size() || column<0 ||
        owner.show.columns[(size_t)column].collapsed) return;
    if (owner.select(row,column)) owner.importFile(juce::File(files[0]));
}
void PinnedCueHeaders::paint(juce::Graphics& g) {
    // Scroll only the body. The first 72px are a genuinely fixed header,
    // not a translated copy that allows rows to show through when scrolling.
    g.saveState();
    g.reduceClipRegion(getLocalBounds().withTrimmedTop(CueGrid::headerHeight));
    g.addTransform(juce::AffineTransform::translation(
        0.0f,-(float)owner.viewport.getViewPositionY()));
    owner.grid.paint(g);
    g.restoreState();
    g.saveState();
    g.reduceClipRegion(getLocalBounds().withHeight(CueGrid::headerHeight));
    owner.grid.paint(g);
    g.restoreState();
    g.setColour(palette::line);
    g.drawHorizontalLine(CueGrid::headerHeight-1,0.0f,(float)getWidth());
    g.drawVerticalLine(getWidth()-1,0,(float)getHeight());
    if (draggingCue && dropCueRow>=0) {
        const int y=CueGridGeometry::overlayRowTop(
            dropCueRow,owner.viewport.getViewPositionY());
        if (y>=CueGrid::headerHeight && y<getHeight()) {
            g.setColour(palette::teal);
            g.fillRect(101,y+1,
                std::max(0,CueGrid::rowPlayBounds(0).getX()-104),3);
        }
    }
}
void PinnedCueHeaders::mouseDown(const juce::MouseEvent& e) {
    dragCueId=dropCueRow=-1;draggingCue=false;
    // A row scrolled behind the fixed header must never receive input.
    if (e.y<CueGrid::headerHeight) return;
    const int y=e.y+owner.viewport.getViewPositionY();
    const int row=CueGridGeometry::rowAt(y);
    if (e.mods.isPopupMenu()) {
        if (row>=0 && row<(int)owner.show.rows.size())
            owner.grid.showRowMenu(row,e.getScreenPosition());
        return;
    }
    if (!e.mods.isLeftButtonDown()) return;
    const int addCueY=CueGridGeometry::rowTop(
        (int)owner.show.rows.size())+7;
    if (y>=addCueY && y<addCueY+29 &&
        e.x>=CueGrid::nameWidth/2-18 && e.x<CueGrid::nameWidth/2+18) {
        owner.addCue(); return;
    }
    if (row<0 || row>=(int)owner.show.rows.size()) return;
    const int localY=CueGridGeometry::rowLocalY(y)-CueGrid::rowContentInset;
    if (CueGrid::rowTitleHit(e.x,localY) && e.getNumberOfClicks()==1)
        dragCueId=owner.show.rows[(size_t)row].id;
    owner.grabKeyboardFocus();
    owner.grid.clickRowHeader(row,e.x,
        (y-CueGrid::headerHeight)%CueGrid::rowHeight-CueGrid::rowContentInset,
        e.getNumberOfClicks(),e.getScreenPosition());
}
void PinnedCueHeaders::mouseDrag(const juce::MouseEvent& e) {
    if (dragCueId<0 || !e.mods.isLeftButtonDown() ||
        e.getDistanceFromDragStart()<6) return;
    draggingCue=true;
    const int y=e.y+owner.viewport.getViewPositionY();
    const int target=CueGridGeometry::rowAt(y);
    dropCueRow=juce::jlimit(0,(int)owner.show.rows.size()-1,target);
    repaint();
}
void PinnedCueHeaders::mouseUp(const juce::MouseEvent&) {
    if (draggingCue && dragCueId>=0 && dropCueRow>=0) {
        int from=-1;
        for (int i=0;i<(int)owner.show.rows.size();++i)
            if (owner.show.rows[(size_t)i].id==dragCueId) {from=i;break;}
        if (from>=0 && from!=dropCueRow)
            owner.moveCueRow(from,dropCueRow);
    }
    dragCueId=dropCueRow=-1;draggingCue=false;
    repaint();
}
void PinnedCueHeaders::mouseDoubleClick(const juce::MouseEvent& e) {
    if (e.y<CueGrid::headerHeight) {
        if (e.mods.isLeftButtonDown() && e.y>=0 && e.y<33 &&
            e.x>=18 && e.x<CueGrid::nameWidth-114)
            owner.renameActiveCuelist();
        return;
    }
    const int y=e.y+owner.viewport.getViewPositionY();
    const int row=CueGridGeometry::rowAt(y);
    const int localY=CueGridGeometry::rowLocalY(y)-CueGrid::rowContentInset;
    if (row>=0 && row<(int)owner.show.rows.size() &&
        CueGrid::rowTitleHit(e.x,localY))
        owner.renameTitle(false,row);
}
void PinnedCueHeaders::mouseMove(const juce::MouseEvent& e) {
    // The pinned cue lane overlays the scrolled CueGrid. Query the actual
    // scroll position and show the untruncated title, without changing any
    // row selection/drag/transport interaction.
    const int gridY=e.y+owner.viewport.getViewPositionY();
    const int row=CueGridGeometry::rowAt(gridY);
    const int localY=CueGridGeometry::rowLocalY(gridY)-CueGrid::rowContentInset;
    if (e.y>=CueGrid::headerHeight &&
        CueGrid::rowNameBounds(0).contains(e.x,localY))
        setTooltip(clippedCueName(owner.show,row));
    else
        setTooltip({});
}
void PinnedCueHeaders::mouseWheelMove(const juce::MouseEvent& e,
                                      const juce::MouseWheelDetails& wheel) {
    const int x=owner.viewport.getViewPositionX();
    const int y=owner.viewport.getViewPositionY();
    if (e.mods.isShiftDown())
        owner.viewport.setViewPosition(x-(int)(wheel.deltaY*100),y);
    else owner.viewport.setViewPosition(x,y-(int)(wheel.deltaY*100));
    repaint();
}
int PinnedColumnHeaders::gridXAt(int localX) const {
    return CueGridGeometry::pinnedColumnGridX(
        localX,owner.viewport.getViewPositionX());
}
void PinnedColumnHeaders::paint(juce::Graphics& g) {
    // The grid owns all header drawing. Clip to the fixed 72px strip and
    // translate by precisely the horizontal scroll offset so the headers
    // track their cells without inheriting vertical scrolling.
    g.saveState();
    g.reduceClipRegion(getLocalBounds());
    g.addTransform(juce::AffineTransform::translation(
        -(float)gridXAt(0),0.0f));
    owner.grid.paint(g);
    g.restoreState();
    g.setColour(palette::line);
    g.drawHorizontalLine(getHeight()-1,0.0f,(float)getWidth());
}
void PinnedColumnHeaders::mouseDown(const juce::MouseEvent& e) {
    if (!e.mods.isLeftButtonDown() || e.mods.isPopupMenu()) return;
    if (e.y<0 || e.y>=CueGrid::headerHeight) return;
    owner.grabKeyboardFocus();
    owner.grid.clickColumnHeader(gridXAt(e.x),e.y,
                                 e.getNumberOfClicks(),e.getScreenPosition());
}
void PinnedColumnHeaders::mouseDoubleClick(const juce::MouseEvent& e) {
    if (e.mods.isLeftButtonDown() && !e.mods.isPopupMenu())
        owner.grid.doubleClickColumnHeader(gridXAt(e.x),e.y);
}
void PinnedColumnHeaders::mouseMove(const juce::MouseEvent& e) {
    if (e.y<0 || e.y>=CueGrid::headerHeight) {
        setTooltip({});
        return;
    }
    const int column=owner.grid.columnAt(gridXAt(e.x));
    if (column<0 || column>=(int)owner.show.columns.size()) {
        setTooltip({});
        return;
    }
    const auto& col=owner.show.columns[(size_t)column];
    if (col.name.isNotEmpty()) {
        setTooltip(col.name);
        return;
    }
    int ordinal=0;
    for (int i=0;i<=column;++i)
        if (owner.show.columns[(size_t)i].type==col.type) ++ordinal;
    const auto defaultName=col.type==ColumnType::audio ? "Audio" :
        col.type==ColumnType::midi ? "MIDI" :
        col.type==ColumnType::osc ? "OSC" : "Control";
    setTooltip(juce::String(defaultName)+" "+juce::String(ordinal));
}
void PinnedColumnHeaders::mouseWheelMove(
        const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel) {
    const int x=owner.viewport.getViewPositionX();
    const int y=owner.viewport.getViewPositionY();
    if (e.mods.isShiftDown())
        owner.viewport.setViewPosition(x-(int)(wheel.deltaY*100),y);
    else
        owner.viewport.setViewPosition(x,y-(int)(wheel.deltaY*100));
}
CueMetricsOverlay::CueMetricsOverlay(MainComponent& parent):owner(parent) {
    // Native child editor, not an AlertWindow, popup, or floating modal.
    bpmEditor.setName("inlineBpmEditor");
    bpmEditor.setMultiLine(false);
    bpmEditor.setReturnKeyStartsNewLine(false);
    bpmEditor.setEscapeAndReturnKeysConsumed(true);
    bpmEditor.setSelectAllWhenFocused(true);
    bpmEditor.setInputRestrictions(7,"0123456789.");
    bpmEditor.setJustification(juce::Justification::centredLeft);
    bpmEditor.setFont(font(15.f,true));
    bpmEditor.setColour(juce::TextEditor::backgroundColourId,palette::input);
    bpmEditor.setColour(juce::TextEditor::textColourId,palette::text);
    bpmEditor.setColour(juce::TextEditor::outlineColourId,palette::teal);
    bpmEditor.setColour(juce::TextEditor::focusedOutlineColourId,palette::teal);
    bpmEditor.setTooltip("Enter confirms the BPM (20–400); Esc cancels.");
    addAndMakeVisible(bpmEditor);
    bpmEditor.setVisible(false);
    bpmEditor.onReturnKey=[this] { finishBpmEdit(true); };
    bpmEditor.onEscapeKey=[this] { finishBpmEdit(false); };
    bpmEditor.onFocusLost=[this] {
        if (editingRowId>0 && !closingBpmEditor) finishBpmEdit(true);
    };
}
void CueMetricsOverlay::beginBpmEdit(int row) {
    if (row<0 || row>=(int)owner.show.rows.size()) return;
    if (editingRowId>0) {
        if (!bpmEditor.isVisible()) editingRowId=0;
        else if (editingRowId==owner.show.rows[(size_t)row].id) {
            bpmEditor.grabKeyboardFocus();
            return;
        } else finishBpmEdit(true,true);
    }
    // A focus-lost callback may run while closing the previous edit.
    // Never carry its editor text to the newly selected cue.
    if (closingBpmEditor || editingRowId>0) return;
    editingRowId=owner.show.rows[(size_t)row].id;
    bpmEditor.setText(juce::String(owner.show.rows[(size_t)row].tempo.front().bpm,1),
                      juce::dontSendNotification);
    bpmEditor.setVisible(true);
    syncBpmEditor();
    if (!bpmEditor.isVisible()) return;
    bpmEditor.toFront(false);
    bpmEditor.grabKeyboardFocus();
    bpmEditor.selectAll();
}
void CueMetricsOverlay::syncBpmEditor() {
    if (editingRowId<=0 || !bpmEditor.isVisible()) return;
    int index=-1;
    for (int r=0;r<(int)owner.show.rows.size();++r)
        if (owner.show.rows[(size_t)r].id==editingRowId) { index=r;break; }
    if (index<0) { finishBpmEdit(false); return; }
    const int y=CueGridGeometry::overlayRowTop(index,
        owner.viewport.getViewPositionY());
    const auto bounds=juce::Rectangle<int>(16,y+15,73,32);
    if (bounds.getY()<CueGrid::headerHeight ||
        bounds.getBottom()>getHeight()) {
        // Never leave an interactive editor underneath the pinned heading
        // or bind it to a different row after scrolling.
        finishBpmEdit(true,true);
        return;
    }
    bpmEditor.setBounds(bounds);
}
void CueMetricsOverlay::finishBpmEdit(bool accept,bool forceClose) {
    if (editingRowId<=0 || closingBpmEditor) return;
    closingBpmEditor=true;
    const int rowId=editingRowId;
    const auto entered=bpmEditor.getText();
    const bool valid=!accept || owner.commitRowBpm(rowId,entered);
    if (valid || forceClose || !bpmEditor.hasKeyboardFocus(true)) {
        editingRowId=0;
        bpmEditor.setVisible(false);
        repaint();
    } else {
        bpmEditor.selectAll();
    }
    closingBpmEditor=false;
}
void CueMetricsOverlay::paint(juce::Graphics& g) {
    // The metrics lane is fixed: only row contents use the viewport's Y,
    // while the 72px heading, 3px rail and controls keep local coordinates.
    g.fillAll(palette::panel);
    g.saveState();
    g.reduceClipRegion(getLocalBounds().withTrimmedTop(CueGrid::headerHeight));
    const int scroll=owner.viewport.getViewPositionY();
    const int first=std::max(0,(scroll-CueGrid::headerHeight)/CueGrid::rowHeight);
    const int last=std::min((int)owner.show.rows.size(),
        (scroll+getHeight()-CueGrid::headerHeight)/CueGrid::rowHeight+1);
    for (int r=first;r<last;++r) {
        const auto& row=owner.show.rows[(size_t)r];
        const int y=CueGridGeometry::overlayRowTop(r,scroll);
        const bool selected=r==owner.selectedRow;
        const bool next=r==owner.nextCueIndex();
        bool playing=false,paused=false;
        if (owner.engine.connected.load()) for (const auto& cell:row.cells) {
            const auto state=owner.engine.getCellPlayback(cell.id);
            playing=playing||state.playing;
            paused=paused||state.paused;
        }
        // Match the N3.3 CueGrid row surface exactly: the persistent
        // 3px teal pinned-edge rail is painted over all rows at the end.
        const auto base=(r&1)==0 ? palette::panel :
            palette::panel.interpolatedWith(palette::raised,.10f);
        g.setColour(selected ? palette::raised.interpolatedWith(palette::panel,.20f) : base);
        g.fillRect(3,y,getWidth()-3,CueGrid::rowHeight);
        if (playing) {
            g.setColour(palette::teal.withAlpha(selected?.08f:.10f));
            g.fillRect(3,y,getWidth()-3,CueGrid::rowHeight);
        }
        if (next) {
            g.setColour(palette::amber.withAlpha(selected?.08f:.10f));
            g.fillRect(3,y,getWidth()-3,CueGrid::rowHeight);
        }
        const auto cueTone=CueGrid::cueColour(row.colour);
        if (row.colour>0) {
            g.setColour(cueTone.withAlpha(.13f));
            g.fillRect(3,y,getWidth()-3,CueGrid::rowHeight);
        }
        // Narrow independent status signals; keep BPM and toggles readable.
        if (selected) {
            g.setColour(palette::text.withAlpha(.78f));
            g.fillRect(4,y,2,CueGrid::rowHeight);
        }
        if (next) {
            g.setColour(palette::amber);
            g.fillRect(6,y,2,CueGrid::rowHeight);
        }
        if (playing) {
            g.setColour(palette::teal);
            g.fillRect(9,y,2,CueGrid::rowHeight);
        }
        if (row.colour>0 && !next && !playing) {
            g.setColour(cueTone.withAlpha(.88f));
            g.fillRect(6,y,2,CueGrid::rowHeight);
        }

        const bool bpmHover=hoverRow==r && hoverTarget==HoverTarget::bpm;
        const bool toggleHover=hoverRow==r && hoverTarget==HoverTarget::autoStop;
        if (bpmHover) {
            g.setColour(palette::raised.withAlpha(.26f));
            g.fillRoundedRectangle(juce::Rectangle<float>(14.f,(float)y+13.f,75.f,36.f),4.f);
        }
        // One readable BPM number. No NEXT/LIVE/PAUSED/EDIT decorative
        // caption: all cue states remain available in the cue-name lane.
        if (editingRowId!=row.id)
            label(g,juce::String(row.tempo.front().bpm,1),
                  {22,y+16,66,29},17,palette::text,true);

        const auto toggle=autoStopToggleBounds(y);
        if (toggleHover) {
            g.setColour(palette::raised.withAlpha(.48f));
            g.fillRoundedRectangle(toggle.toFloat(),4.f);
        }
        // Preserve the original binary Auto Stop behavior and switch geometry.
        g.setColour(row.stopOnNext?palette::teal.withAlpha(.30f):
                   palette::muted.withAlpha(.20f));
        g.fillRoundedRectangle((float)toggle.getX()+3.f,
                               (float)toggle.getY()+2.f,31.f,19.f,9.f);
        g.setColour(row.stopOnNext?palette::teal:palette::muted);
        g.fillEllipse((float)toggle.getX()+(row.stopOnNext?19.f:6.f),
                      (float)toggle.getY()+5.f,13.f,13.f);
        label(g,row.stopOnNext?"ON":"OFF",
              {toggle.getX()+39,toggle.getY()+1,36,21},10,
              row.stopOnNext?palette::teal:palette::muted,true);

        // The 94px separator corresponds to the same original two zones.
        g.setColour(palette::line.withAlpha(.28f));
        g.drawVerticalLine(94,(float)y,(float)(y+CueGrid::rowHeight));
        g.setColour(palette::line.withAlpha(.43f));
        g.drawHorizontalLine(y+CueGrid::rowHeight-1,4.f,(float)getWidth());
    }
    g.restoreState();

    // Draw the pinned header after scrolling content so partial row buttons
    // can never bleed through it; match the native N3.2 header's palette.
    g.setColour(palette::panel);
    g.fillRect(0,0,getWidth(),CueGrid::headerHeight);
    // Lean two-column strip: no redundant "Cue Metrics" caption.
    g.setColour(palette::teal.withAlpha(.65f));
    g.fillRect(3,0,getWidth()-3,2);
    label(g,"BPM",{22,23,66,26},13,palette::text,true);
    label(g,"AUTO STOP",{103,23,77,26},13,palette::text,true);
    g.setColour(palette::line.withAlpha(.26f));
    g.drawVerticalLine(94,8.f,(float)CueGrid::headerHeight-9);
    g.setColour(palette::line.withAlpha(.72f));
    g.drawHorizontalLine(CueGrid::headerHeight-1,4.f,(float)getWidth());

    // Native contract: this 3px teal line is continuous across the whole
    // pinned surface, including between rows and underneath the heading.
    g.setColour(palette::teal);
    g.fillRect(0,0,3,getHeight());
    g.setColour(palette::line);
    g.drawVerticalLine(3,0.f,(float)getHeight());
}
void CueMetricsOverlay::mouseDown(const juce::MouseEvent& e) {
    if (e.y<CueGrid::headerHeight) return;
    const int scroll=owner.viewport.getViewPositionY();
    const int row=CueGridGeometry::overlayRowAt(e.y,scroll);
    if (row<0 || row>=(int)owner.show.rows.size()) return;
    if (e.mods.isPopupMenu()) {
        owner.grid.showRowMenu(row,e.getScreenPosition());
        return;
    }
    if (!e.mods.isLeftButtonDown()) return;
    const int localY=CueGridGeometry::rowLocalY(e.y+scroll);
    // The fixed strip has only BPM and cue-level Auto Stop. Cell exceptions
    // are edited in their own context menu, not an expansion mode.
    if (bpmEditBounds(0).contains(e.x,localY))
        owner.editRowBpm(row);
    else if (autoStopToggleBounds(0).contains(e.x,localY))
        owner.setAutoStop(row,!owner.show.rows[(size_t)row].stopOnNext);
    repaint();
}
void CueMetricsOverlay::mouseMove(const juce::MouseEvent& e) {
    const int scroll=owner.viewport.getViewPositionY();
    const int row=e.y>=CueGrid::headerHeight ?
        CueGridGeometry::overlayRowAt(e.y,scroll) : -1;
    const int localY=CueGridGeometry::rowLocalY(e.y+scroll);
    HoverTarget target=HoverTarget::none;
    if (row>=0 && row<(int)owner.show.rows.size()) {
        if (bpmEditBounds(0).contains(e.x,localY))
            target=HoverTarget::bpm;
        else if (autoStopToggleBounds(0).contains(e.x,localY))
            target=HoverTarget::autoStop;
    }
    const int nextRow=target!=HoverTarget::none?row:-1;
    if (nextRow==hoverRow && target==hoverTarget) return;
    const auto repaintRow=[this,scroll](int index) {
        if (index>=0)
            repaint(0,CueGridGeometry::overlayRowTop(index,scroll),
                    getWidth(),CueGrid::rowHeight);
    };
    repaintRow(hoverRow);
    hoverRow=nextRow;
    hoverTarget=target;
    repaintRow(hoverRow);
    setMouseCursor(target==HoverTarget::none ?
        juce::MouseCursor::NormalCursor : juce::MouseCursor::PointingHandCursor);
}
void CueMetricsOverlay::mouseExit(const juce::MouseEvent&) {
    if (hoverRow>=0)
        repaint(0,CueGridGeometry::overlayRowTop(
                    hoverRow,owner.viewport.getViewPositionY()),
                getWidth(),CueGrid::rowHeight);
    hoverRow=-1;
    hoverTarget=HoverTarget::none;
    setMouseCursor(juce::MouseCursor::NormalCursor);
}
void CueMetricsOverlay::mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel) {
    const int x=owner.viewport.getViewPositionX(),y=owner.viewport.getViewPositionY();
    if (e.mods.isShiftDown())
        owner.viewport.setViewPosition(x-(int)(wheel.deltaY*100),y);
    else owner.viewport.setViewPosition(x,y-(int)(wheel.deltaY*100));
    repaint();
}
}
