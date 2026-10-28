#include "MainComponent.h"
#include <cmath>
#include <cstdlib>

namespace cuelist {
namespace {
// Keep the Inspector's unit conversion/validation with its update logic.
bool parseGainDb(const juce::String& input, double& value) {
    const auto trimmed=input.trim().toStdString();
    if (trimmed.empty()) return false;
    char* end=nullptr;
    const double parsed=std::strtod(trimmed.c_str(),&end);
    if (end==trimmed.c_str() || *end!='\0' || !std::isfinite(parsed)
        || parsed < -60.0 || parsed > 12.0) return false;
    value=parsed;
    return true;
}
}

void MainComponent::refreshInspector() {
    const auto& row=show.rows[selectedRow]; const auto& c=row.cells[selectedColumn]; const auto& col=show.columns[selectedColumn];
    masterSlider.setGainDb(GainMeterFader::fromLinear(show.masterGain));
    masterPan.setValue(show.masterPan,juce::dontSendNotification);
    masterMute.setToggleState(show.masterMuted,juce::dontSendNotification);
    safetyLimiter.setToggleState(show.safetyLimiterEnabled,juce::dontSendNotification);
    engine.monitoredRowId.store(row.id);
    rowName.setTextToShowWhenEmpty("Cue name",palette::muted);
    rowName.setText(row.name,false); cellName.setText(c.name,false);
    cueNotes.setText(row.notes,false);
    cellName.setTextToShowWhenEmpty("Cell name",palette::muted);
    bpm.setText(juce::String(row.tempo.front().bpm,1),false);
    behaviour.setSelectedId((int)c.behaviour+1,juce::dontSendNotification);
    follow.setToggleState(c.followsRow,juce::dontSendNotification); loop.setToggleState(c.loop,juce::dontSendNotification);
    cellAutoStop.setToggleState(c.stopOnNext,juce::dontSendNotification);
    cellAutoStop.setTooltip(row.stopOnNext ? "ON: participates in Auto Stop. OFF: continues after the next GO."
        : "Cue Auto Stop is OFF: this cell option applies when the cue is set to ON.");
    gainDb.setText(juce::String(juce::Decibels::gainToDecibels(c.gain,-60.0f),1),false);
    rowGain.setValue(juce::Decibels::gainToDecibels(row.gain,-60.0f),juce::dontSendNotification);
    rowMute.setToggleState(row.muted,juce::dontSendNotification); rowLoop.setToggleState(row.loop,juce::dontSendNotification);
    rowBehaviour.setSelectedId(row.overrideBehaviour ? (int)row.behaviour+2 : 1,juce::dontSendNotification);
    // The Entire Q controls and Cell controls are visible together, with no
    // redundant transport, behavioural presets or manual Apply button.
    rowSettings=false;
    for(juce::Component* component:std::initializer_list<juce::Component*>{
         &cellTab,&rowTab,&rowGain,&rowMute,&rowBehaviour,&behaviour,&cellAutoStop,
         &playCellButton,&stopCellButton,&clearButton,&applyButton})
        component->setVisible(false);
    for(juce::Component* component:std::initializer_list<juce::Component*>{
         &rowName,&bpm,&tempoButton,&rowLoop,&cellName,&follow,&loop,&cueNotes})
        component->setVisible(true);
    const bool audio=col.type==ColumnType::audio;
    const bool midi=col.type==ColumnType::midi;
    const bool osc=col.type==ColumnType::osc;
    const bool control=col.type==ColumnType::control;
    gainDb.setVisible(audio);
    midiEditButton.setVisible(midi);
    oscEditButton.setVisible(osc || control);
    oscEditButton.setButtonText(control ? (col.controlLocked?"Edit default Control Set":"Edit Control Set")
                                      : c.osc ? "Edit OSC clip" : "Create OSC clip");
    midiEditButton.setButtonText(c.midi ? "Edit piano roll" : "Create MIDI clip");
    importButton.setVisible(audio || midi);
    importButton.setButtonText(midi?"Import MIDI":"Import WAV / MP3");
    importButton.setTooltip(midi?"Import notes from .mid/.midi.":"Import WAV/MP3.");
    for(juce::Component* component:std::initializer_list<juce::Component*>{
        &cellName,&follow,&loop,&gainDb,&importButton,&midiEditButton,&oscEditButton})
        component->setEnabled(!col.collapsed && !(control && col.controlLocked && component!=&oscEditButton));
}

bool MainComponent::inspectorDiffers() const {
    const auto& row=show.rows[selectedRow]; const auto& cell=row.cells[selectedColumn];
    if (rowName.getText().trim()!=row.name || bpm.getText()!=juce::String(row.tempo.front().bpm,1)
        || cueNotes.getText()!=row.notes) return true;
    if(rowLoop.getToggleState()!=row.loop) return true;
    if (show.columns[selectedColumn].collapsed) return false;
    if (show.columns[selectedColumn].type==ColumnType::audio) {
        double value=0;
        if (!parseGainDb(gainDb.getText(),value) ||
            std::abs(value-juce::Decibels::gainToDecibels(cell.gain,-60.0f))>0.051)
            return true;
    }
    return cellName.getText().trim()!=cell.name ||
           follow.getToggleState()!=cell.followsRow || loop.getToggleState()!=cell.loop;
}

bool MainComponent::applyInspector() {
    if (!inspectorDiffers()) return true;
    auto candidate=show; auto& row=candidate.rows[selectedRow]; auto& cell=row.cells[selectedColumn];
    row.name=rowName.getText().trim();
    row.notes=cueNotes.getText();
    row.loop=rowLoop.getToggleState();
    if (bpm.getText()!=juce::String(row.tempo.front().bpm,1)) row.tempo.front().bpm=bpm.getText().getDoubleValue();
    if (!candidate.columns[(size_t)selectedColumn].collapsed) {
        cell.name=cellName.getText().trim();
        cell.followsRow=follow.getToggleState(); cell.loop=loop.getToggleState();
        if (candidate.columns[selectedColumn].type==ColumnType::audio) {
            double value=0;
            if (!parseGainDb(gainDb.getText(),value)) {
                message("Cell Gain must be a number from -60.0 to +12.0 dB.");
                return false;
            }
            cell.gain=juce::Decibels::decibelsToGain((float)value,-60.0f);
        }
    }
    auto result=validateShow(candidate); if (result.failed()) { message(result.getErrorMessage()); return false; }
    show=std::move(candidate); changed("Settings applied. BPM does not change audio playback speed.");
    return true;
}

} // namespace cuelist
