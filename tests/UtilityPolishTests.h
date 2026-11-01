#pragma once
#include "MasterAudioLayout.h"
#include "MainComponent.h"
#include "GainMeterFader.h"

void utilityPolishTests() {
    test("Shared icon surfaces distinguish idle hover pressed active disabled and focus",[] {
        const auto render=[](IconButtonState state,int width,int height) {
            juce::Image image(juce::Image::ARGB,width,height,true);
            juce::Graphics g(image);
            g.fillAll(palette::panel);
            drawIconButtonSurface(g,{0,0,(float)width,(float)height},
                                  palette::red,state);
            return image;
        };
        const auto different=[](const juce::Image& a,const juce::Image& b) {
            if (!a.isValid() || !b.isValid()) return false;
            for (int y=0;y<a.getHeight();++y)
                for (int x=0;x<a.getWidth();++x)
                    if (a.getPixelAt(x,y)!=b.getPixelAt(x,y)) return true;
            return false;
        };
        const auto idle=render({true,false,false,false,false},24,23);
        const auto hover=render({true,true,false,false,false},24,23);
        const auto press=render({true,true,true,false,false},24,23);
        const auto active=render({true,false,false,true,false},24,23);
        const auto disabled=render({false,false,false,false,false},24,23);
        const auto focus=render({true,false,false,false,true},24,23);
        check(different(idle,hover) && different(hover,press) &&
              different(idle,active) && different(idle,disabled) &&
              different(idle,focus),
              "Shared icon states are not visually distinguishable");
        check(iconButtonInk(palette::red,{false,false,false,true,false})
                  .getFloatAlpha()<.5f &&
              iconButtonInk(palette::red,{true,false,false,true,false})
                  .getFloatAlpha()>.9f,
              "Disabled icon ink or active semantic ink has incorrect contrast");
        for (const auto sz:std::initializer_list<juce::Point<int>>{
                 {20,23},{24,23},{32,29},{41,25}}) {
            const auto image=render({true,false,false,true,false},sz.x,sz.y);
            check(image.isValid() && image.getWidth()==sz.x &&
                  image.getHeight()==sz.y,
                  "Compact icon surface is clipped or changed its hitbox");
        }
    });
    test("Noto mute icons switch between sound and silence",[] {
        const auto& symbols=appTypefaces().notoSymbols2;
        check(symbols!=nullptr &&
              symbols->getNominalGlyphForCodepoint(
                  UtilityIcon::speakerCodepoint(false)).has_value() &&
              symbols->getNominalGlyphForCodepoint(
                  UtilityIcon::speakerCodepoint(true)).has_value(),
              "Embedded Noto Symbols 2 lacks speaker ON/OFF glyphs");
        UtilityIcon mute(false);
        mute.setSize(34,31);
        mute.setToggleState(false,juce::dontSendNotification);
        const auto audible=mute.createComponentSnapshot(mute.getLocalBounds());
        mute.setToggleState(true,juce::dontSendNotification);
        const auto silenced=mute.createComponentSnapshot(mute.getLocalBounds());
        bool changed=false;
        if (audible.isValid() && silenced.isValid()) {
            for (int y=0;y<audible.getHeight() && !changed;y+=2)
                for (int x=0;x<audible.getWidth();x+=2)
                    if (audible.getPixelAt(x,y)!=silenced.getPixelAt(x,y)) {
                        changed=true;break;
                    }
        }
        check(changed,"Mute icon looks identical in audible and muted states");
        check(mute.getToggleState(),"Mute icon lost toggle state");
    });
    test("Noto speaker icon centres its actual outline in compact Mute buttons",[] {
        const auto& face=appTypefaces().notoSymbols2;
        check(face!=nullptr,"Embedded Symbols 2 is unavailable");
        if (!face) return;
        for (const bool muted:{false,true}) {
            const auto glyph=face->getNominalGlyphForCodepoint(
                UtilityIcon::speakerCodepoint(muted));
            check(glyph.has_value(),"Speaker glyph missing from bundled font");
            if (!glyph) continue;
            juce::Path path;
            face->getOutlineForGlyph(juce::TypefaceMetricsKind::portable,
                                     static_cast<int>(*glyph),path);
            check(!path.isEmpty(),"Speaker has no usable Noto outline");
            if (path.isEmpty()) continue;
            // Same target as UtilityIcon::paintButton at 32x29.
            const auto target=juce::Rectangle<float>(18.0f,18.0f)
                .withCentre({16.0f,14.5f});
            const auto bounds=path.getBoundsTransformed(
                path.getTransformToScaleToFit(
                    target,true,juce::Justification::centred));
            check(std::abs(bounds.getCentreX()-target.getCentreX())<0.25f &&
                  std::abs(bounds.getCentreY()-target.getCentreY())<0.25f &&
                  bounds.getWidth()<=target.getWidth()+.1f &&
                  bounds.getHeight()<=target.getHeight()+.1f,
                  "Speaker ink is visually offset or clipped in Mute button");
            UtilityIcon mute(false);
            mute.setSize(32,29);
            mute.setToggleState(muted,juce::dontSendNotification);
            const auto image=mute.createComponentSnapshot(mute.getLocalBounds());
            check(image.isValid(),"Compact Mute icon failed to render");
        }
    });
    test("Timeline Snap magnet has visibly different ON and OFF states",[] {
        MainComponent ui(false);
        ui.setSize(1440,900);
        auto* snap=dynamic_cast<juce::TextButton*>(
            controlsV3::named(ui,"timelineSnapToggle"));
        check(snap!=nullptr && snap->getToggleState(),
              "Timeline Snap toggle not found or enabled by default");
        const auto on=snap->createComponentSnapshot(snap->getLocalBounds());
        snap->onClick();
        const auto off=snap->createComponentSnapshot(snap->getLocalBounds());
        bool changed=false;
        if (on.isValid() && off.isValid()) {
            for (int y=0;y<on.getHeight() && !changed;y+=2)
                for (int x=0;x<on.getWidth();x+=2)
                    if (on.getPixelAt(x,y)!=off.getPixelAt(x,y)) {
                        changed=true;break;
                    }
        }
        check(!snap->getToggleState() &&
              snap->getTooltip().contains("Snap OFF") && changed,
              "Snap OFF icon/state is not visually distinguishable");
        snap->onClick();
        check(snap->getToggleState() &&
              snap->getTooltip().contains("Snap ON"),
              "Snap could not return to the enabled state");
    });
    test("Collapsed audio utility keeps functional compact gain and mute",[] {
        MainComponent ui(false);
        ui.setSize(1500,900);
        auto* utility=cueViewFixtures::find<ColumnUtilities>(ui);
        utility->refresh();ui.resized();
        ui.toggleColumnCollapsed(0);
        utility->refresh();ui.resized();
        auto* strip=utility->getChildComponent(0)->getChildComponent(0);
        auto* gain=dynamic_cast<GainMeterFader*>(controlsV3::named(*strip,"columnGainDb"));
        auto* mute=dynamic_cast<juce::ToggleButton*>(controlsV3::named(*strip,"columnMute"));
        check(gain && mute && gain->isVisible() && mute->isVisible() &&
              gain->isCompact() && gain->getWidth()<60,
              "Collapsed audio utility lost its compact gain/mute");
        gain->setGainDb(-6.0f,true);
        near(ui.show.columns[0].gain,std::pow(10.0,-6.0/20.0));
        check(ui.show.columns[0].collapsed,"Gain change expanded the column");
        cueViewFixtures::click(*mute,mute->getWidth()*.5f,mute->getHeight()*.5f);
        check(ui.show.columns[0].muted && ui.hasUnsavedChanges(),
              "Collapsed audio mute did not update the actual column");
    });
    test("Audio OUT is a paired compact control and MIDI OUT retains full width",[] {
        MainComponent ui(false);ui.setSize(1500,900);
        addColumn(ui.show,ColumnType::midi,"M");
        auto* utility=cueViewFixtures::find<ColumnUtilities>(ui);
        utility->refresh();ui.resized();
        auto* audio=utility->getChildComponent(0)->getChildComponent(0);
        auto* midi=utility->getChildComponent(0)->getChildComponent(1);
        auto* audioOut=dynamic_cast<juce::ComboBox*>(controlsV3::named(*audio,"columnOutput"));
        auto* midiOut=dynamic_cast<juce::ComboBox*>(controlsV3::named(*midi,"columnOutput"));
        auto* prepost=dynamic_cast<juce::TextButton*>(controlsV3::named(*audio,"columnDirectPrePost"));
        check(audioOut && midiOut && prepost,"Routing controls absent");
        check(midiOut->getWidth()>audioOut->getWidth()+40,
              "MIDI OUT incorrectly retains the compact Audio OUT width");
        check(audioOut->getY()==prepost->getY() && audioOut->getY()>4,
              "Audio OUT and PRE/POST are not aligned slightly below the panel top");
    });
    test("Unified limiter differentiates OFF, ON and gain reduction",[] {
        MainComponent ui(false);
        auto* button=dynamic_cast<LimiterToggle*>(controlsV3::named(ui,"masterSafetyLimiter"));
        check(button!=nullptr && button->getToggleState(),
              "Master limiter unified button missing or initially bypassed");
        button->setReducing(true);
        check(button->isReducing() && button->getToggleState(),"Active reduction state missing");
        ui.setSafetyLimiter(false);
        check(!button->getToggleState(),"Limiter cannot be bypassed from model");
        button->setReducing(false);
        check(!button->isReducing(),"Limiter reduction indicator cannot return idle");
    });
    test("Data output mute persists without silencing MIDI/OSC transport",[] {
        auto show=makeShow();
        addColumn(show,ColumnType::midi,"M");
        addColumn(show,ColumnType::osc,"O");
        MidiClip midi; midi.lengthBeats=8; midi.notes={{0,6,60,90,1}};
        OscClip osc; osc.length=3; osc.events={{1,"/cue",{{OscValueType::integer,5,0,{}}}},
                            {2.5,"/cue",{{OscValueType::integer,6,0,{}}}}};
        show.rows[0].cells[1].midi=midi;
        show.rows[0].cells[2].osc=osc;
        show.columns[1].muted=true;
        show.columns[2].muted=true;
        Show restored;
        check(decodeShow(encodeShow(show),restored).wasOk(),"Muting MIDI/OSC broke serialization");
        check(restored.columns[1].muted && restored.columns[2].muted,
              "MIDI/OSC mute not persisted");
        struct MidiCounter final: MidiEventSink {
            int messages=0;
            void emit(int,int,int,int,int) noexcept override {++messages;}
        } midiSink;
        struct OscCounter final: OscEventSink {
            int messages=0;
            void emit(int,const uint8_t*,unsigned,int) noexcept override {++messages;}
        } oscSink;
        auto state=makeRenderState(show,{});
        auto* midiVoice=static_cast<Voice*>(nullptr);
        auto* oscVoice=static_cast<Voice*>(nullptr);
        for(auto& voice:state->voices) {
            if(voice.isMidi) {midiVoice=&voice;voice.midiSink=&midiSink;voice.midiRoute=0;}
            if(voice.isOsc) {oscVoice=&voice;voice.oscSink=&oscSink;voice.oscRoute=0;}
        }
        check(midiVoice && oscVoice,"MIDI/OSC voices were not prepared");
        state->trigger(show.rows[0].id,true);
        juce::AudioBuffer<float> out(2,8);
        state->render(out.getArrayOfWritePointers(),2,8,4);
        check(midiSink.messages==0 && oscSink.messages==0,
              "Muted data columns still transmitted events");
        check(midiVoice->position>0 && oscVoice->position>0,
              "Data output mute stopped the cue timeline");
        midiVoice->columnMuted=false;
        midiVoice->midiNeedsSync=true;
        oscVoice->columnMuted=false;
        state->render(out.getArrayOfWritePointers(),2,8,4);
        check(midiSink.messages>0,
              "MIDI output remained muted after unmute/resync");
        check(oscSink.messages>0,
              "OSC output remained muted after unmute");
    });
    test("Data mute control affects only MIDI and OSC utility strips",[] {
        MainComponent ui(false);ui.setSize(1500,900);
        addColumn(ui.show,ColumnType::midi,"M");
        addColumn(ui.show,ColumnType::osc,"O");
        auto* utility=cueViewFixtures::find<ColumnUtilities>(ui);
        utility->refresh();ui.resized();
        auto* audio=utility->getChildComponent(0)->getChildComponent(0);
        auto* midi=utility->getChildComponent(0)->getChildComponent(1);
        auto* osc=utility->getChildComponent(0)->getChildComponent(2);
        check(!controlsV3::named(*audio,"columnDataOutputMute")->isVisible(),
              "Audio has unexpected data mute");
        auto* muteMidi=dynamic_cast<juce::ToggleButton*>(controlsV3::named(*midi,"columnDataOutputMute"));
        auto* muteOsc=dynamic_cast<juce::ToggleButton*>(controlsV3::named(*osc,"columnDataOutputMute"));
        auto* activity=dynamic_cast<MidiActivityLed*>(controlsV3::named(*midi,"columnMidiOutputLed"));
        check(muteMidi && muteOsc && muteMidi->isVisible() && muteOsc->isVisible() &&
              activity && activity->isVisible() && !activity->isActive(),
              "MIDI/OSC utility output controls absent");
        cueViewFixtures::click(*muteMidi,muteMidi->getWidth()*.5f,muteMidi->getHeight()*.5f);
        cueViewFixtures::click(*muteOsc,muteOsc->getWidth()*.5f,muteOsc->getHeight()*.5f);
        check(ui.show.columns[1].muted && ui.show.columns[2].muted,
              "UI data mute did not reach the actual model");
        check(ui.engine.getMidiColumnSent(ui.show.columns[1].id)==0,
              "MIDI output activity counted messages without device dispatch");
    });

    test("Expanded Audio utility restores the calibrated 92px fader and original Pan geometry",[] {
        MainComponent ui(false);ui.setSize(1440,900);
        auto* utility=cueViewFixtures::find<ColumnUtilities>(ui);
        check(utility,"Utility container missing");
        auto* first=utility->getChildComponent(0)->getChildComponent(0);
        auto* fader=dynamic_cast<GainMeterFader*>(
            controlsV3::named(*first,"columnGainDb"));
        auto* pan=controlsV3::named(*first,"columnPan");
        auto* mute=controlsV3::named(*first,"columnMute");
        check(fader && pan && mute,"Expanded Audio strip controls missing");
        check(CueGrid::columnWidth==152 && first->getWidth()==152 &&
              fader->getX()==7 && fader->getWidth()==92 &&
              pan->getWidth()==49 && pan->getX()==92 &&
              mute->getX()==75 && mute->getRight()<=first->getWidth(),
              "The original safe spacing of fader, meter, Pan and Mute was not restored");
        check(mute->getX()>=fader->getX()+fader->getWidth()-24,
              "Audio Mute unexpectedly moved behind the calibrated meter region");
    });
    test("Compact Master Mute is centred below Pan and not over its label",[] {
        MainComponent ui(false);
        auto* util=cueViewFixtures::find<ColumnUtilities>(ui);
        auto* masterPan=controlsV3::named(*util,"masterPan");
        auto* masterMute=controlsV3::named(*util,"masterMute");
        auto* output=controlsV3::named(*util,"masterMainOutput");
        auto* limiter=controlsV3::named(*util,"masterSafetyLimiter");
        check(util && masterPan && masterMute && output && limiter,
              "Master Audio controls missing");
        for (const auto size:std::initializer_list<juce::Point<int>>{
                 {1280,768},{1440,820},{1440,900},{1920,1080}}) {
            ui.setSize(size.x,size.y);
            ui.resized();
            const auto layout=MasterAudioLayout::atHeight(util->getHeight());
            const auto pan=masterPan->getBounds();
            const auto mute=masterMute->getBounds();
            const auto out=output->getBounds();
            check(std::abs(pan.getCentreX()-mute.getCentreX())<=2 &&
                  mute.getY()>=pan.getBottom()+55 &&
                  mute.getRight()<CueGrid::nameWidth &&
                  out.getRight()<pan.getX() &&
                  layout.card.contains(mute) &&
                  layout.card.contains(limiter->getBounds()) &&
                  layout.panCaption.getBottom()<=pan.getY() &&
                  layout.panValue.getY()>=pan.getBottom() &&
                  layout.panValue.getBottom()<=layout.muteCaption.getY() &&
                  layout.muteCaption.getBottom()<=mute.getY(),
                  "Master Mute/Pan/labels overlap or leave the pinned Cuelist lane");
        }
    });

}
