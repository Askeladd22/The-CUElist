#pragma once
#include <juce_core/juce_core.h>
#include <vector>

namespace cuelist {
enum class ColumnType { audio, midi, osc };
enum class Behaviour { retrigger, restart, pauseResume, continuePlaying, stop };
struct TempoPoint { double beat = 0, bpm = 120; int numerator = 4, denominator = 4; };
struct Column { int id = 0; juce::String name; ColumnType type = ColumnType::audio; int output = 0; };
struct Cell {
    int id = 0;
    juce::String name, file;
    bool followsRow = true, loop = false;
    float gain = 1.0f;
    double startSeconds = 0;
    Behaviour behaviour = Behaviour::retrigger;
};
struct Row {
    int id = 0; juce::String name; std::vector<TempoPoint> tempo {{}}; std::vector<Cell> cells;
    float gain = 1.0f;
    bool muted = false, loop = false, overrideBehaviour = false;
    Behaviour behaviour = Behaviour::retrigger;
};
struct Show { juce::String name = "Show 001"; int nextId = 1; std::vector<Column> columns; std::vector<Row> rows; };
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
