#include "MidiClip.h"
#include <cmath>
#include <array>
namespace cuelist {
juce::Result validateMidiClip(const MidiClip& clip) {
    if (!std::isfinite(clip.lengthBeats) || clip.lengthBeats<=0 || clip.lengthBeats>4096 || clip.notes.size()>10000)
        return juce::Result::fail("Clip MIDI non valida: massimo 4096 beat e 10000 note.");
    for (const auto& n:clip.notes)
        if (!std::isfinite(n.beat) || !std::isfinite(n.length) || n.beat<0 || n.length<=0
            || n.beat+n.length>clip.lengthBeats+1.0e-9 || n.pitch<0 || n.pitch>127
            || n.velocity<1 || n.velocity>127 || n.channel<1 || n.channel>16)
            return juce::Result::fail("Nota MIDI non valida.");
    return juce::Result::ok();
}
juce::Result loadMidiClip(const juce::File& file, MidiClip& destination, juce::String& warning) {
    if (!file.existsAsFile() || file.getSize()>8*1024*1024) return juce::Result::fail("File MIDI assente o oltre 8 MB.");
    auto input=file.createInputStream(); juce::MidiFile midi; int type=-1;
    if (!input || !midi.readFrom(*input,false,&type) || type<0 || type>1 || midi.getTimeFormat()<=0)
        return juce::Result::fail("Richiesto MIDI standard tipo 0/1 con tempo PPQ (SMPTE non supportato).");
    MidiClip clip; bool discarded=false; double end=0; const double ppq=midi.getTimeFormat();
    for (int t=0;t<midi.getNumTracks();++t) {
        const auto* track=midi.getTrack(t);
        if (track->getNumEvents()>100000) return juce::Result::fail("Troppi eventi nella traccia MIDI.");
        std::array<std::vector<size_t>,2048> active;
        std::array<size_t,2048> consumed {};
        for (int i=0;i<track->getNumEvents();++i) {
            const auto* e=track->getEventPointer(i); const auto& m=e->message;
            if (!std::isfinite(m.getTimeStamp()) || m.getTimeStamp()<0 || m.getTimeStamp()/ppq>4096)
                return juce::Result::fail("Durata MIDI oltre 4096 beat o timestamp non valido.");
            end=std::max(end,m.getTimeStamp()/ppq);
            if (m.isNoteOn()) {
                active[(size_t)((m.getChannel()-1)*128+m.getNoteNumber())].push_back(clip.notes.size());
                clip.notes.push_back({m.getTimeStamp()/ppq,0,
                    m.getNoteNumber(),m.getVelocity(),m.getChannel()});
                if (clip.notes.size()>10000) return juce::Result::fail("Massimo 10000 note per clip MIDI.");
            } else if (m.isNoteOff()) {
                const auto key=(size_t)((m.getChannel()-1)*128+m.getNoteNumber());
                if (consumed[key]<active[key].size()) {
                    auto& note=clip.notes[active[key][consumed[key]++]]; note.length=m.getTimeStamp()/ppq-note.beat;
                } else discarded=true;
            } else if (!m.isEndOfTrackMetaEvent()) discarded=true;
        }
        for (size_t key=0;key<active.size();++key) if (consumed[key]!=active[key].size())
            return juce::Result::fail("Nota MIDI priva di una fine valida.");
    }
    clip.lengthBeats=end>0 ? end : 4;
    auto valid=validateMidiClip(clip); if (valid.failed()) return valid;
    destination=std::move(clip);
    warning=discarded ? "Importate solo le note. Tempo, controller e altri eventi ignorati; viene usato il tempo della cue." : juce::String();
    return juce::Result::ok();
}
}
