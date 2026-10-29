#pragma once
#include "Theme.h"
#include <vector>

namespace cuelist {

// Compact, data-only operator report, shared by Diagnostics and About. Unlike
// the stock InfoIcon AlertWindow, no oversized ornament obscures the content.
struct OperatorReportRow {
    juce::String name, value;
    enum class Status { neutral, positive, caution, negative };
    Status status=Status::neutral;
};
struct OperatorReportSection {
    juce::String title;
    std::vector<OperatorReportRow> rows;
};

class OperatorReportPanel final : public juce::Component {
public:
    OperatorReportPanel(juce::String title,juce::String subtitle,
                        std::vector<OperatorReportSection> groups,
                        juce::String footnote)
        : heading(std::move(title)),subheading(std::move(subtitle)),
          sections(std::move(groups)),note(std::move(footnote)) {
        int height=81;
        for (const auto& section:sections)
            height+=31+(int)section.rows.size()*24;
        height+=note.isNotEmpty() ? 51 : 12;
        height+=48;
        setSize(542,height);
        addAndMakeVisible(close);
        close.setButtonText("Close");
        close.setName("operatorReportClose");
        close.onClick=[this] {
            if (auto* window=findParentComponentOfClass<juce::DialogWindow>())
                window->exitModalState(0);
        };
    }
    void resized() override {
        close.setBounds(getWidth()-116,getHeight()-49,92,31);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(palette::panel);
        g.setColour(palette::teal.withAlpha(.15f));
        g.fillEllipse(22.0f,20.0f,25.0f,25.0f);
        g.setColour(palette::teal);
        g.drawEllipse(22.5f,20.5f,24.0f,24.0f,1.0f);
        label(g,"i",{23,20,23,24},15,palette::teal,true,
              juce::Justification::centred);
        label(g,heading,{59,15,getWidth()-83,31},22,palette::text,true);
        label(g,subheading,{60,47,getWidth()-86,22},13.5f,palette::muted);
        g.setColour(palette::line);
        g.drawHorizontalLine(73,22.0f,(float)getWidth()-22);

        int y=81;
        for (const auto& section:sections) {
            label(g,section.title.toUpperCase(),{24,y+3,getWidth()-48,19},
                  13,palette::teal,true);
            y+=23;
            for (size_t i=0;i<section.rows.size();++i) {
                const auto& row=section.rows[i];
                if (i%2==0) {
                    g.setColour(palette::raised.withAlpha(.48f));
                    g.fillRoundedRectangle(19.0f,(float)y,(float)getWidth()-38,24.0f,3.0f);
                }
                label(g,row.name,{29,y+2,345,21},14.0f,palette::text);
                const auto tone=row.status==OperatorReportRow::Status::positive ? palette::teal :
                                row.status==OperatorReportRow::Status::caution ? palette::amber :
                                row.status==OperatorReportRow::Status::negative ? palette::red :
                                palette::muted;
                label(g,row.value,{375,y+2,getWidth()-401,21},14.0f,tone,true,
                      juce::Justification::centredRight);
                y+=24;
            }
            y+=8;
        }
        if (note.isNotEmpty()) {
            g.setColour(palette::line);
            g.drawHorizontalLine(y+2,22.0f,(float)getWidth()-22);
            g.setColour(palette::muted);
            g.setFont(font(13.0f));
            g.drawFittedText(note,{25,y+9,getWidth()-50,32},
                             juce::Justification::topLeft,2);
        }
        g.setColour(palette::line);
        g.drawHorizontalLine(getHeight()-60,22.0f,(float)getWidth()-22);
    }
private:
    juce::String heading,subheading,note;
    std::vector<OperatorReportSection> sections;
    juce::TextButton close;
};


class UnsavedChangesPanel final : public juce::Component {
public:
    explicit UnsavedChangesPanel(std::function<void()> confirm)
        : onConfirm(std::move(confirm)) {
        addAndMakeVisible(discard);
        addAndMakeVisible(cancel);
        discard.setName("discardUnsavedChanges");
        discard.setButtonText("Discard and continue");
        discard.setColour(juce::TextButton::buttonColourId,palette::panel);
        discard.setColour(juce::TextButton::textColourOffId,palette::red);
        discard.onClick=[this] {
            // Take ownership of the callback before closing the modal:
            // closing may destroy this panel before the show is replaced.
            auto approved=onConfirm;
            if (auto* window=findParentComponentOfClass<juce::DialogWindow>())
                window->exitModalState(1);
            if (approved) approved();
        };
        cancel.setName("cancelUnsavedChanges");
        cancel.setButtonText("Cancel");
        cancel.setColour(juce::TextButton::buttonColourId,palette::raised);
        cancel.setColour(juce::TextButton::textColourOffId,palette::text);
        cancel.onClick=[this] {
            if (auto* window=findParentComponentOfClass<juce::DialogWindow>())
                window->exitModalState(0);
        };
        // Reuse the primary application fonts; warning mark is vector-drawn.
        setSize(510,228);
    }
    void resized() override {
        cancel.setBounds(getWidth()-121,getHeight()-54,96,34);
        discard.setBounds(cancel.getX()-204,getHeight()-54,190,34);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(palette::panel);
        g.setColour(palette::amber.withAlpha(.12f));
        g.fillEllipse(23.0f,22.0f,30.0f,30.0f);
        g.setColour(palette::amber);
        g.drawEllipse(23.5f,22.5f,29.0f,29.0f,1.2f);
        // A 12px vector exclamation replaces the oversized QuestionIcon.
        g.fillRoundedRectangle(37.0f,29.0f,2.6f,11.0f,1.2f);
        g.fillEllipse(36.7f,42.5f,3.2f,3.2f);
        label(g,"Unsaved changes",{67,20,getWidth()-92,32},
              22,palette::text,true);
        label(g,"Your current show contains changes that have not been saved.",
              {27,76,getWidth()-54,25},15,palette::text);
        label(g,"Continuing will discard those changes.",
              {27,105,getWidth()-54,23},14,palette::amber,true);
        label(g,"Choose Cancel to return to the show and save it.",
              {27,132,getWidth()-54,21},12,palette::muted);
        g.setColour(palette::line);
        g.drawHorizontalLine(getHeight()-69,23.0f,(float)getWidth()-23);
    }
    void focusSafeAction() { cancel.grabKeyboardFocus(); }
private:
    std::function<void()> onConfirm;
    juce::TextButton discard,cancel;
};

inline void showUnsavedChangesConfirmation(juce::Component& parent,
                                           juce::LookAndFeel& theme,
                                           std::function<void()> onDiscard) {
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle="Unsaved changes";
    options.dialogBackgroundColour=palette::panel;
    auto* panel=new UnsavedChangesPanel(std::move(onDiscard));
    options.content.setOwned(panel);
    options.componentToCentreAround=&parent;
    options.useNativeTitleBar=false;
    options.resizable=false;
    options.escapeKeyTriggersCloseButton=true;
    if (auto* dialog=options.launchAsync()) {
        dialog->setLookAndFeel(&theme);
        panel->focusSafeAction();  // Enter/Space must not silently discard.
    }
}

inline void showOperatorReport(juce::Component& parent,juce::LookAndFeel& theme,
                               const juce::String& title,const juce::String& subtitle,
                               std::vector<OperatorReportSection> sections,
                               const juce::String& footnote) {
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle=title;
    options.dialogBackgroundColour=palette::panel;
    options.content.setOwned(new OperatorReportPanel(title,subtitle,
                                                      std::move(sections),footnote));
    options.componentToCentreAround=&parent;
    options.useNativeTitleBar=false;
    options.resizable=false;
    options.escapeKeyTriggersCloseButton=true;
    if (auto* dialog=options.launchAsync())
        dialog->setLookAndFeel(&theme);
}
}
