#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

namespace cuelist {
// Inline, model-independent list of mappable destinations. No modal dialog.
class QuickControlTargets final : public juce::ListBox, private juce::ListBoxModel {
public:
    struct Entry { int functionId=0; juce::String title,detail; };
    std::function<void(int)> onPick;
    QuickControlTargets():juce::ListBox("quickControlTargets",nullptr) {
        setModel(this);setRowHeight(42);
        setColour(juce::ListBox::backgroundColourId,juce::Colour(0xff19222c));
    }
    ~QuickControlTargets() override {setModel(nullptr);}
    void setEntries(std::vector<Entry> newEntries) {
        entries=std::move(newEntries);
        deselectAllRows();updateContent();repaint();
    }
    int getNumRows() override {return (int)entries.size();}
    void selectedRowsChanged(int row) override {
        if (row>=0 && row<(int)entries.size() && onPick)
            onPick(entries[(size_t)row].functionId);
    }
    void paintListBoxItem(int row,juce::Graphics& g,int width,int height,
                          bool selected) override {
        if (row<0 || row>=(int)entries.size()) return;
        const auto& item=entries[(size_t)row];
        if (selected) g.fillAll(juce::Colour(0xff24636a).withAlpha(.35f));
        g.setColour(juce::Colour(0xffe9eeee));
        g.setFont(juce::Font(juce::FontOptions(13.5f,juce::Font::plain)));
        g.drawText(item.title,10,2,width-20,22,juce::Justification::centredLeft,true);
        g.setColour(juce::Colour(0xff99b5ba));
        g.setFont(juce::Font(juce::FontOptions(11.0f,juce::Font::plain)));
        g.drawText(item.detail,10,22,width-20,17,juce::Justification::centredLeft,true);
    }
private:
    std::vector<Entry> entries;
};
} // namespace cuelist
