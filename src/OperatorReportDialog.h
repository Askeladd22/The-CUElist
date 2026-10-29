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
            height+=38+(int)section.rows.size()*23;
        height+=note.isNotEmpty() ? 58 : 18;
        height+=54;
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
        label(g,subheading,{60,49,getWidth()-86,18},11,palette::muted);
        g.setColour(palette::line);
        g.drawHorizontalLine(73,22.0f,(float)getWidth()-22);

        int y=81;
        for (const auto& section:sections) {
            label(g,section.title.toUpperCase(),{24,y+4,getWidth()-48,19},
                  12,palette::teal,true);
            y+=29;
            for (size_t i=0;i<section.rows.size();++i) {
                const auto& row=section.rows[i];
                if (i%2==0) {
                    g.setColour(palette::raised.withAlpha(.48f));
                    g.fillRoundedRectangle(19.0f,(float)y,(float)getWidth()-38,23.0f,3.0f);
                }
                label(g,row.name,{29,y+2,345,19},12,palette::text);
                const auto tone=row.status==OperatorReportRow::Status::positive ? palette::teal :
                                row.status==OperatorReportRow::Status::caution ? palette::amber :
                                row.status==OperatorReportRow::Status::negative ? palette::red :
                                palette::muted;
                label(g,row.value,{375,y+2,getWidth()-401,19},12,tone,true,
                      juce::Justification::centredRight);
                y+=23;
            }
            y+=9;
        }
        if (note.isNotEmpty()) {
            g.setColour(palette::line);
            g.drawHorizontalLine(y+3,22.0f,(float)getWidth()-22);
            g.setColour(palette::muted);
            g.setFont(font(11.0f));
            g.drawFittedText(note,{25,y+12,getWidth()-50,40},
                             juce::Justification::topLeft,2);
        }
        g.setColour(palette::line);
        g.drawHorizontalLine(getHeight()-63,22.0f,(float)getWidth()-22);
    }
private:
    juce::String heading,subheading,note;
    std::vector<OperatorReportSection> sections;
    juce::TextButton close;
};

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
    options.useNativeTitleBar=true;
    options.resizable=false;
    options.escapeKeyTriggersCloseButton=true;
    if (auto* dialog=options.launchAsync())
        dialog->setLookAndFeel(&theme);
}
}
