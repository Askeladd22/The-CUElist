#include "AudioEngine.h"
#include <cmath>
namespace cuelist {
static_assert(std::atomic<double>::is_always_lock_free && std::atomic<uint64_t>::is_always_lock_free
              && std::atomic<unsigned>::is_always_lock_free, "Playback telemetry requires lock-free atomics");
CellPlaybackInfo CellPlaybackTelemetry::read() const noexcept {
    for (;;) {
        const auto before=sequence.load();
        if (before & 1) continue;
        CellPlaybackInfo result;
        result.positionSeconds=positionSeconds.load(); result.durationSeconds=durationSeconds.load();
        const auto state=flags.load();
        result.playing=(state & 1)!=0; result.waiting=(state & 2)!=0; result.paused=(state & 4)!=0;
        if (sequence.load()==before) return result;
    }
}
void CellPlaybackTelemetry::write(CellPlaybackInfo value) noexcept {
    sequence.fetch_add(1);
    positionSeconds.store(value.positionSeconds); durationSeconds.store(value.durationSeconds);
    flags.store((value.playing ? 1u : 0u) | (value.waiting ? 2u : 0u) | (value.paused ? 4u : 0u));
    sequence.fetch_add(1);
}
void RenderState::updateTelemetry() noexcept {
    for (const auto& v:voices) if (v.telemetry) {
        const double rate=(v.isMidi || v.isOsc) ? 1 : (v.clip ? v.clip->sampleRate : 0);
        v.telemetry->write({rate>0 ? v.position/rate : 0,
            v.isMidi ? v.midiDuration : v.isOsc ? v.oscDuration : (rate>0 ? v.clip->samples.getNumSamples()/rate : 0),
            v.playing, v.playing && v.rowMode && v.delaySeconds>1.0e-12, v.paused});
    }
}
namespace {
void placeOnTimeline(Voice& v, double seconds) noexcept {
    v.delaySeconds=std::max(0.0,v.startSeconds-seconds);
    v.position=0;
    if (v.isOsc) {
        const double elapsed=std::max(0.0,seconds-v.startSeconds);
        v.position=(v.loop || v.rowLoop) ? std::fmod(elapsed,v.oscDuration) : std::min(elapsed,v.oscDuration);
        v.oscNeedsSync=true; return;
    }
    if (v.isMidi) {
        const double elapsed=std::max(0.0,seconds-v.startSeconds);
        v.position=(v.loop || v.rowLoop) ? std::fmod(elapsed,v.midiDuration) : std::min(elapsed,v.midiDuration);
        v.midiNeedsSync=true; return;
    }
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
    limiterGain=previous.limiterGain;
    // Copy only scalar runtime state. Shared media ownership stays entirely on the UI thread.
    for (auto& v:voices) for (const auto& old:previous.voices)
        if (v.cellId==old.cellId && v.clip.get()==old.clip.get() && v.isMidi==old.isMidi && v.isOsc==old.isOsc
            && (!v.isMidi || (v.midiHash==old.midiHash && v.midiOutputId==old.midiOutputId))
            && (!v.isOsc || (v.oscHash==old.oscHash && v.oscColumnId==old.oscColumnId))) {
            v.position=old.position; v.playing=old.playing; v.paused=old.paused; v.delaySeconds=old.delaySeconds;
            v.rowMode=old.rowMode; v.pendingSeek=old.pendingSeek;
            v.heldNotes=old.heldNotes; v.midiCursor=old.midiCursor; v.midiNeedsSync=old.midiNeedsSync;
            v.heldVelocity=old.heldVelocity;
            v.oscCursor=old.oscCursor; v.oscNeedsSync=old.oscNeedsSync;
            if (v.rowMode && v.startSeconds!=old.startSeconds)
                for (const auto& row:previous.rows) if (row.id==v.rowId) placeOnTimeline(v,row.seconds);
            break;
        }
    for (auto& row:rows) for (const auto& old:previous.rows)
        if (row.id==old.id) { const bool stopOnNext=row.stopOnNext; row=old; row.stopOnNext=stopOnNext; break; }
}
void RenderState::seekRow(int id,double seconds) noexcept {
    if (!std::isfinite(seconds)) return;
    seconds=std::max(0.0,seconds);
    bool running=false;
    for (auto& row:rows) if (row.id==id) {
        running=row.playing; row.seconds=seconds; row.pendingSeek=true; row.ended=false;
    }
    for (auto& v:voices) if (v.rowId==id && v.followsRow) {
        releaseMidi(v);
        v.rowMode=true; v.pendingSeek=true; placeOnTimeline(v,seconds); v.playing=running;
        if (running) v.paused=false;
    }
}
void RenderState::trigger(int id,bool row,bool stop) noexcept {
    // A valid row GO is one callback transaction, including empty cues. Direct
    // launches and explicit Stops never change another row's transport.
    if (row && !stop) {
        bool exists=false;
        for (const auto& target:rows) if (target.id==id) { exists=true; break; }
        if (exists) {
            // Stop participating voices first. Protected voices keep their
            // audio/MIDI/OSC transport and continue through subsequent GO events.
            for (auto& v:voices) if (v.rowId!=id && v.stopOnNext) {
                releaseMidi(v);
                v.playing=false; v.paused=false; v.position=0; v.delaySeconds=0;
                v.pendingSeek=false; v.rowMode=false;
            }
            for (auto& other:rows) if (other.id!=id && other.stopOnNext) {
                bool survivor=false;
                for (const auto& v:voices)
                    if (v.rowId==other.id && v.rowMode && v.playing) { survivor=true; break; }
                // Keep the timeline running when a protected row-mode clip
                // survives; otherwise reset the old cue transport as before.
                if (!survivor) {
                    other.seconds=0; other.playing=false;
                    other.pendingSeek=false; other.ended=false;
                }
            }
        }
    }
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
            releaseMidi(v);
            v.playing=false; v.paused=false; v.position=0; v.delaySeconds=0; v.pendingSeek=false; v.rowMode=false;
            stopClock=true; continue;
        }
        if (!v.isMidi && !v.isOsc && (!v.clip || v.clip->samples.getNumSamples()==0)) continue;
        if (!row) { v.delaySeconds=0; v.pendingSeek=false; }
        const bool wasRowMode=v.rowMode; v.rowMode=row;
        switch (behaviour) {
            case Behaviour::pauseRestart:
                if (row ? anyPlaying : v.playing) { releaseMidi(v); v.playing=false; break; }
                // An explicit seek wins once, otherwise a paused voice restarts
                // from the row origin (including its offset), or sample zero.
                [[fallthrough]];
            case Behaviour::retrigger:
                releaseMidi(v);
                if (!row || !v.pendingSeek) {
                    if (row) placeOnTimeline(v,0); else v.position=0;
                    restartClock=true;
                }
                v.playing=true; break;
            case Behaviour::pauseResume:
                if (row ? anyPlaying : v.playing) releaseMidi(v);
                if (row && !wasRowMode && !v.pendingSeek && v.position==0) placeOnTimeline(v,0);
                v.playing=row ? !anyPlaying : !v.playing; break;
            case Behaviour::continuePlaying:
                if (row && !wasRowMode && !v.pendingSeek && v.position==0) placeOnTimeline(v,0);
                v.playing=true; break;
            case Behaviour::stop: break;
        }
        v.paused=!v.playing;
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
    for (auto& v:voices) { releaseMidi(v); v.playing=false; v.paused=false; v.position=0; v.delaySeconds=0; v.pendingSeek=false; v.rowMode=false; }
    for (auto& row:rows) { row.seconds=0; row.playing=false; row.pendingSeek=false; row.ended=false; }
}
void RenderState::releaseMidi(Voice& v,int offset,bool cancelPending) noexcept {
    if (v.isOsc && v.oscStream) {
        v.oscStream->epoch.fetch_add(1);
        v.oscNeedsSync=true;
    }
    if (!v.isMidi) return;
    if (cancelPending && v.midiStream) v.midiStream->epoch.fetch_add(1);
    if (v.midiSink) v.midiSink->selectStream(v.midiStream.get());
    for (size_t key=0;key<v.heldNotes.size();++key) if (v.heldNotes[key]) {
        if (v.midiSink) v.midiSink->emit(v.midiRoute,0x80+(int)(key/128),(int)(key%128),0,offset);
        v.heldNotes[key]=0;
    }
    v.midiNeedsSync=true;
}
void RenderState::render(float* const* outputs,int channels,int frames,double sampleRate) noexcept {
    for (int ch=0;ch<channels;++ch) if (outputs[ch]) juce::FloatVectorOperations::clear(outputs[ch],frames);
    preLimiterPeakLeft=preLimiterPeakRight=0;
    limiterReducing=false;
    // The stereo bus is accumulated independently of all physical outputs.
    // No heap allocation or lock occurs in the audio callback.
    const auto busSamples=std::min((size_t)std::max(0,frames),masterMix[0].size());
    for(auto& channel:masterMix) std::fill_n(channel.begin(),busSamples,0.0f);
    // Reset preallocated stereo mix buffers. Per-voice contributions are
    // accumulated into each column separately, even if multiple columns
    // share a physical output pair. No heap activity on the audio thread.
    for (auto& meter:columnMeters) if (meter.second) {
        auto& t=*meter.second;
        const auto used=std::min((size_t)std::max(0,frames),t.mix[0].size());
        for (auto& channel:t.mix) std::fill_n(channel.begin(),used,0.0f);
        t.oversizeVoicePeak={{0.0f,0.0f}};
        t.peak.store(0.0f,std::memory_order_relaxed);
        t.left.store(0.0f,std::memory_order_relaxed);
        t.right.store(0.0f,std::memory_order_relaxed);
    }
    if (sampleRate<=0 || !std::isfinite(sampleRate)) return;
    for (auto& row:rows) if (row.playing) row.seconds+=(double)frames/sampleRate;
    for (auto& v:voices) {
        if (v.isMidi && v.playing) {
            if (v.midiSink) v.midiSink->selectStream(v.midiStream.get());
            for (int i=0;i<frames && v.playing;++i) {
                if (v.rowMode && v.delaySeconds>1.0e-12) { v.delaySeconds=std::max(0.0,v.delaySeconds-1.0/sampleRate); continue; }
                if (v.position>=v.midiDuration) {
                    releaseMidi(v,i,false);
                    if (v.loop || v.rowLoop) v.position=std::fmod(v.position,v.midiDuration);
                    else { v.position=0; v.playing=false; break; }
                }
                if (v.midiNeedsSync) {
                    v.midiCursor=0; v.heldNotes.fill(0);
                    while (v.midiCursor<v.midiEvents.size() && v.midiEvents[v.midiCursor].seconds<v.position-1.0e-10) {
                        const auto& e=v.midiEvents[v.midiCursor++]; auto& count=v.heldNotes[(size_t)((e.status&15)*128+e.pitch)];
                        if ((e.status&0xf0)==0x90) { ++count; v.heldVelocity[(size_t)((e.status&15)*128+e.pitch)]=(unsigned char)e.velocity; } else if (count) --count;
                    }
                    if (!v.rowMuted) for (size_t key=0;key<v.heldNotes.size();++key) if (v.heldNotes[key] && v.midiSink)
                        v.midiSink->emit(v.midiRoute,0x90+(int)(key/128),(int)(key%128),v.heldVelocity[key],i);
                    v.midiNeedsSync=false;
                }
                while (v.midiCursor<v.midiEvents.size() && v.midiEvents[v.midiCursor].seconds<=v.position+1.0e-10) {
                    const auto& e=v.midiEvents[v.midiCursor++]; auto& count=v.heldNotes[(size_t)((e.status&15)*128+e.pitch)];
                    const bool on=(e.status&0xf0)==0x90;
                    if (on) ++count; else if (count) --count;
                    if (v.midiSink && !v.rowMuted && (on || count==0)) v.midiSink->emit(v.midiRoute,e.status,e.pitch,e.velocity,i);
                }
                v.position+=1.0/sampleRate;
                if (v.position>=v.midiDuration-1.0e-10) {
                    releaseMidi(v,i+1,false);
                    if (v.loop || v.rowLoop) v.position=std::max(0.0,v.position-v.midiDuration);
                    else { v.position=0; v.playing=false; }
                }
            }
            continue;
        }
        if (v.isOsc && v.playing) {
            if (v.oscSink) v.oscSink->selectStream(v.oscStream.get());
            for (int i=0;i<frames && v.playing;++i) {
                if (v.rowMode && v.delaySeconds>1.0e-12) {
                    v.delaySeconds=std::max(0.0,v.delaySeconds-1.0/sampleRate); continue;
                }
                if (v.position>=v.oscDuration) {
                    if (v.loop || v.rowLoop) { v.position=std::fmod(v.position,v.oscDuration); v.oscNeedsSync=true; }
                    else { v.position=0; v.playing=false; break; }
                }
                if (v.oscNeedsSync) {
                    v.oscCursor=0;
                    while (v.oscCursor<v.oscEvents.size() && v.oscEvents[v.oscCursor].seconds<v.position-1.0e-10) ++v.oscCursor;
                    v.oscNeedsSync=false;
                }
                while (v.oscCursor<v.oscEvents.size() && v.oscEvents[v.oscCursor].seconds<=v.position+1.0e-10) {
                    const auto& e=v.oscEvents[v.oscCursor++];
                    if (v.oscSink && !v.rowMuted && !v.columnMuted)
                        v.oscSink->emit(v.oscRoute,e.bytes.data(),(unsigned)e.bytes.size(),i);
                }
                v.position+=1.0/sampleRate;
                if (v.position>=v.oscDuration-1.0e-10) {
                    // Events at the exact end (notably automation's final
                    // value) must be emitted once before a natural completion
                    // or loop wraps; otherwise the sample loop would never
                    // visit the endpoint timestamp.
                    while (v.oscCursor<v.oscEvents.size()
                           && v.oscEvents[v.oscCursor].seconds<=v.oscDuration+1.0e-9) {
                        const auto& e=v.oscEvents[v.oscCursor++];
                        if (v.oscSink && !v.rowMuted && !v.columnMuted)
                            v.oscSink->emit(v.oscRoute,e.bytes.data(),(unsigned)e.bytes.size(),i);
                    }
                    if (v.loop || v.rowLoop) { v.position=std::max(0.0,v.position-v.oscDuration); v.oscNeedsSync=true; }
                    else { v.position=0; v.playing=false; }
                }
            }
            continue;
        }
        if (!v.playing || !v.clip) continue;
        const auto& audio=v.clip->samples;
        const int length=audio.getNumSamples();
        if (length==0 || audio.getNumChannels()==0) { v.playing=false; v.paused=false; continue; }
        const bool looping=v.loop || v.rowLoop;
        const double step=v.clip->sampleRate/sampleRate;
        // PRE direct out bypasses only the column fader. It retains clip/row
        // gain, strip pan, polarity and mute. Master Send is always POST.
        const float base=(v.rowMuted || v.columnMuted) ? 0
            : v.gain*v.rowGain*(v.columnPhaseInverted ? -1.0f : 1.0f);
        const float postGain[2] {
            base*v.columnGain*(1.0f-std::max(0.0f,v.columnPan)),
            base*v.columnGain*(1.0f+std::min(0.0f,v.columnPan))
        };
        const float directGain[2] {
            base*(v.directPostFader ? v.columnGain : 1.0f)*(1.0f-std::max(0.0f,v.columnPan)),
            base*(v.directPostFader ? v.columnGain : 1.0f)*(1.0f+std::min(0.0f,v.columnPan))
        };
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
                const float* data=audio.getReadPointer(std::min(side,audio.getNumChannels()-1));
                const float sample=data[a]+fraction*(data[b]-data[a]);
                const float post=sample*postGain[side];
                if (v.masterSend && (size_t)i<busSamples)
                    masterMix[(size_t)side][(size_t)i]+=post;
                if (v.directOutputEnabled) {
                    const int output=v.output+side;
                    if (output>=0 && output<channels && outputs[output])
                        outputs[output][i]+=sample*directGain[side];
                }
                if (v.columnMeter) {
                    auto& telemetry=*v.columnMeter;
                    if ((size_t)i<telemetry.mix[(size_t)side].size())
                        telemetry.mix[(size_t)side][(size_t)i]+=post;
                    else
                        telemetry.oversizeVoicePeak[(size_t)side]=std::max(
                            telemetry.oversizeVoicePeak[(size_t)side],std::abs(post));
                }
            }
            v.position+=step;
            if (v.position>=length) {
                if (looping) v.position=std::fmod(v.position,(double)length);
                else { v.position=0; v.playing=false; }
            }
        }
    }
    for (auto& meter:columnMeters) if (meter.second) {
        auto& t=*meter.second;
        const auto used=std::min((size_t)std::max(0,frames),t.mix[0].size());
        float peaks[2]={t.oversizeVoicePeak[0],t.oversizeVoicePeak[1]};
        for (int ch=0;ch<2;++ch)
            for (size_t i=0;i<used;++i)
                peaks[ch]=std::max(peaks[ch],std::abs(t.mix[(size_t)ch][i]));
        t.left.store(peaks[0],std::memory_order_relaxed);
        t.right.store(peaks[1],std::memory_order_relaxed);
        t.peak.store(std::max(peaks[0],peaks[1]),std::memory_order_relaxed);
    }
    for (auto& row:rows) {
        const bool wasPlaying=row.playing;
        row.playing=false;
        for (const auto& v:voices) if (v.rowId==row.id && v.rowMode && v.playing) { row.playing=true; break; }
        if (wasPlaying && !row.playing) row.ended=true;
    }
    // Dedicated stereo Master Bus: gain/pan/mute only affect L/R sends.
    // A fixed -1 dBFS peak safety limiter uses instantaneous attack and a
    // 150 ms exponential release, with linked L/R gain to preserve imaging.
    // This is a transparent peak guard, not a loudness maximizer.
    constexpr float threshold=0.89125094f; // -1 dBFS
    const float release=std::exp(-1.0f/(0.150f*(float)sampleRate));
    for(size_t i=0;i<busSamples;++i) {
        float left=masterMuted ? 0.0f : masterMix[0][i]*masterGain*(1.0f-std::max(0.0f,masterPan));
        float right=masterMuted ? 0.0f : masterMix[1][i]*masterGain*(1.0f+std::min(0.0f,masterPan));
        preLimiterPeakLeft=std::max(preLimiterPeakLeft,std::abs(left));
        preLimiterPeakRight=std::max(preLimiterPeakRight,std::abs(right));
        if(safetyLimiterEnabled) {
            const float absolutePeak=std::max(std::abs(left),std::abs(right));
            const float desired=absolutePeak>threshold ? threshold/absolutePeak : 1.0f;
            limiterGain=std::min(desired,1.0f+(limiterGain-1.0f)*release);
            if(limiterGain<0.9999f) limiterReducing=true;
            left*=limiterGain;
            right*=limiterGain;
        } else limiterGain=1.0f;
        const int physicalLeft=masterOutput;
        if(physicalLeft>=0 && physicalLeft<channels && outputs[physicalLeft])
            outputs[physicalLeft][i]+=left;
        if(physicalLeft+1>=0 && physicalLeft+1<channels && outputs[physicalLeft+1])
            outputs[physicalLeft+1][i]+=right;
    }
    // Final inter-pair safety clamp. When a direct output shares the physical
    // Master pair, their sum may exceed the separately limited Master Bus.
    // Preflight reports that collision instead of suggesting limiter immunity.
    for(int ch=0;ch<channels;++ch) if(outputs[ch])
        juce::FloatVectorOperations::clip(outputs[ch],outputs[ch],-1.0f,1.0f,frames);
}
juce::Result loadClip(const juce::File& file,std::shared_ptr<const AudioClip>& destination) {
    if (!file.existsAsFile()) return juce::Result::fail("File missing: "+file.getFullPathName());
    if (!file.hasFileExtension("wav;mp3")) return juce::Result::fail("Import a mono or stereo WAV or MP3 file.");
    juce::AudioFormatManager formats;
    formats.registerFormat(new juce::WavAudioFormat(),true);
    formats.registerFormat(new juce::MP3AudioFormat(),false);
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (!reader || reader->sampleRate <= 0 || reader->lengthInSamples <= 0 || reader->numChannels<1 || reader->numChannels>2)
        return juce::Result::fail("Invalid audio: a non-empty mono or stereo file is required.");
    if (reader->lengthInSamples > (128*1024*1024)/(4*reader->numChannels))
        return juce::Result::fail("Audio too large to preload: maximum 128 MB PCM per file.");
    auto clip=std::make_shared<AudioClip>(); clip->sampleRate=reader->sampleRate;
    clip->samples.setSize((int)reader->numChannels,(int)reader->lengthInSamples);
    if (!reader->read(&clip->samples,0,clip->samples.getNumSamples(),0,true,true)) return juce::Result::fail("Could not read audio.");
    for (int ch=0; ch<clip->samples.getNumChannels(); ++ch)
        for (int i=0; i<clip->samples.getNumSamples(); ++i)
            if (!std::isfinite(clip->samples.getSample(ch,i))) return juce::Result::fail("Audio contains non-finite samples.");
    destination=std::move(clip); return juce::Result::ok();
}
std::unique_ptr<RenderState> makeRenderState(const Show& show,const ClipMap& clips) {
    auto state=std::make_unique<RenderState>();
    state->masterGain=show.masterGain;
    state->masterPan=show.masterPan;
    state->masterMuted=show.masterMuted;
    state->masterOutput=show.masterOutput;
    state->safetyLimiterEnabled=show.safetyLimiterEnabled;
    for (const auto& column:show.columns)
        state->columnMeters.push_back({column.id,
            column.type==ColumnType::audio ? std::make_shared<ColumnPeakTelemetry>() : nullptr});
    for (const auto& row : show.rows) {
      state->rows.push_back({row.id}); state->rows.back().stopOnNext=row.stopOnNext;
      for (size_t i=0; i<row.cells.size() && i<show.columns.size(); ++i) {
        const auto& c=row.cells[i];
        Voice v; v.cellId=c.id; v.rowId=row.id; v.output=show.columns[i].output; v.columnIndex=(int)i;
        v.columnMeter=state->columnMeters[i].second;
        if (show.columns[i].type==ColumnType::midi && c.midi) {
            v.isMidi=true; v.midiOutputId=show.columns[i].midiOutputId;
            v.midiDuration=secondsAtBeat(row.tempo,c.midi->lengthBeats);
            if (validateMidiClip(*c.midi).failed() || v.midiDuration<=0) continue;
            for (const auto& n:c.midi->notes) {
                v.midiEvents.push_back({secondsAtBeat(row.tempo,n.beat),0x90+n.channel-1,n.pitch,n.velocity});
                v.midiEvents.push_back({secondsAtBeat(row.tempo,n.beat+n.length),0x80+n.channel-1,n.pitch,0});
            }
            std::stable_sort(v.midiEvents.begin(),v.midiEvents.end(),[](const auto& a,const auto& b) {
                return a.seconds!=b.seconds ? a.seconds<b.seconds : a.status<b.status;
            });
            // Stable content fingerprint; no pointer identity dependency for embedded clips.
            uint64_t hash=1469598103934665603ull;
            const auto mix=[&](const void* ptr,size_t bytes) { const auto* p=static_cast<const unsigned char*>(ptr); for (size_t j=0;j<bytes;++j) { hash^=p[j]; hash*=1099511628211ull; } };
            mix(&v.midiDuration,sizeof(v.midiDuration));
            for (const auto& e:v.midiEvents) { mix(&e.seconds,sizeof(e.seconds)); mix(&e.status,sizeof(e.status)); mix(&e.pitch,sizeof(e.pitch)); mix(&e.velocity,sizeof(e.velocity)); }
            v.midiHash=hash;
        } else if (show.columns[i].type==ColumnType::osc && c.osc) {
            if (validateOscClip(*c.osc).failed()) continue;
            v.isOsc=true; v.oscColumnId=show.columns[i].id;
            v.oscHost=show.columns[i].oscHost; v.oscPort=show.columns[i].oscPort;
            v.oscDuration=c.osc->useBeats ? secondsAtBeat(row.tempo,c.osc->length) : c.osc->length;
            if (v.oscDuration<=0) continue;
            const auto toSeconds=[&](double units) {
                return c.osc->useBeats ? secondsAtBeat(row.tempo,units) : units;
            };
            const auto toUnits=[&](double seconds) {
                if (!c.osc->useBeats) return seconds;
                double low=0,high=std::max(1.0,seconds*4.0);
                while (secondsAtBeat(row.tempo,high)<seconds && high<1.0e7) high*=2;
                for (int i=0;i<48;++i) {
                    const auto middle=(low+high)*0.5;
                    if (secondsAtBeat(row.tempo,middle)<seconds) low=middle;
                    else high=middle;
                }
                return (low+high)*0.5;
            };
            for (auto& e:prepareOscClip(*c.osc,toSeconds,toUnits))
                v.oscEvents.push_back({e.seconds,std::move(e.bytes)});
            // The initial value of each automation is prepared at its exact
            // start and will replay on retrigger and loop. No reset is sent on Stop.
            uint64_t hash=1469598103934665603ull;
            const auto mix=[&](const void* ptr,size_t len) { const auto* p=static_cast<const uint8_t*>(ptr);
                for (size_t k=0;k<len;++k) { hash^=p[k]; hash*=1099511628211ull; } };
            mix(&v.oscDuration,sizeof(v.oscDuration));
            for (const auto& e:v.oscEvents) {
                mix(&e.seconds,sizeof(e.seconds)); mix(e.bytes.data(),e.bytes.size());
            }
            v.oscHash=hash;
        } else if (show.columns[i].type==ColumnType::audio) {
            const auto it=clips.find(c.id); if (it==clips.end()) continue; v.clip=it->second;
        } else continue;
        v.telemetry=std::make_shared<CellPlaybackTelemetry>();
        v.gain=c.gain; v.loop=c.loop; v.followsRow=c.followsRow; v.behaviour=c.behaviour;
        v.startSeconds=c.startSeconds; v.rowGain=row.gain; v.rowMuted=row.muted;
        v.stopOnNext=row.stopOnNext && c.stopOnNext;
        v.columnGain=show.columns[i].gain; v.columnPan=show.columns[i].pan;
        v.columnMuted=show.columns[i].muted; v.columnPhaseInverted=show.columns[i].phaseInverted;
        v.masterSend=show.columns[i].masterSend;
        v.directOutputEnabled=show.columns[i].directOutputEnabled;
        v.directPostFader=show.columns[i].directPostFader;
        v.rowLoop=row.loop; v.overrideBehaviour=row.overrideBehaviour; v.rowBehaviour=row.behaviour;
        state->voices.push_back(std::move(v));
      }
    }
    state->updateTelemetry();
    return state;
}
AudioEngine::AudioEngine() = default;
juce::Result AudioEngine::setMidiOutput(const juce::String& identifier,bool reconnect) { return midiRouter.open(identifier,reconnect); }
void AudioEngine::refreshMidiOutputs() { midiRouter.refresh(); }
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
    std::map<int,std::shared_ptr<ColumnPeakTelemetry>> nextColumnPeaks;
    for (auto& meter:state->columnMeters) if (meter.second) {
        const auto it=columnPeaks.find(meter.first);
        if (it!=columnPeaks.end()) meter.second=it->second;
        nextColumnPeaks.emplace(meter.first,meter.second);
    }
    for (auto& voice:state->voices)
        if (voice.columnIndex>=0 && (size_t)voice.columnIndex<state->columnMeters.size())
            voice.columnMeter=state->columnMeters[(size_t)voice.columnIndex].second;
    columnPeaks=std::move(nextColumnPeaks);
    std::map<int, PlaybackEntry> nextPlayback;
    for (auto& v:state->voices) {
        if (v.isMidi) { v.midiRoute=midiRouter.route(v.midiOutputId); v.midiSink=&midiRouter; v.midiStream=midiRouter.stream(v.cellId,v.midiRoute); }
        if (v.isOsc) {
            v.oscRoute=oscRouter.configure(v.oscColumnId,v.oscHost,v.oscPort);
            v.oscSink=&oscRouter; v.oscStream=oscRouter.stream(v.cellId,v.oscRoute);
        }
        if (!v.telemetry) v.telemetry=std::make_shared<CellPlaybackTelemetry>();
        CellPlaybackInfo display;
        if (v.clip && v.clip->sampleRate>0) display.durationSeconds=v.clip->samples.getNumSamples()/v.clip->sampleRate;
        if (v.isMidi) display.durationSeconds=v.midiDuration;
        if (v.isOsc) display.durationSeconds=v.oscDuration;
        const auto old=playback.find(v.cellId);
        if (preservePlayback && old!=playback.end() && old->second.clip.get()==v.clip.get() && old->second.midiHash==v.midiHash && old->second.oscHash==v.oscHash)
            display=old->second.telemetry->read();
        v.telemetry->write(display);
        nextPlayback.emplace(v.cellId,PlaybackEntry{v.clip,v.telemetry,v.midiHash,v.oscHash});
    }
    playback=std::move(nextPlayback);
    state->revision=++revision;
    if (!preservePlayback) structuralRevision=revision;
    state->minimumCommandRevision=structuralRevision;
    state->preservePlayback=preservePlayback;
    // exchange gives sole ownership of the displaced pending snapshot to the UI.
    delete pending.exchange(state.release());
}
float AudioEngine::getColumnPeak(int columnId) const noexcept {
    const auto stereo=getColumnStereoPeak(columnId);
    return std::max(stereo[0],stereo[1]);
}
std::array<float,2> AudioEngine::getColumnStereoPeak(int columnId) const noexcept {
    const auto it=columnPeaks.find(columnId);
    if (it==columnPeaks.end()) return {{0.0f,0.0f}};
    // UI thread owns columnPeaks; callback publishes atomic samples only.
    return {{it->second->left.load(std::memory_order_relaxed),
             it->second->right.load(std::memory_order_relaxed)}};
}
CellPlaybackInfo AudioEngine::getCellPlayback(int cellId) const noexcept {
    const auto found=playback.find(cellId);
    return found==playback.end() ? CellPlaybackInfo{} : found->second.telemetry->read();
}
bool AudioEngine::send(int id,bool row,bool stop) noexcept {
    const unsigned w=writeIndex.load(std::memory_order_relaxed);
    const unsigned next=(w+1)%commands.size();
    if (next==readIndex.load(std::memory_order_acquire)) {
        commandDrops.fetch_add(1,std::memory_order_relaxed);return false;
    }
    commands[w]={id,row,stop,revision,panicEpoch.load()};
    writeIndex.store(next,std::memory_order_release); return true;
}
bool AudioEngine::seekRow(int id,double seconds) noexcept {
    if (!std::isfinite(seconds)) return false;
    const unsigned w=writeIndex.load(std::memory_order_relaxed);
    const unsigned next=(w+1)%commands.size();
    if (next==readIndex.load(std::memory_order_acquire)) {
        commandDrops.fetch_add(1,std::memory_order_relaxed);return false;
    }
    commands[w]={id,true,false,revision,panicEpoch.load(),true,std::max(0.0,seconds)};
    writeIndex.store(next,std::memory_order_release); return true;
}
bool AudioEngine::sendMappedMidiCc(const juce::String& id,int channel,int cc,int value) {
    return midiRouter.queueMappedCc(id,channel,cc,value);
}
void AudioEngine::panic() noexcept {
    panicEpoch.fetch_add(1);
    acknowledgedCueId.store(0,std::memory_order_release);
    midiRouter.emergencyStop(); oscRouter.emergencyStop();
}
void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device) {
    deviceSampleRate=device->getCurrentSampleRate(); connected.store(true); panic();
}
void AudioEngine::audioDeviceStopped() {
    connected.store(false); activeVoices.store(0); peak.store(0);
    masterPeakLeft.store(0); masterPeakRight.store(0); panic();
}
void AudioEngine::audioDeviceIOCallbackWithContext(const float* const*,int,float* const* outputs,int channels,int frames,
                                                  const juce::AudioIODeviceCallbackContext&) {
    processBlock(outputs,channels,frames,deviceSampleRate);
}
void AudioEngine::processBlock(float* const* outputs,int channels,int frames,double sampleRate) noexcept {
    // MIDI and OSC events from the same audio block share one monotonic
    // origin. Reading the clock separately caused inter-protocol skew.
    const double blockOriginMs=juce::Time::getMillisecondCounterHiRes();
    midiRouter.beginBlock(sampleRate,blockOriginMs);
    oscRouter.beginBlock(sampleRate,blockOriginMs);
    if (retired.load()==nullptr) {
        if (auto* next=pending.exchange(nullptr)) {
            if (current && next->preservePlayback && next->minimumCommandRevision==current->minimumCommandRevision)
                next->inheritPlayback(*current);
            if (current) {
                for (auto& old:current->voices) if (old.isMidi) {
                    bool transferred=false;
                    if (next->preservePlayback && next->minimumCommandRevision==current->minimumCommandRevision)
                        for (auto& v:next->voices) if (v.cellId==old.cellId && v.isMidi && v.midiHash==old.midiHash && v.midiOutputId==old.midiOutputId) {
                            transferred=true;
                            if (v.startSeconds!=old.startSeconds || v.rowMuted!=old.rowMuted) {
                                current->releaseMidi(old); v.heldNotes.fill(0); v.midiNeedsSync=true;
                            }
                            break;
                        }
                    if (!transferred) current->releaseMidi(old);
                }
                for (auto& old:current->voices) if (old.isOsc) {
                    bool transferred=false;
                    if (next->preservePlayback && next->minimumCommandRevision==current->minimumCommandRevision)
                        for (auto& v:next->voices) if (v.cellId==old.cellId && v.isOsc
                            && v.oscHash==old.oscHash && v.oscColumnId==old.oscColumnId) {
                            transferred=true;
                            if (v.startSeconds!=old.startSeconds) current->releaseMidi(old);
                            break;
                        }
                    if (!transferred) current->releaseMidi(old);
                }
            }
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
            else {
                current->trigger(command.id,command.row,command.stop);
                if (command.row && !command.stop) {
                    for (const auto& row:current->rows) if (row.id==command.id) {
                        acknowledgedCueId.store(command.id,std::memory_order_release);
                        break;
                    }
                }
            }
        }
    }
    readIndex.store(r,std::memory_order_release);
    if (current) current->render(outputs,channels,frames,sampleRate);
    else for (int ch=0; ch<channels; ++ch) if (outputs[ch]) juce::FloatVectorOperations::clear(outputs[ch],frames);
    limiterActive.store(current && current->limiterReducing,std::memory_order_relaxed);
    if (panicEpoch.load()!=epoch) {
        acknowledgedCueId.store(0,std::memory_order_release);
        if (current) current->panic();
        for (int ch=0; ch<channels; ++ch) if (outputs[ch]) juce::FloatVectorOperations::clear(outputs[ch],frames);
    }
    if (current) current->updateTelemetry();
    float level=0, stereo[2]{0,0}; int active=0;
    for (int ch=0; ch<channels; ++ch) if (outputs[ch])
        for (int i=0; i<frames; ++i) {
            const float sample=std::abs(outputs[ch][i]);
            level=std::max(level,sample);
            stereo[ch%2]=std::max(stereo[ch%2],sample);
        }
    if (current) for (const auto& v : current->voices) if (v.playing) ++active;
    peak.store(level); activeVoices.store(active);
    masterPeakLeft.store(current ? current->preLimiterPeakLeft : 0.0f,std::memory_order_relaxed);
    masterPeakRight.store(current ? current->preLimiterPeakRight : 0.0f,std::memory_order_relaxed);
    const int monitored=monitoredRowId.load();
    double seconds=0; bool playing=false;
    if (current) for (const auto& row:current->rows) if (row.id==monitored) {
        seconds=row.seconds; playing=row.playing; break;
    }
    playheadSeconds.store(seconds); rowPlaying.store(playing);
}
}
