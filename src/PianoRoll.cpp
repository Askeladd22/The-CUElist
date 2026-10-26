#include "PianoRoll.h"
#include <cmath>

namespace cuelist {
namespace { constexpr int keysWidth=64, noteHeight=18, rulerHeight=26; }
class PianoRollEditor::Canvas final : public juce::Component {
public:
    explicit Canvas(PianoRollEditor& p) : owner(p) { setWantsKeyboardFocus(true); setName("pianoRollCanvas"); }
    juce::Rectangle<float> noteBox(const MidiNote& n) const {
        return {(float)(keysWidth+n.beat*owner.pixelsPerBeat), (float)(rulerHeight+(127-n.pitch)*noteHeight+1),
                (float)std::max(3.0,n.length*owner.pixelsPerBeat), (float)noteHeight-2};
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(palette::background);
        const auto visible=g.getClipBounds();
        for (int pitch=0;pitch<128;++pitch) {
            const int y=rulerHeight+(127-pitch)*noteHeight;
            if (y+noteHeight<visible.getY() || y>visible.getBottom()) continue;
            const int key=pitch%12; const bool black=key==1 || key==3 || key==6 || key==8 || key==10;
            g.setColour(black ? palette::background : palette::panel); g.fillRect(keysWidth,y,getWidth()-keysWidth,noteHeight);
            g.setColour(palette::line.withAlpha(.4f)); g.drawHorizontalLine(y,(float)keysWidth,(float)getWidth());
            g.setColour(black ? palette::raised : juce::Colour(0xffc7d4df)); g.fillRect(0,y,keysWidth-2,noteHeight-1);
            static const char* names[] {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
            label(g,juce::String(names[key])+juce::String(pitch/12-1),{5,y,52,noteHeight},10,black ? palette::text : palette::background);
        }
        const double step=owner.gridStep();
        const int first=std::max(0,(int)std::floor((visible.getX()-keysWidth)/owner.pixelsPerBeat/step));
        const int last=std::min((int)std::ceil(owner.clip.lengthBeats/step),(int)std::ceil((visible.getRight()-keysWidth)/owner.pixelsPerBeat/step));
        for (int i=first;i<=last;++i) {
            const double beat=i*step; const int x=keysWidth+(int)std::round(beat*owner.pixelsPerBeat);
            const bool whole=std::abs(beat-std::round(beat))<.001;
            g.setColour(palette::line.withAlpha(whole ? .9f : .35f)); g.drawVerticalLine(x,0,(float)getHeight());
        }
        for (int i=0;i<(int)owner.clip.notes.size();++i) {
            const auto& n=owner.clip.notes[(size_t)i]; const auto box=noteBox(n);
            if (!box.intersects(visible.toFloat())) continue;
            g.setColour(palette::amber.withAlpha(.35f+.6f*n.velocity/127.0f)); g.fillRoundedRectangle(box,2);
            const bool active=owner.selectedNotes.count(i)>0;
            g.setColour(active ? palette::text : palette::amber.darker(.3f)); g.drawRoundedRectangle(box,2,active ? 2.0f : .7f);
            if (box.getWidth()>26) label(g,juce::String(n.velocity),box.toNearestInt().reduced(4,0),9,palette::background);
        }
        if (selecting) {
            auto bounds=marquee();
            g.setColour(palette::teal.withAlpha(.13f)); g.fillRect(bounds);
            g.setColour(palette::teal); g.drawRect(bounds,1.0f);
        }
    }
    juce::Rectangle<float> marquee() const {
        return {std::min(marqueeStart.x,marqueeEnd.x),std::min(marqueeStart.y,marqueeEnd.y),
                std::abs(marqueeEnd.x-marqueeStart.x),std::abs(marqueeEnd.y-marqueeStart.y)};
    }
    int hit(juce::Point<float> p) const {
        for (int i=(int)owner.clip.notes.size()-1;i>=0;--i) if (noteBox(owner.clip.notes[(size_t)i]).contains(p)) return i;
        return -1;
    }
    void mouseDown(const juce::MouseEvent& e) override {
        grabKeyboardFocus(); dragging=false; selecting=false;
        if (e.mods.isRightButtonDown()) {
            selecting=true; marqueeStart=marqueeEnd=e.position;
            owner.selected=-1; owner.selectedNotes.clear();
            owner.selectionChanged(); repaint(); return;
        }
        if (!e.mods.isLeftButtonDown()) return;
        const int hitIndex=hit(e.position);
        if (e.mods.isShiftDown()) {
            if (hitIndex>=0) {
                if (!owner.selectedNotes.insert(hitIndex).second) owner.selectedNotes.erase(hitIndex);
            }
            owner.selected=owner.selectedNotes.empty() ? -1 : *owner.selectedNotes.rbegin();
            owner.selectionChanged(); repaint(); return;
        }
        if (hitIndex<0) {
            owner.selected=-1; owner.selectedNotes.clear();
            if (e.x>=keysWidth && e.y>=rulerHeight) {
                if (owner.clip.notes.size()>=10000) { owner.hint="Limit: 10,000 notes per clip."; repaint(); return; }
                const auto before=owner.clip;
                const double step=owner.gridStep();
                const double rawBeat=(e.x-keysWidth)/owner.pixelsPerBeat;
                const double beat=owner.snapEnabled() ? std::floor(rawBeat/step)*step : rawBeat;
                if (beat>=0 && beat<owner.clip.lengthBeats) {
                    MidiNote note;
                    note.beat=beat;
                    note.length=std::min(owner.snapEnabled() ? step : .25,owner.clip.lengthBeats-beat);
                    note.pitch=juce::jlimit(0,127,127-(e.y-rulerHeight)/noteHeight);
                    note.velocity=(int)owner.velocity.getValue(); note.channel=owner.channel.getSelectedId();
                    owner.clip.notes.push_back(note); owner.selected=(int)owner.clip.notes.size()-1;
                    owner.selectedNotes.insert(owner.selected);
                    owner.notify(before);
                }
            }
            owner.selectionChanged(); repaint(); return;
        }
        if (!owner.selectedNotes.count(hitIndex)) { owner.selectedNotes.clear(); owner.selectedNotes.insert(hitIndex); }
        owner.selected=hitIndex;
        gestureStart=owner.clip;
        original=owner.clip.notes[(size_t)hitIndex];
        resizing=e.x>=noteBox(original).getRight()-7;
        originals.clear();
        for (int index:owner.selectedNotes) originals.push_back({index,owner.clip.notes[(size_t)index]});
        dragging=true;
        owner.selectionChanged(); repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (selecting) {
            marqueeEnd=e.position;
            const auto rect=marquee();
            owner.selectedNotes.clear();
            for (int i=0;i<(int)owner.clip.notes.size();++i)
                if (noteBox(owner.clip.notes[(size_t)i]).intersects(rect)) owner.selectedNotes.insert(i);
            owner.selected=owner.selectedNotes.empty() ? -1 : *owner.selectedNotes.rbegin();
            owner.selectionChanged(); repaint(); return;
        }
        if (!dragging || owner.selected<0 || e.getDistanceFromDragStart()<3) return;
        const double step=owner.gridStep();
        const double raw=e.getDistanceFromDragStartX()/owner.pixelsPerBeat;
        double delta=owner.snapEnabled() ? std::round(raw/step)*step : raw;
        if (resizing) {
            auto& note=owner.clip.notes[(size_t)owner.selected];
            const double minimum=owner.snapEnabled() ? step : .01;
            note.length=juce::jlimit(std::min(minimum,owner.clip.lengthBeats-note.beat),
                                     owner.clip.lengthBeats-note.beat,original.length+delta);
        } else if (!originals.empty()) {
            double minDelta=-owner.clip.lengthBeats,maxDelta=owner.clip.lengthBeats;
            int minPitch=-127,maxPitch=127;
            for (const auto& pair:originals) {
                const auto& n=pair.second;
                minDelta=std::max(minDelta,-n.beat);
                maxDelta=std::min(maxDelta,owner.clip.lengthBeats-n.beat-n.length);
                minPitch=std::max(minPitch,-n.pitch);
                maxPitch=std::min(maxPitch,127-n.pitch);
            }
            delta=juce::jlimit(minDelta,maxDelta,delta);
            const int pitchDelta=juce::jlimit(minPitch,maxPitch,-(int)std::round((double)e.getDistanceFromDragStartY()/noteHeight));
            for (const auto& pair:originals) {
                auto& note=owner.clip.notes[(size_t)pair.first];
                note.beat=pair.second.beat+delta; note.pitch=pair.second.pitch+pitchDelta;
            }
        }
        owner.notify(gestureStart,false);
        owner.selectionChanged(); repaint();
    }
    void mouseUp(const juce::MouseEvent&) override {
        if (dragging && !selecting) owner.notify(gestureStart,true);
        dragging=false; selecting=false; repaint();
    }
    bool keyPressed(const juce::KeyPress& key) override {
        if (key.getModifiers().isCommandDown() && (key.getKeyCode()=='Z' || key.getKeyCode()=='z')) {
            if (key.getModifiers().isShiftDown()) { if (owner.onRedo) owner.onRedo(); }
            else if (owner.onUndo) owner.onUndo();
            return true;
        }
        if (key.getModifiers().isCommandDown() && (key.getKeyCode()=='Y' || key.getKeyCode()=='y')) {
            if (owner.onRedo) owner.onRedo(); return true;
        }
        if (key==juce::KeyPress::deleteKey || key==juce::KeyPress::backspaceKey) { owner.deleteSelected(); return true; }
        return false;
    }
private:
    PianoRollEditor& owner; MidiNote original; MidiClip gestureStart;
    std::vector<std::pair<int,MidiNote>> originals;
    juce::Point<float> marqueeStart,marqueeEnd;
    bool dragging=false,resizing=false,selecting=false;
};
PianoRollEditor::PianoRollEditor(MidiClip value,OnEdit callback)
    : clip(std::move(value)),onEdit(std::move(callback)),canvas(std::make_unique<Canvas>(*this)) {
    setLookAndFeel(&theme);
    setName("sharedPianoRoll");
    for (juce::Component* c:std::initializer_list<juce::Component*>{
        &viewport,&snap,&channel,&velocity,&length,&remove,&zoomOut,&zoomIn,&detach,&undo,&redo
    }) addAndMakeVisible(c);
    viewport.setViewedComponent(canvas.get(),false); viewport.setScrollBarsShown(true,true);
    viewport.onScroll=[this] { repaint(); };
    snap.addItemList({"1/4","1/8","1/16","1/32"},1); snap.addItem("Off",5);
    snap.setSelectedId(3);
    snap.setName("midiSnap"); channel.setName("midiChannel");
    velocity.setName("midiVelocity"); length.setName("midiLength");
    detach.setName("clipEditorDetach"); undo.setName("clipEditorUndo"); redo.setName("clipEditorRedo");
    for (int i=1;i<=16;++i) channel.addItem(juce::String(i),i);
    channel.setSelectedId(1);
    velocity.setSliderStyle(juce::Slider::LinearHorizontal);
    velocity.setTextBoxStyle(juce::Slider::TextBoxRight,false,46,24);
    velocity.setRange(1,127,1); velocity.setValue(100);
    length.setSliderStyle(juce::Slider::IncDecButtons);
    length.setTextBoxStyle(juce::Slider::TextBoxLeft,false,66,24);
    length.setRange(.25,4096,.25); length.setValue(clip.lengthBeats,juce::dontSendNotification);
    snap.onChange=[this] { canvas->repaint(); };
    channel.onChange=[this] {
        if (selectedNotes.empty()) return;
        auto before=clip;
        for (int index:selectedNotes) clip.notes[(size_t)index].channel=channel.getSelectedId();
        notify(before); selectionChanged(); canvas->repaint();
    };
    velocity.onValueChange=[this] {
        if (selectedNotes.empty()) return;
        auto before=clip;
        for (int index:selectedNotes) clip.notes[(size_t)index].velocity=(int)velocity.getValue();
        notify(before); canvas->repaint();
    };
    length.onValueChange=[this] {
        auto before=clip;
        double minimum=.25;
        for (const auto& n:clip.notes) minimum=std::max(minimum,n.beat+n.length);
        clip.lengthBeats=std::max(std::ceil(minimum*4)/4,length.getValue());
        length.setValue(clip.lengthBeats,juce::dontSendNotification);
        updateCanvas(); notify(before);
    };
    remove.onClick=[this] { deleteSelected(); };
    auto zoom=[this](double factor) {
        const double left=std::max(0,viewport.getViewPositionX()-keysWidth)/pixelsPerBeat;
        pixelsPerBeat=juce::jlimit(20.0,320.0,pixelsPerBeat*factor); updateCanvas();
        viewport.setViewPosition((int)(left*pixelsPerBeat),viewport.getViewPositionY());
    };
    zoomOut.onClick=[zoom] { zoom(.5); }; zoomIn.onClick=[zoom] { zoom(2); };
    detach.onClick=[this] { if (onDetach) onDetach(); };
    undo.onClick=[this] { if (onUndo) onUndo(); };
    redo.onClick=[this] { if (onRedo) onRedo(); };
    for (auto* b:{&remove,&zoomOut,&zoomIn,&detach,&undo,&redo}) b->setWantsKeyboardFocus(false);
    setSize(940,620); updateCanvas(); selectionChanged();
    const int pitch=clip.notes.empty() ? 64 : clip.notes.front().pitch;
    viewport.setViewPosition(0,std::max(0,rulerHeight+(127-pitch)*noteHeight-viewport.getHeight()/2));
}
PianoRollEditor::~PianoRollEditor() { viewport.setViewedComponent(nullptr,false); setLookAndFeel(nullptr); }
juce::Component& PianoRollEditor::getCanvas() { return *canvas; }
double PianoRollEditor::gridStep() const { return snapEnabled() ? std::pow(.5,std::max(0,snap.getSelectedId()-1)) : .25; }
bool PianoRollEditor::snapEnabled() const { return snap.getSelectedId()!=5; }
void PianoRollEditor::updateCanvas() { canvas->setSize(std::max(viewport.getWidth()-14,keysWidth+(int)std::ceil(clip.lengthBeats*pixelsPerBeat)+24),rulerHeight+128*noteHeight); canvas->repaint(); }
void PianoRollEditor::setFloating(bool floating) {
    detach.setButtonText(floating ? "Reattach" : "Detach");
}
void PianoRollEditor::notify(const MidiClip& before,bool complete) {
    if (!onEdit) return;
    const auto valid=validateMidiClip(clip);
    if (valid.failed()) { hint=valid.getErrorMessage(); repaint(); return; }
    onEdit(before,clip,complete);
}
void PianoRollEditor::loadClip(const MidiClip& replacement) {
    clip=replacement; selected=-1; selectedNotes.clear();
    length.setValue(clip.lengthBeats,juce::dontSendNotification);
    updateCanvas(); selectionChanged();
}
void PianoRollEditor::selectionChanged() {
    if (selected>=0) { const auto& n=clip.notes[(size_t)selected]; velocity.setValue(n.velocity,juce::dontSendNotification); channel.setSelectedId(n.channel,juce::dontSendNotification);
        hint=selectedNotes.size()>1 ? juce::String((int)selectedNotes.size())+" notes selected (drag, change velocity/channel or Delete)"
            : "Note "+juce::String(n.pitch)+"  |  Beat "+juce::String(n.beat+1,2)+"  |  Durata "+juce::String(n.length,2)+" beat";
    } else hint="Click: add note  |  Right-drag: select  |  Shift+click: add  |  Delete: remove";
    remove.setEnabled(!selectedNotes.empty()); repaint();
}
void PianoRollEditor::deleteSelected() {
    if (selectedNotes.empty()) return;
    auto before=clip;
    for (auto it=selectedNotes.rbegin();it!=selectedNotes.rend();++it) clip.notes.erase(clip.notes.begin()+*it);
    selectedNotes.clear(); selected=-1; notify(before); selectionChanged(); canvas->repaint();
}
void PianoRollEditor::paint(juce::Graphics& g) {
    g.fillAll(palette::panel);
    label(g,"PIANO ROLL  /  MIDI notes",{16,6,290,25},15,palette::amber,true);
    label(g,"Snap",{16,38,40,22},11,palette::muted);
    label(g,"Channel",{142,38,48,22},11,palette::muted);
    label(g,"Velocity",{270,38,50,22},11,palette::muted);
    label(g,"Beat",{504,38,42,22},11,palette::muted);
    g.saveState(); g.reduceClipRegion(16,67,getWidth()-32,17);
    const int scroll=viewport.getViewPositionX();
    const int first=std::max(0,(int)std::floor((scroll-keysWidth)/pixelsPerBeat));
    const int last=std::min((int)std::ceil(clip.lengthBeats),first+(int)(getWidth()/pixelsPerBeat)+2);
    for (int beat=first;beat<=last;++beat) {
        const int x=16+keysWidth+(int)(beat*pixelsPerBeat)-scroll;
        g.setColour(palette::line); g.drawVerticalLine(x,67,82);
        label(g,juce::String(beat+1),{x+4,67,48,17},10,palette::muted);
    }
    g.restoreState();
    label(g,hint,{16,getHeight()-25,getWidth()-32,22},11,palette::muted);
}
void PianoRollEditor::resized() {
    const int w=getWidth(),h=getHeight();
    snap.setBounds(54,35,80,26); channel.setBounds(190,35,67,26);
    velocity.setBounds(325,35,166,26); length.setBounds(546,35,96,26);
    remove.setBounds(std::max(650,w-333),35,89,26);
    detach.setBounds(w-96,4,86,26);
    undo.setBounds(w-185,4,74,26); redo.setBounds(w-269,4,74,26);
    zoomOut.setBounds(w-90,35,32,26); zoomIn.setBounds(w-50,35,32,26);
    viewport.setBounds(16,85,std::max(20,w-32),std::max(20,h-112));
    if (canvas) updateCanvas();
}
}
