#pragma once
#include "OscClip.h"
#include "ShowModel.h"
#include "Theme.h"
#include <functional>
namespace cuelist {
class OscClipEditor final : public juce::Component, private juce::ListBoxModel {
public:
    explicit OscClipEditor(OscClip,std::vector<TempoPoint>,
                           std::function<void(const OscClip&)>);
    ~OscClipEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void loadClip(const OscClip&);
    void setFloating(bool);
    const OscClip& getClip() const { return clip; }
    std::function<void()> onDetach;
private:
    Theme theme;
    OscClip clip;
    std::vector<TempoPoint> tempo;
    std::function<void(const OscClip&)> onEdit;
    juce::ListBox eventList{"oscEvents",this};
    juce::ComboBox unit;
    juce::TextEditor time,address,args,length;
    juce::TextButton add{"Aggiungi"},remove{"Elimina"},detach{"Stacca"};
    juce::Label help;
    int selected=-1;
    bool updating=false;
    int getNumRows() override { return (int)clip.events.size(); }
    void paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool selectedRow) override;
    void selectedRowsChanged(int row) override;
    void showEvent();
    void commitEvent();
    void notify();
    static juce::Result parseArguments(const juce::String&,std::vector<OscValue>&);
    static juce::String describeArguments(const std::vector<OscValue>&);
    static double beatFromSeconds(const std::vector<TempoPoint>&,double);
};
}
