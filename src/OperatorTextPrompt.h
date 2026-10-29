#pragma once
#include "Theme.h"
#include <functional>

namespace cuelist {

// Shared native editing surface for cue/column names, Cuelist names, BPM and
// multiline tempo maps. The model's validation and callback stay with caller.
class OperatorTextPrompt final : public juce::Component {
public:
    OperatorTextPrompt(juce::String title,juce::String explanation,
                       juce::String field,juce::String initial,bool isMultiline,
                       juce::String accept,std::function<void(juce::String)> onAccepted)
        : heading(std::move(title)),description(std::move(explanation)),
          fieldLabel(std::move(field)),multiline(isMultiline),
          result(std::move(onAccepted)) {
        setName("operatorTextPrompt");
        addAndMakeVisible(editor);
        editor.setName("value");
        editor.setText(initial,false);
        editor.setFont(font(15.0f));
        editor.setColour(juce::TextEditor::backgroundColourId,palette::input);
        editor.setColour(juce::TextEditor::outlineColourId,palette::line);
        editor.setColour(juce::TextEditor::focusedOutlineColourId,palette::teal);
        editor.setMultiLine(multiline);
        editor.setReturnKeyStartsNewLine(multiline);
        if (!multiline) editor.onReturnKey=[this] { acceptChanges(); };
        addAndMakeVisible(confirm);
        addAndMakeVisible(cancel);
        confirm.setButtonText(accept);
        confirm.setName("operatorPromptAccept");
        confirm.onClick=[this] { acceptChanges(); };
        cancel.setButtonText("Cancel");
        cancel.setName("operatorPromptCancel");
        cancel.onClick=[this] {
            if (auto* dialog=findParentComponentOfClass<juce::DialogWindow>())
                dialog->exitModalState(0);
        };
        setSize(510,multiline ? 400 : 256);
    }
    juce::TextEditor& getEditor() noexcept { return editor; }
    void focusEditor() { editor.grabKeyboardFocus(); editor.selectAll(); }
    void resized() override {
        editor.setBounds(24,multiline ? 151 : 143,getWidth()-48,multiline ? 158 : 37);
        confirm.setBounds(getWidth()-231,getHeight()-54,118,34);
        cancel.setBounds(getWidth()-103,getHeight()-54,79,34);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(palette::panel);
        label(g,heading,{24,20,getWidth()-48,34},22,palette::text,true);
        g.setColour(palette::teal);g.fillRoundedRectangle(24.f,57.f,44.f,3.f,1.5f);
        g.setColour(palette::muted);g.setFont(font(14.0f));
        g.drawFittedText(description,{24,75,getWidth()-48,48},
                         juce::Justification::topLeft,2);
        label(g,fieldLabel,{24,multiline ? 128 : 120,getWidth()-48,19},
              12,palette::muted,true);
        if (multiline)
            label(g,"FORMAT  beat BPM   e.g. 0 120  /  16 140",
                  {25,317,getWidth()-50,22},11,palette::muted);
        g.setColour(palette::line);
        g.drawHorizontalLine(getHeight()-70,24.f,(float)getWidth()-24.f);
    }
private:
    void acceptChanges() {
        // Capture everything before closing: the modal owns this panel.
        const auto value=editor.getText();
        auto callback=result;
        if (auto* dialog=findParentComponentOfClass<juce::DialogWindow>())
            dialog->exitModalState(1);
        if (callback) callback(value);
    }
    juce::String heading,description,fieldLabel;
    bool multiline=false;
    std::function<void(juce::String)> result;
    juce::TextEditor editor;
    juce::TextButton confirm,cancel;
};

inline void showOperatorTextPrompt(juce::Component& parent,juce::LookAndFeel& look,
                                   const juce::String& title,const juce::String& description,
                                   const juce::String& labelText,const juce::String& value,
                                   bool multiline,const juce::String& accept,
                                   std::function<void(juce::String)> callback) {
    juce::DialogWindow::LaunchOptions opt;
    opt.dialogTitle=title;
    opt.dialogBackgroundColour=palette::panel;
    auto* content=new OperatorTextPrompt(title,description,labelText,value,
                                         multiline,accept,std::move(callback));
    opt.content.setOwned(content);
    opt.componentToCentreAround=&parent;
    opt.useNativeTitleBar=false;
    opt.escapeKeyTriggersCloseButton=true;
    opt.resizable=false;
    if (auto* dialog=opt.launchAsync()) {
        dialog->setLookAndFeel(&look);
        juce::Component::SafePointer<OperatorTextPrompt> safe(content);
        juce::MessageManager::callAsync([safe] {
            if (safe && safe->isShowing()) safe->focusEditor();
        });
        content->focusEditor();
    }
}
}
