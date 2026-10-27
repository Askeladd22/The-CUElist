#include "OscOutputRouter.h"
#include "OscClip.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
#include <cstring>

// Explicit operator-run probe. Does not open a MIDI output, send notes,
// overwrite projects, or substitute ports. --osc-loopback sends only to
// 127.0.0.1 and never to show equipment.
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    const bool oscLoopback=argc==2 && std::strcmp(argv[1],"--osc-loopback")==0;
    if(argc>1 && !oscLoopback) {
        std::cerr<<"Usage: CuelistHardwareProbe [--osc-loopback]\n";
        return 2;
    }
    std::cout<<"THE CUELIST - LOCAL HARDWARE INVENTORY\n";
    std::cout<<"MIDI INPUTS\n";
    for(const auto& p:juce::MidiInput::getAvailableDevices())
        std::cout<<"  "<<p.name.toStdString()<<" | ID: "<<p.identifier.toStdString()<<"\n";
    std::cout<<"MIDI OUTPUTS\n";
    for(const auto& p:juce::MidiOutput::getAvailableDevices())
        std::cout<<"  "<<p.name.toStdString()<<" | ID: "<<p.identifier.toStdString()<<"\n";
    std::cout<<"AUDIO DEVICE TYPES (no output is opened)\n";
    juce::AudioDeviceManager audio;
    juce::OwnedArray<juce::AudioIODeviceType> types;
    audio.createAudioDeviceTypes(types);
    for(auto* type:types) {
        type->scanForDevices();
        std::cout<<"  "<<type->getTypeName().toStdString()<<"\n";
        for(const auto& name:type->getDeviceNames(false))
            std::cout<<"    OUT: "<<name.toStdString()<<"\n";
        for(const auto& name:type->getDeviceNames(true))
            std::cout<<"    IN: "<<name.toStdString()<<"\n";
    }
    if(!oscLoopback) {
        std::cout<<"No hardware commands were transmitted.\n";
        return 0;
    }
    juce::DatagramSocket receiver(false);
    if(!receiver.bindToPort(0)) {
        std::cerr<<"OSC loopback: could not reserve local UDP port.\n";
        return 1;
    }
    cuelist::OscOutputRouter router;
    const int route=router.configure(44,"127.0.0.1",receiver.getBoundPort());
    cuelist::OscEvent event;event.address="/thecuelist/selftest";
    const auto packet=cuelist::encodeOscMessage(event);
    if(packet.empty()) return 1;
    router.beginBlock(48000,juce::Time::getMillisecondCounterHiRes());
    router.emit(route,packet.data(),(unsigned)packet.size(),0);
    if(receiver.waitUntilReady(true,2000)!=1) {
        std::cerr<<"OSC loopback: no packet received within two seconds.\n";
        return 1;
    }
    char received[1024]{};
    const int size=receiver.read(received,sizeof(received),false);
    if(size!=(int)packet.size() ||
       std::memcmp(received,packet.data(),packet.size())!=0) {
        std::cerr<<"OSC loopback: received bytes differ from sent bytes.\n";
        return 1;
    }
    std::cout<<"OSC 127.0.0.1 loopback: PASS (byte-exact UDP receive)\n";
    std::cout<<"External OSC receivers and physical MIDI hardware were NOT tested.\n";
    return 0;
}
