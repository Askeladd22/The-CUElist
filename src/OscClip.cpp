#include "OscClip.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <numeric>
#include <limits>
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
                || (target.type==OscValueType::integer && (point.value<-2147483648.0 || point.value>2147483647.0))
                || (target.type==OscValueType::decimal
                    && std::abs(point.value)>(double)std::numeric_limits<float>::max()))
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
            if (a.message.address!=b.message.address ||
                a.time>=b.time+b.duration || b.time>=a.time+a.duration) continue;
            if (a.message.arguments.size()!=b.message.arguments.size())
                return juce::Result::fail("Curve sullo stesso address OSC devono avere lo stesso formato.");
            for (size_t arg=0;arg<a.message.arguments.size();++arg)
                if (a.message.arguments[arg].type!=b.message.arguments[arg].type)
                    return juce::Result::fail("Tipi OSC incompatibili fra automazioni dello stesso address.");
            if (a.argumentIndex==b.argumentIndex)
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

namespace {
double interpolatedValue(const OscAutomation& automation,double offset) {
    const auto& points=automation.points;
    if (offset<=0) return points.front().value;
    if (offset>=automation.duration) return points.back().value;
    const auto upper=std::upper_bound(points.begin(),points.end(),offset,
        [](double x,const OscControlPoint& p){ return x<p.time; });
    if (upper==points.begin()) return points.front().value;
    if (upper==points.end()) return points.back().value;
    const auto& b=*upper, &a=*(upper-1);
    const auto fraction=(offset-a.time)/(b.time-a.time);
    return a.value+(b.value-a.value)*fraction;
}
bool matchingTemplate(const OscAutomation& a,const OscAutomation& b) {
    if (a.message.address!=b.message.address ||
        a.message.arguments.size()!=b.message.arguments.size()) return false;
    for (size_t i=0;i<a.message.arguments.size();++i)
        if (a.message.arguments[i].type!=b.message.arguments[i].type) return false;
    return true;
}
void applyValue(OscValue& value,double number) {
    if (value.type==OscValueType::integer)
        value.integer=(int)std::round(juce::jlimit(-2147483648.0,2147483647.0,number));
    else value.decimal=(float)number;
}
}

std::vector<OscScheduledMessage> prepareOscClip(const OscClip& clip,
    const std::function<double(double)>& secondsAt,
    const std::function<double(double)>& unitAtSeconds) {
    std::vector<OscScheduledMessage> result;
    if (validateOscClip(clip).failed()) return result;
    result.reserve(clip.events.size()+std::min<size_t>(8192,clip.automations.size()*1000));
    for (const auto& e:clip.events)
        result.push_back({secondsAt(e.time),encodeOscMessage(e)});
    struct Tick { double seconds=0; size_t source=0; bool initial=false; };
    std::vector<Tick> ticks;
    double summedDuration=0;
    for (const auto& automation:clip.automations)
        summedDuration+=std::max(0.0,secondsAt(automation.time+automation.duration)-secondsAt(automation.time));
    // 30 updates/s for ordinary clips. For extremely long/many overlapping
    // curves, reduce rate to bound precompiled packet count to ~32k per clip.
    const auto updateHz=std::min(30.0,8192.0/std::max(1.0,summedDuration));
    for (size_t index=0;index<clip.automations.size();++index) {
        const auto& au=clip.automations[index];
        const double start=secondsAt(au.time);
        const double finish=secondsAt(au.time+au.duration);
        const auto segments=std::max<int>(1,(int)std::ceil((finish-start)*updateHz));
        for (int n=0;n<=segments;++n) {
            const auto second=n==segments ? finish : start+(finish-start)*(double)n/segments;
            ticks.push_back({second,index,n==0});
        }
    }
    std::stable_sort(ticks.begin(),ticks.end(),[](const Tick& a,const Tick& b) {
        if (a.seconds!=b.seconds) return a.seconds<b.seconds;
        if (a.initial!=b.initial) return a.initial>b.initial;
        return a.source<b.source;
    });
    // A composite packet overlays all active automation values on one base
    // template. This prevents separately scheduled arguments sharing an OSC
    // address from resetting each other to their template defaults.
    double lastSecond=-1;
    size_t lastGroup=(size_t)-1;
    bool alreadyPeriodic=false;
    for (const auto& tick:ticks) {
        const auto& source=clip.automations[tick.source];
        size_t group=tick.source;
        for (size_t k=0;k<tick.source;++k)
            if (matchingTemplate(clip.automations[k],source)) { group=k; break; }
        if (tick.seconds!=lastSecond || group!=lastGroup) {
            alreadyPeriodic=false;
            lastSecond=tick.seconds; lastGroup=group;
        }
        if (alreadyPeriodic && !tick.initial) continue;
        OscEvent composed=clip.automations[group].message;
        const auto unit=unitAtSeconds(tick.seconds);
        for (size_t k=0;k<clip.automations.size();++k) {
            const auto& a=clip.automations[k];
            if (!matchingTemplate(a,source) || unit+1e-9<a.time) continue;
            const double value=interpolatedValue(a,juce::jlimit(0.0,a.duration,unit-a.time));
            applyValue(composed.arguments[(size_t)a.argumentIndex],value);
        }
        auto bytes=encodeOscMessage(composed);
        if (bytes.size()<=1024) result.push_back({tick.seconds,std::move(bytes)});
        alreadyPeriodic=true;
    }
    std::stable_sort(result.begin(),result.end(),[](const auto& a,const auto& b){
        return a.seconds<b.seconds;
    });
    return result;
}
}
