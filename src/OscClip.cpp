#include "OscClip.h"
#include <cmath>
#include <cstring>
namespace cuelist {
namespace {
void appendU32(std::vector<uint8_t>& out,uint32_t value) {
    out.push_back((uint8_t)(value>>24)); out.push_back((uint8_t)(value>>16));
    out.push_back((uint8_t)(value>>8)); out.push_back((uint8_t)value);
}
void appendString(std::vector<uint8_t>& out,const juce::String& value) {
    const auto utf8=value.toRawUTF8(); const auto len=std::strlen(utf8);
    out.insert(out.end(),utf8,utf8+len); out.push_back(0);
    while (out.size()%4) out.push_back(0);
}
bool validText(const juce::String& value) {
    return !value.containsChar(juce::juce_wchar(0))
        && value.getNumBytesAsUTF8()<=256;
}
}
juce::Result validateOscClip(const OscClip& clip) {
    if (!std::isfinite(clip.length) || clip.length<=0 || clip.length>86400
        || clip.events.size()>256) return juce::Result::fail("Durata OSC o numero di eventi non valido.");
    for (const auto& event:clip.events) {
        if (!std::isfinite(event.time) || event.time<0 || event.time>=clip.length
            || !event.address.startsWithChar('/') || event.address.length()>256
            || event.address.containsChar(' ') || event.address.containsChar(',')
            || event.address.containsChar('#') || event.address.containsChar('*')
            || event.address.containsChar('?') || event.address.containsChar('[')
            || event.address.containsChar(']') || event.address.containsChar('{')
            || event.address.containsChar('}') || event.arguments.size()>16)
            return juce::Result::fail("Indirizzo o tempo OSC non valido.");
        for (const auto& a:event.arguments)
            if ((int)a.type<0 || (int)a.type>3
                || (a.type==OscValueType::decimal && !std::isfinite(a.decimal))
                || (a.type==OscValueType::text && !validText(a.text)))
                return juce::Result::fail("Argomento OSC non valido.");
        if (encodeOscMessage(event).size()>1024)
            return juce::Result::fail("Messaggio OSC oltre il limite di 1024 byte.");
    }
    if (clip.automations.size()>16) return juce::Result::fail("Massimo 16 automazioni OSC.");
    for (const auto& automation:clip.automations) {
        if (!std::isfinite(automation.time) || !std::isfinite(automation.duration)
            || automation.time<0 || automation.duration<=0
            || automation.time+automation.duration>clip.length+1.0e-8
            || automation.message.arguments.size()>16
            || automation.argumentIndex<0
            || (size_t)automation.argumentIndex>=automation.message.arguments.size()
            || automation.points.size()<2 || automation.points.size()>128)
            return juce::Result::fail("Automazione OSC con posizione, durata o argomento non valido.");
        const auto& target=automation.message.arguments[(size_t)automation.argumentIndex];
        if (target.type!=OscValueType::integer && target.type!=OscValueType::decimal)
            return juce::Result::fail("Si possono automatizzare solo argomenti integer e float.");
        OscClip singleton; singleton.length=1;
        singleton.events={{0,automation.message.address,automation.message.arguments}};
        if (validateOscClip(singleton).failed()) return juce::Result::fail("Template OSC automazione non valido.");
        double previous=-1;
        for (const auto& point:automation.points) {
            if (!std::isfinite(point.time) || !std::isfinite(point.value)
                || point.time<=previous || point.time<0 || point.time>automation.duration+1.0e-7
                || (target.type==OscValueType::integer && (point.value<-2147483648.0 || point.value>2147483647.0)))
                return juce::Result::fail("Punto automazione OSC non valido.");
            previous=point.time;
        }
        if (std::abs(automation.points.front().time)>1.0e-8
            || std::abs(automation.points.back().time-automation.duration)>1.0e-6)
            return juce::Result::fail("Le curve OSC devono iniziare a zero e finire alla durata.");
    }
    // Concurrent automations of the same OSC address may target distinct
    // arguments, but not write the same argument during overlapping spans.
    for (size_t i=0;i<clip.automations.size();++i)
        for (size_t j=i+1;j<clip.automations.size();++j) {
            const auto& a=clip.automations[i],&b=clip.automations[j];
            if (a.message.address==b.message.address
                && a.message.arguments.size()==b.message.arguments.size()
                && a.argumentIndex==b.argumentIndex
                && a.time<b.time+b.duration && b.time<a.time+a.duration)
                return juce::Result::fail("Automazioni OSC sovrapposte sullo stesso argomento.");
        }
    return juce::Result::ok();
}
std::vector<uint8_t> encodeOscMessage(const OscEvent& event) {
    std::vector<uint8_t> out; out.reserve(256);
    appendString(out,event.address);
    juce::String types=",";
    for (const auto& arg:event.arguments)
        types+=arg.type==OscValueType::integer ? "i" :
               arg.type==OscValueType::decimal ? "f" :
               arg.type==OscValueType::text ? "s" : arg.boolean ? "T" : "F";
    appendString(out,types);
    for (const auto& arg:event.arguments) {
        if (arg.type==OscValueType::integer) appendU32(out,(uint32_t)(int32_t)arg.integer);
        else if (arg.type==OscValueType::decimal) {
            uint32_t bits=0; std::memcpy(&bits,&arg.decimal,sizeof(bits)); appendU32(out,bits);
        } else if (arg.type==OscValueType::text) appendString(out,arg.text);
        // OSC boolean typetags T/F do not carry payload bytes.
    }
    return out;
}
}
