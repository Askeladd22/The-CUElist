#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include <algorithm>
#include <vector>

namespace cuelist {
// Inline, model-independent list of mappable destinations. No modal dialog.
class QuickControlTargets final : public juce::ListBox, private juce::ListBoxModel {
public:
    struct Entry { int functionId=0; juce::String title,detail; };
    std::function<void(int)> onPick;
    std::function<void(int)> onRename;
    void beginInlineRename(int functionId,const juce::String& current,
                           std::function<void(const juce::String&)> commit) {
        int row=-1;
        for (int i=0;i<(int)entries.size();++i)
            if (entries[(size_t)i].functionId==functionId) {row=i;break;}
        if (row<0) return;
        renameCommit=std::move(commit);
        if (!renameEditor) {
            renameEditor=std::make_unique<juce::TextEditor>();
            addAndMakeVisible(*renameEditor);
            renameEditor->onReturnKey=[this]{finishRename(true);};
            renameEditor->onEscapeKey=[this]{finishRename(false);};
            renameEditor->onFocusLost=[this]{finishRename(true);};
        }
        renameEditor->setBounds(10,row*getRowHeight()-
            (getViewport()?getViewport()->getViewPositionY():0)+2,
            std::max(60,getWidth()-20),getRowHeight()-4);
        renameEditor->setText(current,false);
        renameEditor->setVisible(true);renameEditor->toFront(false);
        renameEditor->grabKeyboardFocus();renameEditor->selectAll();
    }
    QuickControlTargets():juce::ListBox("quickControlTargets",nullptr) {
        setModel(this);setRowHeight(42);
        setColour(juce::ListBox::backgroundColourId,juce::Colour(0xff19222c));
    }
    ~QuickControlTargets() override {setModel(nullptr);}
    void setLearnPresentation(juce::Colour colour,int functionId,
                              int directEntryId,bool bright) {
        if (accent==colour && armedFunctionId==functionId &&
            armedEntryId==directEntryId && pulseBright==bright) return;
        accent=colour;armedFunctionId=functionId;
        armedEntryId=directEntryId;pulseBright=bright;repaint();
    }
    juce::Colour getLearnAccent() const noexcept {return accent;}
    int getArmedFunctionId() const noexcept {return armedFunctionId;}
    bool isPulseBright() const noexcept {return pulseBright;}
    void setEntries(std::vector<Entry> newEntries) {
        entries=std::move(newEntries);
        deselectAllRows();updateContent();repaint();
    }
    int getNumRows() override {return (int)entries.size();}
    void selectedRowsChanged(int row) override {
        if (row>=0 && row<(int)entries.size() && onPick)
            onPick(entries[(size_t)row].functionId);
    }
    void listBoxItemDoubleClicked(int row,const juce::MouseEvent&) override {
        if (row>=0 && row<(int)entries.size() &&
            entries[(size_t)row].functionId>0 && onRename)
            onRename(entries[(size_t)row].functionId);
    }
    void paintListBoxItem(int row,juce::Graphics& g,int width,int height,
                          bool rowSelected) override {
        if (row<0 || row>=(int)entries.size()) return;
        const auto& item=entries[(size_t)row];
        const bool armed=armedFunctionId>0 &&
            (armedEntryId<0 ? item.functionId==armedEntryId :
                               item.functionId==armedFunctionId);
        if (armed) {
            g.fillAll(accent.withAlpha(pulseBright ? .30f : .055f));
            g.setColour(accent.withAlpha(pulseBright ? 1.0f : .40f));
            g.fillRect(0,0,4,height);
        } else if (rowSelected)
            g.fillAll(accent.withAlpha(.12f));
        g.setColour(armed ? accent : juce::Colour(0xffe9eeee));
        g.setFont(juce::Font(juce::FontOptions(13.5f,juce::Font::plain)));
        g.drawText(item.title,10,2,width-20,22,juce::Justification::centredLeft,true);
        g.setColour(armed ? accent.withAlpha(.88f) :
                    juce::Colour(0xff99b5ba));
        g.setFont(juce::Font(juce::FontOptions(11.0f,juce::Font::plain)));
        g.drawText(item.detail,10,22,width-20,17,juce::Justification::centredLeft,true);
    }
private:
    std::vector<Entry> entries;
    std::unique_ptr<juce::TextEditor> renameEditor;
    std::function<void(const juce::String&)> renameCommit;
    void finishRename(bool save) {
        if (!renameEditor || !renameEditor->isVisible()) return;
        const auto name=renameEditor->getText().trim();
        renameEditor->setVisible(false);
        auto callback=std::move(renameCommit);renameCommit={};
        if (save && name.isNotEmpty() && callback) callback(name);
    }
    juce::Colour accent{0xfff9c76e};
    int armedFunctionId=0,armedEntryId=0;
    bool pulseBright=true;
};
} // namespace cuelist
