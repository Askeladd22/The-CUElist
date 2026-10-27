#include "ShowModel.h"
#include "MainComponent.h"
#include <cmath>
#include <iostream>
#include <functional>
#include <stdexcept>
using namespace cuelist;
int failures = 0, checks = 0;
void check(bool condition, const char* message) { ++checks; if (!condition) throw std::runtime_error(message); }
void near(double actual, double expected) { check(std::abs(actual - expected) < 0.00001, "Unexpected numeric result"); }
void test(const char* name, const std::function<void()>& body) {
    try { body(); std::cout << "PASS " << name << '\n'; }
    catch (const std::exception& e) { ++failures; std::cerr << "FAIL " << name << ": " << e.what() << '\n'; }
}
#include "AudioTests.h"
#include "ModelV2Tests.h"
#include "CueViewTests.h"
#include "ModelV3Tests.h"
#include "TimelineV3Tests.h"
#include "ControlsV3Tests.h"
#include "EngineV4Tests.h"
#include "ControlsV4Tests.h"
#include "PianoRollTests.h"
#include "MidiTests.h"
#include "OscTests.h"
#include "OscAutomationV9Tests.h"
#include "AutoStopV7Tests.h"
#include "MixerV8Tests.h"
#include "GainMeterFaderTests.h"
#include "UiRefinementTests.h"
#include "RowActionsUiTests.h"
#include "ControlMappingV10Tests.h"
#include "ControlColumnTests.h"
#include "StabilityV7Tests.h"
#include "ArchitectureV8Tests.h"
int main() {
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    pianoRollTests();
    registerMidiTests();
    oscTests();
    oscAutomationV9Tests();
    autoStopV7Tests();
    mixerV8Tests();
    gainMeterFaderTests();
    uiRefinementTests();
    rowActionsUiTests();
    controlMappingV10Tests();
    controlColumnTests();
    stabilityV7Tests();
    architectureV8Tests();
    test("MIDI columns accept file drops and enable their importer", [] {
        MainComponent ui(false); addColumn(ui.show,ColumnType::midi,{}); ui.select(0,1);
        auto* grid=cueViewFixtures::find<CueGrid>(ui);
        check(grid->isInterestedInFileDrag({"notes.mid"}),"MIDI file drop rejected");
        bool importer=false;
        for (auto* child:ui.getChildren()) if (auto* button=dynamic_cast<juce::TextButton*>(child))
            if (button->getButtonText()=="Import MIDI") importer=button->isEnabled();
        check(importer,"MIDI importer unavailable");
    });
    test("tempo changes integrate seconds, independent of other rows", [] {
        near(secondsAtBeat({{0,120,4,4},{4,60,3,4}}, 6), 4);
        near(secondsAtBeat({{0,60,4,4}}, 6), 6);
    });
    test("new rows and columns stay rectangular with unique cell identities", [] {
        auto show = makeShow(); addRow(show, "Extra"); addColumn(show, ColumnType::audio, "FX");
        check(show.rows.size() >= 2, "Rows absent");
        for (const auto& row : show.rows) check(row.cells.size() == show.columns.size(), "Ragged row");
        check(validateShow(show).wasOk(), "Invalid generated show");
    });
    test("project roundtrip preserves path, behaviour, participation, tempo and routing", [] {
        auto show = makeShow();
        check(!show.rows.empty(), "No initial rows");
        auto& cell = show.rows[0].cells[0]; cell.file = "C:/Audio/test.wav"; cell.loop = true; cell.followsRow = false;
        cell.behaviour = Behaviour::pauseResume; show.rows[0].tempo.push_back({8,95,7,8}); show.columns[0].output = 2;
        Show restored; check(decodeShow(encodeShow(show), restored).wasOk(), "Roundtrip rejected");
        check(restored.rows[0].cells[0].file == cell.file, "Path lost");
        check(!restored.rows[0].cells[0].followsRow && restored.rows[0].cells[0].loop, "Flags lost");
        check(restored.rows[0].cells[0].behaviour == Behaviour::pauseResume, "Behaviour lost");
        near(restored.rows[0].tempo[1].bpm, 95); check(restored.columns[0].output == 2, "Routing lost");
    });
    test("malformed and future projects are rejected without replacing current show", [] {
        Show show; show.name = "Keep me";
        for (auto json : {"{", "{}", "{\"version\":99}", "{\"version\":1,\"rows\":null}"}) {
            check(decodeShow(json, show).failed(), "Bad project accepted");
            check(show.name == "Keep me", "Existing show replaced");
        }
    });
    test("invalid tempo and duplicate identity are rejected", [] {
        auto show = makeShow(); check(!show.rows.empty(), "No initial rows");
        show.rows[0].tempo[0].bpm = 0; check(validateShow(show).failed(), "Zero bpm accepted");
        show.rows[0].tempo[0].bpm = 120;
        show.rows[0].cells[0].id = show.rows[0].id; check(validateShow(show).failed(), "Duplicate identity accepted");
    });
    audioTests();
    modelV2Tests();
    cueViewTests();
    modelV3Tests();
    timelineV3Tests();
    controlsV3Tests();
    engineV4Tests();
    controlsV4Tests();
    test("inspector activation menu and dB fader control live PCM without resetting cursor", [] {
        MainComponent ui(false); ui.engine.connected.store(true);
        auto clip=std::make_shared<AudioClip>(); clip->sampleRate=4; clip->samples.setSize(1,20);
        for (int i=0;i<20;++i) clip->samples.setSample(0,i,0.01f*(i+1));
        ui.clips[ui.show.rows[0].cells[0].id]=clip; ui.engine.publish(makeRenderState(ui.show,ui.clips)); ui.select(0,0);
        juce::ComboBox* behaviour=nullptr; juce::Slider* gain=nullptr;
        for (auto* child : ui.getChildren()) {
            if (auto* combo=dynamic_cast<juce::ComboBox*>(child)) if (combo->getItemText(0)=="Retrigger") behaviour=combo;
            if (child->getName()=="cellGainDb") gain=dynamic_cast<juce::Slider*>(child);
        }
        check(behaviour && gain,"Audio inspector controls absent");
        auto sample=[&] { float value=0; float* out[]{&value}; ui.engine.collectRetired(); ui.engine.processBlock(out,1,1,4); return value; };
        ui.triggerCell(); near(sample(),0.01);
        gain->setValue(-6,juce::sendNotificationSync); const auto level=std::pow(10.0,-6.0/20.0); near(sample(),0.02*level);
        behaviour->setSelectedId(3,juce::sendNotificationSync); ui.triggerCell(); near(sample(),0);
        ui.triggerCell(); near(sample(),0.03*level);
        behaviour->setSelectedId(4,juce::sendNotificationSync); ui.triggerCell(); near(sample(),0.04*level);
        check(behaviour->getNumItems()==4,"Removed Stop remains in menu");
        ui.stopCellAt(0,0); near(sample(),0);
        behaviour->setSelectedId(1,juce::sendNotificationSync); ui.triggerCell(); near(sample(),0.01*level);
        behaviour->setSelectedId(2,juce::sendNotificationSync); ui.triggerCell(); near(sample(),0);
        ui.triggerCell(); near(sample(),0.01*level);
    });
    test("Enter commits names and GO advances without wrapping", [] {
        MainComponent ui(false); addRow(ui.show,{}); ui.engine.connected.store(true);
        for (auto* child : ui.getChildren()) if (child->getName()=="cueName") {
            auto* editor=dynamic_cast<juce::TextEditor*>(child); editor->setText("Scena",false); editor->onReturnKey();
        }
        check(ui.show.rows[0].name=="Scena","Enter did not commit");
        ui.goNext(); check(ui.nextCueIndex()==1,"First GO did not advance");
        ui.goNext(); check(ui.nextCueIndex()==2 && ui.selectedRow==1,"Second GO did not advance");
        ui.goNext(); check(ui.nextCueIndex()==2,"Sequence wrapped");
        ui.select(0,0); check(ui.nextCueIndex()==0,"Selection did not rearm");
    });
    test("dB slider updates model immediately and structural controls preserve shape", [] {
        MainComponent ui(false);
        for (auto* child : ui.getChildren()) if (child->getName()=="cellGainDb") {
            auto* slider=dynamic_cast<juce::Slider*>(child); slider->setValue(-6,juce::sendNotificationSync);
        }
        near(ui.show.rows[0].cells[0].gain,std::pow(10.0,-6.0/20.0));
        ui.setClipStart(0,2.37); near(ui.show.rows[0].cells[0].startSeconds,2.37);
        addRow(ui.show,{}); int id=ui.show.rows[0].id; ui.reorderRow(0,1); check(ui.show.rows[1].id==id,"Row move failed");
        addColumn(ui.show,ColumnType::audio,{}); id=ui.show.columns[0].id; ui.reorderColumn(0,1); check(ui.show.columns[1].id==id,"Column move failed");
        ui.deleteRow(1); ui.deleteRow(0); ui.deleteColumn(1); ui.deleteColumn(0);
        check(validateShow(ui.show).wasOk(),"Deleting final row/column broke model");
    });
    test("inspector drafts count as unsaved work", [] {
        MainComponent ui(false);
        for (auto* child : ui.getChildren()) if (child->getName()=="cueName") {
            auto* edit=dynamic_cast<juce::TextEditor*>(child); check(edit!=nullptr,"Editor absent");
            edit->setText("Nuovo nome",true); // TextEditor change callbacks are asynchronous; query must inspect actual draft.
        }
        check(ui.hasUnsavedChanges(),"Pending inspector edit not tracked");
    });
    test("changing selection commits pending cue name", [] {
        MainComponent ui(false);
        addRow(ui.show, {});
        for (auto* child : ui.getChildren()) if (child->getName()=="cueName") dynamic_cast<juce::TextEditor*>(child)->setText("Nuovo nome",false);
        ui.select(1,0); check(ui.show.rows[0].name=="Nuovo nome","Selection lost pending edit");
    });
    test("Escape in inspector bubbles to the Panic handler", [] {
        MainComponent ui(false); int editors=0;
        for (auto* child : ui.getChildren()) if (auto* edit=dynamic_cast<juce::TextEditor*>(child)) {
            ++editors; check(!edit->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)),"Editor swallowed Escape");
        }
        check(editors==3,"Missing inspector fields");
    });
    test("WAV import and atomic project save work on disk with previous-version backup", [] {
        const auto folder=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("test-artifacts");
        check(folder.createDirectory().wasOk(),"Cannot create fixture directory");
        const auto wave=folder.getChildFile("verification.wav");
        juce::AudioBuffer<float> pcm(2,240000);
        for (int i=0;i<pcm.getNumSamples();++i) {
            const auto fade=std::min(1.0f,std::min(i/2400.0f,(240000-i)/2400.0f));
            pcm.setSample(0,i,0.06f*fade*std::sin((float)(i*2.0*juce::MathConstants<double>::pi*220.0/48000.0)));
            pcm.setSample(1,i,0.06f*fade*std::sin((float)(i*2.0*juce::MathConstants<double>::pi*330.0/48000.0)));
        }
        {
            auto stream=wave.createOutputStream(); check(stream!=nullptr,"No WAV stream");
            stream->setPosition(0); stream->truncate(); juce::WavAudioFormat format;
            std::unique_ptr<juce::OutputStream> outputStream(std::move(stream));
            auto writer=format.createWriterFor(outputStream,juce::AudioFormatWriterOptions().withSampleRate(48000).withNumChannels(2).withBitsPerSample(16));
            check(writer!=nullptr,"No WAV writer"); check(writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()),"WAV write failed");
        }
        std::shared_ptr<const AudioClip> loaded; check(loadClip(wave,loaded).wasOk(),"Valid WAV rejected");
        near(loaded->samples.getNumSamples()/loaded->sampleRate,5); check(loaded->samples.getNumChannels()==2,"Stereo lost");
        auto show=makeShow(); show.rows[0].cells[0].file=wave.getFullPathName(); show.rows[0].cells[0].name="Test stereo 5 secondi";
        const auto project=folder.getChildFile("verification.cuelist"); check(saveShow(show,project).wasOk(),"First save failed");
        show.name="Verifica audio"; check(saveShow(show,project).wasOk(),"Second save failed");
        Show restored; check(decodeShow(project.loadFileAsString(),restored).wasOk(),"Saved file unreadable");
        check(restored.name=="Verifica audio","Wrong saved content");
        check(decodeShow(project.getSiblingFile("verification.cuelist.bak").loadFileAsString(),restored).wasOk(),"Backup unreadable");
        check(restored.name=="Show 001","Backup did not preserve previous version");
        check(saveShow(show,folder.getChildFile("missing-parent").getChildFile("show.cuelist")).failed(),"Invalid destination accepted");
    });
    test("device selector is destroyed before its audio manager", [] {
        juce::Component::SafePointer<juce::Component> dialog;
        {
            MainComponent ui(false);
            for (auto* child : ui.getChildren()) if (auto* button=dynamic_cast<juce::TextButton*>(child))
                if (button->getButtonText()=="Audio devices") button->onClick();
            dialog=juce::ModalComponentManager::getInstance()->getModalComponent(0);
            check(dialog!=nullptr,"Device selector did not open");
        }
        check(dialog==nullptr,"Device selector outlived audio manager");
    });
    if (failures==0) {
        // Native JUCE rendering artifact for visual QA, not a substitute for interaction tests.
        const auto folder=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("test-artifacts");
        MainComponent preview(false); preview.importFile(folder.getChildFile("verification.wav"));
        preview.setSize(1440,900);
        auto stream=folder.getChildFile("interface-preview.png").createOutputStream();
        if (stream) { stream->setPosition(0); stream->truncate(); juce::PNGImageFormat{}.writeImageToStream(preview.createComponentSnapshot(preview.getLocalBounds()),*stream); }
        addColumn(preview.show,ColumnType::midi,"MIDI"); addColumn(preview.show,ColumnType::osc,"OSC");
        cueViewFixtures::find<ColumnUtilities>(preview)->refresh(); preview.setSize(1700,900);
        preview.engine.connected.store(true); preview.triggerCell();
        juce::AudioBuffer<float> visualAudio(2,96000); preview.engine.collectRetired();
        preview.engine.processBlock(visualAudio.getArrayOfWritePointers(),2,96000,48000);
        stream=folder.getChildFile("interface-mixed-progress.png").createOutputStream();
        if (stream) { stream->setPosition(0); stream->truncate(); juce::PNGImageFormat{}.writeImageToStream(preview.createComponentSnapshot(preview.getLocalBounds()),*stream); }
        for (auto* child : preview.getChildren()) if (auto* button=dynamic_cast<juce::TextButton*>(child))
            if (button->getButtonText()=="Entire cue") button->onClick();
        preview.setSize(1180,820);
        stream=folder.getChildFile("interface-row-small.png").createOutputStream();
        if (stream) { stream->setPosition(0); stream->truncate(); juce::PNGImageFormat{}.writeImageToStream(preview.createComponentSnapshot(preview.getLocalBounds()),*stream); }
    }
    std::cout << checks << " assertions, " << failures << " failed tests\n";
    return failures == 0 ? 0 : 1;
}
