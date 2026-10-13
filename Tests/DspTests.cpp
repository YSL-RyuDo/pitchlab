#include "../Source/PitchEngine.h"
#include <iostream>
#include <random>
#include <chrono>
#include <stdexcept>
#include <fstream>
#include <limits>

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
        for (double rate : {44100., 48000., 96000.}) {
            pitchlab::LookAheadLimiter limiter; limiter.prepare(rate, 0.5f);
            const int delay = limiter.latencySamples();
            for (int i = 0; i <= delay; ++i) {
                float y = limiter.process(i == 0 ? 10.0f : 0.0f);
                require(i == delay ? std::abs(y - 0.5f) < 1e-6f : y == 0, "lookahead catches impulse at exact delay");
            }
            std::mt19937 rng(73); std::uniform_real_distribution<float> stress(-100, 100);
            for (int i = 0; i < int(rate * 2); ++i) {
                float y = limiter.process(stress(rng));
                require(std::isfinite(y) && std::abs(y) <= 0.500001f, "lookahead ceiling under random overload");
            }
            limiter.reset();
            for (int i = 0; i < int(rate); ++i) {
                float y = limiter.process(2);
                require(std::isfinite(y) && y <= 0.500001f, "DC overload bounded");
            }
            pitchlab::HeadphoneGuard guard; guard.prepare(rate);
            for (int i = 0; i < 2000; ++i) require(guard.process(1, false, -18) == 0, "monitor off is silent");
            float peak = 0;
            for (int i = 0; i < int(rate); ++i) {
                float y = guard.process(1, true, -18);
                require(std::isfinite(y) && std::abs(y) <= 0.030001f, "headphone output ceiling");
                if (i < delay) require(y == 0, "headphone enable begins silent");
                if (i == delay) require(y < 0.0001f, "headphone fade starts gently");
                peak = std::max(peak, y);
            }
            require(peak > 0.029f, "guard passes attenuated audio");
            require(guard.process(1, false, -18) == 0, "disable mutes immediately");
            for (int i = 0; i < delay; ++i) require(guard.process(1, true, -18) == 0, "re-enable clears delayed audio");
            require(guard.process(std::numeric_limits<float>::quiet_NaN(), true, -18) == 0 && guard.fault, "NaN latches mute");
            require(guard.process(0.5f, true, -18) == 0, "fault never resumes automatically");
            guard.prepare(rate);
            require(guard.process(std::numeric_limits<float>::infinity(), true, -18) == 0 && guard.fault, "infinity latches mute");
            guard.prepare(rate);
            require(guard.process(5, true, -18) == 0 && guard.fault, "invalid level latches mute");
        }
        output << "PASS lookahead impulse, overload, headphone ceiling/fade/mute/fault latch at 44.1/48/96 kHz\n";
        require(pitchlab::nearestNote(60.8, 0, 1) == 60, "C major excludes C sharp");
        require(pitchlab::nearestNote(61.7, 0, 1) == 62, "C major selects D");
        require(pitchlab::nearestNote(68.8, 9, 2) == 69, "A minor selects A");
        require(pitchlab::allowedNote(63, 0, 2) && !pitchlab::allowedNote(64, 0, 2), "natural minor intervals");
        require(pitchlab::nearestMaskedNote(68.7, 1 << 9) == 69, "custom A-only scale");
        require(pitchlab::nearestMaskedNote(60.8, pitchlab::scaleMask(0, 1)) == 60, "custom mask agrees with C major");
        require(pitchlab::nearestMaskedNote(72.0, 1 << 1) == 73, "custom mask across octave");
        // On a sustained vibrato note, Humanize should let the original vibrato
        // through by reducing modulation of the corrective pitch ratio.
        double correctionRange[2]{};
        for (int mode = 0; mode < 2; ++mode) {
            pitchlab::PitchEngine engine; engine.prepare(48000);
            engine.parameters.retuneMs = 0; engine.parameters.humanize = float(mode);
            std::array<float, 256> input{}, wet{}, dry{};
            double phase = 0, lo = 100, hi = -100;
            for (int offset = 0; offset < 96000; offset += 256) {
                for (int i = 0; i < 256; ++i) {
                    const double midi = 68.85 + 0.12 * std::sin(2 * pitchlab::pi * 5 * (offset + i) / 48000);
                    phase += 2 * pitchlab::pi * pitchlab::hzFromMidi(midi) / 48000;
                    input[i] = float(0.2 * std::sin(phase) + 0.07 * std::sin(2 * phase));
                }
                engine.process(input.data(), wet.data(), dry.data(), 256);
                if (offset > 48000) { lo = std::min(lo, double(engine.correction.load())); hi = std::max(hi, double(engine.correction.load())); }
                for (float x : wet) require(std::isfinite(x) && std::abs(x) <= 0.500001f, "vibrato output bounded");
            }
            correctionRange[mode] = hi - lo;
        }
        require(correctionRange[0] > 0.1 && correctionRange[1] < correctionRange[0] * 0.5, "Humanize preserves sustained vibrato by slowing correction");
        output << "PASS custom note masks and sustained-vibrato Humanize response: " << correctionRange[0] << " -> " << correctionRange[1] << " semitone correction range\n";
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
                for (float x : result) if (!std::isfinite(x) || std::abs(x) > 0.500001f) throw std::runtime_error("invalid output sample");
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
