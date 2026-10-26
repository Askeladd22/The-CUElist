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
            if ((int)a.type<0 || (int)a.type>2
                || (a.type==OscValueType::decimal && !std::isfinite(a.decimal))
                || (a.type==OscValueType::text && !validText(a.text)))
                return juce::Result::fail("Argomento OSC non valido.");
        if (encodeOscMessage(event).size()>1024)
            return juce::Result::fail("Messaggio OSC oltre il limite di 1024 byte.");
    }
    return juce::Result::ok();
}
std::vector<uint8_t> encodeOscMessage(const OscEvent& event) {
    std::vector<uint8_t> out; out.reserve(256);
    appendString(out,event.address);
    juce::String types=",";
    for (const auto& arg:event.arguments)
        types+=arg.type==OscValueType::integer ? "i" : arg.type==OscValueType::decimal ? "f" : "s";
    appendString(out,types);
    for (const auto& arg:event.arguments) {
        if (arg.type==OscValueType::integer) appendU32(out,(uint32_t)(int32_t)arg.integer);
        else if (arg.type==OscValueType::decimal) {
            uint32_t bits=0; std::memcpy(&bits,&arg.decimal,sizeof(bits)); appendU32(out,bits);
        } else appendString(out,arg.text);
    }
    return out;
}
}
