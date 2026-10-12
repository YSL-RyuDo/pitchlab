#include "AudioEngine.h"
#include <fstream>
#define PITCHLAB_EMBEDDED_TESTS
#include "../Tests/DspTests.cpp"

namespace theme {
const juce::Colour bg{0xff101116}, panel{0xff1a1c24}, field{0xff252834}, text{0xfff0f0f6}, muted{0xff969cad};
const juce::Colour accent{0xffb6a1ff}, green{0xff73e3be}, red{0xffff8096};
juce::Font font(float size, bool bold = false) {
    return juce::Font(juce::FontOptions("Malgun Gothic", size, bold ? juce::Font::bold : juce::Font::plain));
}
struct Look : juce::LookAndFeel_V4 {
    Look() {
        setColour(juce::TextButton::buttonColourId, field);
        setColour(juce::TextButton::textColourOffId, text);
        setColour(juce::ComboBox::backgroundColourId, field);
        setColour(juce::ComboBox::textColourId, text);
        setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour(juce::PopupMenu::backgroundColourId, field);
        setColour(juce::PopupMenu::textColourId, text);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, accent.darker(0.5f));
        setColour(juce::Slider::thumbColourId, accent);
        setColour(juce::Slider::trackColourId, accent);
        setColour(juce::Slider::backgroundColourId, field.brighter(0.15f));
        setColour(juce::Slider::textBoxTextColourId, text);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::ToggleButton::textColourId, text);
        setColour(juce::ToggleButton::tickColourId, green);
        setColour(juce::Label::textColourId, text);
        setColour(juce::ResizableWindow::backgroundColourId, bg);
    }
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return font(15, true); }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return font(15); }
    juce::Font getLabelFont(juce::Label&) override { return font(14); }
    juce::Font getPopupMenuFont() override { return font(15); }
};
}

