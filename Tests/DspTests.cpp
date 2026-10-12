#include "../Source/PitchEngine.h"
#include <iostream>
#include <random>
#include <chrono>
#include <stdexcept>
#include <fstream>

void require(bool value, const char* description) {
    if (!value) throw std::runtime_error(description);
}
double estimate(const std::vector<float>& data, double rate) {
    pitchlab::PitchDetector detector; detector.prepare(rate);
    for (auto sample : data) detector.push(sample);
    return detector.frequency;
}
int runDspTests(std::ostream& output) {
    try {
        require(pitchlab::nearestNote(60.8, 0, 1) == 60, "C major excludes C sharp");
        require(pitchlab::nearestNote(61.7, 0, 1) == 62, "C major selects D");
        require(pitchlab::nearestNote(68.8, 9, 2) == 69, "A minor selects A");
        require(pitchlab::allowedNote(63, 0, 2) && !pitchlab::allowedNote(64, 0, 2), "natural minor intervals");
        for (double rate : {44100., 48000.}) {
            for (double frequency : {82.41, 146.83, 220., 432., 880.}) {
                pitchlab::PitchDetector detector; detector.prepare(rate);
                for (int i = 0; i < int(rate / 3); ++i) {
                    const auto phase = 2 * pitchlab::pi * frequency * i / rate;
                    detector.push(float(0.3 * std::sin(phase) + 0.12 * std::sin(2 * phase) + 0.08 * std::sin(3 * phase)));
                }
                double error = 1200 * std::log2(detector.frequency / frequency);
                output << "  detection " << rate << " Hz: " << frequency << " -> " << detector.frequency << " (" << error << " cents)\n";
                require(std::isfinite(error) && std::abs(error) < 7, "harmonic pitch accuracy < 7 cents");
            }
            pitchlab::PitchDetector detector; detector.prepare(rate);
            for (int i = 0; i < 10000; ++i) detector.push(0);
            require(detector.frequency == 0, "silence is unvoiced");
            std::mt19937 rng(42); std::uniform_real_distribution<float> noise(-0.1f, 0.1f);
            for (int i = 0; i < 10000; ++i) detector.push(noise(rng));
            require(detector.frequency == 0, "broadband noise is unvoiced");
            for (double inputHz : {432., 452., 213.}) {
                pitchlab::PitchEngine engine; engine.prepare(rate); engine.parameters.retuneMs = 0;
                engine.parameters.outputDb = 0;
                std::vector<float> result; std::array<float, 257> in{}, wet{}, dry{};
                const int length = int(rate * 1.2);
                auto start = std::chrono::steady_clock::now();
                for (int offset = 0; offset < length; offset += 257) {
                    int n = std::min(257, length - offset);
                    for (int i = 0; i < n; ++i) in[i] = 0.25f * float(std::sin(2 * pitchlab::pi * inputHz * (offset + i) / rate));
                    engine.process(in.data(), wet.data(), dry.data(), n);
                    result.insert(result.end(), wet.begin(), wet.begin() + n);
                }
                const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
                const auto expected = pitchlab::hzFromMidi(pitchlab::nearestNote(pitchlab::midiFromHz(inputHz), 0, 0));
                auto measured = estimate(result, rate);
                double firstCross = -1, lastCross = 0; int crossings = 0;
                for (size_t i = size_t(rate * 0.6); i < result.size(); ++i)
                    if (result[i-1] < 0 && result[i] >= 0) {
                        double crossing = double(i - 1) - result[i-1] / (result[i] - result[i-1]);
                        if (firstCross < 0) firstCross = crossing;
                        lastCross = crossing; ++crossings;
                    }
                output << "  crossings " << (crossings - 1) * rate / (lastCross - firstCross)
                       << " detected " << engine.detectedHz.load() << " correction " << engine.correction.load() << '\n';
                auto cents = 1200 * std::log2(measured / expected);
                output << "  end-to-end " << inputHz << " -> " << measured << " target " << expected << ", DSP " << elapsed / 1.2 * 100 << "% realtime, latency " << engine.latencySamples() * 1000.0 / rate << " ms\n";
                require(std::isfinite(cents) && std::abs(cents) < 12, "corrected output within 12 cents of target");
                require(elapsed < 1.2, "processing faster than real time");
                require(result.size() == size_t(length), "processing preserves sample count");
                for (float x : result) if (!std::isfinite(x) || std::abs(x) > 0.981f) throw std::runtime_error("invalid output sample");
            }
        }
        pitchlab::PitchEngine bypass; bypass.prepare(48000); bypass.parameters.enabled = false; bypass.parameters.outputDb = 0;
        std::array<float, 256> in{}, wet{}, dry{}; double maximumError = 0;
        for (int offset = 0; offset < 24000; offset += 256) {
            for (int i = 0; i < 256; ++i) in[i] = float(0.2 * std::sin(2 * pitchlab::pi * 333 * (offset + i) / 48000));
            bypass.process(in.data(), wet.data(), dry.data(), 256);
            if (offset > 12000) for (int i = 0; i < 256; ++i) maximumError = std::max(maximumError, double(std::abs(wet[i] - dry[i])));
        }
        require(maximumError < 0.0001, "bypass equals latency-aligned original after crossfade");
        output << "ALL DSP TESTS PASSED\n";
        return 0;
    } catch (const std::exception& e) { output << "FAIL " << e.what() << '\n'; return 1; }
}
#ifndef PITCHLAB_EMBEDDED_TESTS
int main() { return runDspTests(std::cout); }
#endif
