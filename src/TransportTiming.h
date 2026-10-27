#pragma once
#include <algorithm>
#include <cmath>

namespace cuelist {
// A single monotonic audio-callback origin is shared by both outbound
// workers. The callback needs no allocations, locks, or clock queries per cue.
inline double scheduleOutputMillis(double blockOriginMs,int sampleOffset,double msPerSample) noexcept {
    return blockOriginMs + (double)std::max(0,sampleOffset) *
           (std::isfinite(msPerSample)&&msPerSample>0 ? msPerSample : 0);
}
}
