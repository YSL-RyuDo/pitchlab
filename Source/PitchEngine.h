#pragma once
#include <rubberband/RubberBandLiveShifter.h>
#include <memory>
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <vector>

namespace pitchlab {
constexpr double pi = 3.14159265358979323846;
inline double midiFromHz(double hz) { return 69.0 + 12.0 * std::log2(hz / 440.0); }
inline double hzFromMidi(double midi) { return 440.0 * std::pow(2.0, (midi - 69.0) / 12.0); }

// Pure note-selection function: testable without a microphone or JUCE.
inline bool allowedNote(int note, int key, int scale) {
    static constexpr int masks[] = { 0xfff, 0xab5, 0x5ad, 0x4a9 };
    const int relative = ((note - key) % 12 + 12) % 12;
    return (masks[std::clamp(scale, 0, 3)] & (1 << relative)) != 0;
}
inline int nearestNote(double midi, int key, int scale) {
    const int center = int(std::round(midi));
    int best = center;
    double distance = 100;
    for (int n = center - 12; n <= center + 12; ++n)
        if (allowedNote(n, key, scale) && std::abs(n - midi) < distance) {
            best = n; distance = std::abs(n - midi);
        }
    return best;
}

// YIN cumulative mean normalized difference. Downsampling and fixed buffers keep
// all allocation outside the audio callback. Unvoiced sound returns 0 Hz.
class PitchDetector {
public:
    void prepare(double rate) {
        decimation = std::max(1, int(std::round(rate / 24000.0)));
        dsRate = rate / decimation;
        lowPass = 1.0 - std::exp(-2.0 * pi * 3000.0 / rate);
        ring.fill(0); frame.fill(0); difference.fill(0);
        pos = count = decCount = hop = 0;
        lp1 = lp2 = frequency = confidence = 0;
    }
    bool push(float x) {
        lp1 += lowPass * (x - lp1); lp2 += lowPass * (lp1 - lp2);
        if (++decCount < decimation) return false;
        decCount = 0;
        ring[pos] = float(lp2); pos = (pos + 1) % size;
        count = std::min(size, count + 1);
        if (++hop < 128 || count < size) return false;
        hop = 0; analyse(); return true;
    }
    double frequency = 0, confidence = 0;
private:
    static constexpr int size = 1024;
    std::array<float, size> ring{}, frame{}, difference{};
    int pos = 0, count = 0, hop = 0, decCount = 0, decimation = 2;
    double dsRate = 24000, lp1 = 0, lp2 = 0, lowPass = 0.3;
    void analyse() {
        double energy = 0;
        for (int i = 0; i < size; ++i) { frame[i] = ring[(pos + i) % size]; energy += frame[i] * frame[i]; }
        if (energy / size < 0.0000158) { frequency = confidence = 0; return; }
        const int maxLag = std::min(size / 2 - 1, int(dsRate / 65.0));
        const int minLag = std::max(2, int(dsRate / 1000.0));
        const int length = size - maxLag;
        double accumulated = 0;
        difference[0] = 1;
        for (int lag = 1; lag <= maxLag; ++lag) {
            double sum = 0;
            for (int i = 0; i < length; ++i) { double d = frame[i] - frame[i + lag]; sum += d * d; }
            accumulated += sum;
            difference[lag] = accumulated > 1e-12 ? float(sum * lag / accumulated) : 1.0f;
        }
        int chosen = 0;
        for (int lag = minLag; lag < maxLag; ++lag)
            if (difference[lag] < 0.13f) {
                while (lag + 1 <= maxLag && difference[lag + 1] < difference[lag]) ++lag;
                chosen = lag; break;
            }
        if (chosen == 0) {
            chosen = minLag;
            for (int lag = minLag + 1; lag <= maxLag; ++lag)
                if (difference[lag] < difference[chosen]) chosen = lag;
            if (difference[chosen] > 0.25f) { frequency = confidence = 0; return; }
        }
        double refined = chosen;
        if (chosen > 1 && chosen < maxLag) {
            const double a = difference[chosen - 1], b = difference[chosen], c = difference[chosen + 1];
            const double divisor = a - 2 * b + c;
            if (std::abs(divisor) > 1e-12) refined += std::clamp(0.5 * (a - c) / divisor, -0.5, 0.5);
        }
        frequency = dsRate / refined;
        confidence = 1.0 - difference[chosen];
    }
};

struct Parameters {
    std::atomic<int> key{0}, scale{0};
    std::atomic<float> retuneMs{12}, amount{1}, outputDb{-6};
    std::atomic<bool> enabled{true}, preserveFormants{true};
};

class PitchEngine {
public:
    Parameters parameters;
    std::atomic<float> detectedHz{0}, targetMidi{-1}, correction{0}, inputPeak{0}, outputPeak{0};
    std::atomic<bool> clipped{false};
    void prepare(double newRate) {
        rate = newRate;
        detector.prepare(rate);
        shifter = std::make_unique<RubberBand::RubberBandLiveShifter>(size_t(rate), 1,
            RubberBand::RubberBandLiveShifter::OptionWindowShort | RubberBand::RubberBandLiveShifter::OptionFormantPreserved);
        shiftSize = int(shifter->getBlockSize()); shiftPos = 0;
        shiftInput.assign(size_t(shiftSize), 0); shiftOutput.assign(size_t(shiftSize), 0);
        // The fixed-block adapter adds one block to the shifter's stated delay.
        latency = int(shifter->getStartDelay()) + shiftSize;
        dryDelay.assign(size_t(latency + 1), 0);
        dryPos = 0; smoothCorrection = 0; effectMix = 1; gain = 0.501187f;
        lastTarget = -1; lastKey = lastScale = -1; midiHistory.fill(0); validHistory = historyPos = 0;
        detectedHz = 0; targetMidi = -1; correction = inputPeak = outputPeak = 0; clipped = false;
    }
    int latencySamples() const { return latency; }
    double sampleRate() const { return rate; }
    // All arrays are caller-owned; dry is delayed to match the wet path.
    void process(const float* input, float* wet, float* dry, int samples) {
        float inPeak = 0, outPeak = 0;
        for (int offset = 0; offset < samples; offset += quantum) {
            int n = std::min(quantum, samples - offset);
            for (int i = 0; i < n; ++i) {
                float x = std::isfinite(input[offset + i]) ? input[offset + i] : 0;
                blockIn[i] = x;
                inPeak = std::max(inPeak, std::abs(x));
                if (detector.push(x)) updateTarget();
                dryDelay[dryPos] = x;
                dryPos = (dryPos + 1) % dryDelay.size();
                dry[offset + i] = dryDelay[dryPos];
            }
            double wanted = 0;
            if (detector.frequency > 0 && lastTarget >= 0)
                wanted = std::clamp(double(lastTarget) - trackedMidi, -3.0, 3.0) * parameters.amount.load();
            const double time = std::max(1.0, double(parameters.retuneMs.load()));
            const double alpha = 1.0 - std::exp(-n / (rate * time * 0.001));
            smoothCorrection += alpha * (wanted - smoothCorrection);
            for (int i = 0; i < n; ++i) {
                blockOut[i] = shiftOutput[size_t(shiftPos)];
                shiftInput[size_t(shiftPos)] = blockIn[i];
                if (++shiftPos == shiftSize) {
                    shifter->setPitchScale(std::pow(2.0, smoothCorrection / 12.0));
                    shifter->setFormantOption(parameters.preserveFormants.load()
                        ? RubberBand::RubberBandLiveShifter::OptionFormantPreserved
                        : RubberBand::RubberBandLiveShifter::OptionFormantShifted);
                    const float* ins[] = {shiftInput.data()}; float* outs[] = {shiftOutput.data()};
                    shifter->shift(ins, outs); shiftPos = 0;
                }
            }
            const float desiredMix = parameters.enabled.load() ? 1.0f : 0.0f;
            const float desiredGain = std::pow(10.0f, parameters.outputDb.load() / 20.0f);
            const float smooth = float(1.0 - std::exp(-1.0 / (rate * 0.01)));
            for (int i = 0; i < n; ++i) {
                effectMix += smooth * (desiredMix - effectMix); gain += smooth * (desiredGain - gain);
                float y = (blockOut[i] * effectMix + dry[offset + i] * (1 - effectMix)) * gain;
                if (!std::isfinite(y)) y = 0;
                if (std::abs(y) > 0.98f || std::abs(blockIn[i]) >= 0.999f) clipped = true;
                wet[offset + i] = std::clamp(y, -0.98f, 0.98f);
                outPeak = std::max(outPeak, std::abs(wet[offset + i]));
            }
        }
        inputPeak = inPeak; outputPeak = outPeak; correction = float(smoothCorrection);
    }
private:
    static constexpr int quantum = 128;
    PitchDetector detector;
    std::unique_ptr<RubberBand::RubberBandLiveShifter> shifter;
    std::vector<float> shiftInput, shiftOutput;
    int shiftSize = 512, shiftPos = 0;
    std::array<float, quantum> blockIn{}, blockOut{};
    std::vector<float> dryDelay;
    std::array<double, 3> midiHistory{};
    size_t dryPos = 0;
    int latency = 0, lastTarget = -1, lastKey = -1, lastScale = -1, validHistory = 0, historyPos = 0;
    double rate = 48000, trackedMidi = 0, smoothCorrection = 0;
    float effectMix = 1, gain = 0.5f;
    void updateTarget() {
        detectedHz = float(detector.frequency);
        if (detector.frequency == 0) { targetMidi = -1; lastTarget = -1; validHistory = 0; return; }
        midiHistory[historyPos++ % 3] = midiFromHz(detector.frequency);
        validHistory = std::min(validHistory + 1, 3);
        auto sorted = midiHistory;
        std::sort(sorted.begin(), sorted.end());
        trackedMidi = validHistory == 3 ? sorted[1] : midiFromHz(detector.frequency);
        const int key = parameters.key.load(), scale = parameters.scale.load();
        int candidate = nearestNote(trackedMidi, key, scale);
        // 12-cent hysteresis prevents toggling between equally close notes.
        if (key == lastKey && scale == lastScale && lastTarget >= 0 && candidate != lastTarget
            && std::abs(trackedMidi - lastTarget) < std::abs(trackedMidi - candidate) + 0.12)
            candidate = lastTarget;
        lastTarget = candidate; lastKey = key; lastScale = scale; targetMidi = float(candidate);
    }
};
}
