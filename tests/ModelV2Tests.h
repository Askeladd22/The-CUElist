#pragma once
#include <limits>

void modelV2Tests() {
    test("blank show and v2 cue controls survive serialization", [] {
        auto show = makeShow();
        check(show.rows.size()==1 && show.columns.size()==1, "Initial show is not 1x1");
        check(show.rows[0].name.isEmpty() && show.columns[0].name.isEmpty() && show.rows[0].cells[0].name.isEmpty(), "Demo names remain");
        auto& row = show.rows[0]; row.gain=3; row.muted=true; row.loop=true; row.overrideBehaviour=true; row.behaviour=Behaviour::continuePlaying;
        row.cells[0].startSeconds=12.5; row.cells[0].gain=4;
        Show loaded; check(decodeShow(encodeShow(show),loaded).wasOk(), "v2 roundtrip failed");
        near(loaded.rows[0].gain,3); near(loaded.rows[0].cells[0].gain,4); near(loaded.rows[0].cells[0].startSeconds,12.5);
        check(loaded.rows[0].muted && loaded.rows[0].loop && loaded.rows[0].overrideBehaviour, "Cue flags lost");
        check(loaded.rows[0].behaviour==Behaviour::continuePlaying, "Cue behaviour lost");
    });
    test("version 1 migrates omitted controls to neutral defaults", [] {
        auto root = juce::JSON::parse(encodeShow(makeShow())); root.getDynamicObject()->setProperty("version",1);
        auto row = root["rows"][0];
        for (auto key : {"gain","muted","loop","overrideBehaviour","behaviour"}) row.getDynamicObject()->removeProperty(key);
        row["cells"][0].getDynamicObject()->removeProperty("startSeconds");
        Show loaded; check(decodeShow(juce::JSON::toString(root),loaded).wasOk(), "v1 migration failed");
        near(loaded.rows[0].gain,1); near(loaded.rows[0].cells[0].startSeconds,0);
        check(!loaded.rows[0].muted && !loaded.rows[0].loop && !loaded.rows[0].overrideBehaviour, "Migration enabled cue overrides");
    });
    test("bad offsets gains and relative traversal never replace destination", [] {
        for (const auto value : {-1.0,86401.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
            auto show=makeShow(); show.rows[0].cells[0].startSeconds=value;
            check(validateShow(show).failed(), "Bad offset accepted");
        }
        for (const auto value : {-1.0f,4.1f,std::numeric_limits<float>::infinity()}) {
            auto show=makeShow(); show.rows[0].gain=value; check(validateShow(show).failed(), "Bad row gain accepted");
            show.rows[0].gain=1; show.rows[0].cells[0].gain=value; check(validateShow(show).failed(), "Bad cell gain accepted");
        }
        for (auto path : {"../escape.wav","media/../../escape.wav","media\\..\\escape.wav","media/file.wav:stream"}) {
            auto show=makeShow(); show.rows[0].cells[0].file=path; Show loaded; loaded.name="Keep";
            check(decodeShow(encodeShow(show),loaded).failed(), "Traversal accepted"); check(loaded.name=="Keep", "Failed decode mutated destination");
        }
        auto root=juce::JSON::parse(encodeShow(makeShow())); root["rows"][0]["cells"][0].getDynamicObject()->setProperty("startSeconds","bad");
        Show loaded; check(decodeShow(juce::JSON::toString(root),loaded).failed(), "Text offset accepted");
    });
    test("moving cells swaps their identities and rejects invalid targets", [] {
        auto show=makeShow(); addRow(show,{}); addColumn(show,ColumnType::audio,{}); addColumn(show,ColumnType::midi,{});
        auto& a=show.rows[0].cells[0]; auto& b=show.rows[1].cells[1];
        const auto aid=a.id, bid=b.id; a.name="One"; b.name="Two"; a.startSeconds=5; b.loop=true;
        check(moveCell(show,0,0,1,1).wasOk(), "Swap failed");
        check(a.id==bid && b.id==aid && a.name=="Two" && b.name=="One" && a.loop, "Swap lost identity or data"); near(b.startSeconds,5);
        const auto before=encodeShow(show);
        check(moveCell(show,0,0,0,2).failed(), "Incompatible column accepted");
        check(moveCell(show,-1,0,0,0).failed() && moveCell(show,0,0,99,0).failed(), "Invalid index accepted");
        check(encodeShow(show)==before, "Failed move changed show"); check(validateShow(show).wasOk(), "Swap broke model");
    });
    test("portable media relocation backup and failed save preserve project", [] {
        const auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("cuelist-model-"+juce::Uuid().toString());
        check(folder.createDirectory().wasOk(), "Cannot create temp fixture");
        struct Cleanup { juce::File folder; ~Cleanup() { folder.deleteRecursively(); } } cleanup {folder};
        const auto original=folder.getChildFile("original"), relocated=folder.getChildFile("relocated");
        check(original.createDirectory().wasOk(), "Cannot create project folder");
        const auto sourceA=folder.getChildFile("a").getChildFile("same.wav"), sourceB=folder.getChildFile("b").getChildFile("same.wav");
        check(sourceA.getParentDirectory().createDirectory().wasOk() && sourceB.getParentDirectory().createDirectory().wasOk(), "Cannot create sources");
        check(sourceA.replaceWithText("audio-one") && sourceB.replaceWithText("audio-two"), "Cannot write media fixtures");
        auto show=makeShow(); addRow(show,{}); show.rows[0].cells[0].file=sourceA.getFullPathName(); show.rows[1].cells[0].file=sourceB.getFullPathName();
        const auto project=original.getChildFile("portable.cuelist");
        check(saveShow(show,project).wasOk(), "First portable save failed");
        check(show.rows[0].cells[0].file==sourceA.getFullPathName(), "Save changed live model");
        Show serialized; check(decodeShow(project.loadFileAsString(),serialized).wasOk(), "Portable JSON rejected");
        check(!juce::File::isAbsolutePath(serialized.rows[0].cells[0].file), "Saved absolute media path");
        check(serialized.rows[0].cells[0].file!=serialized.rows[1].cells[0].file, "Same basename collided");
        show.name="Second"; check(saveShow(show,project).wasOk(), "Second save failed");
        const auto backup=project.getSiblingFile("portable.cuelist.bak");
        const auto projectBefore=project.loadFileAsString(), backupBefore=backup.loadFileAsString();
        show.rows[1].cells[0].file=folder.getChildFile("missing.wav").getFullPathName();
        check(saveShow(show,project).failed(), "Missing source save succeeded");
        check(project.loadFileAsString()==projectBefore && backup.loadFileAsString()==backupBefore, "Failed save modified project or backup");
        check(original.copyDirectoryTo(relocated), "Relocation failed");
        check(sourceA.deleteFile() && sourceB.deleteFile() && original.deleteRecursively(), "Cannot remove original fixtures");
        Show loaded; check(loadShow(relocated.getChildFile("portable.cuelist"),loaded).wasOk(), "Relocated load failed");
        check(juce::File(loaded.rows[0].cells[0].file).loadFileAsString()=="audio-one", "First media unavailable after relocation");
        check(juce::File(loaded.rows[1].cells[0].file).loadFileAsString()=="audio-two", "Second media collided or unavailable");
        check(loadShow(relocated.getChildFile("portable.cuelist.bak"),loaded).wasOk(), "Relocated backup rejected");
        check(loaded.name=="Show 001" && juce::File(loaded.rows[0].cells[0].file).loadFileAsString()=="audio-one", "Backup references broken");
    });
}
