#pragma once
#include "ProjectMediaLoader.h"

void cuelistGuiMockupTests() {
    using namespace cueViewFixtures;
    test("Cue notepad survives switching Cuelists and v10 persistence", [] {
        MainComponent ui(false);
        auto* cue=dynamic_cast<juce::TextEditor*>(controlsV3::named(ui,"cueName"));
        auto* notes=dynamic_cast<juce::TextEditor*>(controlsV3::named(ui,"cueNotes"));
        auto* plus=dynamic_cast<juce::TextButton*>(controlsV3::named(ui,"newCuelist"));
        check(cue && notes && plus,"Cue editor, notes or Cuelist plus missing");
        cue->setText("Preshow cue",false);
        notes->setText("Stand by the conductor\nOpen the main curtain",false);
        ui.createCuelist();
        check(ui.show.activeCuelist==1 && ui.show.cuelists.size()==2,
              "Creating Cuelist did not select a second cue list");
        check(ui.show.rows.size()==1 && ui.show.rows[0].name.isEmpty(),
              "New Cuelist did not begin with an independent blank cue");
        notes->setText("Musical notepad",false);
        ui.activateCuelist(0);
        check(ui.show.rows[0].name=="Preshow cue" &&
              ui.show.rows[0].notes.contains("conductor"),
              "Changing Cuelist lost the original cue and notes");
        check(notes->getText().contains("conductor"),
              "Inspector notepad failed to follow the active Cuelist");
        ui.activateCuelist(1);
        check(ui.show.rows[0].notes=="Musical notepad",
              "Second Cuelist note content was lost on return");
        const auto snapshot=encodeShow(ui.show);
        Show restored;
        check(decodeShow(snapshot,restored).wasOk(),"Multi-Cuelist v10 file rejected");
        check(restored.activeCuelist==1 && restored.cuelists.size()==2 &&
              restored.cuelists[0].rows[0].notes.contains("conductor") &&
              restored.rows[0].notes=="Musical notepad",
              "Cuelist names, selection or cue notes failed round-trip");
    });

    test("Drag reorders Cuelist tabs without activating or stopping the active list",[] {
        MainComponent ui(false);
        ui.createCuelist();
        ui.createCuelist();
        ui.setSize(1440,900);
        const int activeCue=ui.show.rows[0].id;
        const auto originalOrder=juce::StringArray{
            ui.show.cuelists[0].name,ui.show.cuelists[1].name,
            ui.show.cuelists[2].name};
        auto* first=dynamic_cast<CuelistTabButton*>(
            controlsV3::named(ui,"cuelistTab0"));
        auto* third=dynamic_cast<CuelistTabButton*>(
            controlsV3::named(ui,"cuelistTab2"));
        check(first && third && first->isVisible() && third->isVisible(),
              "Reorder test requires three visible draggable tabs.");
        const juce::Point<float> start{12.0f,13.0f};
        const juce::Point<float> drop{
            (float)(third->getBounds().getCentreX()-first->getX()),13.0f};
        first->mouseDown(event(*first,start,start));
        first->mouseDrag(event(*first,drop,start,1,true));
        first->mouseUp(event(*first,drop,start,1,true));
        check(ui.show.cuelists[0].name==originalOrder[1] &&
              ui.show.cuelists[1].name==originalOrder[2] &&
              ui.show.cuelists[2].name==originalOrder[0] &&
              ui.show.activeCuelist==1 &&
              ui.show.rows[0].id==activeCue,
              "Tab drag failed to reorder or accidentally changed active Cuelist.");
        check(validateShow(ui.show).wasOk(),
              "Reordering tabs must retain valid independent cue lists.");
        Show decoded;
        check(decodeShow(encodeShow(ui.show),decoded).wasOk() &&
              decoded.cuelists[2].name==originalOrder[0] &&
              decoded.activeCuelist==1 && decoded.rows[0].id==activeCue,
              "Dragged Cuelist order did not persist in project data.");
    });
    test("Cuelists move by dedicated arrows and keyboard shortcut",[] {
        MainComponent ui(false);
        ui.createCuelist();
        ui.createCuelist();
        auto* left=dynamic_cast<juce::TextButton*>(
            controlsV3::named(ui,"cuelistMoveLeft"));
        auto* right=dynamic_cast<juce::TextButton*>(
            controlsV3::named(ui,"cuelistMoveRight"));
        check(left && right && left->isEnabled() && !right->isEnabled(),
              "Cuelist arrows should reflect active tab boundaries.");
        const int cueId=ui.show.rows[0].id;
        click(*left,10.f,10.f);
        check(ui.show.activeCuelist==1 && ui.show.rows[0].id==cueId &&
              ui.show.cuelists[1].name=="Cuelist 3" &&
              left->isEnabled() && right->isEnabled(),
              "Left arrow failed to move the active tab one place.");
        const juce::ModifierKeys shortcut(
            juce::ModifierKeys::ctrlModifier |
            juce::ModifierKeys::shiftModifier);
        check(ui.keyPressed(juce::KeyPress(juce::KeyPress::leftKey,shortcut,0)) &&
              ui.show.activeCuelist==0 && ui.show.rows[0].id==cueId,
              "Ctrl+Shift+Left must move the active Cuelist without changing cues.");
        check(ui.keyPressed(juce::KeyPress(juce::KeyPress::rightKey,shortcut,0)) &&
              ui.show.activeCuelist==1 && ui.show.rows[0].id==cueId,
              "Ctrl+Shift+Right must restore the active Cuelist ordering.");
        click(*right,10.f,10.f);
        check(ui.show.activeCuelist==2 && ui.show.rows[0].id==cueId &&
              !right->isEnabled(),"Right arrow should stop at final Cuelist.");
        check(!ui.moveCuelist(2,3) && !ui.moveCuelist(-1,0),
              "Invalid move indices must not change the show.");
    });

    test("Drag cues within the Cuelist and keyboard arrows preserve their cell identities",[] {
        MainComponent ui(false);
        addRow(ui.show,"Second");
        addRow(ui.show,"Third");
        const int firstId=ui.show.rows[0].id;
        const int secondId=ui.show.rows[1].id;
        const int thirdId=ui.show.rows[2].id;
        const int firstCell=ui.show.rows[0].cells[0].id;
        auto* pinned=cueViewFixtures::find<PinnedCueHeaders>(ui);
        check(pinned!=nullptr,"Pinned cue header missing.");
        const juce::Point<float> from{156.0f,
            (float)CueGrid::headerHeight+CueGrid::rowContentInset+20};
        const juce::Point<float> to{156.0f,
            from.y+2*CueGrid::rowHeight};
        pinned->mouseDown(event(*pinned,from,from));
        pinned->mouseDrag(event(*pinned,to,from,1,true));
        pinned->mouseUp(event(*pinned,to,from,1,true));
        check(ui.show.rows[0].id==secondId &&
              ui.show.rows[1].id==thirdId &&
              ui.show.rows[2].id==firstId &&
              ui.show.rows[2].cells[0].id==firstCell &&
              ui.selectedRow==2,
              "Cue drag failed to move the complete row with its cells.");
        const juce::ModifierKeys mods(
            juce::ModifierKeys::ctrlModifier |
            juce::ModifierKeys::shiftModifier);
        check(ui.keyPressed(juce::KeyPress(juce::KeyPress::upKey,mods,0)) &&
              ui.selectedRow==1 && ui.show.rows[1].id==firstId,
              "Ctrl+Shift+Up did not move the selected cue.");
        check(ui.keyPressed(juce::KeyPress(juce::KeyPress::downKey,mods,0)) &&
              ui.selectedRow==2 && ui.show.rows[2].id==firstId,
              "Ctrl+Shift+Down did not move the selected cue.");
        check(validateShow(ui.show).wasOk(),
              "Cue row reordering invalidated project model.");
        Show restored;
        check(decodeShow(encodeShow(ui.show),restored).wasOk() &&
              restored.rows[2].id==firstId,
              "Cue drag order was not persisted.");
    });

    test("Cuelist names update active tab and Cue List header bidirectionally",[] {
        MainComponent ui(false);
        ui.createCuelist();
        check(ui.setCuelistName(1,"Act I - Musical"),
              "Active Cuelist rename was rejected");
        auto* second=dynamic_cast<juce::TextButton*>(
            controlsV3::named(ui,"cuelistTab1"));
        check(second && second->getButtonText().startsWith("Act I") &&
              ui.show.cuelists[1].name=="Act I - Musical",
              "Renaming the active heading did not update the selected tab");
        check(ui.setCuelistName(0,"Pre-show"),
              "An inactive Cuelist could not be renamed from its tab");
        auto* first=dynamic_cast<juce::TextButton*>(
            controlsV3::named(ui,"cuelistTab0"));
        check(first && first->getButtonText()=="Pre-show",
              "Renaming an inactive tab did not update its label");
        ui.activateCuelist(0);
        check(ui.show.cuelists[ui.show.activeCuelist].name=="Pre-show",
              "Switching to the renamed tab did not update the Cue List heading");
        check(!ui.setCuelistName(0,"Act I - Musical") &&
              !ui.setCuelistName(0,"  ") &&
              !ui.setCuelistName(0,juce::String::repeatedString("X",65)),
              "Duplicate, blank or oversized Cuelist names should be rejected");
        check(ui.show.cuelists[0].name=="Pre-show",
              "Invalid rename overwrote the original name");
        Show restored;
        check(decodeShow(encodeShow(ui.show),restored).wasOk() &&
              restored.cuelists[0].name=="Pre-show" &&
              restored.cuelists[1].name=="Act I - Musical",
              "Bidirectional Cuelist names did not persist to the project");
    });

    test("Cuelist tabs stay consecutive and inline until the real width limit",[] {
        MainComponent ui(false);
        check(ui.show.cuelists.size()==1 &&
              ui.show.cuelists[0].name=="Cuelist 1","Default tab name incorrect");
        for (int i=2;i<=9;++i) {
            ui.createCuelist();
            check(ui.show.cuelists.back().name=="Cuelist "+juce::String(i),
                  "Cuelist numbering skipped a value");
        }
        ui.setSize(1647,895);
        ui.resized();
        int previousX=-1;
        for (int i=0;i<9;++i) {
            auto* tab=controlsV3::named(ui,"cuelistTab"+juce::String(i));
            auto* close=controlsV3::named(ui,"deleteCuelist"+juce::String(i));
            check(tab && close && tab->isVisible() && close->isVisible(),
                  "Every one of nine Cuelists must have a real tab and a close button");
            check(tab->getX()>previousX,"Cuelist tabs did not appear side by side");
            previousX=tab->getX();
        }
        auto* overflow=controlsV3::named(ui,"cuelistOverflow");
        check(overflow && !overflow->isVisible(),
              "Overflow menu appeared despite room for nine inline tabs");
        ui.setSize(1180,820);
        ui.resized();
        check(overflow->isVisible(),"Narrow windows must retain access to hidden Cuelists");
        check(controlsV3::named(ui,"cuelistTab8")->isVisible(),
              "Currently active Cuelist must remain visible when tabs overflow");
        ui.activateCuelist(0);
        check(controlsV3::named(ui,"cuelistTab0")->isVisible() &&
              controlsV3::named(ui,"cuelistTab1")->isVisible(),
              "Overflow scrolling skipped sequential Cuelists");
    });

    test("Deleting Cuelists protects data and reuses the first free number",[] {
        MainComponent ui(false);
        ui.createCuelist();
        ui.createCuelist();
        check(ui.show.cuelists.size()==3 && ui.show.activeCuelist==2,
              "Test could not create three Cuelists");
        const auto originalId=ui.show.cuelists[0].rows[0].id;
        const auto deletedId=ui.show.cuelists[1].rows[0].id;
        const auto deletedCellId=ui.show.cuelists[1].rows[0].cells[0].id;
        ui.deleteCuelist(1);
        check(ui.show.cuelists.size()==2 &&
              ui.show.cuelists[0].name=="Cuelist 1" &&
              ui.show.cuelists[1].name=="Cuelist 3" &&
              ui.show.activeCuelist==1 &&
              ui.show.rows[0].id==ui.show.cuelists[1].rows[0].id,
              "Delete did not update active index and preserve other Cuelists");
        check(ui.show.cuelists[0].rows[0].id==originalId &&
              ui.show.cuelists[0].rows[0].id!=deletedId &&
              ui.show.cuelists[0].rows[0].cells[0].id!=deletedCellId,
              "Deleting a Cuelist damaged surviving cue identities");
        ui.createCuelist();
        check(ui.show.cuelists.back().name=="Cuelist 2",
              "A deleted numbered slot was not reused");
        ui.deleteCuelist(ui.show.activeCuelist);
        check(ui.show.cuelists.size()==2 &&
              ui.show.activeCuelist>=0 &&
              ui.show.activeCuelist<(int)ui.show.cuelists.size() &&
              validateShow(ui.show).wasOk(),
              "Deleting the active Cuelist left the show invalid");
        ui.deleteCuelist(1);
        const auto copy=encodeShow(ui.show);
        ui.deleteCuelist(0);
        check(ui.show.cuelists.size()==1 && encodeShow(ui.show)==copy,
              "The final remaining Cuelist must not be deletable");
        Show restored;
        check(decodeShow(copy,restored).wasOk(),
              "Cuelist deletion left an invalid persisted project");
    });
    test("Historical projects without notes or Cuelists remain readable", [] {
        auto root=juce::JSON::parse(encodeShow(makeShow()));
        root.getDynamicObject()->removeProperty("cuelists");
        root.getDynamicObject()->removeProperty("activeCuelist");
        root["rows"].getArray()->getReference(0).getDynamicObject()->removeProperty("notes");
        Show restored;
        check(decodeShow(juce::JSON::toString(root),restored).wasOk(),
              "Original v10 project could no longer load");
        check(restored.cuelists.size()==1 && restored.activeCuelist==0 &&
              restored.rows[0].notes.isEmpty(),
              "Missing historical notes were not initialized empty");
    });

    test("Collapse-all and toolbar are layout-only across supported window sizes", [] {
        MainComponent ui(false);
        addColumn(ui.show,ColumnType::midi,"MIDI");
        addColumn(ui.show,ColumnType::osc,"OSC");
        ui.setAllColumnsCollapsed(true);
        for (const auto& c:ui.show.columns)
            check(c.collapsed,"Collapse-all missed a column");
        ui.setAllColumnsCollapsed(false);
        for (const auto& c:ui.show.columns)
            check(!c.collapsed,"Expand-all missed a column");
        auto* toggle=controlsV3::named(ui,"toggleAllColumns");
        auto* headers=cueViewFixtures::find<PinnedCueHeaders>(ui);
        auto* notes=controlsV3::named(ui,"cueNotes");
        auto* mapping=controlsV3::named(ui,"controlMapping");
        auto* plus=controlsV3::named(ui,"newCuelist");
        check(toggle && headers && notes && mapping && plus,
              "Mockup controls are missing");
        for (const auto window:std::initializer_list<juce::Point<int>>{
                 {1180,820},{1440,900},{1647,895},{1920,1080}}) {
            ui.setSize(window.x,window.y);
            ui.resized();
            // Keep the button inside the actual pinned header. Its width
            // changed when the cue-name lane grew to fit Play/Stop safely.
            check(toggle->isVisible() &&
                  toggle->getX()>=headers->getX() &&
                  toggle->getRight()<=headers->getRight() &&
                  toggle->getY()>=headers->getY() &&
                  toggle->getBottom()<=headers->getY()+CueGrid::headerHeight,
                  "Collapse-all escaped the Cue List header");
            check(mapping->isVisible() && mapping->getX()>=0 &&
                  mapping->getRight()<=180,
                  "Control Mapping overlapped the show toolbar");
            check(notes->getHeight()>=38 && notes->getRight()<=window.x &&
                  notes->getBottom()<window.y-30,
                  "Cue notes escaped the Inspector on a compact window");
            check(plus->getRight()<window.x-650,
                  "Cuelist tabs overlapped the clock");
        }
        auto* plusButton=dynamic_cast<juce::TextButton*>(plus);
        check(plusButton!=nullptr,"Cuelist plus must be a real button");
        click(*plusButton,12.f,12.f);
        check(ui.show.cuelists.size()==2 && ui.show.activeCuelist==1,
              "Top-bar plus did not create and select a Cuelist");
    });

    test("Multiple Cuelists preserve inactive audio on actual disk save and reload",[] {
        auto directory=juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("cuelist-multi-test-"+juce::Uuid().toString());
        check(directory.createDirectory().wasOk(),"Cannot create temporary test folder");
        const auto first=directory.getChildFile("pres.mp3");
        const auto second=directory.getChildFile("mus.mp3");
        check(first.replaceWithText("audio fixture 1") &&
              second.replaceWithText("audio fixture 2"),
              "Cannot create temporary media files");
        auto show=makeShow();
        show.cuelists[0].name="Preshow";
        show.rows[0].cells[0].file=first.getFullPathName();
        show.rows[0].notes="First notes";
        Cuelist secondList;
        secondList.name="Musical";
        Row other;other.id=show.nextId++;other.notes="Second notes";
        Cell cell;cell.id=show.nextId++;cell.file=second.getFullPathName();
        other.cells.push_back(cell);secondList.rows.push_back(other);
        show.cuelists.push_back(secondList);
        const auto target=directory.getChildFile("performance.cuelist");
        check(saveShow(show,target).wasOk(),"Multi-Cuelist save failed");
        Show loaded;
        check(loadShow(target,loaded).wasOk(),"Multi-Cuelist load failed");
        check(loaded.cuelists.size()==2 &&
              loaded.cuelists[0].name=="Preshow" &&
              loaded.cuelists[1].name=="Musical" &&
              loaded.rows[0].notes=="First notes" &&
              loaded.cuelists[1].rows[0].notes=="Second notes",
              "Cuelist names or notes were lost on disk");
        check(juce::File(loaded.rows[0].cells[0].file).existsAsFile() &&
              juce::File(loaded.cuelists[1].rows[0].cells[0].file).existsAsFile(),
              "Inactive Cuelist media was not copied into the project folder");
        check(directory.deleteRecursively(),"Cannot clean temporary Cuelist test");
    });

    test("Inactive Cuelist audio is decoded with the same PCM budget", [] {
        auto show=makeShow();
        const auto file1=juce::File::getCurrentWorkingDirectory()
                             .getChildFile("mockup-a.wav").getFullPathName();
        const auto file2=juce::File::getCurrentWorkingDirectory()
                             .getChildFile("mockup-b.wav").getFullPathName();
        show.rows[0].cells[0].file=file1;
        Cuelist other;
        other.name="Musical";
        Row row;
        row.id=show.nextId++;
        Cell cell;cell.id=show.nextId++;cell.file=file2;
        row.cells.push_back(cell);other.rows.push_back(row);
        show.cuelists.push_back(other);
        PreparedShowMedia prepared;
        int loads=0;
        const AudioClipLoader loader=[&](const juce::File&,
                   std::shared_ptr<const AudioClip>& output)->juce::Result {
            ++loads;
            auto audio=std::make_shared<AudioClip>();
            audio->samples.setSize(1,128);
            audio->samples.clear();
            output=std::move(audio);
            return juce::Result::ok();
        };
        check(prepareShowMedia(show,prepared,loader).wasOk(),
              "Media preparation for multiple Cuelists failed");
        check(loads==2 && prepared.clips.size()==2,
              "Media belonging to the inactive Cuelist was not loaded");
        auto corrupt=show;
        corrupt.cuelists[1].rows[0].id=show.rows[0].id;
        check(validateShow(corrupt).failed(),
              "Overlapping cue identities across Cuelists went undetected");
    });
}
