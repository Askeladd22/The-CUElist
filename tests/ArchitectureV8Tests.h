#pragma once
#include "ProjectMediaLoader.h"
#include "ProjectSafety.h"
#include "DeviceIdentity.h"
#include <memory>

void architectureV8Tests() {
    test("Media preparation deduplicates shared audio and enforces the PCM cap atomically",[] {
        auto show=makeShow();
        addRow(show,"Second cue");
        const auto file=juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("reusable-test-audio.wav");
        show.rows[0].cells[0].file=file.getFullPathName();
        show.rows[1].cells[0].file=file.getFullPathName();
        int decoding=0;
        AudioClipLoader mock=[&](const juce::File&,
                                std::shared_ptr<const AudioClip>& output) {
            ++decoding;
            auto media=std::make_shared<AudioClip>();
            media->samples.setSize(2,64);
            output=media;
            return juce::Result::ok();
        };
        PreparedShowMedia result;
        check(prepareShowMedia(show,result,mock,512).wasOk(),
              "Identical media references exceeded the correctly deduplicated limit");
        check(decoding==1 && result.clips.size()==2 && result.pcmBytes==512,
              "Duplicate media was decoded twice or PCM bytes double-counted");
        check(result.clips.at(show.rows[0].cells[0].id)==
              result.clips.at(show.rows[1].cells[0].id),
              "Two cue cells did not share the same immutable PCM object");
        PreparedShowMedia unchanged; unchanged.pcmBytes=1234;
        check(prepareShowMedia(show,unchanged,mock,511).failed() &&
              unchanged.pcmBytes==1234 && unchanged.clips.empty(),
              "Failed preparation mutated the existing prepared media state");
    });
    test("Missing media is reported without rejecting an otherwise valid show",[] {
        auto show=makeShow();
        show.rows[0].cells[0].file=juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("unavailable-test-audio.wav").getFullPathName();
        AudioClipLoader mock=[](const juce::File&,std::shared_ptr<const AudioClip>&) {
            return juce::Result::fail("No decodable media");
        };
        PreparedShowMedia result;
        check(prepareShowMedia(show,result,mock).wasOk() &&
              result.clips.empty() && result.issues.size()==1,
              "Missing-media compatibility no longer reports a recoverable warning");
    });
    test("Background recovery snapshots coalesce and flush without replacing user projects",[] {
        auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("cuelist-async-recovery-"+juce::Uuid().toString());
        check(dir.createDirectory().wasOk(),"Could not create recovery integration folder");
        auto original=dir.getChildFile("original.cuelist");
        check(original.replaceWithText("Keep original project"),"Cannot create original project");
        {
            RecoveryCheckpointWriter worker(dir);
            auto show=makeShow();
            for(int n=0;n<30;++n) {
                show.name="Snapshot "+juce::String(n);
                check(worker.enqueue(show).wasOk(),"Could not queue recovery snapshot");
                // The live object can change immediately after enqueue.
                show.name="Subsequent GUI edit";
            }
            worker.flush();
            Show restored;
            check(worker.failedWrites()==0 && ProjectRecovery::restore(dir,restored).wasOk()
                  && restored.name=="Snapshot 29",
                  "Background recovery did not publish the latest validated snapshot");
        }
        check(original.loadFileAsString()=="Keep original project",
              "Recovery worker overwrote the operator's project");
        check(ProjectRecovery::discard(dir),"Cannot discard completed recovery checkpoint");
        dir.deleteRecursively();
    });
    test("0.9 cancelled media preparation does not expose a partially loaded show",[] {
        auto show=makeShow();
        const auto path=juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("cuelist-cancelled-media.wav");
        show.rows[0].cells[0].file=path.getFullPathName();
        int called=0;
        AudioClipLoader mock=[&](const juce::File&,std::shared_ptr<const AudioClip>& clip) {
            ++called;
            auto result=std::make_shared<AudioClip>();
            result->samples.setSize(2,16);
            clip=std::move(result);
            return juce::Result::ok();
        };
        PreparedShowMedia untouched;
        untouched.pcmBytes=4321;
        check(prepareShowMedia(show,untouched,mock,maxShowPcmBytes,
                    [] {return true;}).failed(),
              "A cancelled prepare was accepted");
        check(untouched.clips.empty() && untouched.pcmBytes==4321 && called==0,
              "Cancelled preparation mutated the caller's loaded media");
    });
    test("0.9 superseded project loads never return stale audio or show metadata",[] {
        const auto directory=juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("cuelist-async-open-"+juce::Uuid().toString());
        check(directory.createDirectory().wasOk(),"Could not create asynchronous test directory");
        const auto oldFile=directory.getChildFile("previous.cuelist");
        const auto latestFile=directory.getChildFile("latest.cuelist");
        auto oldShow=makeShow(), latestShow=makeShow();
        oldShow.name="OLD"; latestShow.name="LATEST";
        const auto fakeMedia=directory.getChildFile("virtual.wav").getFullPathName();
        oldShow.rows[0].cells[0].file=fakeMedia;
        latestShow.rows[0].cells[0].file=fakeMedia;
        check(oldFile.replaceWithText(encodeShow(oldShow)) &&
              latestFile.replaceWithText(encodeShow(latestShow)),
              "Could not write projects for background load");
        std::atomic<int> started{0};
        AudioClipLoader decoder=[&](const juce::File&,std::shared_ptr<const AudioClip>& clip) {
            started.fetch_add(1);
            juce::Thread::sleep(65); // emulate an expensive disk decoder
            auto audio=std::make_shared<AudioClip>();
            audio->samples.setSize(2,16);
            clip=std::move(audio);
            return juce::Result::ok();
        };
        {
            ProjectLoadWorker worker(decoder);
            worker.request(oldFile);
            for(int n=0;n<200 && started.load()==0;++n) juce::Thread::sleep(5);
            check(started.load()>0,"Background worker never began decoding");
            worker.cancel();
            worker.request(latestFile);
            PreparedProject completed;
            bool received=false;
            for(int n=0;n<400 && !received;++n) {
                received=worker.takeCompleted(completed);
                if(!received) juce::Thread::sleep(5);
            }
            check(received && completed.status.wasOk() &&
                  completed.source==latestFile && completed.show.name=="LATEST" &&
                  completed.media.clips.size()==1,
                  "Background worker committed an obsolete/cancelled project");
            check(!worker.takeCompleted(completed),
                  "Background worker emitted a second stale completion");
        }
        directory.deleteRecursively();
    });
    test("MIDI reconnection policy rejects renamed or reordered hardware",[] {
        struct Port {juce::String identifier;};
        std::vector<Port> ports={{"keyboard-2"},{"lighting-desk"}};
        check(hasExactMidiIdentifier(ports,"keyboard-2"),
              "Exact configured MIDI identifier not recognised");
        check(!hasExactMidiIdentifier(ports,"keyboard") &&
              !hasExactMidiIdentifier(ports,"lighting") &&
              !hasExactMidiIdentifier(ports,"") &&
              !hasExactMidiIdentifier(ports,"keyboard-1"),
              "MIDI device identity policy accepted a substitute or absent port");
    });
    test("OSC route reconfiguration dispatches to the new UDP destination only",[] {
        juce::DatagramSocket initial(false),replacement(false);
        check(initial.bindToPort(0)&&replacement.bindToPort(0),
              "Could not reserve two local OSC receivers");
        OscOutputRouter output;
        const int route=output.configure(77,"127.0.0.1",initial.getBoundPort());
        const auto payload=encodeOscMessage(OscEvent{0,"/test/route",{}});
        check(!payload.empty(),"OSC integration payload could not be encoded");
        output.beginBlock(48000,juce::Time::getMillisecondCounterHiRes());
        output.emit(route,payload.data(),(unsigned)payload.size(),0);
        check(initial.waitUntilReady(true,1500)==1,"Initial OSC receiver got no packet");
        char got[1024]{};
        check(initial.read(got,sizeof(got),false)==(int)payload.size() &&
              std::memcmp(got,payload.data(),payload.size())==0,
              "OSC bytes changed before reaching first receiver");
        check(output.configure(77,"127.0.0.1",replacement.getBoundPort())==route,
              "OSC route identity changed unexpectedly");
        output.beginBlock(48000,juce::Time::getMillisecondCounterHiRes());
        output.emit(route,payload.data(),(unsigned)payload.size(),0);
        check(replacement.waitUntilReady(true,1500)==1,
              "OSC output did not follow updated endpoint");
        check(replacement.read(got,sizeof(got),false)==(int)payload.size() &&
              std::memcmp(got,payload.data(),payload.size())==0,
              "Updated OSC destination received damaged packet");
        check(initial.waitUntilReady(true,50)==0,
              "Old OSC receiver received a message after endpoint change");
    });
}
