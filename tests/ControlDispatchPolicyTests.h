#pragma once
#include "ControlDispatchPolicy.h"
#include <cmath>

void controlDispatchPolicyTests() {
    test("physical control edges use device/channel/number rather than mapping identity",[] {
        PhysicalControlEdges edges;
        auto press=ControlEvent{"pedal",ControlKind::note,1,64,127,false};
        auto release=ControlEvent{"pedal",ControlKind::note,1,64,0,true};
        check(!edges.recordAndWasDown(press),"First GO note must be a rising edge");
        check(edges.recordAndWasDown(press),"Held GO must not generate a second edge");
        check(edges.recordAndWasDown(release),"Release must observe previous held state");
        check(!edges.recordAndWasDown(press),"New GO press after release not detected");
        auto otherDevice=press; otherDevice.deviceId="another pedal";
        auto otherChannel=press; otherChannel.channel=2;
        auto otherNumber=press; otherNumber.number=65;
        auto cc=press;cc.kind=ControlKind::cc;
        check(!edges.recordAndWasDown(otherDevice) &&
              !edges.recordAndWasDown(otherChannel) &&
              !edges.recordAndWasDown(otherNumber) &&
              !edges.recordAndWasDown(cc),
              "Independent physical inputs shared a debounce state");
        edges.clear();
        check(!edges.recordAndWasDown(press),"Disconnected device kept stale held state");
    });

    test("unmatched releases clear hardware state across cue and binding changes",[] {
        PhysicalControlEdges edges;
        const auto press=ControlEvent{"control surface",ControlKind::note,4,90,127,false};
        const auto release=ControlEvent{"control surface",ControlKind::note,4,90,0,true};
        check(!edges.recordAndWasDown(press),"Initial GO input was already down");
        // The current cue has switched mapping sets. No binding matches
        // this release, but it must still be recorded.
        check(edges.recordAndWasDown(release),"Unmatched release did not see held input");
        check(!edges.recordAndWasDown(press),
              "A re-armed cue failed to accept the next legitimate GO edge");
        auto cc=ControlEvent{"fader",ControlKind::cc,2,7,80,false};
        check(!edges.recordAndWasDown(cc) && edges.recordAndWasDown(cc),
              "Above-midpoint CC threshold did not latch");
        cc.value=63;
        check(edges.recordAndWasDown(cc),"Crossing below CC press threshold lost held state");
        cc.value=127;
        check(!edges.recordAndWasDown(cc),"Rearmed CC did not generate an edge");
    });

    test("pickup current-target normalization honors curve/invert and stable ID",[] {
        auto show=makeShow();
        addColumn(show,ColumnType::audio,"Other audio");
        const int lastAudio=show.columns[1].id;
        show.masterGain=0.6f;
        show.masterPan=-0.5f;
        show.columns[1].gain=0.8f;
        show.columns[1].pan=-0.5f;
        ControlBinding binding;
        binding.minimum=0.2f; binding.maximum=1.0f;
        binding.target=ControlTarget::masterGain;
        auto val=mappedPickupTarget(show,binding);
        check(val && std::abs(*val-0.5f)<0.00001f,
              "Master gain pickup normalization is wrong");
        binding.invert=true;
        binding.curve=ControlCurve::easeIn;
        val=mappedPickupTarget(show,binding);
        check(val && std::abs(*val-(1.0f-std::sqrt(0.5f)))<0.00001f,
              "Response curve/inversion was not reversed before pickup");
        binding.invert=false;
        binding.curve=ControlCurve::linear;
        binding.target=ControlTarget::columnGain;
        binding.targetColumnId=lastAudio;
        val=mappedPickupTarget(show,binding);
        check(val && std::abs(*val-0.75f)<0.00001f,
              "Column pickup did not use the stable destination ID");
        binding.target=ControlTarget::columnPan;
        binding.minimum=-1;binding.maximum=1;
        val=mappedPickupTarget(show,binding);
        check(val && std::abs(*val-0.25f)<0.00001f,
              "Column pan pickup normalization is incorrect");
        binding.target=ControlTarget::masterPan;
        val=mappedPickupTarget(show,binding);
        check(val && std::abs(*val-0.25f)<0.00001f,
              "Master pan pickup normalization is incorrect");
        binding.target=ControlTarget::columnGain;
        binding.targetColumnId=999999;
        check(!mappedPickupTarget(show,binding),
              "Missing column produced an invented pickup target");
        binding.target=ControlTarget::go;
        check(!mappedPickupTarget(show,binding),
              "GO must not require a slider pickup value");
        binding.target=ControlTarget::masterGain;
        binding.minimum=1;binding.maximum=1;
        check(!mappedPickupTarget(show,binding),
              "Zero-span pickup incorrectly produced a normalization");
    });

    test("routing selectors reject wrong types and preserve destinations after column reorder",[] {
        auto show=makeShow();
        addColumn(show,ColumnType::midi,"MIDI output");
        addColumn(show,ColumnType::osc,"OSC output");
        addColumn(show,ColumnType::control,"Controller");
        const int midiId=show.columns[1].id;
        const int oscId=show.columns[2].id;
        const int audioId=show.columns[0].id;
        check(mappedTargetColumn(show,midiId,ColumnType::midi)==1 &&
              mappedTargetColumn(show,oscId,ColumnType::osc)==2 &&
              mappedTargetColumn(show,audioId,ColumnType::audio)==0,
              "Audio/MIDI/OSC mapping did not find correct targets");
        check(mappedTargetColumn(show,midiId,ColumnType::osc)==-1 &&
              mappedTargetColumn(show,oscId,ColumnType::audio)==-1 &&
              mappedTargetColumn(show,999999,ColumnType::midi)==-1,
              "Dispatch policy accepted wrong or deleted destination");
        check(swapCueColumns(show,1,2).wasOk(),"Could not reorder MIDI and OSC outputs");
        check(mappedTargetColumn(show,midiId,ColumnType::midi)==2 &&
              mappedTargetColumn(show,oscId,ColumnType::osc)==1,
              "Mapping followed a visual index instead of the stable column ID");
    });

    test("mapped gain deadband and MIDI CC quantization match the original dispatch",[] {
        check(!shouldApplyMappedValue(0.5f,0.503f) &&
              shouldApplyMappedValue(0.5f,0.506f),
              "Mapped fader deadband changed");
        check(mappedMidiCcValue(-20)==0 &&
              mappedMidiCcValue(0)==0 &&
              mappedMidiCcValue(63.49f)==63 &&
              mappedMidiCcValue(63.51f)==64 &&
              mappedMidiCcValue(127)==127 &&
              mappedMidiCcValue(500)==127,
              "Mapped MIDI CC rounding or clamping changed");
    });
}