class MainComponent : public juce::Component, private juce::Timer {
public:
    MainComponent() {
        setLookAndFeel(&look); setSize(1000, 760);
        recordings = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("Recordings");
        setupButton(deviceButton, juce::String::fromUTF8("마이크 / 헤드폰 설정"));
        setupButton(startButton, juce::String::fromUTF8("마이크 시작"));
        setupButton(recordButton, juce::String::fromUTF8("●  녹음 시작"));
        setupButton(folderButton, juce::String::fromUTF8("녹음 폴더 열기"));
        setupButton(locationButton, juce::String::fromUTF8("저장 위치 변경"));
        startButton.setColour(juce::TextButton::buttonColourId, theme::accent);
        startButton.setColour(juce::TextButton::textColourOffId, theme::bg);
        recordButton.setColour(juce::TextButton::buttonColourId, theme::red.withAlpha(0.18f));
        recordButton.setColour(juce::TextButton::textColourOffId, theme::red);
        setupToggle(monitor, juce::String::fromUTF8("헤드폰으로 듣기"), false);
        setupToggle(bypass, juce::String::fromUTF8("보정 켜기"), true);
        setupToggle(formants, juce::String::fromUTF8("목소리 음색 유지"), true);
        key.addItemList({"C", "C# / Db", "D", "D# / Eb", "E", "F", "F# / Gb", "G", "G# / Ab", "A", "A# / Bb", "B"}, 1);
        scale.addItemList({juce::String::fromUTF8("모든 반음 · Chromatic"), juce::String::fromUTF8("장조 · Major"), juce::String::fromUTF8("자연단음계 · Minor"), juce::String::fromUTF8("마이너 펜타토닉")}, 1);
        key.setSelectedId(1); scale.setSelectedId(1);
        key.setTitle(juce::String::fromUTF8("곡의 키")); scale.setTitle(juce::String::fromUTF8("곡의 스케일"));
        addAndMakeVisible(key); addAndMakeVisible(scale);
        setupSlider(speed, 0, 150, 1, 12, " ms", juce::String::fromUTF8("보정 속도"));
        setupSlider(amount, 0, 100, 1, 100, "%", juce::String::fromUTF8("보정 강도"));
        setupSlider(volume, -60, -18, 1, -36, " dB", juce::String::fromUTF8("헤드폰 음량"));
        key.onChange = [this] { audio.pitch.parameters.key = key.getSelectedId() - 1; };
        scale.onChange = [this] { audio.pitch.parameters.scale = scale.getSelectedId() - 1; };
        speed.onValueChange = [this] { audio.pitch.parameters.retuneMs = float(speed.getValue()); };
        amount.onValueChange = [this] { audio.pitch.parameters.amount = float(amount.getValue() / 100); };
        volume.onValueChange = [this] { audio.monitorDb = float(volume.getValue()); };
        monitor.onClick = [this] { audio.monitoring = monitor.getToggleState(); };
        bypass.onClick = [this] { audio.pitch.parameters.enabled = bypass.getToggleState(); };
        formants.onClick = [this] { audio.pitch.parameters.preserveFormants = formants.getToggleState(); };
        startButton.onClick = [this] { toggleAudio(); };
        recordButton.onClick = [this] { toggleRecording(); };
        deviceButton.onClick = [this] { showDevices(); };
        folderButton.onClick = [this] {
            if (recordings.createDirectory().wasOk()) recordings.startAsProcess();
            else showError(juce::String::fromUTF8("녹음 폴더를 열지 못했어요. 저장 위치를 바꿔 주세요."));
        };
        locationButton.onClick = [this] {
            chooser = std::make_unique<juce::FileChooser>(juce::String::fromUTF8("녹음을 저장할 폴더"), recordings);
            juce::Component::SafePointer<MainComponent> safe(this);
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                [safe](const juce::FileChooser& fc) {
                    if (safe && fc.getResult().isDirectory()) {
                        safe->recordings = fc.getResult(); safe->status = juce::String::fromUTF8("저장 위치를 변경했어요."); safe->repaint();
                    }
                });
        };
        speed.setTooltip(juce::String::fromUTF8("숫자가 작을수록 목표 음에 빨리 붙습니다. 0–15 ms: 강한 효과, 50–100 ms: 부드러운 보정."));
        monitor.setTooltip(juce::String::fromUTF8("유선 헤드폰을 연결한 뒤 켜 주세요. 녹음은 듣기 설정과 관계없이 저장됩니다."));
        bypass.setTooltip(juce::String::fromUTF8("끄면 보정하지 않은 소리를 출력합니다. 전환은 부드럽게 이어집니다."));
        recordButton.setEnabled(false); monitor.setEnabled(false);
        historyInput.fill(-1); historyTarget.fill(-1);
        status = juce::String::fromUTF8("유선 헤드폰을 연결하고 ‘마이크 시작’을 눌러 주세요.");
        startTimerHz(30);
    }
    ~MainComponent() override {
        stopTimer();
        if (deviceWindow) delete deviceWindow.getComponent();
        audio.stop(); setLookAndFeel(nullptr);
    }
    void resized() override {
        deviceButton.setBounds(28, 99, 204, 38); startButton.setBounds(792, 94, 180, 48);
        key.setBounds(668, 208, 282, 34); scale.setBounds(668, 278, 282, 34);
        speed.setBounds(658, 356, 300, 42); amount.setBounds(658, 441, 300, 42);
        formants.setBounds(663, 506, 285, 30);
        recordButton.setBounds(50, 606, 186, 48);
        folderButton.setBounds(50, 666, 140, 30); locationButton.setBounds(202, 666, 140, 30);
        monitor.setBounds(634, 601, 168, 32); bypass.setBounds(820, 601, 140, 32);
        volume.setBounds(724, 651, 236, 38);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(theme::bg);
        g.setColour(theme::accent); g.fillRoundedRectangle(28, 24, 42, 42, 12);
        g.setColour(theme::bg);
        for (int i = 0; i < 5; ++i) { float h = float(10 + (2 - std::abs(i - 2)) * 7); g.fillRoundedRectangle(float(37 + i * 5), 45 - h / 2, 3, h, 1.5f); }
        text(g, "PitchLab", 84, 19, 230, 40, 28, theme::text, true);
        text(g, "LIVE VOCAL STUDIO", 85, 59, 300, 20, 10, theme::muted, true);
        text(g, juce::String::fromUTF8("내 목소리, 원하는 음으로."), 525, 35, 274, 26, 15, theme::muted);
        g.setColour(running ? theme::green.withAlpha(0.12f) : theme::field);
        g.fillRoundedRectangle(823, 30, 149, 34, 17);
        text(g, running ? juce::String::fromUTF8("●  LIVE") : juce::String::fromUTF8("●  STANDBY"), 823, 32, 149, 29, 12, running ? theme::green : theme::muted, true, juce::Justification::centred);
        text(g, deviceDescription, 247, 99, 523, 22, 12, theme::muted);
        auto info = running ? juce::String(audio.currentRate.load() / 1000, 1) + " kHz  /  " + juce::String(audio.bufferSamples.load()) + juce::String::fromUTF8(" samples  /  예상 왕복 ")
            + juce::String(audio.totalLatencySamples.load() * 1000.0 / audio.currentRate.load(), 0) + " ms" : juce::String::fromUTF8("장치 선택 → 마이크 시작 → 녹음");
        text(g, info, 247, 122, 523, 19, 11, theme::muted);
        card(g, 28, 166, 594, 398); card(g, 642, 166, 330, 398); card(g, 28, 584, 944, 123);
        text(g, "LIVE PITCH", 50, 183, 220, 20, 11, theme::muted, true);
        text(g, juce::String::fromUTF8("입력 음"), 52, 219, 170, 20, 12, theme::muted);
        text(g, juce::String::fromUTF8("목표 음"), 362, 219, 200, 20, 12, theme::muted);
        auto hz = audio.pitch.detectedHz.load();
        auto midi = hz > 0 && running ? pitchlab::midiFromHz(hz) : -1;
        auto target = running ? audio.pitch.targetMidi.load() : -1;
        text(g, noteName(midi), 47, 244, 235, 66, 48, theme::text, true);
        text(g, juce::String::fromUTF8("→"), 275, 253, 60, 48, 30, theme::muted);
        text(g, noteName(target), 355, 244, 230, 66, 48, theme::accent, true);
        text(g, midi >= 0 ? juce::String(hz, 1) + " Hz" : juce::String::fromUTF8("목소리를 기다리고 있어요"), 53, 308, 230, 20, 12, theme::muted);
        text(g, target >= 0 ? juce::String(audio.pitch.correction.load(), 2) + " semitones" : juce::String::fromUTF8("스케일에 맞춰 자동 선택"), 362, 308, 224, 20, 12, theme::muted);
        drawHistory(g);
        text(g, juce::String::fromUTF8("입력"), 50, 515, 36, 20, 11, theme::muted); meter(g, 90, 522, 190, displayedInput, theme::green);
        text(g, juce::String::fromUTF8("출력"), 310, 515, 36, 20, 11, theme::muted); meter(g, 350, 522, 190, displayedOutput, theme::accent);
        if (clipFrames > 0) text(g, "CLIP", 550, 515, 55, 20, 10, theme::red, true);
        text(g, juce::String::fromUTF8("곡의 키"), 668, 182, 282, 22, 12, theme::muted);
        text(g, juce::String::fromUTF8("스케일"), 668, 252, 282, 22, 12, theme::muted);
        text(g, juce::String::fromUTF8("보정 속도"), 668, 328, 240, 24, 13, theme::text, true);
        text(g, juce::String::fromUTF8("빠르게 · 기계적인 효과           천천히 · 부드럽게"), 668, 400, 285, 20, 10, theme::muted);
        text(g, juce::String::fromUTF8("보정 강도"), 668, 419, 240, 24, 13, theme::text, true);
        text(g, juce::String::fromUTF8("0% 원래 음정                         100% 목표 음정"), 668, 484, 285, 20, 10, theme::muted);
        const auto sec = audio.recorder.seconds();
        text(g, recording ? juce::String::fromUTF8("●  REC  ") + timeString(sec) : lastTake.isNotEmpty() ? "SAVED  " + timeString(lastSeconds) : "READY TO RECORD", 260, 608, 350, 25, 16,
            recording ? theme::red : theme::text, true);
        text(g, recording ? juce::String::fromUTF8("보정음 + 원본을 함께 저장 중") : juce::String::fromUTF8("WAV · 24-bit · 원본도 별도로 저장"), 260, 640, 350, 20, 11, theme::muted);
        text(g, juce::String::fromUTF8("헤드폰 음량"), 642, 655, 83, 27, 12, theme::muted);
        text(g, status, 29, 719, 810, 27, 12, statusError ? theme::red : theme::muted);
        text(g, "STUDY BUILD 0.1.1", 830, 721, 142, 22, 10, theme::muted, false, juce::Justification::right);
    }
