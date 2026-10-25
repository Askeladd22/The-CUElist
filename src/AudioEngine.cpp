#include "AudioEngine.h"
#include <cmath>
namespace cuelist {
void RenderState::trigger(int id, bool row, bool stop) noexcept {
    for (auto& v : voices) {
        if (row ? (v.rowId != id || (!stop && !v.followsRow)) : v.cellId != id) continue;
        if (stop || v.behaviour == Behaviour::stop) { v.playing=false; v.position=0; continue; }
        if (!v.clip || v.clip->samples.getNumSamples()==0) continue;
        switch (v.behaviour) {
            case Behaviour::retrigger: case Behaviour::restart: v.position=0; v.playing=true; break;
            case Behaviour::pauseResume: v.playing=!v.playing; break;
            case Behaviour::continuePlaying: v.playing=true; break;
            case Behaviour::stop: break;
        }
    }
}
void RenderState::panic() noexcept { for (auto& v : voices) { v.playing=false; v.position=0; } }
void RenderState::render(float* const* outputs, int channels, int frames, double sampleRate) noexcept {
    for (int ch=0; ch<channels; ++ch) if (outputs[ch]) juce::FloatVectorOperations::clear(outputs[ch],frames);
    if (sampleRate<=0) return;
    for (auto& v : voices) {
        if (!v.playing || !v.clip) continue;
        const auto& audio=v.clip->samples;
        const int length=audio.getNumSamples(); if (length==0) { v.playing=false; continue; }
        const double step=v.clip->sampleRate/sampleRate;
        for (int i=0; i<frames && v.playing; ++i) {
            const int a=(int)v.position;
            const int b=a+1 < length ? a+1 : (v.loop ? 0 : a);
            const float fraction=(float)(v.position-a);
            for (int side=0; side<2; ++side) {
                int output=v.output+side;
                if (output>=channels || output<0 || !outputs[output]) continue;
                const float* data=audio.getReadPointer(std::min(side,audio.getNumChannels()-1));
                outputs[output][i]+=(data[a]+fraction*(data[b]-data[a]))*v.gain;
            }
            v.position+=step;
            if (v.position>=length) {
                if (v.loop) v.position=std::fmod(v.position,(double)length);
                else { v.position=0; v.playing=false; }
            }
        }
    }
    // Saturation is an emergency bound, not a mastering limiter.
    for (int ch=0; ch<channels; ++ch) if (outputs[ch])
        juce::FloatVectorOperations::clip(outputs[ch],outputs[ch],-1.0f,1.0f,frames);
}
juce::Result loadClip(const juce::File& file,std::shared_ptr<const AudioClip>& destination) {
    if (!file.existsAsFile()) return juce::Result::fail("File mancante: "+file.getFullPathName());
    if (!file.hasFileExtension("wav")) return juce::Result::fail("Questa versione importa file WAV mono o stereo.");
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(new juce::FileInputStream(file),true));
    if (!reader || reader->sampleRate <= 0 || reader->lengthInSamples <= 0 || reader->numChannels<1 || reader->numChannels>2)
        return juce::Result::fail("WAV non valido: richiesto un file mono o stereo non vuoto.");
    if (reader->lengthInSamples > (128*1024*1024)/(4*reader->numChannels))
        return juce::Result::fail("WAV troppo grande per il precaricamento: massimo 128 MB di PCM per file.");
    auto clip=std::make_shared<AudioClip>(); clip->sampleRate=reader->sampleRate;
    clip->samples.setSize((int)reader->numChannels,(int)reader->lengthInSamples);
    if (!reader->read(&clip->samples,0,clip->samples.getNumSamples(),0,true,true)) return juce::Result::fail("Lettura WAV non riuscita.");
    for (int ch=0; ch<clip->samples.getNumChannels(); ++ch)
        for (int i=0; i<clip->samples.getNumSamples(); ++i)
            if (!std::isfinite(clip->samples.getSample(ch,i))) return juce::Result::fail("WAV con campioni non finiti.");
    destination=std::move(clip); return juce::Result::ok();
}
std::unique_ptr<RenderState> makeRenderState(const Show& show,const ClipMap& clips) {
    auto state=std::make_unique<RenderState>();
    for (const auto& row : show.rows) for (size_t i=0; i<row.cells.size(); ++i) {
        if (show.columns[i].type!=ColumnType::audio) continue;
        const auto& c=row.cells[i]; const auto it=clips.find(c.id); if (it==clips.end()) continue;
        Voice v; v.cellId=c.id; v.rowId=row.id; v.clip=it->second; v.output=show.columns[i].output;
        v.gain=c.gain; v.loop=c.loop; v.followsRow=c.followsRow; v.behaviour=c.behaviour;
        state->voices.push_back(std::move(v));
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
void AudioEngine::publish(std::unique_ptr<RenderState> state) {
    state->revision=++revision;
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
        if (auto* next=pending.exchange(nullptr)) { retired.store(current); current=next; }
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
        if (current && command.revision==current->revision && command.epoch==epoch)
            current->trigger(command.id,command.row,command.stop);
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
}
}
