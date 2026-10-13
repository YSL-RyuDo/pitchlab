#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PitchEngine.h"
#include "Recorder.h"

// The callback only copies bounded FIFOs and applies headphone protection.
// Pitch detection, FFTs and recording run on the dedicated DSP worker.
class AudioEngine : public juce::AudioIODeviceCallback, private juce::Thread {
public:
    AudioEngine() : Thread("PitchLab DSP"), inputFifo(capacity), outputFifo(capacity) {}
    pitchlab::PitchEngine pitch;
    Recorder recorder;
    juce::AudioDeviceManager devices;
    std::atomic<bool> monitoring{false}, ready{false};
    std::atomic<float> monitorDb{-36};
    std::atomic<double> currentRate{48000};
    std::atomic<int> bufferSamples{0}, totalLatencySamples{0};
    std::atomic<bool> deviceStopped{false};
    std::atomic<int> streamFault{0}; // 1 capture overflow, 2 playback underrun, 3 invalid output, 4 worker
    std::atomic<int> captureOverflows{0}, outputUnderruns{0}, dspOverruns{0};
    std::atomic<float> maxDspMs{0};
    std::atomic<juce::int64> processedSamples{0};
    ~AudioEngine() override { stop(); }
    juce::String initialiseDevices() {
        if (devices.getCurrentAudioDevice()) return {};
        juce::AudioDeviceManager::AudioDeviceSetup options;
        options.sampleRate = 48000; options.bufferSize = 512;
        return devices.initialise(1, 2, nullptr, true, {}, &options);
    }
    juce::String start() {
        auto error = initialiseDevices();
        if (error.isNotEmpty()) return error;
        auto* device = devices.getCurrentAudioDevice();
        if (!device || device->getActiveInputChannels().isZero()) return juce::String::fromUTF8("마이크 입력을 선택해 주세요.");
        if (device->getActiveOutputChannels().isZero()) return juce::String::fromUTF8("헤드폰 출력 장치를 선택해 주세요.");
        const auto sourceName = devices.getAudioDeviceSetup().inputDeviceName.toLowerCase();
        if (devices.getCurrentAudioDeviceType().containsIgnoreCase("loopback")
            || sourceName.contains("stereo mix") || sourceName.contains("what u hear")
            || sourceName.contains(juce::String::fromUTF8("스테레오 믹스")))
            return juce::String::fromUTF8("출력 소리를 다시 받는 장치입니다. 실제 마이크를 선택해 주세요.");
        deviceStopped = false; monitoring = false; streamFault = 0;
        devices.addAudioCallback(this); attached = true;
        if (!ready.load()) { stop(); return juce::String::fromUTF8("오디오 처리 스레드를 시작하지 못했어요."); }
        return {};
    }
    void stop() {
        monitoring = false;
        if (attached) { devices.removeAudioCallback(this); attached = false; }
        stopWorker(); recorder.stop(); devices.closeAudioDevice(); ready = false;
    }
    void prepareStream(double rate, int deviceBuffer) {
        stopWorker();
        currentRate = rate; bufferSamples = deviceBuffer;
        pitch.prepare(rate); headphone.prepare(rate);
        inputFifo.reset(); outputFifo.reset(); primed = false;
        reserve = std::clamp(std::max(2048, deviceBuffer * 3), 2048, capacity / 2);
        totalLatencySamples = pitch.latencySamples() + reserve + headphone.latencySamples();
        streamFault = 0; monitoring = false;
        captureOverflows = 0; outputUnderruns = 0; dspOverruns = 0; maxDspMs = 0; processedSamples = 0;
        ready = startThread(juce::Thread::Priority::high);
        if (!ready.load()) streamFault = 4;
    }
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override {
        if (recorder.isRecording()) { ready = false; deviceStopped = true; return; }
        prepareStream(device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
        totalLatencySamples.fetch_add(device->getInputLatencyInSamples() + device->getOutputLatencyInSamples());
    }
    void audioDeviceStopped() override { ready = false; deviceStopped = true; stopWorker(); }
    void audioDeviceError(const juce::String&) override { ready = false; deviceStopped = true; }
    int bufferedOutputSamples() const { return outputFifo.getNumReady(); }
    void audioDeviceIOCallbackWithContext(const float* const* input, int inputChannels,
        float* const* output, int outputChannels, int count, const juce::AudioIODeviceCallbackContext&) override {
        juce::ScopedNoDenormals noDenormals;
        auto silence = [&] {
            for (int ch = 0; ch < outputChannels; ++ch)
                if (output[ch]) std::fill(output[ch], output[ch] + count, 0.0f);
        };
        if (!ready.load() || streamFault.load() != 0) { silence(); return; }
        const float* mic = nullptr;
        for (int ch = 0; ch < inputChannels; ++ch) if (input[ch]) { mic = input[ch]; break; }
        if (inputFifo.getFreeSpace() < count) { fail(1); silence(); return; }
        if (primed && outputFifo.getNumReady() < count) { fail(2); silence(); return; }
        if (mic) for (int i = 0; i < count; ++i)
            if (!std::isfinite(mic[i]) || std::abs(mic[i]) > 4) { fail(3); silence(); return; }
        int a, n1, b, n2;
        inputFifo.prepareToWrite(count, a, n1, b, n2);
        for (int i = 0; i < n1; ++i) capture[size_t(a + i)] = mic ? mic[i] : 0;
        for (int i = 0; i < n2; ++i) capture[size_t(b + i)] = mic ? mic[n1 + i] : 0;
        inputFifo.finishedWrite(n1 + n2); notify();
        if (!primed) {
            if (outputFifo.getNumReady() < reserve) { silence(); return; }
            primed = true;
        }
        if (outputFifo.getNumReady() < count) { fail(2); silence(); return; }
        outputFifo.prepareToRead(count, a, n1, b, n2);
        const bool listen = monitoring.load(); const float volume = monitorDb.load();
        for (int i = 0; i < count; ++i) {
            const float sample = playback[size_t(i < n1 ? a + i : b + i - n1)];
            float y = headphone.process(sample, listen, volume);
            if (headphone.fault) { fail(3); y = 0; }
            for (int ch = 0; ch < outputChannels; ++ch) if (output[ch]) output[ch][i] = y;
        }
        outputFifo.finishedRead(n1 + n2);
    }
private:
    static constexpr int chunkSize = 512, capacity = 32768;
    juce::AbstractFifo inputFifo, outputFifo;
    std::array<float, capacity> capture{}, playback{};
    std::array<float, chunkSize> source{}, wet{}, dry{};
    pitchlab::HeadphoneGuard headphone;
    bool attached = false, primed = false;
    int reserve = 2048;
    void fail(int code) {
        if (code == 1) ++captureOverflows;
        if (code == 2) ++outputUnderruns;
        streamFault = code; monitoring = false;
    }
    void stopWorker() { ready = false; signalThreadShouldExit(); notify(); waitForThreadToExit(-1); }
    void run() override {
        juce::ScopedNoDenormals noDenormals;
        while (!threadShouldExit()) {
            if (streamFault.load() != 0 || inputFifo.getNumReady() < chunkSize || outputFifo.getFreeSpace() < chunkSize) { wait(2); continue; }
            int a, n1, b, n2;
            inputFifo.prepareToRead(chunkSize, a, n1, b, n2);
            std::copy_n(capture.data() + a, n1, source.data());
            if (n2) std::copy_n(capture.data() + b, n2, source.data() + n1);
            inputFifo.finishedRead(n1 + n2);
            const auto started = juce::Time::getMillisecondCounterHiRes();
            pitch.process(source.data(), wet.data(), dry.data(), chunkSize);
            const auto elapsed = float(juce::Time::getMillisecondCounterHiRes() - started);
            maxDspMs = std::max(maxDspMs.load(), elapsed);
            if (elapsed > chunkSize * 1000.0 / currentRate.load()) ++dspOverruns;
            processedSamples.fetch_add(chunkSize);
            recorder.push(wet.data(), dry.data(), chunkSize);
            outputFifo.prepareToWrite(chunkSize, a, n1, b, n2);
            std::copy_n(wet.data(), n1, playback.data() + a);
            if (n2) std::copy_n(wet.data() + n1, n2, playback.data() + b);
            outputFifo.finishedWrite(n1 + n2);
        }
    }
};