private:
    theme::Look look;
    AudioEngine audio;
    juce::TextButton deviceButton, startButton, recordButton, folderButton, locationButton;
    juce::ToggleButton monitor, bypass, formants;
    juce::ComboBox key, scale;
    juce::Slider speed, amount, volume;
    juce::TooltipWindow tooltips{this, 700};
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Component::SafePointer<juce::DialogWindow> deviceWindow;
    juce::File recordings;
    juce::String status, lastTake, deviceDescription{juce::String::fromUTF8("기본 마이크 / 기본 출력 장치")};
    bool running = false, recording = false, statusError = false;
    double lastSeconds = 0;
    float displayedInput = 0, displayedOutput = 0;
    int historyPos = 0, clipFrames = 0;
    std::array<float, 160> historyInput{}, historyTarget{};
    void setupButton(juce::TextButton& button, const juce::String& label) { button.setButtonText(label); addAndMakeVisible(button); }
    void setupToggle(juce::ToggleButton& button, const juce::String& label, bool value) {
        button.setButtonText(label); button.setToggleState(value, juce::dontSendNotification); addAndMakeVisible(button);
    }
    void setupSlider(juce::Slider& slider, double lo, double hi, double interval, double initial, const juce::String& suffix, const juce::String& name) {
        slider.setSliderStyle(juce::Slider::LinearHorizontal); slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 76, 25);
        slider.setRange(lo, hi, interval); slider.setValue(initial); slider.setTextValueSuffix(suffix); slider.setTitle(name); addAndMakeVisible(slider);
    }
    static void text(juce::Graphics& g, const juce::String& s, int x, int y, int w, int h, float size, juce::Colour c,
                     bool bold = false, juce::Justification align = juce::Justification::left) {
        g.setColour(c); g.setFont(theme::font(size, bold)); g.drawFittedText(s, x, y, w, h, align, 1);
    }
    static void card(juce::Graphics& g, float x, float y, float w, float h) {
        g.setColour(theme::panel); g.fillRoundedRectangle(x, y, w, h, 15);
        g.setColour(theme::field); g.drawRoundedRectangle(x, y, w, h, 15, 1);
    }
    static juce::String noteName(double midi) {
        if (midi < 0) return juce::String::fromUTF8("—");
        const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        int note = int(std::round(midi)); return juce::String(names[(note % 12 + 12) % 12]) + juce::String(note / 12 - 1);
    }
    static juce::String timeString(double seconds) {
        int s = int(seconds); return juce::String(s / 60).paddedLeft('0', 2) + ":" + juce::String(s % 60).paddedLeft('0', 2);
    }
    static void meter(juce::Graphics& g, float x, float y, float width, float amplitude, juce::Colour colour) {
        g.setColour(theme::field); g.fillRoundedRectangle(x, y, width, 5, 2);
        float normalized = std::clamp((20 * std::log10(std::max(amplitude, 0.001f)) + 60) / 60, 0.0f, 1.0f);
        g.setColour(colour); g.fillRoundedRectangle(x, y, width * normalized, 5, 2);
    }
    void drawHistory(juce::Graphics& g) {
        const juce::Rectangle<float> plot(52, 354, 543, 129);
        g.setColour(theme::bg.withAlpha(0.65f)); g.fillRoundedRectangle(plot, 8);
        for (int i = 1; i < 4; ++i) { g.setColour(theme::field); g.drawHorizontalLine(int(plot.getY() + i * plot.getHeight() / 4), plot.getX(), plot.getRight()); }
        auto draw = [&](const std::array<float, 160>& history, juce::Colour color, float thickness) {
            juce::Path path; bool pen = false;
            for (int i = 0; i < 160; ++i) {
                float note = history[(historyPos + i) % 160];
                if (note < 0) { pen = false; continue; }
                const float x = plot.getX() + plot.getWidth() * i / 159;
                const float y = plot.getBottom() - 8 - std::clamp((note - 36) / 48, 0.0f, 1.0f) * (plot.getHeight() - 16);
                if (!pen) path.startNewSubPath(x, y); else path.lineTo(x, y); pen = true;
            }
            g.setColour(color); g.strokePath(path, juce::PathStrokeType(thickness));
        };
        draw(historyTarget, theme::accent.withAlpha(0.7f), 1.5f); draw(historyInput, theme::green, 2);
        if (!running) text(g, juce::String::fromUTF8("마이크를 시작하면 음높이가 여기에 표시돼요"), 65, 397, 516, 28, 12, theme::muted, false, juce::Justification::centred);
        text(g, juce::String::fromUTF8("● 입력"), 444, 333, 66, 18, 10, theme::green); text(g, juce::String::fromUTF8("● 목표"), 516, 333, 76, 18, 10, theme::accent);
    }
    void showError(const juce::String& error) { status = error; statusError = true; repaint(); }
    void toggleAudio() {
        monitor.setToggleState(false, juce::dontSendNotification); audio.monitoring = false;
        if (running) {
            if (recording) finishRecording();
            audio.stop(); running = false; startButton.setButtonText(juce::String::fromUTF8("마이크 시작")); recordButton.setEnabled(false);
            deviceButton.setEnabled(true); status = juce::String::fromUTF8("마이크를 멈췄어요."); statusError = false;
        } else {
            if (deviceWindow) { deviceWindow->exitModalState(0); deviceWindow = nullptr; }
            auto error = audio.start();
            if (error.isNotEmpty()) { showError(error); return; }
            running = true; startButton.setButtonText(juce::String::fromUTF8("마이크 멈추기")); recordButton.setEnabled(true); deviceButton.setEnabled(false);
            historyInput.fill(-1); historyTarget.fill(-1);
            status = juce::String::fromUTF8("노래해 보세요. 헤드폰을 연결했다면 ‘헤드폰으로 듣기’를 켜 주세요."); statusError = false;
            updateDeviceDescription();
        }
        monitor.setEnabled(running);
        repaint();
    }
    void updateDeviceDescription() {
        auto setup = audio.devices.getAudioDeviceSetup();
        deviceDescription = setup.inputDeviceName + juce::String::fromUTF8(" → ") + setup.outputDeviceName;
    }
    void toggleRecording() {
        if (recording) { finishRecording(); return; }
        if (!running || !audio.ready.load()) { showError(juce::String::fromUTF8("먼저 마이크를 시작해 주세요.")); return; }
        auto error = audio.recorder.start(recordings, audio.currentRate.load());
        if (error.isNotEmpty()) { showError(error); return; }
        recording = true; recordButton.setButtonText(juce::String::fromUTF8("■  녹음 끝내기")); locationButton.setEnabled(false);
        status = juce::String::fromUTF8("녹음 중 · 듣기를 꺼도 보정된 소리는 저장돼요."); statusError = false; repaint();
    }
    void finishRecording() {
        audio.recorder.stop(); lastSeconds = audio.recorder.seconds(); lastTake = audio.recorder.wetFile.getFileName();
        recording = false; recordButton.setButtonText(juce::String::fromUTF8("●  녹음 시작")); locationButton.setEnabled(true);
        statusError = audio.recorder.hasFailed();
        status = statusError ? juce::String::fromUTF8("저장 중 문제가 생겼어요. 파일과 남은 디스크 공간을 확인해 주세요.") : juce::String::fromUTF8("저장 완료 · ") + lastTake;
        repaint();
    }
    void showDevices() {
        if (deviceWindow) { deviceWindow->toFront(true); return; }
        const auto error = audio.initialiseDevices();
        if (error.isNotEmpty()) showError(error);
        auto* selector = new juce::AudioDeviceSelectorComponent(audio.devices, 1, 1, 1, 2, false, false, true, false);
        selector->setSize(600, 450);
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(selector); options.dialogTitle = juce::String::fromUTF8("PitchLab · 마이크 / 헤드폰 설정");
        options.dialogBackgroundColour = theme::panel; options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true; options.resizable = false; options.componentToCentreAround = this;
        deviceWindow = options.launchAsync();
    }
    void timerCallback() override {
        if (running && (audio.deviceStopped.load() || audio.streamFault.load() != 0)) {
            const int fault = audio.streamFault.load();
            if (recording) finishRecording();
            audio.stop(); running = false; startButton.setButtonText(juce::String::fromUTF8("마이크 시작")); recordButton.setEnabled(false); deviceButton.setEnabled(true);
            monitor.setToggleState(false, juce::dontSendNotification); monitor.setEnabled(false);
            showError(fault != 0 ? juce::String::fromUTF8("오디오 처리 이상으로 출력을 차단하고 녹음을 멈췄어요. 오류 ") + juce::String(fault)
                                : juce::String::fromUTF8("오디오 장치 연결이 끊겼어요. 장치를 확인하고 다시 시작해 주세요."));
        }
        if (recording && audio.recorder.hasFailed()) finishRecording();
        if (!audio.monitoring.load()) monitor.setToggleState(false, juce::dontSendNotification);
        if (recording && audio.recorder.seconds() >= 3600) { finishRecording(); status = juce::String::fromUTF8("1시간 녹음을 저장했어요. 새 녹음을 시작할 수 있어요."); }
        const float hz = audio.pitch.detectedHz.load();
        historyInput[historyPos] = running && hz > 0 ? float(pitchlab::midiFromHz(hz)) : -1;
        historyTarget[historyPos] = running ? audio.pitch.targetMidi.load() : -1;
        historyPos = (historyPos + 1) % 160;
        displayedInput = std::max(displayedInput * 0.85f, running ? audio.pitch.inputPeak.load() : 0);
        displayedOutput = std::max(displayedOutput * 0.85f, running ? audio.pitch.outputPeak.load() : 0);
        if (audio.pitch.clipped.exchange(false)) clipFrames = 45; else clipFrames = std::max(0, clipFrames - 1);
        if (deviceWindow) updateDeviceDescription();
        repaint();
    }
};

