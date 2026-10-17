#pragma once
#include <rubberband/RubberBandLiveShifter.h>
#include <memory>
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <vector>
#include "OutputGuard.h"
#include "VocalTone.h"
#include "../ThirdParty/Signalsmith/signalsmith-stretch.h"

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
inline int scaleMask(int key, int scale) {
    int mask = 0;
    for (int n = 0; n < 12; ++n) if (allowedNote(n, key, scale)) mask |= 1 << n;
    return mask;
}
inline int nearestMaskedNote(double midi, int mask) {
    int best = int(std::round(midi)); double distance = 100;
    mask &= 0xfff;
    if (mask == 0) return best;
    for (int n = best - 12; n <= int(std::round(midi)) + 12; ++n)
        if ((mask & (1 << ((n % 12 + 12) % 12))) && std::abs(n - midi) < distance) {
            best = n; distance = std::abs(n - midi);
        }
    return best;
}

// YIN cumulative mean normalized difference. Downsampling and fixed buffers keep
// all allocation outside the audio callback. Unvoiced sound returns 0 Hz.
class PitchDetector {
public:
    void prepare(double rate, bool fastTracking = false) {
        windowSize = fastTracking ? 768 : size;
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
        ring[pos] = float(lp2); pos = (pos + 1) % windowSize;
        count = std::min(windowSize, count + 1);
        if (++hop < 128 || count < windowSize) return false;
        hop = 0; analyse(); return true;
    }
    double frequency = 0, confidence = 0;
    void setRange(int range) { minHz = range == 2 ? 130 : 65; maxHz = range == 1 ? 400 : 1000; }
private:
    static constexpr int size = 1024;
    int windowSize = size;
    std::array<float, size> ring{}, frame{}, difference{};
    int pos = 0, count = 0, hop = 0, decCount = 0, decimation = 2;
    double dsRate = 24000, lp1 = 0, lp2 = 0, lowPass = 0.3, minHz = 65, maxHz = 1000;
    void analyse() {
        double energy = 0;
        for (int i = 0; i < windowSize; ++i) { frame[i] = ring[(pos + i) % windowSize]; energy += frame[i] * frame[i]; }
        if (energy / windowSize < 0.0000158) { frequency = confidence = 0; return; }
        const int maxLag = std::min(windowSize / 2 - 1, int(dsRate / minHz));
        const int minLag = std::max(2, int(dsRate / maxHz));
        const int length = windowSize - maxLag;
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
    std::atomic<int> noteMask{0}, voiceRange{0}; // 0 mask uses key/scale
    std::atomic<float> humanize{0};
    std::atomic<int> toneStyle{0};
    std::atomic<float> fxMix{0}, bpm{140};
    std::atomic<bool> enabled{true}, preserveFormants{true};
};

class PitchEngine {
public:
    Parameters parameters;
    std::atomic<float> detectedHz{0}, targetMidi{-1}, correction{0}, inputPeak{0}, outputPeak{0};
    std::atomic<float> confidence{0};
    std::atomic<bool> clipped{false};
    void prepare(double newRate, bool useLowLatency = false) {
        rate = newRate;
        lowLatency = useLowLatency;
        detector.prepare(rate, lowLatency);
        tone.prepare(rate);
        if (lowLatency) {
            liveShifter = std::make_unique<signalsmith::stretch::SignalsmithStretch<float>>(42);
            const int window = std::max(384, int(std::round(rate * 0.022 / 32)) * 32);
            const int hop = std::max(32, int(std::round(rate * (64.0 / 48000) / 32)) * 32);
            liveShifter->configure(1, window, hop, false);
            liveShifter->setFormantBase(float(220 / rate));
            latency = liveShifter->inputLatency() + liveShifter->outputLatency();
            shifter.reset();
        } else {
            liveShifter.reset();
            shifter = std::make_unique<RubberBand::RubberBandLiveShifter>(size_t(rate), 1,
            RubberBand::RubberBandLiveShifter::OptionWindowShort | RubberBand::RubberBandLiveShifter::OptionFormantPreserved);
            shiftSize = int(shifter->getBlockSize()); shiftPos = 0;
            shiftInput.assign(size_t(shiftSize), 0); shiftOutput.assign(size_t(shiftSize), 0);
            latency = int(shifter->getStartDelay()) + shiftSize;
        }
        // The fixed-block adapter adds one block to the shifter's stated delay.
        outputLimiter.prepare(rate, 0.5f, lowLatency ? 0.001 : 0.005);
        dryDelay.assign(size_t(latency + 1), 0);
        finalDryDelay.assign(size_t(outputLimiter.latencySamples() + 1), 0); finalDryPos = 0;
        dryPos = 0; smoothCorrection = 0; effectMix = 1; gain = 0.501187f;
        lastTarget = -1; lastKey = lastScale = lastMask = -1; midiHistory.fill(0); validHistory = historyPos = 0;
        heldSamples = unvoicedSamples = 0; confidence = 0;
        detectedHz = 0; targetMidi = -1; correction = inputPeak = outputPeak = 0; clipped = false;
    }
    int latencySamples() const { return latency + outputLimiter.latencySamples(); }
    double sampleRate() const { return rate; }
    bool isLowLatency() const { return lowLatency; }
    // All arrays are caller-owned; dry is delayed to match the wet path.
    void process(const float* input, float* wet, float* dry, int samples) {
        float inPeak = 0, outPeak = 0;
        detector.setRange(parameters.voiceRange.load());
        for (int offset = 0; offset < samples; offset += quantum) {
            int n = std::min(quantum, samples - offset);
            for (int i = 0; i < n; ++i) {
                float x = std::isfinite(input[offset + i]) ? std::clamp(input[offset + i], -1.0f, 1.0f) : 0;
                blockIn[i] = x;
                inPeak = std::max(inPeak, std::abs(x));
                if (detector.push(x)) updateTarget();
                dryDelay[dryPos] = x;
                dryPos = (dryPos + 1) % dryDelay.size();
                dry[offset + i] = dryDelay[dryPos];
            }
            // Brief consonants keep the previous ratio; do not wobble back to
            // unity at every unvoiced detector frame. Long gaps ease to unity.
            double wanted = smoothCorrection;
            if (detector.frequency > 0 && lastTarget >= 0)
            {
                unvoicedSamples = 0; heldSamples = std::min(heldSamples + n, int(rate));
                wanted = std::clamp(double(lastTarget) - trackedMidi, -12.0, 12.0) * std::clamp(parameters.amount.load(), 0.0f, 1.0f);
            } else { unvoicedSamples = std::min(unvoicedSamples + n, int(rate)); if (unvoicedSamples > rate * 0.06) wanted = 0; }
            const double sustain = std::clamp((heldSamples / rate - 0.12) / 0.2, 0.0, 1.0);
            const double time = detector.frequency > 0
                ? std::max(0.1, double(parameters.retuneMs.load()) + 180 * std::clamp(parameters.humanize.load(), 0.0f, 1.0f) * sustain)
                : 45.0;
            const double alpha = 1.0 - std::exp(-n / (rate * time * 0.001));
            smoothCorrection += alpha * (wanted - smoothCorrection);
            if (lowLatency) {
                liveShifter->setTransposeFactor(float(std::pow(2.0, smoothCorrection / 12.0)));
                liveShifter->setFormantFactor(1, parameters.preserveFormants.load());
                if (detector.frequency > 0) liveShifter->setFormantBase(float(detector.frequency / rate));
                const float* ins[] = {blockIn.data()}; float* outs[] = {blockOut.data()};
                liveShifter->process(ins, n, outs, n);
            } else for (int i = 0; i < n; ++i) {
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
            tone.select(parameters.toneStyle.load(), parameters.fxMix.load(), parameters.bpm.load());
            const float requestedDb = parameters.outputDb.load();
            const float desiredGain = std::isfinite(requestedDb) ? std::pow(10.0f, std::clamp(requestedDb, -60.0f, 0.0f) / 20.0f) : 0;
            const float smooth = float(1.0 - std::exp(-1.0 / (rate * 0.01)));
            for (int i = 0; i < n; ++i) {
                effectMix += smooth * (desiredMix - effectMix); gain += smooth * (desiredGain - gain);
                float y = (tone.process(blockOut[i]) * effectMix + dry[offset + i] * (1 - effectMix)) * gain;
                if (!std::isfinite(y)) y = 0;
                if (std::abs(y) > 0.98f || std::abs(blockIn[i]) >= 0.999f) clipped = true;
                wet[offset + i] = outputLimiter.process(y);
                finalDryDelay[finalDryPos] = dry[offset + i];
                finalDryPos = (finalDryPos + 1) % finalDryDelay.size();
                dry[offset + i] = finalDryDelay[finalDryPos];
                outPeak = std::max(outPeak, std::abs(wet[offset + i]));
            }
        }
        inputPeak = inPeak; outputPeak = outPeak; correction = float(smoothCorrection);
    }
private:
    static constexpr int quantum = 128;
    PitchDetector detector;
    VocalTone tone;
    std::unique_ptr<RubberBand::RubberBandLiveShifter> shifter;
    std::unique_ptr<signalsmith::stretch::SignalsmithStretch<float>> liveShifter;
    bool lowLatency = false;
    LookAheadLimiter outputLimiter;
    std::vector<float> shiftInput, shiftOutput;
    int shiftSize = 512, shiftPos = 0;
    std::array<float, quantum> blockIn{}, blockOut{};
    std::vector<float> dryDelay;
    std::vector<float> finalDryDelay;
    size_t finalDryPos = 0;
    std::array<double, 3> midiHistory{};
    size_t dryPos = 0;
    int latency = 0, lastTarget = -1, lastKey = -1, lastScale = -1, lastMask = -1, validHistory = 0, historyPos = 0;
    int heldSamples = 0, unvoicedSamples = 0;
    double rate = 48000, trackedMidi = 0, smoothCorrection = 0;
    float effectMix = 1, gain = 0.5f;
    void updateTarget() {
        detectedHz = float(detector.frequency);
        confidence = float(detector.confidence);
        if (detector.frequency == 0) { targetMidi = -1; lastTarget = -1; validHistory = 0; historyPos = 0; heldSamples = 0; return; }
        midiHistory[historyPos++ % 3] = midiFromHz(detector.frequency);
        validHistory = std::min(validHistory + 1, 3);
        auto sorted = midiHistory;
        std::sort(sorted.begin(), sorted.end());
        trackedMidi = validHistory == 3 ? sorted[1] : midiFromHz(detector.frequency);
        const int key = parameters.key.load(), scale = parameters.scale.load();
        const int mask = parameters.noteMask.load() > 0 ? parameters.noteMask.load() : scaleMask(key, scale);
        int candidate = nearestMaskedNote(trackedMidi, mask);
        // 12-cent hysteresis prevents toggling between equally close notes.
        if (mask == lastMask && key == lastKey && scale == lastScale && lastTarget >= 0 && candidate != lastTarget
            && std::abs(trackedMidi - lastTarget) < std::abs(trackedMidi - candidate) + 0.12)
            candidate = lastTarget;
        if (candidate != lastTarget) heldSamples = 0;
        lastTarget = candidate; lastKey = key; lastScale = scale; lastMask = mask; targetMidi = float(candidate);
    }
};
}
