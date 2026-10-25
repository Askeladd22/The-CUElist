#include "AudioEngine.h"
#include <cmath>
namespace cuelist {
namespace {
void placeOnTimeline(Voice& v, double seconds) noexcept {
    v.delaySeconds=std::max(0.0,v.startSeconds-seconds);
    v.position=0;
    if (!v.clip || v.clip->sampleRate<=0 || v.clip->samples.getNumSamples()==0) return;
    const double elapsed=std::max(0.0,seconds-v.startSeconds);
    const double length=(double)v.clip->samples.getNumSamples();
    const double duration=length/v.clip->sampleRate;
    // Bound in seconds first: a finite seek can overflow when multiplied by Hz.
    if (v.loop || v.rowLoop) v.position=std::fmod(elapsed,duration)*v.clip->sampleRate;
    else v.position=elapsed>=duration ? length : elapsed*v.clip->sampleRate;
}
}
void RenderState::inheritPlayback(const RenderState& previous) noexcept {
    // Copy only scalar runtime state. Shared media ownership stays entirely on the UI thread.
    for (auto& v:voices) for (const auto& old:previous.voices)
        if (v.cellId==old.cellId && v.clip.get()==old.clip.get()) {
            v.position=old.position; v.playing=old.playing; v.delaySeconds=old.delaySeconds;
            v.rowMode=old.rowMode; v.pendingSeek=old.pendingSeek;
            if (v.rowMode && v.startSeconds!=old.startSeconds)
                for (const auto& row:previous.rows) if (row.id==v.rowId) placeOnTimeline(v,row.seconds);
            break;
        }
    for (auto& row:rows) for (const auto& old:previous.rows)
        if (row.id==old.id) { row=old; break; }
}
void RenderState::seekRow(int id,double seconds) noexcept {
    if (!std::isfinite(seconds)) return;
    seconds=std::max(0.0,seconds);
    bool running=false;
    for (auto& row:rows) if (row.id==id) {
        running=row.playing; row.seconds=seconds; row.pendingSeek=true; row.ended=false;
    }
    for (auto& v:voices) if (v.rowId==id && v.followsRow) {
        v.rowMode=true; v.pendingSeek=true; placeOnTimeline(v,seconds); v.playing=running;
    }
}
void RenderState::trigger(int id,bool row,bool stop) noexcept {
    // Natural completion is distinct from a paused row. Its next GO starts a
    // fresh timeline, while a paused row keeps its media cursor and delay.
    if (row) for (auto& clock:rows) if (clock.id==id && clock.ended && !clock.pendingSeek) {
        clock.seconds=0; clock.ended=false;
        for (auto& v:voices) if (v.rowId==id && v.followsRow) {
            v.position=0; v.delaySeconds=0; v.rowMode=false; v.pendingSeek=false;
        }
    }
    bool anyPlaying=false;
    if (row) for (const auto& v:voices)
        if (v.rowId==id && v.followsRow && v.playing
            && ((v.overrideBehaviour ? v.rowBehaviour : v.behaviour)==Behaviour::pauseResume
                || (v.overrideBehaviour ? v.rowBehaviour : v.behaviour)==Behaviour::pauseRestart)) anyPlaying=true;
    bool restartClock=false, stopClock=stop;
    for (auto& v:voices) {
        if (row ? (v.rowId!=id || (!stop && !v.followsRow)) : v.cellId!=id) continue;
        const auto behaviour=row && v.overrideBehaviour ? v.rowBehaviour : v.behaviour;
        if (stop || behaviour==Behaviour::stop) {
            v.playing=false; v.position=0; v.delaySeconds=0; v.pendingSeek=false; v.rowMode=false;
            stopClock=true; continue;
        }
        if (!v.clip || v.clip->samples.getNumSamples()==0) continue;
        if (!row) { v.delaySeconds=0; v.pendingSeek=false; }
        const bool wasRowMode=v.rowMode; v.rowMode=row;
        switch (behaviour) {
            case Behaviour::pauseRestart:
                if (row ? anyPlaying : v.playing) { v.playing=false; break; }
                // An explicit seek wins once, otherwise a paused voice restarts
                // from the row origin (including its offset), or sample zero.
                [[fallthrough]];
            case Behaviour::retrigger:
                if (!row || !v.pendingSeek) {
                    if (row) placeOnTimeline(v,0); else v.position=0;
                    restartClock=true;
                }
                v.playing=true; break;
            case Behaviour::pauseResume:
                if (row && !wasRowMode && !v.pendingSeek && v.position==0) placeOnTimeline(v,0);
                v.playing=row ? !anyPlaying : !v.playing; break;
            case Behaviour::continuePlaying:
                if (row && !wasRowMode && !v.pendingSeek && v.position==0) placeOnTimeline(v,0);
                v.playing=true; break;
            case Behaviour::stop: break;
        }
        v.pendingSeek=false;
    }
    if (row) for (auto& clock:rows) if (clock.id==id) {
        bool active=false;
        for (const auto& v:voices) if (v.rowId==id && v.rowMode && v.playing) active=true;
        if ((restartClock && !clock.pendingSeek) || (stopClock && !active)) clock.seconds=0;
        clock.playing=active; clock.pendingSeek=false;
        if (active || stopClock) clock.ended=false;
    }
}
void RenderState::panic() noexcept {
    for (auto& v:voices) { v.playing=false; v.position=0; v.delaySeconds=0; v.pendingSeek=false; v.rowMode=false; }
    for (auto& row:rows) { row.seconds=0; row.playing=false; row.pendingSeek=false; row.ended=false; }
}
void RenderState::render(float* const* outputs,int channels,int frames,double sampleRate) noexcept {
    for (int ch=0;ch<channels;++ch) if (outputs[ch]) juce::FloatVectorOperations::clear(outputs[ch],frames);
    if (sampleRate<=0 || !std::isfinite(sampleRate)) return;
    for (auto& row:rows) if (row.playing) row.seconds+=(double)frames/sampleRate;
    for (auto& v:voices) {
        if (!v.playing || !v.clip) continue;
        const auto& audio=v.clip->samples;
        const int length=audio.getNumSamples();
        if (length==0 || audio.getNumChannels()==0) { v.playing=false; continue; }
        const bool looping=v.loop || v.rowLoop;
        const double step=v.clip->sampleRate/sampleRate;
        const float gain=(v.rowMuted || v.columnMuted) ? 0
            : v.gain*v.rowGain*v.columnGain*(v.columnPhaseInverted ? -1.0f : 1.0f);
        const float sideGain[2] {gain*(1.0f-std::max(0.0f,v.columnPan)),
                                 gain*(1.0f+std::min(0.0f,v.columnPan))};
        for (int i=0;i<frames && v.playing;++i) {
            if (v.rowMode && v.delaySeconds>1.0e-12) { v.delaySeconds=std::max(0.0,v.delaySeconds-1.0/sampleRate); continue; }
            v.delaySeconds=0;
            if (v.position>=length) {
                if (looping) v.position=std::fmod(v.position,(double)length);
                else { v.position=0; v.playing=false; break; }
            }
            const int a=(int)v.position, b=a+1<length ? a+1 : (looping ? 0 : a);
            const float fraction=(float)(v.position-a);
            for (int side=0;side<2;++side) {
                const int output=v.output+side;
                if (output>=channels || output<0 || !outputs[output]) continue;
                const float* data=audio.getReadPointer(std::min(side,audio.getNumChannels()-1));
                outputs[output][i]+=(data[a]+fraction*(data[b]-data[a]))*sideGain[side];
            }
            v.position+=step;
            if (v.position>=length) {
                if (looping) v.position=std::fmod(v.position,(double)length);
                else { v.position=0; v.playing=false; }
            }
        }
    }
    for (auto& row:rows) {
        const bool wasPlaying=row.playing;
        row.playing=false;
        for (const auto& v:voices) if (v.rowId==row.id && v.rowMode && v.playing) { row.playing=true; break; }
        if (wasPlaying && !row.playing) row.ended=true;
    }
    // Saturation is an emergency bound, not a mastering limiter.
    for (int ch=0;ch<channels;++ch) if (outputs[ch])
        juce::FloatVectorOperations::clip(outputs[ch],outputs[ch],-1.0f,1.0f,frames);
}
juce::Result loadClip(const juce::File& file,std::shared_ptr<const AudioClip>& destination) {
    if (!file.existsAsFile()) return juce::Result::fail("File mancante: "+file.getFullPathName());
    if (!file.hasFileExtension("wav;mp3")) return juce::Result::fail("Importare un file WAV o MP3 mono o stereo.");
    juce::AudioFormatManager formats;
    formats.registerFormat(new juce::WavAudioFormat(),true);
    formats.registerFormat(new juce::MP3AudioFormat(),false);
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (!reader || reader->sampleRate <= 0 || reader->lengthInSamples <= 0 || reader->numChannels<1 || reader->numChannels>2)
        return juce::Result::fail("Audio non valido: richiesto un file mono o stereo non vuoto.");
    if (reader->lengthInSamples > (128*1024*1024)/(4*reader->numChannels))
        return juce::Result::fail("Audio troppo grande per il precaricamento: massimo 128 MB di PCM per file.");
    auto clip=std::make_shared<AudioClip>(); clip->sampleRate=reader->sampleRate;
    clip->samples.setSize((int)reader->numChannels,(int)reader->lengthInSamples);
    if (!reader->read(&clip->samples,0,clip->samples.getNumSamples(),0,true,true)) return juce::Result::fail("Lettura audio non riuscita.");
    for (int ch=0; ch<clip->samples.getNumChannels(); ++ch)
        for (int i=0; i<clip->samples.getNumSamples(); ++i)
            if (!std::isfinite(clip->samples.getSample(ch,i))) return juce::Result::fail("Audio con campioni non finiti.");
    destination=std::move(clip); return juce::Result::ok();
}
std::unique_ptr<RenderState> makeRenderState(const Show& show,const ClipMap& clips) {
    auto state=std::make_unique<RenderState>();
    for (const auto& row : show.rows) {
      state->rows.push_back({row.id});
      for (size_t i=0; i<row.cells.size() && i<show.columns.size(); ++i) {
        if (show.columns[i].type!=ColumnType::audio) continue;
        const auto& c=row.cells[i]; const auto it=clips.find(c.id); if (it==clips.end()) continue;
        Voice v; v.cellId=c.id; v.rowId=row.id; v.clip=it->second; v.output=show.columns[i].output;
        v.gain=c.gain; v.loop=c.loop; v.followsRow=c.followsRow; v.behaviour=c.behaviour;
        v.startSeconds=c.startSeconds; v.rowGain=row.gain; v.rowMuted=row.muted;
        v.columnGain=show.columns[i].gain; v.columnPan=show.columns[i].pan;
        v.columnMuted=show.columns[i].muted; v.columnPhaseInverted=show.columns[i].phaseInverted;
        v.rowLoop=row.loop; v.overrideBehaviour=row.overrideBehaviour; v.rowBehaviour=row.behaviour;
        state->voices.push_back(std::move(v));
      }
    }
    return state;
}
AudioEngine::AudioEngine() = default;
AudioEngine::~AudioEngine() {
    devices.removeAudioCallback(this); devices.closeAudioDevice();
    delete current; delete pending.exchange(nullptr); delete retired.exchange(nullptr);
}
juce::String AudioEngine::initialise() {
    auto error=devices.initialiseWithDefaultDevices(0,2); devices.addAudioCallback(this); return error;
}
void AudioEngine::collectRetired() { delete retired.exchange(nullptr); }
void AudioEngine::publish(std::unique_ptr<RenderState> state,bool preservePlayback) {
    if (!state) return;
    state->revision=++revision;
    if (!preservePlayback) structuralRevision=revision;
    state->minimumCommandRevision=structuralRevision;
    state->preservePlayback=preservePlayback;
    // exchange gives sole ownership of the displaced pending snapshot to the UI.
    delete pending.exchange(state.release());
}
bool AudioEngine::send(int id,bool row,bool stop) noexcept {
    const unsigned w=writeIndex.load(std::memory_order_relaxed);
    const unsigned next=(w+1)%commands.size();
    if (next==readIndex.load(std::memory_order_acquire)) return false;
    commands[w]={id,row,stop,revision,panicEpoch.load()};
    writeIndex.store(next,std::memory_order_release); return true;
}
bool AudioEngine::seekRow(int id,double seconds) noexcept {
    if (!std::isfinite(seconds)) return false;
    const unsigned w=writeIndex.load(std::memory_order_relaxed);
    const unsigned next=(w+1)%commands.size();
    if (next==readIndex.load(std::memory_order_acquire)) return false;
    commands[w]={id,true,false,revision,panicEpoch.load(),true,std::max(0.0,seconds)};
    writeIndex.store(next,std::memory_order_release); return true;
}
void AudioEngine::panic() noexcept { panicEpoch.fetch_add(1); }
void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device) {
    deviceSampleRate=device->getCurrentSampleRate(); connected.store(true); panic();
}
void AudioEngine::audioDeviceStopped() { connected.store(false); activeVoices.store(0); peak.store(0); panic(); }
void AudioEngine::audioDeviceIOCallbackWithContext(const float* const*,int,float* const* outputs,int channels,int frames,
                                                  const juce::AudioIODeviceCallbackContext&) {
    processBlock(outputs,channels,frames,deviceSampleRate);
}
void AudioEngine::processBlock(float* const* outputs,int channels,int frames,double sampleRate) noexcept {
    if (retired.load()==nullptr) {
        if (auto* next=pending.exchange(nullptr)) {
            if (current && next->preservePlayback && next->minimumCommandRevision==current->minimumCommandRevision)
                next->inheritPlayback(*current);
            retired.store(current); current=next;
        }
    }
    const auto epoch=panicEpoch.load();
    if (handledEpoch!=epoch) { if (current) current->panic(); handledEpoch=epoch; }
    auto r=readIndex.load(std::memory_order_relaxed);
    const auto w=writeIndex.load(std::memory_order_acquire);
    for (; r!=w; r=(r+1)%commands.size()) {
        const auto& command=commands[r];
        // A UI publication can happen after this block's snapshot check. Leave its
        // command queued until adoption; only stale revisions/epochs may be dropped.
        if (command.epoch==epoch && (!current || command.revision>current->revision)) break;
        if (current && command.revision>=current->minimumCommandRevision && command.epoch==epoch) {
            if (command.seek) current->seekRow(command.id,command.seconds);
            else current->trigger(command.id,command.row,command.stop);
        }
    }
    readIndex.store(r,std::memory_order_release);
    if (current) current->render(outputs,channels,frames,sampleRate);
    else for (int ch=0; ch<channels; ++ch) if (outputs[ch]) juce::FloatVectorOperations::clear(outputs[ch],frames);
    if (panicEpoch.load()!=epoch) {
        if (current) current->panic();
        for (int ch=0; ch<channels; ++ch) if (outputs[ch]) juce::FloatVectorOperations::clear(outputs[ch],frames);
    }
    float level=0; int active=0;
    for (int ch=0; ch<channels; ++ch) if (outputs[ch])
        for (int i=0; i<frames; ++i) level=std::max(level,std::abs(outputs[ch][i]));
    if (current) for (const auto& v : current->voices) if (v.playing) ++active;
    peak.store(level); activeVoices.store(active);
    const int monitored=monitoredRowId.load();
    double seconds=0; bool playing=false;
    if (current) for (const auto& row:current->rows) if (row.id==monitored) {
        seconds=row.seconds; playing=row.playing; break;
    }
    playheadSeconds.store(seconds); rowPlaying.store(playing);
}
}