// Exercise the actual callback and DSP worker with memory-only buffers.
int runStreamTests(std::ostream& log) {
    try {
        AudioEngine engine;
        std::vector<float> input(32768), left(32768), right(32768);
        const float* ins[] = {input.data()}; float* outs[] = {left.data(), right.data()};
        juce::AudioIODeviceCallbackContext context;
        for (double rate : {44100., 48000.}) {
            engine.prepareStream(rate, 512);
            require(engine.ready.load() && !engine.monitoring.load(), "stream starts with monitor off");
            engine.monitoring = true; engine.monitorDb = -18;
            int supplied = 0, consumed = 0;
            auto feed = [&](int count, bool consume) {
                for (int i = 0; i < count; ++i)
                    input[size_t(i)] = float(0.8 * std::sin(2 * pitchlab::pi * 432 * (supplied + i) / rate));
                engine.audioDeviceIOCallbackWithContext(ins, 1, outs, 2, count, context);
                supplied += count; if (consume) consumed += count;
                const int expectedReady = supplied / 512 * 512 - consumed;
                const auto deadline = juce::Time::getMillisecondCounter() + 2000;
                while (engine.bufferedOutputSamples() < expectedReady && engine.streamFault.load() == 0
                       && juce::Time::getMillisecondCounter() < deadline) juce::Thread::sleep(1);
                require(engine.streamFault.load() == 0 && engine.bufferedOutputSamples() >= expectedReady, "DSP worker drains capture FIFO");
                for (int i = 0; i < count; ++i)
                    require(std::isfinite(left[size_t(i)]) && std::abs(left[size_t(i)]) <= 0.030001f
                            && left[size_t(i)] == right[size_t(i)], "callback output finite, bounded and stereo matched");
            };
            for (int i = 0; i < 4; ++i) feed(512, false);
            float peak = 0;
            for (int i = 0; i < 200; ++i) {
                const int sizes[] = {128, 257, 512, 1024}; const int count = sizes[i % 4];
                feed(count, true);
                for (int j = 0; j < count; ++j) peak = std::max(peak, std::abs(left[size_t(j)]));
            }
            require(peak > 0.001f, "callback passes processed audio");
            std::fill(left.begin(), left.end(), 1.0f);
            engine.audioDeviceIOCallbackWithContext(ins, 1, outs, 2, 8192, context);
            require(engine.streamFault.load() == 2 && !engine.monitoring.load(), "underrun latches mute");
            for (int i = 0; i < 8192; ++i) require(left[size_t(i)] == 0 && right[size_t(i)] == 0, "underrun emits silence");
            engine.monitoring = true;
            engine.audioDeviceIOCallbackWithContext(ins, 1, outs, 2, 512, context);
            for (int i = 0; i < 512; ++i) require(left[size_t(i)] == 0, "monitor toggle cannot bypass stream fault");
            engine.stop();
        }
        engine.prepareStream(48000, 512); engine.monitoring = true;
        engine.audioDeviceIOCallbackWithContext(ins, 1, outs, 2, 32768, context);
        require(engine.streamFault.load() == 1 && !engine.monitoring.load(), "oversized capture latches mute");
        for (float x : left) require(x == 0, "capture overflow emits silence");
        engine.prepareStream(48000, 512); engine.monitoring = true;
        input[7] = std::numeric_limits<float>::quiet_NaN();
        engine.audioDeviceIOCallbackWithContext(ins, 1, outs, 2, 512, context);
        require(engine.streamFault.load() == 3 && !engine.monitoring.load(), "invalid device input latches mute");
        for (int i = 0; i < 512; ++i) require(left[size_t(i)] == 0, "invalid input emits silence");
        engine.stop();
        log << "PASS actual callback/worker: variable blocks, FIFO wrap, 44.1/48 kHz, restart, underrun, overflow, NaN and latched silence (no audio device opened)\n";
        return 0;
    } catch (const std::exception& e) { log << "FAIL stream: " << e.what() << '\n'; return 10; }
}

