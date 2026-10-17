#pragma once
#include "PitchEngine.h"
#include "RecordLevel.h"
#include <juce_audio_formats/juce_audio_formats.h>

struct VocalSettings {
    int key = 0, scale = 0, mask = 0, range = 0;
    float speed = 12, amount = 1, humanize = 0;
    bool formants = true, enabled = true;
    bool lowLatency = false;
    int style = 0;
    float fx = 0, bpm = 140, recordDb = 0;
    bool levelOnly = false;
    bool noiseGate = true;
    float noiseThresholdDb = -60;
    static constexpr int presetCount = 3+int(pitchlab::styleProfiles.size()), customId = presetCount+1;
    static VocalSettings capture(const pitchlab::Parameters& p) {
        VocalSettings s;
        s.key=p.key.load(); s.scale=p.scale.load(); s.mask=p.noteMask.load(); s.range=p.voiceRange.load();
        s.speed=p.retuneMs.load(); s.amount=p.amount.load(); s.humanize=p.humanize.load();
        s.formants=p.preserveFormants.load(); s.enabled=p.enabled.load();
        s.style=p.toneStyle.load(); s.fx=p.fxMix.load(); s.bpm=p.bpm.load();
        s.noiseGate=p.noiseGate.load(); s.noiseThresholdDb=p.noiseThresholdDb.load(); return s;
    }
    void apply(pitchlab::Parameters& p) const {
        p.key = key; p.scale = scale; p.noteMask = mask; p.voiceRange = range;
        p.retuneMs = speed; p.amount = amount; p.humanize = humanize;
        p.preserveFormants = formants; p.enabled = enabled;
        p.toneStyle=style; p.fxMix=fx; p.bpm=bpm;
        p.noiseGate=noiseGate; p.noiseThresholdDb=noiseThresholdDb;
    }
    static VocalSettings preset(int index) {
        VocalSettings s;
        if (index == 0) { s.speed = 45; s.amount = 0.85f; s.humanize = 0.65f; }
        if (index == 1) { s.speed = 5; s.amount = 1; s.humanize = 0.15f; }
        if (index == 2) { s.speed = 0; s.amount = 1; s.humanize = 0; }
        if(index>=3 && index<presetCount) {
            const auto& p=pitchlab::styleProfiles[size_t(index-3)];
            s.speed=p.speed; s.amount=p.amount; s.humanize=p.humanize; s.style=index-2; s.fx=1;
        }
        return s;
    }
};

