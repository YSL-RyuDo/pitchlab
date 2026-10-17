#pragma once
#include <algorithm>
#include <cmath>

namespace pitchlab {
// File-only gain. Memoryless soft knee adds no latency and never alters the
// headphone feed or original recording. This is sample-peak, not true-peak/LUFS.
class RecordLevel {
public:
    static constexpr float ceiling = .89125094f; // -1 dBFS sample peak
    void setGain(float db) { gain = std::pow(10.f, std::clamp(std::isfinite(db)?db:0.f, 0.f, 30.f)/20.f); }
    float process(float x) const {
        if (!std::isfinite(x)) return 0;
        x *= gain;
        constexpr float knee = .70794578f;
        const float a = std::abs(x), room = ceiling-knee;
        return a <= knee ? x : std::copysign(knee+room*std::tanh((a-knee)/room),x);
    }
private:
    float gain = 1;
};
}
