#pragma once
#include "ProjectSafety.h"

void stabilityV7Tests() {
    test("Preflight flags missing files, unavailable MIDI outputs and unverifiable OSC",[] {
        auto show=makeShow();
        auto& audio=show.rows[0].cells[0];
        const auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory)
             .getChildFile("cuelist-preflight-"+juce::Uuid().toString());
        check(dir.createDirectory().wasOk(),"Could not prepare preflight fixture");
        const auto wav=dir.getChildFile("media.wav");
        audio.file=wav.getFullPathName();
        addColumn(show,ColumnType::midi,"Backing synth");
        show.columns[1].midiOutputId="unavailable-test-device";
        addColumn(show,ColumnType::osc,"Lights");
        show.columns[2].oscHost="127.0.0.1";show.columns[2].oscPort=9000;
        const auto report=preflightShow(show,false,juce::StringArray{},false);
        check(report.audioFiles==1 && report.missingFiles==1 &&
              report.criticalCount()>0 && report.warningCount()>0,
              "Preflight failed to report missing media or audio/MIDI output");
        bool udpUnknown=false;
        for(const auto& item:report.issues)
            if(item.category=="OSC/UDP" && item.level==PreflightLevel::info) udpUnknown=true;
        check(udpUnknown,"Preflight incorrectly claimed UDP receiver connected");
        check(report.describe().contains("CRITICAL") &&
              report.describe().contains("receiver"),"Diagnostic summary omitted safety caveat");
        check(wav.replaceWithText("not a real WAV but an existing file"),
              "Could not make mock file");
        const auto after=preflightShow(show,true,{"unavailable-test-device"},false);
        check(after.missingFiles==0 && after.criticalCount()==0,
              "Preflight reported existing file as missing");
        dir.deleteRecursively();
    });
    test("Recovery saves atomic independent snapshots without touching the real project",[] {
        auto show=makeShow();
        const auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory)
             .getChildFile("cuelist-recovery-"+juce::Uuid().toString());
        check(dir.createDirectory().wasOk(),"Could not create test recovery dir");
        const auto userFile=dir.getChildFile("original.cuelist");
        check(userFile.replaceWithText("original project, not a recovery checkpoint"),
              "Could not write original sentinel");
        show.name="Recovered work";
        check(ProjectRecovery::checkpoint(show,dir).wasOk(),"Could not checkpoint");
        check(ProjectRecovery::exists(dir),"Checkpoint file missing");
        show.name="Modified again";
        check(ProjectRecovery::checkpoint(show,dir).wasOk(),"Could not replace checkpoint atomically");
        Show result;
        check(ProjectRecovery::restore(dir,result).wasOk() &&
              result.name=="Modified again","Recovery did not restore latest valid snapshot");
        check(userFile.loadFileAsString()=="original project, not a recovery checkpoint",
              "Recovery overwrote an existing user project");
        check(ProjectRecovery::discard(dir) && !ProjectRecovery::exists(dir),
              "Recovery discard left files behind");
        dir.deleteRecursively();
    });
    test("Corrupt recovery never replaces a valid destination and relative media is rejected",[] {
        const auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory)
             .getChildFile("cuelist-recovery-corrupt-"+juce::Uuid().toString());
        check(dir.createDirectory().wasOk(),"Could not create recovery folder");
        auto show=makeShow();show.rows[0].cells[0].file="relative.wav";
        check(ProjectRecovery::checkpoint(show,dir).failed(),
              "Unsafe relative media was accepted in a recovery checkpoint");
        const auto file=ProjectRecovery::snapshotFile(dir);
        check(file.replaceWithText("{not valid"),"Could not write corrupt checkpoint fixture");
        Show destination=makeShow();destination.name="Do not replace";
        check(ProjectRecovery::restore(dir,destination).failed() &&
              destination.name=="Do not replace",
              "Corrupt recovery discarded the existing in-memory show");
        dir.deleteRecursively();
    });
    test("Sustained offline callback stress preserves finite output and bounded commands",[] {
        auto show=makeShow();
        AudioEngine engine;
        engine.publish(makeRenderState(show,ClipMap{}));
        std::array<float,128> left{},right{};
        float* channels[]={left.data(),right.data()};
        for(int n=0;n<3000;++n) {
            if(n%17==0) engine.send(show.rows[0].id,true,false);
            if(n%31==0) engine.seekRow(show.rows[0].id,n*.01);
            engine.processBlock(channels,2,(int)left.size(),48000.0);
            for(int i=0;i<128;++i)
                check(std::isfinite(left[(size_t)i]) && std::isfinite(right[(size_t)i]),
                      "Nonfinite sample after repeated offline GO/seek operations");
        }
        engine.panic();
        engine.processBlock(channels,2,128,48000.0);
        check(engine.activeVoices.load()==0,
              "Panic left voices running after offline stress");
    });
}
