#pragma once

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