// Disk-only processing: no device, playback, or automatic gain normalisation.
class OfflineRenderer : private juce::Thread {
public:
    OfflineRenderer() : Thread("PitchLab file render") {}
    ~OfflineRenderer() override { cancel = true; stopThread(-1); }
    std::atomic<float> progress{0};
    std::atomic<bool> done{false}, cancel{false};
    juce::String error;
    juce::File wetFile, dryFile, reportFile;
    bool start(const juce::File& source, const juce::File& destination, VocalSettings settings) {
        if (isThreadRunning()) return false;
        inputFile = source; folder = destination; config = settings;
        done = false; cancel = false; progress = 0; error.clear();
        return startThread();
    }
    void waitForCompletion() { waitForThreadToExit(-1); }
    static juce::String render(const juce::File& source, const juce::File& destination,
                              VocalSettings settings, OfflineRenderer& job) {
        juce::ScopedNoDenormals noDenormals;
        juce::WavAudioFormat format;
        auto sourceStream = source.createInputStream();
        if (!sourceStream) return juce::String::fromUTF8("입력 파일을 열 수 없어요.");
        std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(sourceStream.release(), true));
        if (!reader) return juce::String::fromUTF8("읽을 수 있는 WAV 파일을 선택해 주세요.");
        const double rate = reader->sampleRate;
        const auto length = reader->lengthInSamples;
        if (rate < 16000 || rate > 96000 || reader->numChannels < 1 || reader->numChannels > 2 || length <= 0 || length > rate * 3600)
            return juce::String::fromUTF8("16–96 kHz, 모노/스테레오, 1시간 이하 WAV를 지원합니다.");
        auto result = destination.createDirectory();
        if (result.failed()) return result.getErrorMessage();
        auto base = source.getFileNameWithoutExtension() + "_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
        job.wetFile = destination.getNonexistentChildFile(base + (settings.levelOnly?"_louder":"_tuned"), ".wav");
        job.dryFile = destination.getNonexistentChildFile(base + "_original", ".wav");
        job.reportFile = destination.getNonexistentChildFile(base + "_report", ".txt");
        const auto options = juce::AudioFormatWriterOptions().withSampleRate(rate).withNumChannels(1).withBitsPerSample(24);
        std::unique_ptr<juce::OutputStream> wetStream(job.wetFile.createOutputStream()), dryStream(job.dryFile.createOutputStream());
        if (!wetStream || !dryStream) return juce::String::fromUTF8("출력 파일을 만들지 못했어요.");
        auto wetWriter = format.createWriterFor(wetStream, options), dryWriter = format.createWriterFor(dryStream, options);
        if (!wetWriter || !dryWriter) return juce::String::fromUTF8("WAV 저장을 시작하지 못했어요.");
        pitchlab::PitchEngine engine;
        if(!settings.levelOnly) { settings.apply(engine.parameters); engine.prepare(rate, settings.lowLatency); }
        pitchlab::RecordLevel level; level.setGain(settings.recordDb);
        constexpr int block = 512;
        juce::AudioBuffer<float> readBuffer(int(reader->numChannels), block);
        std::array<float, block> input{}, wet{}, dry{};
        const int latency = settings.levelOnly?0:engine.latencySamples();
        juce::int64 written = 0, voiced = 0, invalid = 0;
        float peak = 0, maxStep = 0, previous = 0;
        const double started = juce::Time::getMillisecondCounterHiRes();
        for (juce::int64 offset = 0; offset < length + latency; offset += block) {
            if (job.cancel.load()) { wetWriter.reset(); dryWriter.reset(); job.wetFile.deleteFile(); job.dryFile.deleteFile(); return juce::String::fromUTF8("파일 보정을 취소했어요."); }
            const int count = int(std::min<juce::int64>(block, length + latency - offset));
            readBuffer.clear(); input.fill(0);
            if (offset < length) {
                const int readCount = int(std::min<juce::int64>(count, length - offset));
                if (!reader->read(&readBuffer, 0, readCount, offset, true, true)) return "WAV read failed";
                for (int i = 0; i < readCount; ++i) {
                    float sample = 0;
                    for (int ch = 0; ch < readBuffer.getNumChannels(); ++ch) sample += readBuffer.getSample(ch, i) / float(readBuffer.getNumChannels());
                    if (!std::isfinite(sample)) { ++invalid; sample = 0; }
                    input[size_t(i)] = sample;
                }
            }
            if(settings.levelOnly) { std::copy_n(input.data(),count,wet.data()); std::copy_n(input.data(),count,dry.data()); }
            else engine.process(input.data(), wet.data(), dry.data(), count);
            if (engine.detectedHz.load() > 0) voiced += count;
            const int begin = int(std::max<juce::int64>(0, latency - offset));
            const int end = int(std::min<juce::int64>(count, length + latency - offset));
            if (begin < end) {
                for (int i = begin; i < end; ++i) {
                    wet[size_t(i)] = level.process(wet[size_t(i)]);
                    if (!std::isfinite(wet[size_t(i)]) || std::abs(wet[size_t(i)]) > pitchlab::RecordLevel::ceiling+.000001f) return "Invalid processed output";
                    peak = std::max(peak, std::abs(wet[size_t(i)]));
                    maxStep = std::max(maxStep, std::abs(wet[size_t(i)] - previous)); previous = wet[size_t(i)];
                }
                const float* w[] = {wet.data() + begin}; const float* d[] = {dry.data() + begin};
                if (!wetWriter->writeFromFloatArrays(w, 1, end - begin) || !dryWriter->writeFromFloatArrays(d, 1, end - begin)) return "WAV write failed";
                written += end - begin;
            }
            job.progress = float(std::min(1.0, double(offset + count) / double(length + latency)));
        }
        if (!wetWriter->flush() || !dryWriter->flush() || written != length) return "WAV finalisation failed";
        wetWriter.reset(); dryWriter.reset();
        juce::String report = "PitchLab 0.4.1 offline render\nNo audio device opened. No playback.\n";
        report += "Sample rate: " + juce::String(rate) + "\nInput/output frames: " + juce::String(length) + "/" + juce::String(written);
        report += "\nLatency trimmed: " + juce::String(latency) + " samples\nPeak: " + juce::String(peak, 6);
        report += "\nMaximum adjacent sample change: " + juce::String(maxStep, 6);
        report += "\nEstimated voiced frame fraction: " + juce::String(double(voiced) / double(length + latency), 3);
        report += "\nNonfinite input samples replaced: " + juce::String(invalid);
        report += "\nProcessing seconds: " + juce::String((juce::Time::getMillisecondCounterHiRes() - started) / 1000, 3);
        report += "\nRetune ms: " + juce::String(settings.speed) + "\nAmount: " + juce::String(settings.amount) + "\nHumanize: " + juce::String(settings.humanize);
        report += "\nVoice range: " + juce::String(settings.range) + "\nKey: " + juce::String(settings.key) + "\nScale: " + juce::String(settings.scale) + "\nNote mask: " + juce::String(settings.mask);
        report += "\nFormants: " + juce::String(int(settings.formants)) + "\nEffect enabled: " + juce::String(int(settings.enabled)) + "\nListening quality has NOT been verified.\n";
        report += "Engine: " + juce::String(settings.levelOnly ? "File gain only / no pitch processing" : settings.lowLatency ? "Live / Signalsmith short window" : "File / Rubber Band") + "\n";
        report += "Level-only (no tuning): " + juce::String(int(settings.levelOnly)) + "\nFile gain dB: " + juce::String(settings.recordDb);
        report += "\nStyle: " + juce::String(settings.style) + "\nFX mix: " + juce::String(settings.fx) + "\nBPM: " + juce::String(settings.bpm);
        report += "\nNoise gate: " + juce::String(int(settings.noiseGate && !settings.levelOnly)) + "\nNoise threshold dBFS: " + juce::String(settings.noiseThresholdDb);
        report += "\nFile sample-peak ceiling: -1 dBFS (not LUFS/true-peak normalisation). Length preserved; effect tails after EOF are not appended.\n";
        if (!job.reportFile.replaceWithText(report)) return "Report write failed";
        return {};
    }
private:
    juce::File inputFile, folder;
    VocalSettings config;
    void run() override { error = render(inputFile, folder, config, *this); done = true; }
};
