#pragma once
#include "OscClip.h"
#include "ShowModel.h"
#include "Theme.h"
#include <functional>

namespace cuelist {
// One shared dockable/floating OSC editor: event lane and automation curves
// share their ruler, grid, snap, and horizontally scrolling beat/second axis.
class OscClipEditor final : public juce::Component {
public:
    explicit OscClipEditor(OscClip,std::vector<TempoPoint>,
        std::function<void(const OscClip&,const OscClip&,bool)>);
    ~OscClipEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void loadClip(const OscClip&);
    void setTempo(const std::vector<TempoPoint>& currentTempo) { tempo=currentTempo; }
    void setFloating(bool);
    const OscClip& getClip() const { return clip; }
    void editEvent(int index);
    void addEvent();
    void addAutomation();
    void editAutomation(int index);
    std::function<void()> onDetach,onUndo,onRedo;

private:
    Theme theme;
    OscClip clip;
    std::vector<TempoPoint> tempo;
    std::function<void(const OscClip&,const OscClip&,bool)> onEdit;
    juce::ComboBox unit,resolution;
    juce::ToggleButton grid{"Griglia"},snap{"Snap"};
    juce::TextButton add{"+"},ramp{"+ Rampa"},remove{"Elimina"},duplicate{"Duplica"},detach{"Stacca"};
    int selectedEvent=-1,selectedAutomation=-1,selectedPoint=-1,dragEvent=-1,dragAutomation=-1,dragPoint=-1;
    bool dragging=false,changedDuringDrag=false;
    int laneScroll=0;
    double cursor=0,dragStartTime=0;
    OscClip dragBefore;
    uint64_t revision=0;
    static constexpr int rulerTop=67,rulerBottom=94,eventTop=101,eventHeight=47,laneHeight=83;
    int axisLeft() const { return 78; }
    int axisWidth() const { return std::max(1,getWidth()-100); }
    float xForTime(double t) const;
    double timeForX(float x) const;
    double quantize(double t) const;
    int laneTop(int index) const { return eventTop+eventHeight+11+index*laneHeight-laneScroll; }
    int automationAt(float y) const;
    float yForValue(int index,double value) const;
    double valueForY(int index,float y) const;
    void chooseEvent(int index);
    void chooseAutomation(int index);
    void notify(const OscClip& before,bool complete=true);
    void deleteSelection();
    void duplicateSelection();
    void showContext(const juce::MouseEvent&);
    static juce::Result parseArguments(const juce::String&,std::vector<OscValue>&);
    static juce::String describeArguments(const std::vector<OscValue>&);
    static juce::String describePoints(const std::vector<OscControlPoint>&);
    static juce::Result parsePoints(const juce::String&,std::vector<OscControlPoint>&);
    static double beatFromSeconds(const std::vector<TempoPoint>&,double);
};
}
