#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <array>
#include "RecordLevel.h"

// Two-channel single-producer/single-consumer FIFO. The callback only copies;
// filesystem writes and WAV header finalisation happen on the recorder thread.
class Recorder : private juce::Thread {
public:
    Recorder() : Thread("PitchLab WAV writer"), fifo(capacity), buffer(2, capacity) {}
    ~Recorder() override { stop(); }
    juce::String start(const juce::File& folder, double rate, float recordDb = 0) {
        stop(); fifo.reset(); failed = false; samplesWritten = 0;
        level.setGain(recordDb);
        auto result = folder.createDirectory();
        if (result.failed()) return result.getErrorMessage();
        const auto name = "take_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S")
                        + "_" + juce::String(juce::Time::getMillisecondCounter());
        wetFile = folder.getNonexistentChildFile(name + "_tuned", ".wav");
        dryFile = folder.getNonexistentChildFile(name + "_original", ".wav");
        juce::WavAudioFormat format;
        const auto options = juce::AudioFormatWriterOptions().withSampleRate(rate).withNumChannels(1).withBitsPerSample(24);
        std::unique_ptr<juce::OutputStream> wetStream(wetFile.createOutputStream());
        std::unique_ptr<juce::OutputStream> dryStream(dryFile.createOutputStream());
        if (!wetStream || !dryStream) return juce::String::fromUTF8("녹음 폴더에 파일을 만들 수 없어요.");
        wetWriter = format.createWriterFor(wetStream, options);
        dryWriter = format.createWriterFor(dryStream, options);
        if (!wetWriter || !dryWriter) { wetWriter.reset(); dryWriter.reset(); return juce::String::fromUTF8("WAV 파일을 열지 못했어요."); }
        sampleRate = rate;
        if (!startThread()) { wetWriter.reset(); dryWriter.reset(); return juce::String::fromUTF8("녹음 스레드를 시작하지 못했어요."); }
        active = true; return {};
    }
    void push(const float* wet, const float* dry, int count) {
        ++producers;
        if (active.load()) {
            if (fifo.getFreeSpace() < count) { failed = true; active = false; }
            else {
                int a, n1, b, n2; fifo.prepareToWrite(count, a, n1, b, n2);
                buffer.copyFrom(0, a, wet, n1); buffer.copyFrom(1, a, dry, n1);
                if (n2 > 0) { buffer.copyFrom(0, b, wet + n1, n2); buffer.copyFrom(1, b, dry + n1, n2); }
                fifo.finishedWrite(n1 + n2); notify();
            }
        }
        --producers;
    }
    void stop() {
        active = false;
        while (producers.load() != 0) juce::Thread::yield();
        signalThreadShouldExit(); notify(); waitForThreadToExit(-1);
        // Destructors update the WAV lengths after all queued audio is drained.
        wetWriter.reset(); dryWriter.reset();
    }
    bool isRecording() const { return active.load(); }
    bool hasFailed() const { return failed.load(); }
    double seconds() const { return samplesWritten.load() / sampleRate; }
    juce::File wetFile, dryFile;
private:
    static constexpr int capacity = 262144;
    juce::AbstractFifo fifo;
    juce::AudioBuffer<float> buffer;
    std::unique_ptr<juce::AudioFormatWriter> wetWriter, dryWriter;
    std::atomic<bool> active{false}, failed{false};
    std::atomic<int> producers{0};
    std::atomic<juce::int64> samplesWritten{0};
    double sampleRate = 48000;
    pitchlab::RecordLevel level;
    std::array<float,4096> levelBuffer{};
    void run() override {
        while (!threadShouldExit() || fifo.getNumReady() > 0) {
            int a, n1, b, n2; fifo.prepareToRead(std::min(4096, fifo.getNumReady()), a, n1, b, n2);
            auto write = [&](int start, int count) {
                if (!count) return;
                for(int i=0;i<count;++i) levelBuffer[size_t(i)]=level.process(buffer.getSample(0,start+i));
                const float* wet[] = {levelBuffer.data()};
                const float* dry[] = {buffer.getReadPointer(1, start)};
                const bool ok1 = wetWriter->writeFromFloatArrays(wet, 1, count);
                const bool ok2 = dryWriter->writeFromFloatArrays(dry, 1, count);
                if (!ok1 || !ok2) { failed = true; active = false; }
                samplesWritten.fetch_add(count);
            };
            write(a, n1); write(b, n2); fifo.finishedRead(n1 + n2);
            if (n1 + n2 == 0) wait(10);
        }
        if (wetWriter && !wetWriter->flush()) failed = true;
        if (dryWriter && !dryWriter->flush()) failed = true;
    }
};
