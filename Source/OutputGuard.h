#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace pitchlab {
// Preallocated look-ahead gain limiter. Unlike clipping, this scales the entire
// delayed waveform down before a peak reaches the output. No audio I/O here.
class LookAheadLimiter {
public:
    void prepare(double rate, float ceilingIn, double lookAheadSeconds = 0.005) {
        ceiling = ceilingIn;
        delay = std::max(1, int(std::ceil(rate * lookAheadSeconds)));
        ring.assign(size_t(delay + 1), 0);
        peaks.assign(size_t(delay + 2), {});
        release = float(1 - std::exp(-1 / (rate * 0.15)));
        reset();
    }
    void reset() {
        std::fill(ring.begin(), ring.end(), 0.0f); position = head = tail = size = 0;
        time = 0; gain = 1;
    }
    int latencySamples() const { return delay; }
    float process(float x) {
        if (!std::isfinite(x)) x = 0;
        float amplitude = std::abs(x);
        const int capacity = int(peaks.size());
        while (size > 0) {
            int back = (tail + capacity - 1) % capacity;
            if (peaks[size_t(back)].value > amplitude) break;
            tail = back; --size;
        }
        peaks[size_t(tail)] = {amplitude, time}; tail = (tail + 1) % capacity; ++size;
        while (size > 0 && peaks[size_t(head)].index + uint64_t(delay) < time) {
            head = (head + 1) % capacity; --size;
        }
        const float peak = size > 0 ? peaks[size_t(head)].value : 0;
        const float wanted = peak > ceiling ? ceiling / peak : 1;
        if (wanted < gain) gain = wanted; else gain += release * (wanted - gain);
        ring[position] = x; position = (position + 1) % ring.size();
        ++time;
        return ring[position] * gain;
    }
private:
    struct Peak { float value = 0; uint64_t index = 0; };
    std::vector<float> ring;
    std::vector<Peak> peaks;
    size_t position = 0;
    int delay = 1, head = 0, tail = 0, size = 0;
    uint64_t time = 0;
    float ceiling = 0.5f, gain = 1, release = 0;
};

class HeadphoneGuard {
public:
    static constexpr float peakCeiling = 0.03f; // -30.46 dBFS; not an acoustic SPL guarantee
    void prepare(double rate, bool lowLatency = false) {
        limiter.prepare(rate, peakCeiling, lowLatency ? 0.001 : 0.005); rampStep = float(1 / (rate * 0.25));
        ramp = 0; wasEnabled = false; fault = false;
    }
    int latencySamples() const { return limiter.latencySamples(); }
    float process(float input, bool enabled, float volumeDb) {
        if (!std::isfinite(input) || std::abs(input) > 4 || !std::isfinite(volumeDb)) fault = true;
        if (!enabled || fault) {
            if (wasEnabled) limiter.reset();
            wasEnabled = false; ramp = 0; return 0; // mute must be immediate
        }
        wasEnabled = true;
        ramp = std::min(1.0f, ramp + rampStep);
        const float gain = std::pow(10.0f, std::clamp(volumeDb, -60.0f, -18.0f) / 20);
        return limiter.process(input * gain * ramp);
    }
    bool fault = false;
private:
    LookAheadLimiter limiter;
    float ramp = 0, rampStep = 0;
    bool wasEnabled = false;
};
}
