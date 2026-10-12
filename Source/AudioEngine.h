#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PitchEngine.h"
#include "Recorder.h"

class AudioEngine : public juce::AudioIODeviceCallback {
public:
    pitchlab::PitchEngine pitch;
    Recorder recorder;
    juce::AudioDeviceManager devices;
    std::atomic<bool> monitoring{false}, ready{false};
    std::atomic<double> currentRate{48000};
    std::atomic<int> bufferSamples{0}, totalLatencySamples{0};
    std::atomic<bool> deviceStopped{false};
    ~AudioEngine() override { stop(); }
    juce::String initialiseDevices() {
        if (devices.getCurrentAudioDevice()) return {};
        juce::AudioDeviceManager::AudioDeviceSetup options;
        options.sampleRate = 48000; options.bufferSize = 256;
        return devices.initialise(1, 2, nullptr, true, {}, &options);
    }
    juce::String start() {
        auto error = initialiseDevices();
        if (error.isNotEmpty()) return error;
        auto* device = devices.getCurrentAudioDevice();
        if (!device || device->getActiveInputChannels().isZero()) return juce::String::fromUTF8("마이크 입력을 선택해 주세요.");
        if (device->getActiveOutputChannels().isZero()) return juce::String::fromUTF8("헤드폰 출력 장치를 선택해 주세요.");
        deviceStopped = false;
        devices.addAudioCallback(this); attached = true;
        return {};
    }
    void stop() {
        if (attached) { devices.removeAudioCallback(this); attached = false; }
        recorder.stop(); devices.closeAudioDevice(); ready = false;
    }
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override {
        if (recorder.isRecording()) { ready = false; deviceStopped = true; return; }
        const auto rate = device->getCurrentSampleRate();
        currentRate = rate; bufferSamples = device->getCurrentBufferSizeSamples();
        pitch.prepare(rate);
        totalLatencySamples = pitch.latencySamples() + device->getInputLatencyInSamples() + device->getOutputLatencyInSamples();
        monitorGain = 0; ready = true;
    }
    void audioDeviceStopped() override { ready = false; deviceStopped = true; }
    void audioDeviceError(const juce::String&) override { ready = false; deviceStopped = true; }
    void audioDeviceIOCallbackWithContext(const float* const* input, int inputChannels,
        float* const* output, int outputChannels, int count, const juce::AudioIODeviceCallbackContext&) override {
        if (!ready.load()) {
            for (int ch = 0; ch < outputChannels; ++ch)
                if (output[ch]) std::fill(output[ch], output[ch] + count, 0.0f);
            return;
        }
        // Find the first active input rather than averaging unrelated microphones.
        const float* mic = nullptr;
        for (int ch = 0; ch < inputChannels; ++ch) if (input[ch]) { mic = input[ch]; break; }
        for (int offset = 0; offset < count; offset += chunkSize) {
            const int n = std::min(chunkSize, count - offset);
            for (int i = 0; i < n; ++i) source[i] = mic ? mic[offset + i] : 0;
            pitch.process(source.data(), wet.data(), dry.data(), n);
            recorder.push(wet.data(), dry.data(), n);
            const float target = monitoring.load() ? 1.0f : 0.0f;
            const float alpha = float(1 - std::exp(-1.0 / (currentRate.load() * 0.01)));
            for (int i = 0; i < n; ++i) {
                monitorGain += alpha * (target - monitorGain);
                for (int ch = 0; ch < outputChannels; ++ch)
                    if (output[ch]) output[ch][offset + i] = wet[i] * monitorGain;
            }
        }
    }
private:
    static constexpr int chunkSize = 512;
    std::array<float, chunkSize> source{}, wet{}, dry{};
    bool attached = false;
    float monitorGain = 0;
};
