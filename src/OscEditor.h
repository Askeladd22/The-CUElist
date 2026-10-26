#pragma once
#include "OscClip.h"
#include "ShowModel.h"
#include "Theme.h"
#include <functional>

namespace cuelist {
// Compact OSC event list. Double-click opens an explicit edit dialog, so
// the main editor is not cluttered with persistent argument/length fields.
class OscClipEditor final : public juce::Component, private juce::ListBoxModel {
public:
    explicit OscClipEditor(OscClip,std::vector<TempoPoint>,
                           std::function<void(const OscClip&)>);
    ~OscClipEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void loadClip(const OscClip&);
    void setTempo(const std::vector<TempoPoint>& currentTempo) { tempo=currentTempo; }
    void setFloating(bool);
    const OscClip& getClip() const { return clip; }
    void editEvent(int index);
    void addEvent();
    std::function<void()> onDetach;
private:
    Theme theme;
    OscClip clip;
    std::vector<TempoPoint> tempo;
    std::function<void(const OscClip&)> onEdit;
    juce::ListBox eventList{"oscEvents",this};
    juce::ComboBox unit;
    juce::TextButton add{"+"},remove{"Elimina"},detach{"Stacca"};
    int selected=-1;
    uint64_t revision=0;
    int getNumRows() override { return (int)clip.events.size(); }
    void paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool selectedRow) override;
    void selectedRowsChanged(int row) override { selected=row; }
    void listBoxItemDoubleClicked(int row,const juce::MouseEvent&) override { editEvent(row); }
    void notify();
    static juce::Result parseArguments(const juce::String&,std::vector<OscValue>&);
    static juce::String describeArguments(const std::vector<OscValue>&);
    static double beatFromSeconds(const std::vector<TempoPoint>&,double);
};
}
