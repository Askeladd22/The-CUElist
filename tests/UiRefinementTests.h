#pragma once
#include "GainMeterFader.h"
#include "HelpGuide.h"
#include "OscEditor.h"
void uiRefinementTests() {
    using namespace cueViewFixtures;
    test("Native Guide renders Markdown and supports section search",[] {
        GuideDocument document;
        document.setMarkdown("# The Cuelist\n\n## 1. Introduction\n"
                             "Welcome to the **Cuelist** guide.\n"
                             "- First cue\n- Second cue\n"
                             "| Action | Behavior |\n| --- | --- |\n"
                             "| GO | Plays the next cue |\n"
                             "```text\nhello\n```");
        document.layoutAtWidth(620);
        check(document.blockCount()>=8 && document.getHeight()>120,
              "Markdown guide sections were not laid out");
        const int intro=document.findNext("Introduction");
        const int cue=document.findNext("Second cue");
        check(intro>=0 && cue>intro &&
              document.blockTop(cue)>document.blockTop(intro),
              "In-app guide search cannot navigate ordered sections");
        check(document.findPrevious("Introduction",cue)==intro &&
              document.findNext("not in guide")==-1,
              "Guide navigation should wrap safely and report no matches");
        juce::Image canvas(juce::Image::ARGB,620,220,true);
        {
            juce::Graphics g(canvas);
            document.paint(g);
        }
        check(canvas.isValid(),"Native guide document failed to render");
        GuideViewer reader("# The Cuelist\n## Getting started\nGo.");
        reader.setSize(900,610);
        check(reader.getDocument().findNext("Getting started")>=0,
              "Guide viewer lost its Markdown content");
    });

    test("Exo 2 small text and Hind headings use embedded OFL typefaces",[] {
        check(appTypefaces().exo2!=nullptr &&
              appTypefaces().hind!=nullptr &&
              appTypefaces().hindSemibold!=nullptr,
              "Embedded typography assets were not loaded");
        auto compact=font(9.0f,true);
        auto body=font(14.5f);
        auto heading=font(18.0f,true);
        check(compact.getHeightInPoints()>=10.0f &&
              body.getHeightInPoints()>=10.0f,
              "Small typography must never be below 10 pt");
        check(compact.getTypefaceName().containsIgnoreCase("Exo") &&
              body.getTypefaceName().containsIgnoreCase("Exo") &&
              heading.getTypefaceName().containsIgnoreCase("Hind"),
              "Typography typeface routing differs from Exo 2 / Hind");
        Theme theme;
        juce::TextButton sample;
        juce::ComboBox combo;
        check(theme.getPopupMenuFont().getTypefaceName().containsIgnoreCase("Exo") &&
              theme.getTextButtonFont(sample,24).getHeightInPoints()>=10.0f &&
              theme.getTextButtonFont(sample,32).getTypefaceName().containsIgnoreCase("Hind") &&
              theme.getComboBoxFont(combo).getHeightInPoints()>=10.0f,
              "Native controls bypass the shared typography hierarchy");
    });

    test("Bundled Unicode fallbacks cover infinity and are preferred by UI fonts",[] {
        const auto& faces=appTypefaces();
        check(faces.notoMath!=nullptr && faces.notoSymbols2!=nullptr,
              "Noto fallback typefaces failed to register");
        check(faces.notoMath->getNominalGlyphForCodepoint(0x221E).has_value(),
              "Bundled Noto Sans Math lacks infinity U+221E");
        for (const auto size: {12.0f,20.0f}) {
            const auto uiFont=font(size);
            const auto families=uiFont.getPreferredFallbackFamilies();
            check(uiFont.getFallbackEnabled() &&
                  families.contains("Noto Sans Math") &&
                  families.contains("Noto Sans Symbols 2"),
                  "Exo 2 / Hind did not retain the configured glyph fallback");
            const auto infinity=uiFont.findSuitableFontForText(
                juce::String::charToString((juce::juce_wchar) 0x221E));
            const auto resolved=infinity.getTypefacePtr();
            check(resolved!=nullptr &&
                  resolved->getNominalGlyphForCodepoint(0x221E).has_value(),
                  "Infinity is not renderable through the configured fallback");
        }
    });

    test("Midnight Slate palette preserves layered dark surfaces and safety accents",[] {
        const auto brightness=[](juce::Colour colour) {
            return 0.2126f*(float)colour.getRed()+
                   0.7152f*(float)colour.getGreen()+
                   0.0722f*(float)colour.getBlue();
        };
        check(brightness(palette::background)<brightness(palette::input) &&
              brightness(palette::input)<brightness(palette::panel) &&
              brightness(palette::panel)<brightness(palette::toolbar) &&
              brightness(palette::toolbar)<brightness(palette::raised) &&
              brightness(palette::raised)<brightness(palette::line),
              "Palette no longer separates canvas, fields, panels and toolbar");
        check(brightness(palette::text)-brightness(palette::panel)>160.0f &&
              brightness(palette::muted)-brightness(palette::raised)>100.0f,
              "Text and secondary labels are too dim against console surfaces");
        check(palette::teal.getGreen()>palette::teal.getRed() &&
              palette::teal.getGreen()>palette::teal.getBlue() &&
              palette::red.getRed()>palette::red.getGreen() &&
              palette::amber.getRed()>palette::amber.getBlue() &&
              palette::go!=palette::panic &&
              palette::selectedTab!=palette::panel,
              "GO, Panic, active tabs or live-state accents lost their distinction");
    });
    test("Popup menus use Midnight Slate with readable hover and separator",[] {
        Theme theme;
        // Check the actual LookAndFeel colours rather than exact sampled
        // pixels: JUCE's Windows rasteriser can blend ARGB images differently
        // across renderers and Windows SDK versions.
        check(theme.findColour(juce::PopupMenu::backgroundColourId)==palette::panel &&
              theme.findColour(juce::PopupMenu::textColourId)==palette::text &&
              theme.findColour(juce::PopupMenu::headerTextColourId)==palette::teal &&
              theme.findColour(juce::PopupMenu::highlightedBackgroundColourId)==palette::raised &&
              theme.findColour(juce::PopupMenu::highlightedTextColourId)==palette::text,
              "Popup theme colours have diverged from Midnight Slate");
        juce::Image pixels(juce::Image::ARGB,220,120,true);
        {
            juce::Graphics g(pixels);
            theme.drawPopupMenuBackground(g,220,120);
            theme.drawPopupMenuItem(g,{0,28,220,34},
                                    false,true,true,true,true,
                                    "Fader Range",{},nullptr,nullptr);
            theme.drawPopupMenuItem(g,{0,81,220,10},
                                    true,true,false,false,false,
                                    {},{},nullptr,nullptr);
            theme.drawPopupMenuSectionHeader(g,{0,93,220,20},"Options");
        }
        check(pixels.isValid() && pixels.getWidth()==220 && pixels.getHeight()==120,
              "Popup render smoke test did not produce a valid image");
        int width=0,height=0;
        theme.getIdealPopupMenuItemSize("Rename active Cuelist",false,24,width,height);
        check(width>=140 && height>=30,
              "Popup row dimensions are too small for clear stage use");
    });
    test("Undo and Redo controls fit before Clock without covering Cuelist tabs",[] {
        MainComponent ui(false);
        auto* undo=dynamic_cast<juce::TextButton*>(
            controlsV3::named(ui,"showUndo"));
        auto* redo=dynamic_cast<juce::TextButton*>(
            controlsV3::named(ui,"showRedo"));
        check(undo && redo,"Global Undo/Redo controls missing");
        check(!undo->isEnabled() && !redo->isEnabled(),
              "Undo/Redo must be disabled when the edit history is empty");
        for (const auto dimensions:std::initializer_list<juce::Point<int>>{
                 {1180,820},{1440,900},{1647,895},{1920,1080}}) {
            ui.setSize(dimensions.x,dimensions.y);
            ui.resized();
            check(undo->isVisible() && redo->isVisible() &&
                  undo->getX()>=190 && redo->getX()>undo->getRight() &&
                  redo->getRight()<=dimensions.x-650 &&
                  undo->getY()<40 && redo->getBottom()<43,
                  "Undo/Redo obscured the tabs or the fixed Clock");
            const auto image=ui.createComponentSnapshot({dimensions.x-365,8,354,56});
            check(image.isValid() && image.getHeight()==56,
                  "The compact MIDI/OSC/Audio monitor failed to render");
        }
    });

    test("Help menu and versioned About stay accessible at minimum window width",[] {
        MainComponent ui(false);
        auto* help=dynamic_cast<juce::TextButton*>(
            controlsV3::named(ui,"showHelpMenu"));
        auto* undo=dynamic_cast<juce::TextButton*>(
            controlsV3::named(ui,"showUndo"));
        auto* tab=dynamic_cast<juce::TextButton*>(
            controlsV3::named(ui,"inspectorSideToggle"));
        check(help && undo && tab,"Help, Undo or Inspector grip missing");
        check(juce::String(THE_CUELIST_VERSION).startsWith("0.9.0-beta."),
              "About version is not bound to VERSION.txt in the build");
        for (const auto size:std::initializer_list<juce::Point<int>>{
                 {1180,820},{1440,900},{1647,895},{1920,1080}}) {
            ui.setSize(size.x,size.y);
            ui.resized();
            check(help->isVisible() && help->getRight()+5<=undo->getX() &&
                  help->getY()>=0 && help->getBottom()<43,
                  "Help overlaps Undo or Cuelist tab strip");
            check(tab->getButtonText().isEmpty() &&
                  tab->getWidth()==20 && tab->getHeight()==46,
                  "Inspector grip must use directional vector symbols");
            auto icon=tab->createComponentSnapshot(tab->getLocalBounds());
            check(icon.isValid() && icon.getWidth()==20 && icon.getHeight()==46,
                  "Inspector vector symbol failed to render");
        }
    });

    test("Inspector edge tab collapses right and restores unsaved cue notes",[] {
        MainComponent ui(false);
        ui.setSize(1440,900);
        auto* tab=dynamic_cast<juce::TextButton*>(
            controlsV3::named(ui,"inspectorSideToggle"));
        auto* cueName=dynamic_cast<juce::TextEditor*>(
            controlsV3::named(ui,"cueName"));
        auto* notes=dynamic_cast<juce::TextEditor*>(
            controlsV3::named(ui,"cueNotes"));
        auto* gain=controlsV3::named(ui,"cellGainDb");
        auto* timeline=find<Timeline>(ui);
        auto* metrics=find<CueMetricsOverlay>(ui);
        auto* utilities=find<ColumnUtilities>(ui);
        juce::Viewport* view=nullptr;
        for (auto* component:ui.getChildren())
            if (auto* candidate=dynamic_cast<juce::Viewport*>(component))
                view=candidate;
        check(tab && cueName && notes && gain && timeline && metrics &&
              utilities && view,"Inspector toggle or workspace component missing");
        const int originalView=view->getWidth();
        const int originalTimeline=timeline->getWidth();
        const int originalUtility=utilities->getWidth();
        const int originalMetrics=metrics->getX();
        check(tab->isVisible() && tab->getRight()>metrics->getRight() &&
              tab->getX()<metrics->getRight() &&
              tab->getWidth()==20 && tab->getHeight()==46,
              "Compact Inspector grip must straddle the panel boundary");
        cueName->setText("Opening cue",false);
        notes->setText("Remember the stage blackout",false);
        click(*tab,12.0f,22.0f);
        check(!cueName->isVisible() && !notes->isVisible() &&
              !gain->isVisible() && ui.show.rows[0].name=="Opening cue" &&
              ui.show.rows[0].notes=="Remember the stage blackout",
              "Inspector collapse lost pending fields or left them visible");
        check(view->getWidth()==originalView+262 &&
              timeline->getWidth()==originalTimeline+262 &&
              utilities->getWidth()==originalUtility+262 &&
              metrics->getX()==originalMetrics+262 &&
              metrics->getRight()==ui.getWidth()-26 &&
              tab->getX()==ui.getWidth()-23 &&
              tab->getRight()==ui.getWidth()-3 &&
              tab->getHeight()==46,
              "Inspector collapse did not reclaim width or keep compact grip inside rail");
        // Exercise the public selection path: it refreshes the Inspector
        // without exposing its private refresh() implementation to tests.
        check(ui.select(0,0),"Public cue selection failed with Inspector collapsed");
        check(!cueName->isVisible() && !notes->isVisible() &&
              !gain->isVisible(),
              "Inspector fields reappeared after a show refresh");
        click(*tab,12.0f,22.0f);
        check(cueName->isVisible() && notes->isVisible() &&
              gain->isVisible() && cueName->getText()=="Opening cue" &&
              notes->getText()=="Remember the stage blackout" &&
              view->getWidth()==originalView &&
              timeline->getWidth()==originalTimeline &&
              metrics->getX()==originalMetrics &&
              utilities->getWidth()==originalUtility,
              "Reopening Inspector failed to restore content or geometry");
        check(ui.show.rows[0].cells[0].gain==1.0f,
              "Collapsing Inspector unexpectedly changed audio gain");
        for (const auto dimensions:std::initializer_list<juce::Point<int>>{
                 {1180,820},{1440,900},{1647,895},{1920,1080}}) {
            ui.setSize(dimensions.x,dimensions.y);
            ui.resized();
            check(tab->getWidth()==20 && tab->getHeight()==46 &&
                  tab->getX()==ui.getWidth()-288-10 &&
                  tab->getRight()>metrics->getRight(),
                  "Compact side tab escaped the Inspector edge on window resize");
        }
    });
    test("Ctrl+C/V style cell clipboard preserves target identity and copies audio media", [] {
        MainComponent ui(false); addRow(ui.show,"Next");
        auto media=std::make_shared<AudioClip>(); media->sampleRate=48000;
        media->samples.setSize(1,48000); media->samples.clear();
        const int oldId=ui.show.rows[1].cells[0].id;
        ui.clips[ui.show.rows[0].cells[0].id]=media;
        ui.show.rows[0].cells[0].name="Applausi";
        ui.show.rows[0].cells[0].loop=true;
        check(ui.copySelectedCell(),"Audio cell copy failed");
        check(ui.select(1,0),"Could not select destination audio cell");
        check(ui.pasteSelectedCell(),"Audio paste failed");
        check(ui.show.rows[1].cells[0].id==oldId,"Paste modified target stable ID");
        check(ui.show.rows[1].cells[0].name=="Applausi" && ui.show.rows[1].cells[0].loop,
              "Paste lost audio metadata");
        check(ui.clips[oldId]==media,"Paste lost media sample reference");
        addColumn(ui.show,ColumnType::midi,"M");
        check(ui.select(1,1),"MIDI destination not selected");
        // Mismatched types are rejected by pasteSelectedCell; avoid opening a
        // warning modal here, which could interfere with subsequent GUI tests.
    });
    test("MIDI and OSC cells copy their embedded content independently", [] {
        MainComponent ui(false); addRow(ui.show,"Next");
        addColumn(ui.show,ColumnType::midi,"M");
        addColumn(ui.show,ColumnType::osc,"O");
        ui.show.rows[0].cells[1].midi=MidiClip{8,{{0,2,60,100,1}}};
        ui.show.rows[0].cells[2].osc=OscClip{false,5,{{2,"/scene",{{OscValueType::integer,4,0,{}}}}}};
        check(ui.select(0,1) && ui.copySelectedCell(),"MIDI source copy failed");
        const int midiId=ui.show.rows[1].cells[1].id;
        check(ui.select(1,1) && ui.pasteSelectedCell(),"MIDI paste failed");
        check(ui.show.rows[1].cells[1].id==midiId && ui.show.rows[1].cells[1].midi->notes.size()==1,
              "MIDI pasted with wrong identity or data");
        check(ui.select(0,2) && ui.copySelectedCell(),"OSC source copy failed");
        const int oscId=ui.show.rows[1].cells[2].id;
        check(ui.select(1,2) && ui.pasteSelectedCell(),"OSC paste failed");
        check(ui.show.rows[1].cells[2].id==oscId && ui.show.rows[1].cells[2].osc->events[0].address=="/scene",
              "OSC pasted with wrong identity or data");
        ui.show.rows[0].cells[2].osc->events[0].address="/changed";
        check(ui.show.rows[1].cells[2].osc->events[0].address=="/scene",
              "OSC clipboard pasted mutable shared message data");
    });
    test("BPM row label opens direct edit dialog without metric control", [] {
        MainComponent ui(false);
        auto* metrics=find<CueMetricsOverlay>(ui);
        check(metrics!=nullptr,"Fixed BPM / Auto Stop overlay missing");
        click(*metrics,22.0f,(float)CueGrid::headerHeight+17);
        auto* modal=dynamic_cast<juce::AlertWindow*>(juce::ModalComponentManager::getInstance()->getModalComponent(0));
        check(modal!=nullptr && modal->getTextEditor("value")!=nullptr,"BPM row label did not open editor");
        check(modal->getTextEditor("value")->getText().contains("120"),"BPM dialog initial value incorrect");
        delete modal;
    });
    test("MIDI and OSC timeline edges resize durations without moving clip start", [] {
        MainComponent ui(false);
        addColumn(ui.show,ColumnType::midi,"M"); addColumn(ui.show,ColumnType::osc,"O");
        ui.show.rows[0].cells[1].midi=MidiClip{4,{{0,1,60,100,1}}};
        OscClip osc; osc.length=4; osc.events.push_back({1,"/stage",{}});
        ui.show.rows[0].cells[2].osc=osc;
        auto* timeline=find<Timeline>(ui); timeline->setSize(800,250);
        const juce::Point<float> midiFrom{268,136}, midiTo{332,136};
        timeline->mouseDown(event(*timeline,midiFrom,midiFrom));
        timeline->mouseDrag(event(*timeline,midiTo,midiFrom,1,true));
        timeline->mouseUp(event(*timeline,midiTo,midiFrom,1,true));
        near(ui.show.rows[0].cells[1].startSeconds,0);
        near(ui.show.rows[0].cells[1].midi->lengthBeats,6);
        // At 120 BPM, a 4-second OSC clip occupies 256 px of the 10-second view.
        const juce::Point<float> oscFrom{396,178}, oscTo{460,178};
        timeline->mouseDown(event(*timeline,oscFrom,oscFrom));
        timeline->mouseDrag(event(*timeline,oscTo,oscFrom,1,true));
        timeline->mouseUp(event(*timeline,oscTo,oscFrom,1,true));
        near(ui.show.rows[0].cells[2].startSeconds,0);
        near(ui.show.rows[0].cells[2].osc->length,5);
    });
    test("embedded Clip Editor height can be adjusted by dragging handle", [] {
        MainComponent ui(false);
        addColumn(ui.show,ColumnType::midi,"M"); ui.select(0,1); ui.editMidiClip();
        auto* handle=controlsV3::named(ui,"clipEditorResizeHandle");
        check(handle && handle->isVisible(),"Dock resize handle absent");
        auto* editor=ui.activeClipEditor();
        check(editor!=nullptr,"No docked editor");
        const int oldHeight=editor->getHeight();
        const juce::Point<float> from{45,6},to{45,-34};
        handle->mouseDown(event(*handle,from,from));
        handle->mouseDrag(event(*handle,to,from,1,true));
        handle->mouseUp(event(*handle,to,from,1,true));
        check(editor->getHeight()>oldHeight,"Editor dock did not grow after drag");
    });
    test("compact OSC editor exposes plus and double-click editing, not persistent argument forms", [] {
        MainComponent ui(false); addColumn(ui.show,ColumnType::osc,"OSC");
        check(ui.select(0,1),"Could not select OSC column");
        ui.editOscClip();
        auto* editor=ui.activeOscEditor();
        check(editor!=nullptr,"OSC editor missing");
        check(controlsV3::named(*editor,"oscAddAutomation")!=nullptr &&
              controlsV3::named(*editor,"oscAddMessage")==nullptr,
              "OSC plus must add a track; the redundant event button must be removed");
        check(controlsV3::named(*editor,"oscClipLength")==nullptr &&
              controlsV3::named(*editor,"oscEventArguments")==nullptr,
              "Legacy OSC field boxes still displayed");
        editor->addEvent();
        check(ui.show.rows[0].cells[1].osc->events.size()==1,"Event creation failed to persist an OSC message");
        auto* modal=juce::ModalComponentManager::getInstance()->getModalComponent(0);
        check(modal!=nullptr,"Explicit OSC event editing did not open its dialog");
        delete modal;
    });
    test("Master audio is within left column utilities and audio strips are compact", [] {
        MainComponent ui(false); ui.setSize(1500,900);
        auto* utilities=find<ColumnUtilities>(ui);
        auto* master=dynamic_cast<GainMeterFader*>(controlsV3::named(*utilities,"masterAudioDb"));
        check(master && master->getParentComponent()==utilities,"Integrated Master not placed in utility panel");
        auto* masterPan=dynamic_cast<juce::Slider*>(controlsV3::named(*utilities,"masterPan"));
        auto* masterMute=dynamic_cast<juce::ToggleButton*>(controlsV3::named(*utilities,"masterMute"));
        check(masterPan && masterMute,"Master pan and mute missing from utilities");
        auto* audio=utilities->getChildComponent(0)->getChildComponent(0);
        auto* fader=dynamic_cast<GainMeterFader*>(controlsV3::named(*audio,"columnGainDb"));
        auto* pan=dynamic_cast<juce::Slider*>(controlsV3::named(*audio,"columnPan"));
        check(fader && pan && pan->getWidth()>=43 && pan->getWidth()<=50 && fader->getWidth()<=96,"Strip controls were not reduced");
        check(master->getWidth()<=96 && master->getHeight()>90 &&
              master->getNumChildComponents()==0,
              "Master does not use the compact integrated meter/fader");
    });
    test("Midnight Console layout enlarges fields and meters without dropping controls",[] {
        MainComponent ui(false);
        ui.setSize(1640,900);
        auto* cue=controlsV3::named(ui,"cueName");
        auto* bpm=controlsV3::named(ui,"cueBpm");
        auto* cell=controlsV3::named(ui,"cellName");
        auto* grid=cueViewFixtures::find<CueGrid>(ui);
        auto* mixer=cueViewFixtures::find<ColumnUtilities>(ui);
        auto* ruler=cueViewFixtures::find<Timeline>(ui);
        auto* master=controlsV3::named(*mixer,"masterAudioDb");
        auto* pan=controlsV3::named(*mixer,"masterPan");
        auto* mute=controlsV3::named(*mixer,"masterMute");
        check(cue && bpm && cell && cue->getWidth()>=220 &&
              bpm->getWidth()>=220 && cell->getWidth()>=220,
              "Inspector restyling removed or clipped text inputs");
        check(grid && mixer && ruler && master && pan && mute &&
              mixer->getHeight()>=200 && master->getHeight()>=155 &&
              ruler->getHeight()>=175,
              "Performance layout clipped the mixer or made the timeline unusable");
        check(controlsV3::named(*ruler,"timelineGridVisibility") &&
              controlsV3::named(*ruler,"timelineSnapToggle"),
              "Grid and snap controls disappeared during restyling");
        check(CueGrid::rowHeight<=66 && CueGrid::columnWidth<=156 &&
              CueGrid::compressedWidth<=50,
              "Performance-sized cue cards or name lane are missing");
        check(palette::background!=palette::panel &&
              palette::panel!=palette::raised &&
              palette::muted!=palette::text,
              "Dark palette lost its text or panel contrast");
        ui.setSize(1450,768);
        check(cue->getWidth()>=220 && cue->getBottom()<ui.getHeight() &&
              bpm->getBottom()<ui.getHeight(),
              "Compact-height window clipped important Inspector fields");
    });


    test("Compressed columns retain a compact audio fader and mute and roundtrip in v10",[] {
        MainComponent ui(false);addColumn(ui.show,ColumnType::midi,"Playback MIDI");
        const int original=ui.show.rows[0].cells[0].id;
        auto* grid=cueViewFixtures::find<CueGrid>(ui);
        const int expanded=grid->columnWidthAt(0);
        check(expanded==CueGrid::columnWidth,"Unexpected expanded column size");
        ui.toggleColumnCollapsed(0);
        check(ui.show.columns[0].collapsed &&
              grid->columnWidthAt(0)==CueGrid::compressedWidth &&
              grid->columnX(1)==CueGrid::nameWidth+CueGrid::compressedWidth,
              "Column did not collapse or adjacent position remained unchanged");
        check(ui.show.rows[0].cells[0].id==original && ui.select(0,0),
              "Collapsed column lost its cue selection or cell identity");
        auto* utilities=cueViewFixtures::find<ColumnUtilities>(ui);
        auto* fader=dynamic_cast<GainMeterFader*>(controlsV3::named(*utilities,"columnGainDb"));
        auto* mute=controlsV3::named(*utilities,"columnMute");
        auto* pan=controlsV3::named(*utilities,"columnPan");
        check(fader && fader->isVisible() && fader->isCompact() &&
              mute && mute->isVisible() && pan && !pan->isVisible(),
              "Collapsed Audio Column must retain only compact gain and mute controls");
        Show loaded;
        check(decodeShow(encodeShow(ui.show),loaded).wasOk() &&
              loaded.columns[0].collapsed && !loaded.columns[1].collapsed,
              "Visual compression state did not survive project save");
        ui.toggleColumnCollapsed(0);
        check(!ui.show.columns[0].collapsed && grid->columnWidthAt(0)==expanded &&
              ui.select(0,0),"Column could not be expanded and edited again");
    });
    test("Cue colours persist independently from engine and legacy documents",[] {
        MainComponent ui(false);
        const auto cells=ui.show.rows[0].cells;
        ui.setRowColour(0,4);
        check(ui.show.rows[0].colour==4 && ui.hasUnsavedChanges() &&
              ui.show.rows[0].cells[0].id==cells[0].id,"Cue colouring changed media identity");
        Show loaded;
        check(decodeShow(encodeShow(ui.show),loaded).wasOk() &&
              loaded.rows[0].colour==4,"Cue colour was not persisted");
        auto legacy=juce::JSON::parse(encodeShow(ui.show));
        legacy["rows"].getArray()->getReference(0).getDynamicObject()->removeProperty("colour");
        check(decodeShow(juce::JSON::toString(legacy),loaded).wasOk() &&
              loaded.rows[0].colour==0,"Older documents no longer default to uncoloured cues");
        ui.show.rows[0].colour=8;
        check(validateShow(ui.show).failed(),"Invalid cue palette value was accepted");
    });
    test("BPM and Auto Stop remain pinned while cue columns scroll horizontally",[] {
        MainComponent ui(false);
        for (int i=0;i<7;++i) addColumn(ui.show,ColumnType::audio,"Extra");
        ui.setSize(1250,900);
        auto* overlay=cueViewFixtures::find<CueMetricsOverlay>(ui);
        // TextEditor contains its own internal Viewport. Find the main cue
        // viewport among direct children, not an unrelated editor scroller.
        juce::Viewport* view=nullptr;
        for (auto* child:ui.getChildren())
            if (auto* candidate=dynamic_cast<juce::Viewport*>(child)) {
                view=candidate;break;
            }
        auto* grid=cueViewFixtures::find<CueGrid>(ui);
        check(overlay && view && grid,"Fixed cue controls or grid missing");
        const int pinnedX=overlay->getX();
        check(overlay->getWidth()==CueMetricsOverlay::width &&
              pinnedX==view->getRight(),
              "BPM and Auto Stop are not anchored to the viewport edge");
        view->setViewPosition(180,0);
        check(overlay->getX()==pinnedX,"Cue metrics moved with horizontally scrolled columns");
        const auto stop=ui.show.rows[0].stopOnNext;
        cueViewFixtures::click(*overlay,118,CueGrid::headerHeight+16);
        check(ui.show.rows[0].stopOnNext!=stop,"Fixed Auto Stop control was not clickable");
        check(grid->totalColumnsWidth()==(int)ui.show.columns.size()*CueGrid::columnWidth,
              "Expanded column positions are inconsistent");
    });

    test("Cue list fills compact outer gutters while pinned BPM and Gain editor stay usable",[] {
        MainComponent ui(false);
        ui.setSize(1647,895);
        auto* overlay=cueViewFixtures::find<CueMetricsOverlay>(ui);
        auto* grid=cueViewFixtures::find<CueGrid>(ui);
        juce::Viewport* view=nullptr;
        for (auto* child:ui.getChildren())
            if (auto* candidate=dynamic_cast<juce::Viewport*>(child)) {
                view=candidate; break;
            }
        check(overlay && grid && view,"Cue grid, viewport or pinned controls missing");
        check(view->getX()==182 && view->getY()==74 &&
              ui.getWidth()-288-overlay->getRight()==0,
              "Excessive left/right/top cue-list margins returned");
        check(overlay->getX()==view->getRight() &&
              overlay->getWidth()==CueMetricsOverlay::width &&
              CueMetricsOverlay::width>=180,
              "BPM/Auto Stop column lost its padded fixed position");
        check(grid->getWidth()<=view->getWidth(),
              "One-column show should not force unnecessary horizontal scrolling");
        auto* gain=dynamic_cast<juce::TextEditor*>(controlsV3::named(ui,"cellGainDb"));
        check(gain && gain->isVisible() && gain->getWidth()<=110,
              "Inspector should offer a compact numeric dB editor, not a fader");
        gain->setText("5.5",false);
        check(ui.hasUnsavedChanges(),"Uncommitted Gain dB changes must be tracked");
        gain->onReturnKey();
        check(std::abs(juce::Decibels::gainToDecibels(ui.show.rows[0].cells[0].gain,-60.0f)-5.5f)<0.06f,
              "Numeric Gain did not update the audio cell");
        ui.setSize(1180,820);
        check(overlay->getX()==view->getRight() &&
              view->getY()==74 && ui.getWidth()-288-overlay->getRight()==0,
              "Compact cue-list positioning broke at smaller window size");
    });
    test("Sidebar navigation has breathing room without overlapping On Deck",[] {
        MainComponent ui(false);
        for (const auto dimension:std::initializer_list<juce::Point<int>>{
                {1180,820},{1440,900},{1647,895},{1920,1080}}) {
            ui.setSize(dimension.x,dimension.y);
            ui.resized();
            auto* title=controlsV3::named(ui,"projectName");
            auto* preferences=controlsV3::named(ui,"projectPreferences");
            auto* mapping=controlsV3::named(ui,"controlMapping");
            check(title && preferences && mapping,
                  "Sidebar title, Settings or Control Mapping is missing");
            check(title->getBottom()+22<=preferences->getY() &&
                  preferences->getBottom()+14<=mapping->getY() &&
                  mapping->getBottom()<=373 &&
                  mapping->getBottom()+20<=dimension.y-339-12,
                  "Sidebar spacing overlaps navigation or the emergency controls");
            check(mapping->getRight()<=180 && mapping->getHeight()>=33,
                  "Expanded spacing reduced the clickable Control Mapping target");
        }
    });
    test("Project title is editable and survives v10 serialization",[] {
        MainComponent ui(false);
        auto* title=dynamic_cast<juce::Label*>(controlsV3::named(ui,"projectName"));
        check(title && title->isVisible(),"Editable project title is missing");
        title->setText("Teatro - Autunno",juce::sendNotification);
        check(ui.show.name=="Teatro - Autunno" && ui.hasUnsavedChanges(),
              "Renaming project failed to update persisted model");
        Show loaded;
        check(decodeShow(encodeShow(ui.show),loaded).wasOk() &&
              loaded.name=="Teatro - Autunno","Renamed title was not serialized");
        auto* preferences=controlsV3::named(ui,"projectPreferences");
        auto* diagnostics=controlsV3::named(ui,"showDiagnostics");
        check(preferences && preferences->isVisible() &&
              preferences->getX()<180 && diagnostics && !diagnostics->isVisible(),
              "Preferences menu did not replace toolbar diagnostics");
    });
    test("Contextual plus adds a cue below the existing sequence",[] {
        MainComponent ui(false);
        auto* grid=cueViewFixtures::find<CueGrid>(ui);
        check(grid!=nullptr,"Cue grid is unavailable");
        const int old=(int)ui.show.rows.size();
        const int plusY=CueGrid::headerHeight+old*CueGrid::rowHeight+19;
        check(grid->getHeight()>plusY,"Add-cue button is outside scrollable grid");
        cueViewFixtures::click(*grid,CueGrid::nameWidth/2.0f,(float)plusY);
        check((int)ui.show.rows.size()==old+1 && ui.selectedRow==old,
              "Contextual add-cue button did not create a new cue");
    });
    test("Control lock icon follows actual priority and lock model",[] {
        MainComponent ui(false);addColumn(ui.show,ColumnType::control,"Control");
        auto* utility=cueViewFixtures::find<ColumnUtilities>(ui);
        utility->refresh();
        auto* strip=utility->getChildComponent(0)->getChildComponent(1);
        auto* lock=dynamic_cast<juce::Button*>(controlsV3::named(*strip,"controlColumnLock"));
        check(lock && lock->isVisible() && !lock->getToggleState(),
              "Unlocked icon state missing");
        check(ui.setControlColumnLocked(1,true,0),"Could not lock Control Column");
        utility->refresh();
        check(lock->getToggleState() && ui.show.columns[1].controlPriority==1,
              "Closed-lock icon and model disagreed");
        check(ui.setControlColumnLocked(1,false,0),"Could not unlock Control Column");
        utility->refresh();
        check(!lock->getToggleState(),"Opened-lock icon did not update");
    });
    test("MIDI RX activity counts accepted physical input callbacks, not connections",[] {
        MidiInputManager input;
        check(input.receivedCount()==0,"MIDI RX counter started nonzero");
        input.ingestForTest(juce::MidiMessage::controllerEvent(1,21,72));
        check(input.receivedCount()==1 && !input.connected(),
              "MIDI RX activity should count accepted messages without requiring a port");
        ControlEvent event;
        check(input.poll(event) && event.number==21 && event.value==72,
              "MIDI RX activity diverged from queued controller message");
    });


    test("Pinned row headers stay clickable while many cue columns scroll",[] {
        MainComponent ui(false);
        addRow(ui.show,"Second cue");
        for (int c=0;c<11;++c) addColumn(ui.show,ColumnType::audio,"Extension");
        ui.setSize(1180,820);
        auto* headers=cueViewFixtures::find<PinnedCueHeaders>(ui);
        auto* metrics=cueViewFixtures::find<CueMetricsOverlay>(ui);
        auto* utilities=cueViewFixtures::find<ColumnUtilities>(ui);
        juce::Viewport* view=nullptr;
        for (auto* child:ui.getChildren())
            if (auto* found=dynamic_cast<juce::Viewport*>(child)) {view=found;break;}
        check(headers && metrics && utilities && view,"Fixed workspace regions are missing");
        check(headers->getX()==view->getX() &&
              headers->getRight()==view->getX()+CueGrid::nameWidth &&
              metrics->getX()==view->getRight() &&
              utilities->getRight()==metrics->getX(),
              "Three-zone workspace does not exclude pinned strips from scrolling");
        view->setViewPosition(350,0);
        check(headers->getX()==view->getX() &&
              metrics->getX()==view->getRight(),
              "Pinned header or metrics moved when scrolling right");
        cueViewFixtures::click(*headers,120.f,
            (float)(CueGrid::headerHeight+CueGrid::rowHeight+18));
        check(ui.selectedRow==1,
              "Fixed row title failed to select its own cue after horizontal scrolling");
        check(utilities->getChildComponent(0)->getX()==CueGrid::nameWidth &&
              utilities->getChildComponent(0)->getRight()<=utilities->getWidth(),
              "Scrolled utility strips were not confined to the central region");
    });

    test("Collapsed Audio MIDI and OSC mute glyphs share the same vertical baseline",[] {
        MainComponent ui(false);
        addColumn(ui.show,ColumnType::midi,"MIDI"); addColumn(ui.show,ColumnType::osc,"OSC");
        for (int c=0;c<3;++c) ui.show.columns[(size_t)c].collapsed=true;
        auto* utility=cueViewFixtures::find<ColumnUtilities>(ui);
        utility->refresh();ui.resized();
        auto* region=utility->getChildComponent(0);
        auto* audio=region->getChildComponent(0);
        auto* midi=region->getChildComponent(1);
        auto* osc=region->getChildComponent(2);
        auto* mute=controlsV3::named(*audio,"columnMute");
        auto* midiMute=controlsV3::named(*midi,"columnDataOutputMute");
        auto* oscMute=controlsV3::named(*osc,"columnDataOutputMute");
        check(mute && midiMute && oscMute &&
              mute->getY()==midiMute->getY() && mute->getY()==oscMute->getY(),
              "Collapsed MIDI/OSC mute buttons sit above the Audio mute");
    });

    test("Fader range preference persists without rewriting Show gain or routing",[] {
        MainComponent ui(false);
        const int prior=ui.audioFaderCeilingDb();
        const auto before=encodeShow(ui.show);
        ui.setAudioFaderCeilingDb(0);
        check(ui.audioFaderCeilingDb()==0 && encodeShow(ui.show)==before,
              "Fader range change altered the saved show, routing or gain");
        {
            MainComponent reopened(false);
            check(reopened.audioFaderCeilingDb()==0,
                  "Fader range was not saved in application preferences");
        }
        ui.setAudioFaderCeilingDb(prior); // Leave the operator preference unchanged.
    });

    test("MIDI control at maximum honors the selected 0 dB and +6 dB fader ceilings",[] {
        MainComponent ui(false);
        const int prior=ui.audioFaderCeilingDb();
        ControlBinding gain;
        gain.deviceId="controller";gain.kind=ControlKind::cc;gain.channel=1;
        gain.number=27;gain.target=ControlTarget::masterGain;
        gain.takeover=ControlTakeover::jump;gain.minimum=0;gain.maximum=4;
        check(ui.addMappingBinding(gain),"Could not assign gain fader to controller");
        ui.setAudioFaderCeilingDb(0);
        ui.dispatchControlForTest({"controller",ControlKind::cc,1,27,64,false});
        check(std::abs(ui.show.masterGain-64.0/127.0)<0.0002,
              "Intermediate MIDI CC did not span the full 0 dB fader range");
        ui.dispatchControlForTest({"controller",ControlKind::cc,1,27,127,false});
        near(ui.show.masterGain,1.0);
        ui.setAudioFaderCeilingDb(6);
        ui.dispatchControlForTest({"controller",ControlKind::cc,1,27,127,false});
        check(std::abs(ui.show.masterGain-std::pow(10.0,6.0/20.0))<0.0002,
              "Full MIDI CC value did not reach the +6 dB ceiling");
        ui.setAudioFaderCeilingDb(prior);
    });

}