// End-to-end recorder verification uses generated audio and never opens a mic.
int runSelfTest(const juce::File& folder) {
    juce::ScopedNoDenormals noDenormals;
    std::ofstream log(folder.getChildFile("integration-test.txt").getFullPathName().toStdString());
    if (!log) return 2;
    if (runDspTests(log) != 0) return 9;
    if (runStreamTests(log) != 0) return 10;
    AudioEngine engine; engine.pitch.prepare(48000); engine.pitch.parameters.retuneMs = 0;
    auto error = engine.recorder.start(folder, 48000);
    if (error.isNotEmpty()) { log << error << '\n'; return 3; }
    constexpr int samples = 48000 * 3;
    std::array<float, 256> in{}, out{}, dry{};
    for (int offset = 0; offset < samples; offset += 256) {
        int n = std::min(256, samples - offset);
        double frequency = offset < 48000 ? 213 : offset < 96000 ? 432 : 452;
        for (int i = 0; i < n; ++i) {
            double phase = 2 * pitchlab::pi * frequency * (offset + i) / 48000;
            in[i] = float(0.2 * std::sin(phase) + 0.08 * std::sin(phase * 2) + 0.04 * std::sin(phase * 3));
        }
        engine.pitch.process(in.data(), out.data(), dry.data(), n);
        engine.recorder.push(out.data(), dry.data(), n);
    }
    engine.recorder.stop();
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatReader> wetReader(format.createReaderFor(engine.recorder.wetFile.createInputStream().release(), true));
    std::unique_ptr<juce::AudioFormatReader> dryReader(format.createReaderFor(engine.recorder.dryFile.createInputStream().release(), true));
    if (!wetReader || !dryReader || engine.recorder.hasFailed()) { log << "FAIL writer or reader\n"; return 4; }
    if (wetReader->lengthInSamples != samples || dryReader->lengthInSamples != samples || wetReader->bitsPerSample != 24 || wetReader->sampleRate != 48000) {
        log << "FAIL WAV format/sample count\n"; return 5;
    }
    juce::AudioBuffer<float> check(1, 16000);
    for (int segment = 0; segment < 3; ++segment) {
        wetReader->read(&check, 0, 16000, segment * 48000 + 30000, true, false);
        pitchlab::PitchDetector detector; detector.prepare(48000);
        for (int i = 0; i < check.getNumSamples(); ++i) detector.push(check.getSample(0, i));
        const double expected = segment == 0 ? pitchlab::hzFromMidi(56) : 440;
        const double cents = 1200 * std::log2(detector.frequency / expected);
        log << "segment " << segment << ": " << detector.frequency << " Hz, error " << cents << " cents\n";
        if (!std::isfinite(cents) || std::abs(cents) > 15) return 6;
    }
    log << "PASS 24-bit mono WAV x 2, 48000 Hz, " << samples << " samples each; actual saved audio is pitch-corrected\n";
    log << engine.recorder.wetFile.getFileName() << '\n' << engine.recorder.dryFile.getFileName() << '\n';
    // Repeated takes catch FIFO reset / thread restart / WAV header lifecycle bugs.
    for (int take = 0; take < 3; ++take) {
        auto e = engine.recorder.start(folder, 44100);
        if (e.isNotEmpty()) return 7;
        engine.recorder.push(out.data(), dry.data(), 256); engine.recorder.stop();
        std::unique_ptr<juce::AudioFormatReader> r(format.createReaderFor(engine.recorder.wetFile.createInputStream().release(), true));
        if (!r || r->lengthInSamples != 256 || r->sampleRate != 44100) return 8;
    }
    log << "PASS repeated start/stop at a changed sample rate\n";
    return 0;
}

class PitchLabApplication : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "PitchLab"; }
    const juce::String getApplicationVersion() override { return "0.1.1"; }
    void initialise(const juce::String& commandLine) override {
        auto args = juce::StringArray::fromTokens(commandLine, true); args.removeEmptyStrings();
        if (args.size() >= 2 && args[0] == "--self-test") {
            juce::File folder(args[1].unquoted()); folder.createDirectory();
            setApplicationReturnValue(runSelfTest(folder)); quit(); return;
        }
        window = std::make_unique<Window>();
    }
    void shutdown() override { window.reset(); }
    void systemRequestedQuit() override { quit(); }
    class Window : public juce::DocumentWindow {
    public:
        Window() : DocumentWindow(juce::String::fromUTF8("PitchLab · 실시간 보컬 보정"), theme::bg, DocumentWindow::allButtons) {
            setUsingNativeTitleBar(true); setContentOwned(new MainComponent(), true);
            setResizable(false, false); centreWithSize(getWidth(), getHeight()); setVisible(true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<Window> window;
};
START_JUCE_APPLICATION(PitchLabApplication)
