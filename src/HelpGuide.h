#pragma once
#include "Theme.h"
#include <memory>
#include <vector>

namespace cuelist {

// A small native Markdown reader. It intentionally handles the constructs
// used by complete-user-guide.md without launching a browser or installing
// a Markdown-rendering engine.
class GuideDocument final : public juce::Component {
public:
    enum class Kind { title, heading, subheading, paragraph, list, quote, code, table, gap, rule };
    struct Block {
        Kind kind;
        juce::String text;
        float top=0.0f, height=0.0f;
        std::unique_ptr<juce::TextLayout> layout;
    };

    void setMarkdown(const juce::String& markdown);
    void layoutAtWidth(int contentWidth);
    int blockCount() const noexcept { return (int)blocks.size(); }
    int findNext(const juce::String& phrase,int after=-1) const;
    int findPrevious(const juce::String& phrase,int before) const;
    int blockTop(int index) const;
    void paint(juce::Graphics& g) override;

private:
    static juce::String readableMarkdown(juce::String text);
    static juce::Font blockFont(Kind kind);
    static juce::Colour blockColour(Kind kind);
    std::vector<Block> blocks;
};

class GuideViewer final : public juce::Component {
public:
    explicit GuideViewer(const juce::String& markdown);
    void setMarkdown(const juce::String& markdown);
    void resized() override;
    void paint(juce::Graphics& g) override;
    GuideDocument& getDocument() noexcept { return document; }

private:
    void find(bool forwards);
    GuideDocument document;
    juce::Viewport viewport;
    juce::TextEditor search;
    juce::TextButton next{"Next"};
    juce::TextButton previous{"Previous"};
    juce::TextButton close{"Close"};
    int lastMatch=-1;
};

class GuideWindow final : public juce::DocumentWindow {
public:
    explicit GuideWindow(const juce::String& markdown);
    void setMarkdown(const juce::String& markdown);
    void closeButtonPressed() override;
private:
    GuideViewer* viewer=nullptr; // owned by DocumentWindow through setContentOwned
};

} // namespace cuelist
