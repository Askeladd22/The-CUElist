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
        auto* notes=controlsV3::named(ui,"cueNotes");
        auto* mapping=controlsV3::named(ui,"controlMapping");
        auto* plus=controlsV3::named(ui,"newCuelist");
        check(toggle && notes && mapping && plus,
              "Mockup controls are missing");
        for (const auto window:std::initializer_list<juce::Point<int>>{
                 {1180,820},{1440,900},{1647,895},{1920,1080}}) {
            ui.setSize(window.x,window.y);
            ui.resized();
            check(toggle->isVisible() && toggle->getX()>=182 &&
                  toggle->getRight()<=472 && toggle->getBottom()<=150,
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
