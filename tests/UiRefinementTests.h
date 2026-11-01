#pragma once
#include "GainMeterFader.h"
#include "HelpGuide.h"
#include "OperatorReportDialog.h"
#include "OperatorTextPrompt.h"
#include "OscEditor.h"
#include "HeaderTelemetry.h"
void uiRefinementTests() {
    using namespace cueViewFixtures;
    test("Cuelist renames directly on header/tab with Enter, Escape and validation",[] {
        MainComponent ui(false);
        auto* editor=dynamic_cast<juce::TextEditor*>(
            controlsV3::named(ui,"inlineNameEditor"));
        check(editor && !editor->isVisible(),"Inline rename must start hidden.");
        const auto original=ui.show.cuelists[(size_t)ui.show.activeCuelist].name;
        ui.renameActiveCuelist();
        check(editor->isVisible() && editor->getText()==original &&
              editor->getWidth()>=100 && editor->getHeight()>=20,
              "Double-click Cuelist header should edit directly on the title.");
        editor->setText("",juce::dontSendNotification);
        editor->onReturnKey();
        check(editor->isVisible() &&
              ui.show.cuelists[(size_t)ui.show.activeCuelist].name==original,
              "Invalid Cuelist name should remain editable in place.");
        editor->setText("Scene A",juce::dontSendNotification);
        editor->onReturnKey();
        check(!editor->isVisible() && !ui.isInlineRenaming() &&
              ui.show.cuelists[(size_t)ui.show.activeCuelist].name=="Scene A",
              "Enter should update the persisted Cuelist and close inline editor.");
        ui.renameCuelist(ui.show.activeCuelist);
        check(editor->isVisible() && editor->getY()<80,
              "Double-click Cuelist tab should edit on top of the tab.");
        editor->setText("Discard this",juce::dontSendNotification);
        editor->onEscapeKey();
        check(!editor->isVisible() &&
              ui.show.cuelists[(size_t)ui.show.activeCuelist].name=="Scene A",
              "Escape must cancel name editing without changing the show.");
        // The project name already has JUCE's native in-place Label editor.
        auto* project=dynamic_cast<juce::Label*>(
            controlsV3::named(ui,"projectName"));
        check(project && project->isEditableOnDoubleClick(),
              "Workspace project naming must remain in-place.");
    });
    test("Cue and column renaming never creates a modal dialog",[] {
        MainComponent ui(false);
        auto* editor=dynamic_cast<juce::TextEditor*>(
            controlsV3::named(ui,"inlineNameEditor"));
        check(editor!=nullptr,"In-place editor is missing.");
        ui.renameTitle(false,0);
        check(editor->isVisible() && editor->getWidth()>=80 &&
              editor->getHeight()>=20 &&
              editor->getY()>CueGrid::headerHeight,
              "Cue editor must appear over the cue-name label.");
        editor->setText("Lighting cue",juce::dontSendNotification);
        editor->onReturnKey();
        check(ui.show.rows[0].name=="Lighting cue" &&
              !editor->isVisible(),"Cue name was not committed inline.");
        ui.renameTitle(true,0);
        check(editor->isVisible() && editor->getWidth()>=80,
              "Column editor must appear over the column name.");
        editor->setText("Main Audio",juce::dontSendNotification);
        editor->onFocusLost();
        check(ui.show.columns[0].name=="Main Audio" &&
              !editor->isVisible(),
              "Clicking away must commit the column rename in place.");
        ui.renameTitle(false,0);
        editor->setText("Do not save",juce::dontSendNotification);
        editor->onEscapeKey();
        check(ui.show.rows[0].name=="Lighting cue" &&
              !editor->isVisible(),"Escape must restore the original cue name.");
    });
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

    test("Noto Math and Symbols 2 are explicitly chosen for their glyphs",[] {
        const auto& faces=appTypefaces();
        check(faces.notoMath!=nullptr && faces.notoSymbols2!=nullptr,
              "Bundled Noto typefaces failed to register");
        check(faces.notoMath->getNominalGlyphForCodepoint(0x221E).has_value() &&
              faces.notoMath->getNominalGlyphForCodepoint(0x2212).has_value(),
              "Noto Sans Math does not include minus/infinity");
        check(faces.notoSymbols2->getNominalGlyphForCodepoint(0x25B2).has_value() &&
              faces.notoSymbols2->getNominalGlyphForCodepoint(0x25BC).has_value() &&
              faces.notoSymbols2->getNominalGlyphForCodepoint(0x25C0).has_value() &&
              faces.notoSymbols2->getNominalGlyphForCodepoint(0x25B6).has_value(),
              "Noto Sans Symbols 2 does not include directional arrows");
        const auto body=font(12.0f),heading=font(20.0f);
        check(body.getTypefaceName().containsIgnoreCase("Exo") &&
              heading.getTypefaceName().containsIgnoreCase("Hind"),
              "Standard typography must remain Exo 2 / Hind");
        check(mathFont(12.0f).getTypefaceName().containsIgnoreCase("Noto Sans Math") &&
              symbolsFont(12.0f).getTypefaceName().containsIgnoreCase("Noto Sans Symbols"),
              "Noto special-purpose fonts are not selected explicitly");
        check(!body.getPreferredFallbackFamilies().contains("Noto Sans Math") &&
              !body.getPreferredFallbackFamilies().contains("Noto Sans Symbols 2"),
              "Noto fonts must be explicit rather than generic text fallback");
    });

    test("UI symbol audit uses only embedded Noto glyphs with safe coverage checks",[] {
        const auto& symbols=appTypefaces().notoSymbols2;
        check(symbols!=nullptr,"Bundled Noto Sans Symbols 2 is unavailable");
        const int icons[]={
            0x25B2, // row up
            0x25BC, // row down
            0x25C0, // column left
            0x25B6, // column right / Play
            0x23F8, // Pause
            0x23F9, // Stop
            0x22EF, // More actions
            0x2139, // Information
            0x26A0, // Caution
            0x1F50A, // Audible
            0x1F507  // Muted
        };
        for (const int codepoint:icons) {
            // Nominal presence can still mean .notdef or a non-renderable
            // glyph. An icon is supported only when JUCE supplies real ink.
            bool drawable=false;
            if (symbols!=nullptr) {
                const auto glyph=symbols->getNominalGlyphForCodepoint(
                    (juce::juce_wchar)codepoint);
                if (glyph && static_cast<int>(*glyph)!=0) {
                    juce::Path path;
                    symbols->getOutlineForGlyph(juce::TypefaceMetricsKind::portable,
                                                static_cast<int>(*glyph),path);
                    const auto bounds=path.getBounds();
                    drawable=!path.isEmpty() && bounds.getWidth()>0.0f &&
                             bounds.getHeight()>0.0f;
                }
            }
            juce::Image image(juce::Image::ARGB,36,36,true);
            juce::Graphics graphics(image);
            const bool drawn=drawNotoSymbol(graphics,
                (juce::juce_wchar)codepoint,{0,0,36,36},22.0f,palette::teal);
            const auto coverageMessage=
                "Noto symbol renderer wrongly accepted a missing/empty glyph (U+"+
                juce::String::toHexString(codepoint)+")";
            check(drawn==drawable,coverageMessage.toRawUTF8());
        }
        // Keep primary UI labels in Exo 2 / Hind, even after adding symbols.
        check(!font(12.0f).getPreferredFallbackFamilies().contains("Noto Sans Symbols 2"),
              "Noto must be explicitly selected, not a global font fallback");
    });

    test("Noto transport outlines are optically centred within icon buttons",[] {
        int rendered=0;
        for (const int cp:{0x25B6,0x23F9,0x25B2,0x25BC}) {
            constexpr int w=24,h=23;
            juce::Image image(juce::Image::ARGB,w,h,true);
            bool drawn=false;
            {
                // On Windows the graphics backend may defer raster writes
                // until the Graphics context is destroyed. Reading pixels
                // with that context still alive falsely reports a blank glyph.
                juce::Graphics g(image);
                drawn=drawNotoSymbol(g,(juce::juce_wchar)cp,{0,0,w,h},
                                     17.5f,palette::teal);
            }
            if (!drawn)
                continue; // Exact embedded-font coverage determines use.
            ++rendered;
            int left=w,top=h,right=-1,bottom=-1;
            for (int y=0;y<h;++y)
                for (int x=0;x<w;++x)
                    if (image.getPixelAt(x,y).getAlpha()>24) {
                        left=std::min(left,x);top=std::min(top,y);
                        right=std::max(right,x);bottom=std::max(bottom,y);
                    }
            const auto outlineMessage=
                "Drawable Noto glyph rendered blank (U+"+
                juce::String::toHexString(cp)+")";
            check(right>=left && bottom>=top,outlineMessage.toRawUTF8());
            if (right<left || bottom<top) continue;
            check(std::abs((left+right)*.5f-(w-1)*.5f)<=1.5f &&
                  std::abs((top+bottom)*.5f-(h-1)*.5f)<=1.5f,
                  "Noto glyph ink is offset from the actual button centre");
            check(left>=1 && top>=1 && right<w-1 && bottom<h-1,
                  "Noto glyph is too close to a small button edge");
        }
        check(rendered>=2,"Transport test found too few supported Noto symbols");
        juce::Image missing(juce::Image::ARGB,24,23,true);
        juce::Graphics missingGraphics(missing);
        check(!drawNotoSymbol(missingGraphics,(juce::juce_wchar)0x10FFFF,
                              {0,0,24,23},17.5f,palette::teal),
              "Unsupported symbol incorrectly reports successful rendering");
    });

    test("Fader Range menu explicitly draws Noto Math instead of a fallback",[] {
        const auto leading=faderMinusInfinity();
        check(leading.length()==2 && leading[0]==(juce::juce_wchar)0x2212 &&
              leading[1]==(juce::juce_wchar)0x221e,
              "Fader menu lost its U+2212/U+221E mathematical pair");
        for (const int ceiling:{0,6}) {
            const auto title=faderRangeMenuTitle(ceiling);
            check(title.startsWith(leading) &&
                  title.endsWith(ceiling==0?"0 dB":"+6 dB"),
                  "Fader menu caption is not an explicit mixed-font label");
            Theme theme;
            int width=0,height=0;
            theme.getIdealPopupMenuItemSize(title,false,24,width,height);
            check(width>=142 && height>=34,
                  "Fader menu clips the mathematical portion of its caption");
            juce::Image img(juce::Image::ARGB,width,height,true);
            juce::Graphics graphics(img);
            theme.drawPopupMenuItem(graphics,{0,0,width,height},
                                    false,true,false,true,false,
                                    title,{},nullptr,nullptr);
            check(img.isValid(),"Fader menu mixed-font rendering failed");
        }
    });

    test("Unsaved changes confirmation remains compact and Cancel is safe",[] {
        int discardCount=0;
        UnsavedChangesPanel panel([&]{ ++discardCount; });
        check(panel.getWidth()==510 && panel.getHeight()==228,
              "Discard confirmation no longer fits a compact dialog");
        auto* cancel=dynamic_cast<juce::TextButton*>(
            controlsV3::named(panel,"cancelUnsavedChanges"));
        auto* discard=dynamic_cast<juce::TextButton*>(
            controlsV3::named(panel,"discardUnsavedChanges"));
        check(cancel && discard,"Confirm/Cancel actions are missing");
        check(cancel->getButtonText()=="Cancel" &&
              discard->getButtonText()=="Discard and continue" &&
              discard->getRight()+10<=cancel->getX() &&
              discard->getBottom()<=panel.getHeight() &&
              cancel->getBottom()<=panel.getHeight(),
              "Confirmation buttons overlap or have incorrect labels");
        auto rendered=panel.createComponentSnapshot(panel.getLocalBounds());
        check(rendered.isValid() && rendered.getWidth()==510,
              "Unsaved changes confirmation did not render");
        cancel->onClick();
        check(discardCount==0,"Cancel discarded unsaved work");
        discard->onClick();
        check(discardCount==1,
              "Destructive action must run only after explicit confirmation");
    });

    test("About and Diagnostics use compact aligned panels without InfoIcon",[] {
        OperatorReportPanel about("About The Cuelist","Version 0.9",{
            {"AUTHORS",{{"Davide Salvatore","Co-author"},
                        {"Riccardo D'Ambrosio","Co-author"}}},
            {"LICENCE",{{"Software","Proprietary"}}}
        },"Licence and third-party notices");
        OperatorReportPanel diag("Diagnostics","Engine diagnostics",{
            {"AUDIO",{{"Device","ONLINE",OperatorReportRow::Status::positive}}},
            {"MIDI",{{"Input","DISCONNECTED",OperatorReportRow::Status::caution}}},
            {"OSC",{{"UDP packets sent","0"}}},
            {"RECOVERY",{{"I/O failures","0"}}}
        },"Local counters only");
        for (auto* panel:std::initializer_list<OperatorReportPanel*>{&about,&diag}) {
            check(panel->getWidth()==542 && panel->getHeight()<720,
                  "Operator report escaped compact desktop bounds");
            auto* button=controlsV3::named(*panel,"operatorReportClose");
            check(button && button->getBounds().getBottom()<=panel->getHeight(),
                  "Close button is not visible inside the information panel");
            auto image=panel->createComponentSnapshot(panel->getLocalBounds());
            check(image.isValid(),"Information dialog content cannot be rendered");
        }
    });

    test("Shared editor prompts use Hind/Exo styling and explicit form fields",[] {
        OperatorTextPrompt single("Rename cue","Enter a cue name.",
                                  "Cue name","Scena",false,"Apply",[](juce::String){});
        OperatorTextPrompt tempo("Cue tempo map","Each line is beat BPM.",
                                 "Tempo changes","0 120\\n16 140",true,"Apply",
                                 [](juce::String){});
        check(single.getWidth()==510 && single.getHeight()==256 &&
              tempo.getHeight()==400,
              "Native editing surfaces have unexpected dimensions");
        check(single.getEditor().getName()=="value" &&
              single.getEditor().getText()=="Scena" &&
              tempo.getEditor().isMultiLine() &&
              tempo.getEditor().getText().contains("16 140"),
              "Shared prompt lost editable text or tempo multiline support");
        for (auto* editor :std::initializer_list<OperatorTextPrompt*>{&single,&tempo}) {
            auto* apply=controlsV3::named(*editor,"operatorPromptAccept");
            auto* cancel=controlsV3::named(*editor,"operatorPromptCancel");
            check(apply && cancel && apply->getRight()+7<=cancel->getX() &&
                  cancel->getBottom()<editor->getHeight(),
                  "Prompt action buttons overlap or leave the dialog");
            check(editor->getEditor().getBounds().getWidth()>=450,
                  "Cue editor field is not visibly sized");
            const auto image=editor->createComponentSnapshot(editor->getLocalBounds());
            check(image.isValid(),"Shared editor failed native render");
        }
    });

    test("Control Mapping exposes labeled targets and safe empty mapping actions",[] {
        MainComponent ui(false);
        ui.openControlPanel(false);
        auto* dialog=dynamic_cast<juce::DialogWindow*>(
            juce::ModalComponentManager::getInstance()->getModalComponent(0));
        check(dialog!=nullptr,"Control Mapping window did not open");
        auto* pane=controlsV3::named(*dialog,"controlMappingPanel");
        auto* target=dynamic_cast<juce::ComboBox*>(controlsV3::named(*dialog,"mappingTarget"));
        auto* destination=dynamic_cast<juce::ComboBox*>(
            controlsV3::named(*dialog,"mappingDestinationColumn"));
        auto* output=controlsV3::named(*dialog,"mappingOutputCc");
        auto* osc=controlsV3::named(*dialog,"mappingOscAddress");
        auto* remove=controlsV3::named(*dialog,"mappingRemoveSelected");
        auto* update=controlsV3::named(*dialog,"mappingUpdateSelected");
        auto* empty=controlsV3::named(*dialog,"mappingEmptyState");
        check(pane && target && destination && output && osc && remove &&
              update && empty,"Control Mapping controls or empty-state label missing");
        check(pane->getHeight()>=650 && empty->isVisible() &&
              !update->isEnabled() && !remove->isEnabled() &&
              target->getSelectedId()==5 && destination->isEnabled(),
              "Starter functions or empty physical mapping state incorrect");
        // The ComboBox default is sendNotificationAsync. Drive the real
        // onChange callback synchronously before asserting dependent fields.
        target->setSelectedId(5,juce::sendNotificationSync); // MIDI CC
        check(output->isEnabled() && !osc->isEnabled() &&
              destination->isEnabled(),
              "MIDI CC target does not enable only relevant controls");
        target->setSelectedId(6,juce::sendNotificationSync); // OSC float
        check(!output->isEnabled() && osc->isEnabled(),
              "OSC target does not enable its address editor");
        delete dialog;
    });

    test("Control Mappings archive is independent from cue assignment and opens details on demand",[] {
        MainComponent ui(false);
        const auto originalColumns=ui.show.columns.size();
        const auto originalAssignments=ui.show.rows[0].cells.size();
        ui.openControlPanel(false);
        auto* dialog=dynamic_cast<juce::DialogWindow*>(
            juce::ModalComponentManager::getInstance()->getModalComponent(0));
        check(dialog!=nullptr,"Map archive window failed to open");
        auto* chooser=dynamic_cast<juce::ComboBox*>(
            controlsV3::named(*dialog,"mappingMapSelector"));
        auto* newMap=controlsV3::named(*dialog,"mappingNewMap");
        auto* duplicate=controlsV3::named(*dialog,"mappingDuplicateMap");
        auto* menu=controlsV3::named(*dialog,"mappingMoreMenu");
        auto* add=dynamic_cast<juce::TextButton*>(
            controlsV3::named(*dialog,"mappingAddMapping"));
        auto* kind=controlsV3::named(*dialog,"mappingInputKind");
        auto* target=controlsV3::named(*dialog,"mappingTarget");
        auto* save=controlsV3::named(*dialog,"mappingUpdateSelected");
        auto* midi=controlsV3::named(*dialog,"mappingCaptureMidi");
        auto* osc=controlsV3::named(*dialog,"mappingCaptureOsc");
        check(chooser && newMap && duplicate && menu && add && kind &&
              target && save && midi && osc,
              "Focused mapping archive controls missing");
        check(chooser->getSelectedId()==1 &&
              chooser->getText()=="Default Map (fallback)",
              "Default Map must open selected as fallback");
        check(newMap->isVisible() && duplicate->isVisible() &&
              menu->isVisible() && midi->isVisible() && osc->isVisible(),
              "Map archive or Learn actions invisible");
        check(!kind->isVisible() && !target->isVisible() &&
              !save->isVisible(),
              "Detailed mapping fields must be hidden until selection");
        check(ui.show.columns.size()==originalColumns &&
              ui.show.rows[0].cells.size()==originalAssignments &&
              ui.show.controls.templates.empty(),
              "Opening the archive unexpectedly created a Control Column or map");
        add->onClick();
        check(kind->isVisible() && target->isVisible() &&
              save->isVisible() && save->isEnabled(),
              "Add Mapping failed to open a dedicated editor");
        delete dialog;
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
    test("Native Edit menu preserves Undo and Redo without obscuring tabs",[] {
        MainComponent ui(false);
        auto* undo=dynamic_cast<juce::TextButton*>(
            controlsV3::named(ui,"showUndo"));
        auto* redo=dynamic_cast<juce::TextButton*>(
            controlsV3::named(ui,"showRedo"));
        auto* edit=dynamic_cast<juce::TextButton*>(
            controlsV3::named(ui,"operatorMenuEdit"));
        auto* more=dynamic_cast<juce::TextButton*>(
            controlsV3::named(ui,"operatorMenuMore"));
        check(undo && redo && edit && more,
              "Native Edit, overflow or original Undo/Redo callbacks missing");
        check(!undo->isEnabled() && !redo->isEnabled(),
              "Empty history must still disable Undo/Redo");
        for (const auto dimensions:std::initializer_list<juce::Point<int>>{
                 {1180,820},{1440,900},{1647,895},{1920,1080}}) {
            ui.setSize(dimensions.x,dimensions.y);
            ui.resized();
            check(!undo->isVisible() && !redo->isVisible() &&
                  (edit->isVisible() || more->isVisible()),
                  "Undo/Redo are not reachable through native Edit/More");
            const auto image=ui.createComponentSnapshot(
                {dimensions.x-365,8,354,56});
            check(image.isValid() && image.getHeight()==56,
                  "Live MIDI/OSC/audio monitor stopped rendering");
        }
    });

    test("Native Guide and About stay reachable at every supported width",[] {
        MainComponent ui(false);
        auto* oldHelp=dynamic_cast<juce::TextButton*>(
            controlsV3::named(ui,"showHelpMenu"));
        auto* guide=dynamic_cast<juce::TextButton*>(
            controlsV3::named(ui,"operatorMenuGuide"));
        auto* more=dynamic_cast<juce::TextButton*>(
            controlsV3::named(ui,"operatorMenuMore"));
        auto* tab=dynamic_cast<juce::TextButton*>(
            controlsV3::named(ui,"inspectorSideToggle"));
        check(oldHelp && guide && more && tab,
              "Guide, More or Inspector grip missing");
        check(juce::String(THE_CUELIST_VERSION).startsWith("0.9.0-beta."),
              "About version is not bound to VERSION.txt");
        for (const auto size:std::initializer_list<juce::Point<int>>{
                 {1180,820},{1440,900},{1647,895},{1920,1080}}) {
            ui.setSize(size.x,size.y);
            ui.resized();
            check(!oldHelp->isVisible() &&
                  (guide->isVisible() || more->isVisible()),
                  "Guide/About must remain in a native menu at every width");
            check(tab->getButtonText().isEmpty() &&
                  tab->getWidth()==20 && tab->getHeight()==46,
                  "Inspector grip must preserve directional vector symbols");
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
        auto* modal=dynamic_cast<juce::DialogWindow*>(
            juce::ModalComponentManager::getInstance()->getModalComponent(0));
        auto* editor=modal ? dynamic_cast<OperatorTextPrompt*>(modal->getContentComponent()) : nullptr;
        check(editor!=nullptr,"BPM row label did not open shared editor");
        check(editor->getEditor().getText().contains("120"),"BPM dialog initial value incorrect");
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

    test("Cue rows meet BPM without a right scrollbar gutter",[] {
        MainComponent ui(false);
        ui.setSize(1440,900);
        auto* headers=cueViewFixtures::find<PinnedCueHeaders>(ui);
        auto* metrics=cueViewFixtures::find<CueMetricsOverlay>(ui);
        auto* grid=cueViewFixtures::find<CueGrid>(ui);
        juce::Viewport* view=nullptr;
        for (auto* child:ui.getChildren())
            if (auto* candidate=dynamic_cast<juce::Viewport*>(child))
                view=candidate;
        check(view && headers && metrics && grid,
              "Cue grid, header or metrics unavailable");
        check(!view->isVerticalScrollbarOnTheRight(),
              "Cue vertical scrollbar is still between cells and BPM");
        check(!view->getVerticalScrollBar().isVisible() &&
              grid->getWidth()==view->getWidth() &&
              headers->getX()==view->getX(),
              "Short Cue List still reserves an empty right-side gutter");
        for (int i=0;i<16;++i) addRow(ui.show,"Scroll cue");
        ui.resized();
        check(view->getVerticalScrollBar().isVisible(),
              "Tall Cue List did not expose its vertical scrollbar");
        const int inset=view->getScrollBarThickness();
        check(view->getVerticalScrollBar().getX()==0 &&
              headers->getX()==view->getX()+inset &&
              headers->getWidth()==CueGrid::nameWidth,
              "Pinned header did not follow the left-side scrollbar inset");
        check(grid->getWidth()==view->getWidth()-inset &&
              metrics->getX()==view->getRight(),
              "Right edge did not meet the pinned BPM strip");
        view->setViewPosition(0,112);
        check(view->getViewPositionY()==112,
              "Left-side scrollbar broke cue-list vertical scrolling");
        const auto corner=view->createComponentSnapshot({
            view->getWidth()-7,CueGrid::headerHeight+25,7,28});
        check(corner.isValid(),"Cue list right edge failed to render");
        const auto body=corner.getPixelAt(3,6);
        check(body!=juce::Colour(0xff071725),
              "A dark scrollbar gutter is still painted against BPM");
    });

    test("BPM/Auto Stop edge remains a continuous accent while rows scroll",[] {
        MainComponent ui(false);
        for (int i=0;i<8;++i) addRow(ui.show,"Cue");
        ui.setSize(1440,900);
        auto* metrics=cueViewFixtures::find<CueMetricsOverlay>(ui);
        juce::Viewport* view=nullptr;
        for (auto* child:ui.getChildren())
            if (auto* candidate=dynamic_cast<juce::Viewport*>(child))
                view=candidate;
        check(metrics && view,"Cannot locate pinned metrics for border test");
        view->setViewPosition(0,83);
        const auto rail=metrics->createComponentSnapshot({0,0,3,metrics->getHeight()});
        check(rail.isValid() && rail.getWidth()==3,
              "Pinned metrics accent rail not rendered");
        for (int y=0;y<rail.getHeight();y+=7)
            check(rail.getPixelAt(1,y)==palette::teal,
                  "Row separators interrupt the pinned metrics accent rail");
    });

    test("32:9 live UI keeps cue, columns and BPM headers locked together",[] {
        for (const auto size:std::initializer_list<juce::Point<int>>{
                 {3840,1080},{5120,1440}}) {
            MainComponent ui(false);
            for (int i=0;i<22;++i) addRow(ui.show,"Ultrawide cue");
            for (int i=0;i<3;++i) addColumn(ui.show,ColumnType::audio,{});
            ui.setSize(size.x,size.y);
            auto* names=cueViewFixtures::find<PinnedCueHeaders>(ui);
            auto* columns=cueViewFixtures::find<PinnedColumnHeaders>(ui);
            auto* metrics=cueViewFixtures::find<CueMetricsOverlay>(ui);
            juce::Viewport* viewport=nullptr;
            for (auto* child:ui.getChildren())
                if (auto* candidate=dynamic_cast<juce::Viewport*>(child))
                    viewport=candidate;
            check(names && columns && metrics && viewport,
                  "Ultrawide pinned heading/viewport components missing");
            if (!(names && columns && metrics && viewport)) continue;
            check(columns->getX()==names->getRight() &&
                  columns->getRight()==viewport->getRight() &&
                  metrics->getX()==viewport->getRight() &&
                  columns->getHeight()==CueGrid::headerHeight &&
                  metrics->getRight()<size.x &&
                  viewport->getWidth()>2000,
                  "Ultrawide UI leaves gaps or overlaps between fixed headings");
            viewport->setViewPosition(0,146);
            check(viewport->getViewPositionY()==146 &&
                  columns->getY()==viewport->getY() &&
                  columns->gridXAt(0)==CueGrid::nameWidth,
                  "32:9 vertical scrolling detached column headers from cue cells");
            const auto headerImage=columns->createComponentSnapshot(
                {0,0,std::min(300,columns->getWidth()),CueGrid::headerHeight});
            check(headerImage.isValid() &&
                  headerImage.getHeight()==CueGrid::headerHeight,
                  "Ultrawide clipped or failed to paint the pinned header");
        }
    });

    test("Audio MIDI OSC headers remain fixed while cue rows scroll",[] {
        MainComponent ui(false);
        for (int i=0;i<12;++i) addRow(ui.show,"Scrolled cue");
        for (int i=0;i<7;++i) addColumn(ui.show,ColumnType::audio,{});
        ui.setSize(1180,820);
        auto* columns=cueViewFixtures::find<PinnedColumnHeaders>(ui);
        auto* names=cueViewFixtures::find<PinnedCueHeaders>(ui);
        auto* metrics=cueViewFixtures::find<CueMetricsOverlay>(ui);
        auto* grid=cueViewFixtures::find<CueGrid>(ui);
        juce::Viewport* view=nullptr;
        for (auto* child:ui.getChildren())
            if (auto* candidate=dynamic_cast<juce::Viewport*>(child))
                view=candidate;
        check(columns && names && metrics && grid && view,
              "Fixed column heading or viewport is missing");
        check(columns->getHeight()==CueGrid::headerHeight &&
              columns->getY()==view->getY() &&
              columns->getX()==names->getRight() &&
              columns->getRight()==view->getRight() &&
              metrics->getX()==view->getRight(),
              "Cue, column and BPM headings do not share one fixed boundary");
        const auto heading=juce::Rectangle<int>(
            0,0,columns->getWidth(),CueGrid::headerHeight);
        const auto before=columns->createComponentSnapshot(heading);
        view->setViewPosition(0,115);
        check(view->getViewPositionY()==115,
              "Vertical scrolling did not move the underlying cue rows");
        const auto after=columns->createComponentSnapshot(heading);
        bool same=before.isValid() && after.isValid() &&
                  before.getBounds()==after.getBounds();
        if(same)
            for (int y=0;y<before.getHeight() && same;y+=4)
                for (int x=0;x<before.getWidth();x+=4)
                    if (before.getPixelAt(x,y)!=after.getPixelAt(x,y)) {
                        same=false;break;
                    }
        check(same,"Column headings move or leak cue rows during vertical scrolling");
        check(columns->getY()==view->getY(),
              "Column heading shifted after vertical scrolling");

        view->setViewPosition(160,0);
        check(view->getViewPositionX()==160,
              "Horizontal scroll does not reach additional audio columns");
        check(columns->gridXAt(0)==CueGrid::nameWidth+160,
              "Header and body are not using the same horizontal offset");
        // Local x=40 is now inside Audio 2; selecting the header should
        // choose that column, not a cell in the underlying first cue.
        const int selectedRow=ui.selectedRow;
        cueViewFixtures::click(*columns,40.0f,45.0f);
        check(ui.selectedColumn==1 && ui.selectedRow==selectedRow,
              "Pinned column-header click used unscrolled hit targets");
        view->setViewPosition(0,0);
        const bool wasCollapsed=ui.show.columns[0].collapsed;
        cueViewFixtures::click(*columns,(float)CueGrid::columnWidth-15,47.0f);
        check(ui.show.columns[0].collapsed!=wasCollapsed,
              "Column header collapse is no longer clickable above the scroll");
    });

    test("Scrolled cues remain below pinned headings and header targets are protected",[] {
        MainComponent ui(false);
        for (int i=0;i<11;++i) addRow(ui.show,"Extra cue");
        ui.setSize(1180,820);
        auto* metrics=cueViewFixtures::find<CueMetricsOverlay>(ui);
        auto* headers=cueViewFixtures::find<PinnedCueHeaders>(ui);
        juce::Viewport* view=nullptr;
        for (auto* child:ui.getChildren())
            if (auto* candidate=dynamic_cast<juce::Viewport*>(child)) {
                view=candidate;break;
            }
        check(metrics && headers && view,"Pinned heading fixture unavailable");
        const auto heading=juce::Rectangle<int>(0,0,metrics->getWidth(),CueGrid::headerHeight);
        const auto cueHeading=juce::Rectangle<int>(0,0,headers->getWidth(),CueGrid::headerHeight);
        const auto metricBefore=metrics->createComponentSnapshot(heading);
        const auto cueBefore=headers->createComponentSnapshot(cueHeading);
        view->setViewPosition(0,115);
        check(view->getViewPositionY()==115,"Test did not scroll the cue grid");
        const auto metricAfter=metrics->createComponentSnapshot(heading);
        const auto cueAfter=headers->createComponentSnapshot(cueHeading);
        const auto unchanged=[](const juce::Image& before,const juce::Image& after) {
            if (!before.isValid() || !after.isValid() ||
                before.getWidth()!=after.getWidth() || before.getHeight()!=after.getHeight())
                return false;
            for (int y=0;y<before.getHeight();y+=3)
                for (int x=0;x<before.getWidth();x+=3)
                    if (before.getPixelAt(x,y)!=after.getPixelAt(x,y))
                        return false;
            return true;
        };
        check(unchanged(metricBefore,metricAfter) &&
              unchanged(cueBefore,cueAfter),
              "Scrolled cue text or toggles leaked through a fixed heading");
        const auto oldSelection=ui.selectedRow;
        juce::Array<bool> stopBefore;
        for (const auto& row:ui.show.rows) stopBefore.add(row.stopOnNext);
        cueViewFixtures::click(*metrics,118.0f,45.0f);
        cueViewFixtures::click(*headers,120.0f,45.0f);
        check(ui.selectedRow==oldSelection,
              "A scrolled cue was selected through the pinned header");
        bool stopUnchanged=true;
        for (int i=0;i<(int)ui.show.rows.size();++i)
            stopUnchanged &= (ui.show.rows[(size_t)i].stopOnNext==stopBefore[i]);
        check(stopUnchanged,"Auto Stop was toggled through the fixed BPM header");
        ui.select(0,0);
        check(view->getViewPositionY()==0,
              "Selecting the first cue leaves it hidden behind the pinned heading");
    });

    test("Cue transport buttons have independent visible and clickable bounds",[] {
        const auto play=CueGrid::rowPlayBounds(0);
        const auto stop=CueGrid::rowStopBounds(0);
        const auto menu=CueGrid::rowMenuBounds(0);
        check(play.getWidth()>=28 && stop.getWidth()>=28 && menu.getWidth()>=28 &&
              play.getHeight()>=24 && stop.getHeight()>=24 && menu.getHeight()>=24,
              "Cue transport buttons are too small");
        check(play.getRight()+5<=stop.getX() &&
              stop.getRight()+5<=menu.getX() &&
              menu.getRight()<=CueGrid::nameWidth-7 &&
              107+CueGrid::nameWidth-224<play.getX(),
              "Cue Play, Stop, menu or title overlap");
        MainComponent ui(false);
        auto* collapse=dynamic_cast<juce::TextButton*>(
            controlsV3::named(ui,"toggleAllColumns"));
        check(collapse && collapse->getTooltip().isEmpty() &&
              collapse->getBottom()<=74+CueGrid::headerHeight,
              "Expand all still creates a tooltip over the first cue");
    });

    test("Cue-row Play Stop More have compact, equal spacing",[] {
        const auto play=CueGrid::rowPlayBounds(0);
        const auto stop=CueGrid::rowStopBounds(0);
        const auto menu=CueGrid::rowMenuBounds(0);
        check(stop.getX()-play.getRight()==5 &&
              menu.getX()-stop.getRight()==5 &&
              menu.getRight()<=CueGrid::nameWidth-7,
              "Cue-row action gaps must be 5px and stay within the title column");
    });
    test("Individual cell transports have smaller visuals with full-size hitboxes",[] {
        const auto play=CueGrid::cellPlayBounds(0,CueGrid::columnWidth,0);
        const auto stop=CueGrid::cellStopBounds(0,CueGrid::columnWidth,0);
        check(play.getWidth()>=24 && play.getHeight()>=23 &&
              stop.getWidth()>=24 && stop.getHeight()>=23 &&
              stop.getX()-play.getRight()>=8 &&
              stop.getRight()<=CueGrid::columnWidth-7,
              "Cell Play and Stop are cramped or overlapping");
        check(!play.contains(stop.getCentre()) &&
              !stop.contains(play.getCentre()),
              "Cell transports share hit targets");
        const auto visualPlay=CueGrid::cellButtonVisualBounds(play);
        const auto visualStop=CueGrid::cellButtonVisualBounds(stop);
        check(visualPlay.getWidth()==20 && visualPlay.getHeight()==21 &&
              visualStop.getWidth()==20 && visualStop.getHeight()==21 &&
              play.contains(visualPlay) && stop.contains(visualStop),
              "Cell button visuals must be smaller than their 24x23 hit areas");
    });

    test("Header telemetry has balanced cards and CPU one-decimal formatting",[] {
        MainComponent ui(false);
        check(HeaderTelemetryLayout::percentage(5.8593f,true)=="5.9%" &&
              HeaderTelemetryLayout::percentage(47.0f,false)=="47%" &&
              HeaderTelemetryLayout::percentage(-1.0f,true)=="N/A",
              "CPU must show one decimal, RAM a whole percent, or N/A");
        for (const auto size:std::initializer_list<juce::Point<int>>{
                 {1180,820},{1440,900},{1647,895},{1920,1080}}) {
            ui.setSize(size.x,size.y);
            const auto layout=HeaderTelemetryLayout::atWidth(size.x);
            check(layout.clock.getRight()+14==layout.usage.getX() &&
                  layout.clock.getY()==layout.usage.getY() &&
                  layout.clock.getHeight()==layout.usage.getHeight(),
                  "Clock and CPU/RAM panels need a consistent 14px gutter");
            check(layout.usage.contains(layout.cpuValue) &&
                  layout.usage.contains(layout.ramValue) &&
                  layout.usage.contains(layout.cpuMeter.toNearestInt()) &&
                  layout.usage.contains(layout.ramMeter.toNearestInt()),
                  "CPU/RAM labels and meters must fit inside the card");
            const auto clock=ui.createComponentSnapshot(layout.clock);
            const auto cpu=ui.createComponentSnapshot(layout.usage);
            check(clock.isValid() && clock.getWidth()==124 &&
                  cpu.isValid() && cpu.getWidth()==132,
                  "Header cards must render at compact window widths");
            check(font(11.0f).getTypefaceName().containsIgnoreCase("Exo"),
                  "Small telemetry text lost Exo 2");
        }
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
    test("Control Column exposes cue overrides without a legacy lock",[] {
        MainComponent ui(false);
        addColumn(ui.show,ColumnType::control,"Control");
        auto* utility=cueViewFixtures::find<ColumnUtilities>(ui);
        utility->refresh();
        auto* strip=utility->getChildComponent(0)->getChildComponent(1);
        auto* lock=controlsV3::named(*strip,"controlColumnLock");
        auto* priority=dynamic_cast<juce::TextButton*>(
            controlsV3::named(*strip,"controlColumnPriority"));
        auto* editor=dynamic_cast<juce::TextButton*>(
            controlsV3::named(*strip,"controlColumnEditSet"));
        check(lock==nullptr && priority!=nullptr && editor!=nullptr,
              "Retired per-column Lock is still mounted in the UI");
        check(!ui.show.columns[1].controlLocked &&
              ui.show.columns[1].controlPriority==2 &&
              editor->getButtonText()=="Edit Q set",
              "Cue-local Control Set UI is not the new default");
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
