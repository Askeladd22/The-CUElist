#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <optional>
#include "MidiClip.h"
#include "OscClip.h"
#include "ControlMapping.h"

namespace cuelist {
enum class ColumnType { audio, midi, osc, control };
enum class Behaviour { retrigger, pauseRestart, pauseResume, continuePlaying, stop };
struct TempoPoint { double beat = 0, bpm = 120; int numerator = 4, denominator = 4; };
struct Column {
    int id = 0; juce::String name; ColumnType type = ColumnType::audio; int output = 0;
    float gain = 1.0f, pan = 0.0f;
    bool muted = false, phaseInverted = false;
    juce::String midiOutputId;
    juce::String oscHost="127.0.0.1";
    int oscPort=9000;
    bool collapsed=false; // visual-only: all clips and routes stay intact
    bool controlLocked=false;
    int controlPriority=2; // 1=Low (locked), 2=Medium, 3=High, 4=Maximum
    ControlSet controlDefaultSet;
    int controlDefaultSourceRowId=0; // original cue for lossless unlock
    // Audio-only routing; independent from the physical direct output pair.
    bool masterSend=true;
    bool directOutputEnabled=false;
    bool directPostFader=true;
};
struct Cell {
    int id = 0;
    juce::String name, file;
    bool followsRow = true, loop = false;
    // Participates in the next cue's automatic stop, only if the containing row is armed.
    // True is the legacy behaviour (all cells follow the row-level Auto Stop policy).
    bool stopOnNext = true;
    float gain = 1.0f;
    double startSeconds = 0;
    Behaviour behaviour = Behaviour::retrigger;
    std::optional<MidiClip> midi;
    std::optional<OscClip> osc;
    std::optional<ControlSet> controlSet;
};
struct Row {
    int id = 0; juce::String name; juce::String notes; std::vector<TempoPoint> tempo {{}}; std::vector<Cell> cells;
    float gain = 1.0f;
    int mappingPresetId=0;
    int colour=0; // 0=automatic; 1..7 are operator cue colours
    bool muted = false, loop = false, overrideBehaviour = false, stopOnNext = true;
    Behaviour behaviour = Behaviour::retrigger;
};
// Cuelists share output columns, mix routing and control configuration, while
// their cue order, content and technical notes are independent.
struct Cuelist {
    juce::String name="Cuelist 1";
    std::vector<Row> rows;
};
struct Show {
    juce::String name = "Show 001";
    float masterGain=1.0f, masterPan=0.0f;
    bool masterMuted=false;
    int masterOutput=0;
    bool safetyLimiterEnabled=true;
    int nextId = 1;
    std::vector<Column> columns;
    std::vector<Row> rows; // currently selected Cuelist; operational source of truth
    ControlSettings controls;
    std::vector<Cuelist> cuelists;
    int activeCuelist=0;
};
Show makeShow();
void addRow(Show&, const juce::String&);
void addColumn(Show&, ColumnType, const juce::String&);
double secondsAtBeat(const std::vector<TempoPoint>&, double beat);
juce::Result validateShow(const Show&);
juce::String encodeShow(const Show&);
juce::Result decodeShow(const juce::String&, Show&);
juce::Result saveShow(const Show&, const juce::File&);
juce::Result loadShow(const juce::File&, Show&);
juce::Result moveCell(Show&, int sourceRow, int sourceColumn, int targetRow, int targetColumn);
}
