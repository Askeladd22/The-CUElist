#include "HelpGuide.h"
#include <cmath>

namespace cuelist {

juce::String GuideDocument::readableMarkdown(juce::String text) {
    // Resolve Markdown links to the visible label; no external URLs are
    // opened in the operator guide. Keep paths inside backticks readable.
    int start=text.indexOfChar('[');
    while (start>=0) {
        const int middle=text.indexOf(start,"](");
        if (middle<0) break;
        const int end=text.indexOfChar(middle+2,')');
        if (end<0) break;
        text=text.substring(0,start)+text.substring(start+1,middle)
                 +text.substring(end+1);
        start=text.indexOfChar(start,'[');
    }
    return text.replace("**","").replace("~~","").replace("`","")
               .replace(" *"," ").replace("* "," ");
}

juce::Font GuideDocument::blockFont(Kind kind) {
    switch(kind) {
        case Kind::title:      return font(28.0f,true);
        case Kind::heading:    return font(22.0f,true);
        case Kind::subheading: return font(18.0f,true);
        case Kind::code:       return font(14.0f);
        case Kind::table:      return font(14.0f);
        default:               return font(15.5f);
    }
}

juce::Colour GuideDocument::blockColour(Kind kind) {
    if (kind==Kind::heading || kind==Kind::subheading) return palette::teal;
    if (kind==Kind::code || kind==Kind::table) return palette::muted;
    if (kind==Kind::quote) return palette::amber;
    return palette::text;
}

void GuideDocument::setMarkdown(const juce::String& markdown) {
    blocks.clear();
    juce::StringArray lines;
    lines.addLines(markdown);
    bool code=false;
    for (const auto& line : lines) {
        const auto trimmed=line.trim();
        if (trimmed.startsWith("```")) {
            code=!code;
            continue;
        }
        Kind kind=Kind::paragraph;
        juce::String visible=trimmed;

        if (trimmed.isEmpty()) kind=Kind::gap;
        else if (code) { kind=Kind::code; visible=line; }
        else if (trimmed.startsWith("### ")) {
            kind=Kind::subheading; visible=trimmed.substring(4);
        }
        else if (trimmed.startsWith("## ")) {
            kind=Kind::heading; visible=trimmed.substring(3);
        }
        else if (trimmed.startsWith("# ")) {
            kind=Kind::title; visible=trimmed.substring(2);
        }
        else if (trimmed.startsWith("- ") || trimmed.startsWith("* ")) {
            kind=Kind::list; visible=juce::String::fromUTF8("•  ")+trimmed.substring(2);
        }
        else if (juce::CharacterFunctions::isDigit(trimmed[0]) &&
                 trimmed.indexOfChar('.')>0 &&
                 trimmed.indexOfChar('.')<4) {
            kind=Kind::list;
        }
        else if (trimmed.startsWith(">")) {
            kind=Kind::quote; visible=trimmed.substring(1).trim();
        }
        else if (trimmed.startsWith("|")) {
            if (trimmed.containsOnly("|-: ")) kind=Kind::gap;
            else { kind=Kind::table;
                   visible=trimmed.trimCharactersAtStart("|")
                                  .trimCharactersAtEnd("|")
                                  .replace("|","    ·    "); }
        }
        else if (trimmed.startsWith("---")) kind=Kind::rule;
        if (kind!=Kind::code) visible=readableMarkdown(visible);
        blocks.push_back({kind,visible});
    }
    layoutAtWidth(juce::jmax(500,getWidth()));
}

void GuideDocument::layoutAtWidth(int contentWidth) {
    const int width=juce::jmax(300,contentWidth);
    float y=24.0f;
    for (auto& block:blocks) {
        block.top=y;
        block.layout.reset();
        if (block.kind==Kind::gap) {
            block.height=10.0f;
        } else if (block.kind==Kind::rule) {
            block.height=20.0f;
        } else {
            juce::AttributedString attributed;
            attributed.setWordWrap(juce::AttributedString::byWord);
            attributed.setJustification(juce::Justification::topLeft);
            attributed.append(block.text,blockFont(block.kind),
                              blockColour(block.kind));
            block.layout=std::make_unique<juce::TextLayout>();
            block.layout->createLayout(attributed,(float)(width-100));
            const float padding=block.kind==Kind::title ? 18.0f :
                                block.kind==Kind::heading ? 15.0f :
                                block.kind==Kind::subheading ? 11.0f : 7.0f;
            block.height=juce::jmax(18.0f,block.layout->getHeight())+padding;
        }
        y+=block.height;
    }
    const int newHeight=(int)std::ceil(y+22.0f);
    if (getWidth()!=width || getHeight()!=newHeight)
        setSize(width,newHeight);
    repaint();
}

int GuideDocument::findNext(const juce::String& phrase,int after) const {
    const int count=(int)blocks.size();
    if (count==0 || phrase.trim().isEmpty()) return -1;
    for (int i=0;i<count;++i) {
        const int index=(after+1+i+count)%count;
        if (blocks[(size_t)index].text.containsIgnoreCase(phrase)) return index;
    }
    return -1;
}

int GuideDocument::findPrevious(const juce::String& phrase,int before) const {
    const int count=(int)blocks.size();
    if (count==0 || phrase.trim().isEmpty()) return -1;
    for (int i=0;i<count;++i) {
        const int index=(before-1-i+count*2)%count;
        if (blocks[(size_t)index].text.containsIgnoreCase(phrase)) return index;
    }
    return -1;
}

int GuideDocument::blockTop(int index) const {
    if (index<0 || index>=(int)blocks.size()) return 0;
    return (int)blocks[(size_t)index].top;
}

void GuideDocument::paint(juce::Graphics& g) {
    g.fillAll(palette::background);
    const auto clip=g.getClipBounds();
    for (const auto& block:blocks) {
        if (block.top+block.height<(float)clip.getY() ||
            block.top>(float)clip.getBottom()) continue;
        if (block.kind==Kind::rule) {
            g.setColour(palette::line);
            g.drawHorizontalLine((int)block.top+9,36.0f,(float)getWidth()-36);
        } else if (block.layout) {
            const int left=block.kind==Kind::list ? 52 : 36;
            if (block.kind==Kind::quote || block.kind==Kind::code) {
                const auto panel=juce::Rectangle<float>(28.0f,block.top-2.0f,
                    (float)getWidth()-56.0f,block.height-1.0f);
                g.setColour(palette::panel);
                g.fillRoundedRectangle(panel,4.0f);
                g.setColour(block.kind==Kind::quote ? palette::amber : palette::line);
                g.fillRect(28.0f,block.top-2.0f,3.0f,block.height-1.0f);
            }
            block.layout->draw(g,{(float)left,block.top,
                (float)getWidth()-(float)left-36.0f,block.height});
        }
    }
}

GuideViewer::GuideViewer(const juce::String& markdown) {
    viewport.setViewedComponent(&document,false);
    viewport.setScrollBarsShown(true,false);
    addAndMakeVisible(viewport);
    addAndMakeVisible(search);
    addAndMakeVisible(previous);
    addAndMakeVisible(next);
    addAndMakeVisible(close);
    search.setName("guideSearch");
    search.setFont(font(14.0f));
    search.setTextToShowWhenEmpty("Search the guide",palette::muted);
    search.onTextChange=[this] { lastMatch=-1;find(true); };
    search.onReturnKey=[this] { find(true); };
    previous.onClick=[this] { find(false); };
    next.onClick=[this] { find(true); };
    close.onClick=[this] {
        if (auto* dialog=findParentComponentOfClass<juce::DocumentWindow>())
            dialog->closeButtonPressed();
    };
    setMarkdown(markdown);
}

void GuideViewer::setMarkdown(const juce::String& markdown) {
    lastMatch=-1;
    document.setMarkdown(markdown);
    resized();
    viewport.setViewPosition(0,0);
}

void GuideViewer::find(bool forwards) {
    const auto query=search.getText().trim();
    const int match=forwards ? document.findNext(query,lastMatch)
                             : document.findPrevious(query,
                                  lastMatch<0 ? document.blockCount() : lastMatch);
    if (match>=0) {
        lastMatch=match;
        viewport.setViewPosition(0,juce::jmax(0,document.blockTop(match)-16));
    }
}

void GuideViewer::paint(juce::Graphics& g) {
    g.fillAll(palette::background);
    g.setColour(palette::panel);
    g.fillRect(0,0,getWidth(),64);
    g.setColour(palette::line);
    g.drawHorizontalLine(63,0.0f,(float)getWidth());
    label(g,"THE CUELIST / GUIDE",{22,13,215,35},19,palette::text,true);
}

void GuideViewer::resized() {
    const int w=getWidth(),h=getHeight();
    close.setBounds(w-77,17,61,30);
    next.setBounds(w-143,17,58,30);
    previous.setBounds(w-221,17,71,30);
    search.setBounds(250,17,juce::jmax(80,w-482),30);
    viewport.setBounds(0,64,w,juce::jmax(0,h-64));
    document.layoutAtWidth(juce::jmax(300,viewport.getWidth()-18));
}

GuideWindow::GuideWindow(const juce::String& markdown)
    : juce::DocumentWindow("The Cuelist - Guide",palette::background,
                           juce::DocumentWindow::closeButton) {
    setUsingNativeTitleBar(true);
    viewer=new GuideViewer(markdown);
    setContentOwned(viewer,true);
    setResizable(true,true);
    setResizeLimits(620,420,1600,1100);
    centreWithSize(980,740);
}

void GuideWindow::setMarkdown(const juce::String& markdown) {
    if (viewer) viewer->setMarkdown(markdown);
}

void GuideWindow::closeButtonPressed() {
    setVisible(false);
}

} // namespace cuelist
