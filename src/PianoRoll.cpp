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
            g.setColour(i==owner.selected ? palette::text : palette::amber.darker(.3f)); g.drawRoundedRectangle(box,2,i==owner.selected ? 2.0f : .7f);
            if (box.getWidth()>26) label(g,juce::String(n.velocity),box.toNearestInt().reduced(4,0),9,palette::background);
        }
    }
    int hit(juce::Point<float> p) const {
        for (int i=(int)owner.clip.notes.size()-1;i>=0;--i) if (noteBox(owner.clip.notes[(size_t)i]).contains(p)) return i;
        return -1;
    }
    void mouseDown(const juce::MouseEvent& e) override {
        if (!e.mods.isLeftButtonDown()) return;
        grabKeyboardFocus(); owner.selected=hit(e.position); dragging=false;
        if (owner.selected>=0) {
            original=owner.clip.notes[(size_t)owner.selected];
            resizing=e.x>=noteBox(original).getRight()-7; dragging=true;
        }
        owner.selectionChanged(); repaint();
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override {
        if (!e.mods.isLeftButtonDown() || e.x<keysWidth || e.y<rulerHeight || hit(e.position)>=0) return;
        if (owner.clip.notes.size()>=10000) { owner.hint="Limite: 10.000 note per clip."; owner.repaint(); return; }
        const double step=owner.gridStep();
        const double beat=std::floor((e.x-keysWidth)/owner.pixelsPerBeat/step)*step;
        if (beat<0 || beat>=owner.clip.lengthBeats) return;
        MidiNote note; note.beat=beat; note.length=std::min(step,owner.clip.lengthBeats-beat);
        note.pitch=juce::jlimit(0,127,127-(e.y-rulerHeight)/noteHeight);
        note.velocity=(int)owner.velocity.getValue(); note.channel=owner.channel.getSelectedId();
        owner.clip.notes.push_back(note); owner.selected=(int)owner.clip.notes.size()-1; owner.selectionChanged(); repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (!dragging || owner.selected<0 || e.getDistanceFromDragStart()<3) return;
        auto& note=owner.clip.notes[(size_t)owner.selected]; const double step=owner.gridStep();
        const double delta=std::round(e.getDistanceFromDragStartX()/owner.pixelsPerBeat/step)*step;
        if (resizing) note.length=juce::jlimit(std::min(step,owner.clip.lengthBeats-note.beat),owner.clip.lengthBeats-note.beat,original.length+delta);
        else {
            note.beat=juce::jlimit(0.0,owner.clip.lengthBeats-original.length,original.beat+delta);
            note.pitch=juce::jlimit(0,127,original.pitch-(int)std::round((double)e.getDistanceFromDragStartY()/noteHeight));
        }
        owner.selectionChanged(); repaint();
    }
    void mouseUp(const juce::MouseEvent&) override { dragging=false; }
    bool keyPressed(const juce::KeyPress& key) override {
        if (key==juce::KeyPress::deleteKey || key==juce::KeyPress::backspaceKey) { owner.deleteSelected(); return true; }
        return false;
    }
private:
    PianoRollEditor& owner; MidiNote original; bool dragging=false,resizing=false;
};
PianoRollEditor::PianoRollEditor(MidiClip value,std::function<void(const MidiClip&)> callback)
    : clip(std::move(value)),commit(std::move(callback)),canvas(std::make_unique<Canvas>(*this)) {
    setLookAndFeel(&theme);
    for (juce::Component* c:std::initializer_list<juce::Component*>{&viewport,&snap,&channel,&velocity,&length,&remove,&zoomOut,&zoomIn,&apply,&cancel}) addAndMakeVisible(c);
    viewport.setViewedComponent(canvas.get(),false); viewport.setScrollBarsShown(true,true);
    viewport.onScroll=[this] { repaint(); };
    snap.addItemList({"1/4","1/8","1/16","1/32"},1); snap.setSelectedId(3);
    snap.setName("midiSnap"); channel.setName("midiChannel"); velocity.setName("midiVelocity"); length.setName("midiLength");
    for (int i=1;i<=16;++i) channel.addItem(juce::String(i),i); channel.setSelectedId(1);
    velocity.setSliderStyle(juce::Slider::LinearHorizontal); velocity.setTextBoxStyle(juce::Slider::TextBoxRight,false,46,24); velocity.setRange(1,127,1); velocity.setValue(100);
    length.setSliderStyle(juce::Slider::IncDecButtons); length.setTextBoxStyle(juce::Slider::TextBoxLeft,false,66,24); length.setRange(.25,4096,.25); length.setValue(clip.lengthBeats,juce::dontSendNotification);
    snap.onChange=[this] { canvas->repaint(); };
    channel.onChange=[this] { if (selected>=0) { clip.notes[(size_t)selected].channel=channel.getSelectedId(); selectionChanged(); } };
    velocity.onValueChange=[this] { if (selected>=0) { clip.notes[(size_t)selected].velocity=(int)velocity.getValue(); canvas->repaint(); } };
    length.onValueChange=[this] {
        double minimum=.25; for (const auto& n:clip.notes) minimum=std::max(minimum,n.beat+n.length);
        clip.lengthBeats=std::max(std::ceil(minimum*4)/4,length.getValue()); length.setValue(clip.lengthBeats,juce::dontSendNotification); updateCanvas();
    };
    remove.onClick=[this] { deleteSelected(); };
    auto zoom=[this](double factor) {
        const double left=std::max(0,viewport.getViewPositionX()-keysWidth)/pixelsPerBeat;
        pixelsPerBeat=juce::jlimit(20.0,320.0,pixelsPerBeat*factor); updateCanvas();
        viewport.setViewPosition((int)(left*pixelsPerBeat),viewport.getViewPositionY());
    };
    zoomOut.onClick=[zoom] { zoom(.5); }; zoomIn.onClick=[zoom] { zoom(2); };
    apply.onClick=[this] { auto result=validateMidiClip(clip); if (result.failed()) { hint=result.getErrorMessage(); repaint(); return; } commit(clip); close(); };
    cancel.onClick=[this] { close(); };
    for (auto* b:{&remove,&zoomOut,&zoomIn,&apply,&cancel}) b->setWantsKeyboardFocus(false);
    setSize(940,620); updateCanvas(); selectionChanged();
    const int pitch=clip.notes.empty() ? 64 : clip.notes.front().pitch;
    viewport.setViewPosition(0,std::max(0,rulerHeight+(127-pitch)*noteHeight-viewport.getHeight()/2));
}
PianoRollEditor::~PianoRollEditor() { viewport.setViewedComponent(nullptr,false); setLookAndFeel(nullptr); }
juce::Component& PianoRollEditor::getCanvas() { return *canvas; }
double PianoRollEditor::gridStep() const { return std::pow(.5,std::max(0,snap.getSelectedId()-1)); }
void PianoRollEditor::updateCanvas() { canvas->setSize(std::max(viewport.getWidth()-14,keysWidth+(int)std::ceil(clip.lengthBeats*pixelsPerBeat)+24),rulerHeight+128*noteHeight); canvas->repaint(); }
void PianoRollEditor::selectionChanged() {
    if (selected>=0) { const auto& n=clip.notes[(size_t)selected]; velocity.setValue(n.velocity,juce::dontSendNotification); channel.setSelectedId(n.channel,juce::dontSendNotification);
        hint="Nota "+juce::String(n.pitch)+"  |  Beat "+juce::String(n.beat+1,2)+"  |  Durata "+juce::String(n.length,2)+" beat";
    } else hint="Doppio clic: aggiungi nota  |  Trascina: sposta  |  Bordo destro: durata  |  Canc: elimina";
    remove.setEnabled(selected>=0); repaint();
}
void PianoRollEditor::deleteSelected() { if (selected<0) return; clip.notes.erase(clip.notes.begin()+selected); selected=-1; selectionChanged(); canvas->repaint(); }
void PianoRollEditor::close() { if (auto* dialog=findParentComponentOfClass<juce::DialogWindow>()) dialog->exitModalState(0); }
void PianoRollEditor::paint(juce::Graphics& g) {
    g.fillAll(palette::panel);
    label(g,"PIANO ROLL  /  note MIDI",{16,8,310,25},17,palette::amber,true);
    label(g,"Snap",{16,44,40,24},11,palette::muted); label(g,"Canale",{142,44,48,24},11,palette::muted);
    label(g,"Velocity",{274,44,50,24},11,palette::muted); label(g,"Beat clip",{510,44,58,24},11,palette::muted);
    g.saveState(); g.reduceClipRegion(16,80,getWidth()-48,24);
    label(g,"Beat",{20,80,50,24},10,palette::muted);
    const int scroll=viewport.getViewPositionX();
    const int first=std::max(0,(int)std::floor((scroll-keysWidth)/pixelsPerBeat));
    const int last=std::min((int)std::ceil(clip.lengthBeats),first+(int)(getWidth()/pixelsPerBeat)+2);
    for (int beat=first;beat<=last;++beat) {
        const int x=16+keysWidth+(int)(beat*pixelsPerBeat)-scroll;
        g.setColour(palette::line); g.drawVerticalLine(x,84,104);
        label(g,juce::String(beat+1),{x+4,80,48,24},10,palette::muted);
    }
    g.restoreState();
    label(g,hint,{16,getHeight()-58,getWidth()-32,22},11,palette::muted);
    label(g,"Tempo della cue. Uscita MIDI nelle utility; nessun sintetizzatore interno.",{16,getHeight()-32,getWidth()-220,22},10,palette::muted);
}
void PianoRollEditor::resized() {
    snap.setBounds(56,44,76,26); channel.setBounds(194,44,66,26); velocity.setBounds(328,44,172,26); length.setBounds(570,44,110,26);
    zoomOut.setBounds(getWidth()-88,8,32,26); zoomIn.setBounds(getWidth()-48,8,32,26); remove.setBounds(getWidth()-140,44,124,26);
    viewport.setBounds(16,106,getWidth()-32,getHeight()-170);
    cancel.setBounds(getWidth()-194,getHeight()-32,82,26); apply.setBounds(getWidth()-104,getHeight()-32,88,26);
    if (canvas) updateCanvas();
}
}
