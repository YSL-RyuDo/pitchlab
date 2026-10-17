#include "AudioEngine.h"
#include "OfflineRenderer.h"
#include "VocalKeyboard.h"
#include <fstream>
#define PITCHLAB_EMBEDDED_TESTS
#include "../Tests/DspTests.cpp"
#include "../Tests/LiveStreamTests.h"
#include "../Tests/WasapiFormatTests.h"

namespace theme {
const juce::Colour bg{0xff11191f}, panel{0xff202b33}, field{0xff2c3a44}, text{0xffedf3f6}, muted{0xff9cacb8};
const juce::Colour accent{0xff52dec7}, green{0xff73e3be}, red{0xffff8096};
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
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float position,
                          float start, float end, juce::Slider&) override {
        const float radius = std::min(width, height) * 0.39f, cx = x + width * 0.5f, cy = y + height * 0.5f;
        juce::Path track; track.addCentredArc(cx,cy,radius+7,radius+7,0,start,end,true);
        g.setColour(field.brighter(0.2f)); g.strokePath(track,juce::PathStrokeType(3));
        juce::Path arc; arc.addCentredArc(cx,cy,radius+7,radius+7,0,start,start+position*(end-start),true);
        g.setColour(accent); g.strokePath(arc,juce::PathStrokeType(3));
        g.setColour(juce::Colours::black.withAlpha(0.3f)); g.fillEllipse(cx-radius,cy-radius+4,radius*2,radius*2);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff465763),cx,cy-radius,juce::Colour(0xff1d2a33),cx,cy+radius,false));
        g.fillEllipse(cx-radius,cy-radius,radius*2,radius*2);
        g.setColour(juce::Colour(0xff627581)); g.drawEllipse(cx-radius,cy-radius,radius*2,radius*2,1);
        const auto centre=juce::Point<float>(cx,cy); const float angle=start+position*(end-start);
        g.setColour(text); g.drawLine({centre.getPointOnCircumference(radius*0.60f,angle),centre.getPointOnCircumference(radius*0.84f,angle)},3);
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
        setLookAndFeel(&look); setSize(1100, 940);
        recordings = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("Recordings");
        setupButton(deviceButton, juce::String::fromUTF8("오디오 설정"));
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
        scale.addItemList({juce::String::fromUTF8("모든 반음 · Chromatic"), juce::String::fromUTF8("장조 · Major"), juce::String::fromUTF8("자연단음계 · Minor"), juce::String::fromUTF8("마이너 펜타토닉"), juce::String::fromUTF8("직접 고른 음")}, 1);
        key.setSelectedId(1); scale.setSelectedId(1);
        key.setTitle(juce::String::fromUTF8("곡의 키")); scale.setTitle(juce::String::fromUTF8("곡의 스케일"));
        addAndMakeVisible(key); addAndMakeVisible(scale);
        setupSlider(speed, 0, 150, 1, 12, " ms", juce::String::fromUTF8("보정 속도"));
        setupSlider(amount, 0, 100, 1, 100, "%", juce::String::fromUTF8("보정 강도"));
        setupSlider(volume, pitchlab::HeadphoneGuard::minimumDb, pitchlab::HeadphoneGuard::maximumDb, 1,
                    pitchlab::HeadphoneGuard::defaultDb, " dB", juce::String::fromUTF8("헤드폰 음량"));
        key.onChange = [this] { updateScale(); };
        scale.onChange = [this] { updateScale(); };
        speed.onValueChange = [this] { audio.pitch.parameters.retuneMs = float(speed.getValue()); markCustom(); };
        amount.onValueChange = [this] { audio.pitch.parameters.amount = float(amount.getValue() / 100); markCustom(); };
        volume.onValueChange = [this] { audio.monitorDb = float(volume.getValue()); };
        monitor.onClick = [this] {
            audio.monitoring = monitor.getToggleState() && running && audio.streamFault.load() == 0;
            monitor.setToggleState(audio.monitoring.load(), juce::dontSendNotification);
            if (running) {
                statusError = false;
                status = audio.monitoring.load()
                    ? juce::String::fromUTF8("듣기 켬 · ") + audio.devices.getAudioDeviceSetup().outputDeviceName
                    : juce::String::fromUTF8("듣기 끔 · 녹음에는 영향을 주지 않아요.");
            }
        };
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
        volume.setTooltip(juce::String::fromUTF8("0 dB는 보정음을 그대로 듣는 크기입니다. 작으면 +6, +12 dB 순서로 올려 보세요. 출력 상한은 유지되며 녹음 음량에는 영향을 주지 않습니다."));
        bypass.setTooltip(juce::String::fromUTF8("끄면 보정하지 않은 소리를 출력합니다. 전환은 부드럽게 이어집니다."));
        recordButton.setEnabled(false); monitor.setEnabled(false);
        setupSlider(humanize, 0, 100, 1, 65, "%", juce::String::fromUTF8("긴 음의 흔들림 유지"));
        for (auto* knob : {&speed, &amount, &humanize, &volume}) {
            knob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            knob->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 95, 24);
            knob->setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        }
        humanize.onValueChange = [this] { audio.pitch.parameters.humanize = float(humanize.getValue() / 100); markCustom(); };
        humanize.setTooltip(juce::String::fromUTF8("길게 유지하는 음에서 보정을 느리게 해 작은 흔들림을 남깁니다."));
        setupSlider(fx,0,100,1,0,"%",juce::String::fromUTF8("스타일 효과"));
        setupSlider(tempo,60,200,1,140," BPM",juce::String::fromUTF8("곡의 BPM"));
        setupSlider(recordGain,0,30,1,18," dB",juce::String::fromUTF8("녹음 파일 증폭"));
        fx.onValueChange=[this] { audio.pitch.parameters.fxMix=float(fx.getValue()/100); markCustom(); };
        tempo.onValueChange=[this] { audio.pitch.parameters.bpm=float(tempo.getValue()); };
        fx.setTooltip(juce::String::fromUTF8("스타일 프리셋의 음색·메아리·룸 효과 양입니다. 0%면 음정 보정만 남습니다."));
        tempo.setTooltip(juce::String::fromUTF8("메아리 박자를 맞춥니다. 마이크 시작 전에 곡의 BPM을 넣으세요."));
        recordGain.setTooltip(juce::String::fromUTF8("저장되는 보정 WAV만 키웁니다. 기본 +18 dB, 최대 +30 dB. 이미 큰 입력은 0 dB로 낮추세요. 원본과 헤드폰 음량은 바뀌지 않습니다. 녹음 시작 시 고정됩니다."));
        setupButton(levelButton,juce::String::fromUTF8("기존 WAV 음량 키우기")); levelButton.onClick=[this] { chooseFile(true); };
        preset.addItemList({"Natural vocal", "Tight pop", "Hard tune"}, 1);
        for(int i=0;i<6;++i) preset.addItem(juce::String::fromUTF8(pitchlab::styleProfiles[size_t(i)].name),i+4);
        preset.addItem("Custom",VocalSettings::customId); addAndMakeVisible(preset);
        preset.setTooltip(juce::String::fromUTF8("아티스트 스타일 참고용 자체 프리셋입니다. 공식 세팅이나 목소리 복제가 아닙니다. KEY/SCALE/BPM은 곡에 맞추세요."));
        preset.onChange = [this] { if (preset.getSelectedId() >= 1 && preset.getSelectedId() <= VocalSettings::presetCount) applyPreset(preset.getSelectedId() - 1); };
        preset.setSelectedId(1, juce::sendNotificationSync);
        voice.addItemList({juce::String::fromUTF8("자동 음역"), juce::String::fromUTF8("낮은 음역 · 65–400 Hz"), juce::String::fromUTF8("높은 음역 · 130–1000 Hz")}, 1);
        voice.setSelectedId(1); addAndMakeVisible(voice);
        voice.onChange = [this] { audio.pitch.parameters.voiceRange = voice.getSelectedId() - 1; };
        addAndMakeVisible(keyboard);
        keyboard.onMaskChanged = [this](int mask) { audio.pitch.parameters.noteMask = mask; scale.setSelectedId(5, juce::dontSendNotification); };
        setupButton(fileButton, juce::String::fromUTF8("WAV 파일 보정")); fileButton.onClick = [this] { chooseFile(); };
        fileButton.setTooltip(juce::String::fromUTF8("파일용 엔진으로 처리합니다. 실시간 녹음과 소리의 질감이 다를 수 있어요."));
        setupButton(diagnosticButton, juce::String::fromUTF8("진단 저장")); diagnosticButton.onClick = [this] { saveDiagnostics(); };
        historyInput.fill(-1); historyTarget.fill(-1);
        status = juce::String::fromUTF8("유선 헤드폰을 연결하고 ‘마이크 시작’을 눌러 주세요.");
        startTimerHz(30);
    }
    ~MainComponent() override {
        stopTimer();
        if (deviceWindow) delete deviceWindow.getComponent();
        audio.stop(); setLookAndFeel(nullptr);
    }
    void verifyControls() {
        require(std::abs(audio.pitch.parameters.humanize.load()-0.65f)<0.001f && audio.pitch.parameters.retuneMs.load()==45, "initial preset reaches DSP even when slider value is unchanged");
        preset.setSelectedId(3,juce::sendNotificationSync);
        require(audio.pitch.parameters.retuneMs.load()==0 && audio.pitch.parameters.humanize.load()==0, "Hard tune preset reaches DSP");
        require(!audio.monitoring.load() && audio.monitorDb.load()==0, "preset leaves monitor off at unity gain");
        volume.setValue(12, juce::sendNotificationSync);
        require(audio.monitorDb.load()==12 && !audio.monitoring.load(), "monitor boost control does not enable playback");
        preset.setSelectedId(1, juce::sendNotificationSync);
        require(audio.monitorDb.load()==12, "tone preset preserves monitor gain");
        speed.setValue(23,juce::sendNotificationSync);
        require(preset.getSelectedId()==VocalSettings::customId && audio.pitch.parameters.retuneMs.load()==23, "manual editing marks Custom");
        scale.setSelectedId(2,juce::sendNotificationSync);
        require(!(keyboard.mask & 2), "major scale excludes C sharp in keyboard");
        keyboard.togglePitchClass(1);
        require((audio.pitch.parameters.noteMask.load() & 2) && scale.getSelectedId()==5, "keyboard edits reach custom DSP mask");
        keyboard.mask=1; keyboard.togglePitchClass(0);
        require(keyboard.mask==1, "keyboard cannot disable every note");
        const int songMask=audio.pitch.parameters.noteMask.load();
        for(int id=4;id<=9;++id) {
            preset.setSelectedId(id,juce::sendNotificationSync);
            require(audio.pitch.parameters.toneStyle.load()==id-3 && audio.pitch.parameters.fxMix.load()==1,"artist style reaches live processor");
            require(audio.monitorDb.load()==12 && recordGain.getValue()==18 && audio.pitch.parameters.noteMask.load()==songMask,
                    "styles preserve headphone gain, file gain and song notes");
        }
        auto settings=VocalSettings::capture(audio.pitch.parameters);
        require(settings.style==6 && settings.fx==1,"offline capture carries style FX");
    }
    void resized() override {
        preset.setBounds(489, 32, 445, 34);
        key.setBounds(45, 117, 126, 33); scale.setBounds(185, 117, 258, 33);
        voice.setBounds(461, 117, 237, 33); formants.setBounds(718, 118, 196, 32); bypass.setBounds(925, 118, 142, 32);
        speed.setBounds(52, 250, 145, 134); amount.setBounds(218, 250, 145, 134);
        humanize.setBounds(739, 250, 145, 134); volume.setBounds(905, 250, 145, 134);
        keyboard.setBounds(50, 608, 706, 80);
        startButton.setBounds(44, 727, 146, 38); recordButton.setBounds(204, 727, 152, 38);
        monitor.setBounds(376, 730, 171, 32); fileButton.setBounds(562, 727, 156, 38);
        diagnosticButton.setBounds(734, 727, 123, 38); deviceButton.setBounds(875, 727, 180, 38);
        folderButton.setBounds(44, 775, 146, 24); locationButton.setBounds(204, 775, 152, 24);
        for(auto* c : std::initializer_list<juce::Component*>{&startButton,&recordButton,&monitor,&fileButton,&diagnosticButton,&deviceButton,&folderButton,&locationButton})
            c->setTopLeftPosition(c->getX(),c->getY()+100);
        fx.setBounds(45,752,245,38); tempo.setBounds(320,752,175,38);
        recordGain.setBounds(530,752,245,38); levelButton.setBounds(815,749,240,38);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(theme::bg);
        g.setColour(theme::accent); g.fillRoundedRectangle(28, 27, 42, 42, 11);
        g.setColour(theme::bg);
        for (int i = 0; i < 5; ++i) { float h = float(12 + (2 - std::abs(i - 2)) * 8); g.fillRoundedRectangle(float(37 + i * 5), 48 - h / 2, 3, h, 1.5f); }
        text(g, "PitchLab", 84, 21, 215, 38, 28, theme::text, true);
        text(g, "VOCAL TUNING / 04", 87, 60, 250, 16, 10, theme::muted, true);
        text(g, "PRESET", 423, 37, 60, 23, 10, theme::muted, true);
        text(g,juce::String::fromUTF8("스타일 참고 프리셋 · 곡의 KEY / SCALE / BPM을 맞춰 주세요"),489,68,560,18,10,theme::muted);
        g.setColour(running ? theme::accent.withAlpha(0.12f) : theme::field); g.fillRoundedRectangle(951, 32, 121, 34, 17);
        text(g, running ? "LIVE" : "STANDBY", 952, 36, 120, 24, 11, running ? theme::accent : theme::muted, true, juce::Justification::centred);
        card(g, 28, 91, 1044, 74);
        text(g, "KEY", 47, 96, 125, 18, 10, theme::muted, true); text(g, "SCALE", 187, 96, 240, 18, 10, theme::muted, true);
        text(g, "VOICE RANGE", 463, 96, 230, 18, 10, theme::muted, true);
        card(g, 28, 180, 1044, 267);
        text(g, "RETUNE SPEED", 40, 208, 170, 22, 11, theme::muted, true, juce::Justification::centred);
        text(g, "CORRECTION", 207, 208, 167, 22, 11, theme::muted, true, juce::Justification::centred);
        text(g, "HUMANIZE", 729, 208, 166, 22, 11, theme::muted, true, juce::Justification::centred);
        text(g, "HEADPHONES", 894, 208, 167, 22, 11, theme::muted, true, juce::Justification::centred);
        text(g, juce::String::fromUTF8("음에 붙는 속도"), 40, 398, 170, 22, 12, theme::text, false, juce::Justification::centred);
        text(g, juce::String::fromUTF8("목표 음으로 보정"), 207, 398, 167, 22, 12, theme::text, false, juce::Justification::centred);
        text(g, juce::String::fromUTF8("긴 음의 흔들림 유지"), 723, 398, 174, 22, 12, theme::text, false, juce::Justification::centred);
        text(g, juce::String::fromUTF8("듣기 음량만 조절"), 891, 398, 173, 22, 12, theme::text, false, juce::Justification::centred);
        const float cx = 550, cy = 302, r = 105;
        g.setColour(juce::Colour(0xff10181e)); g.fillEllipse(cx-r,cy-r,2*r,2*r);
        g.setColour(theme::field); g.drawEllipse(cx-r,cy-r,2*r,2*r,2);
        for (int i=0;i<=40;++i) {
            const float angle = -2.3f + i * 4.6f / 40;
            const auto centre = juce::Point<float>(cx,cy);
            const auto a = centre.getPointOnCircumference(r-8,angle), b = centre.getPointOnCircumference(r-(i%5==0?17.0f:12.0f),angle);
            g.setColour(i%5==0?theme::muted:theme::field); g.drawLine({a,b},1.4f);
        }
        const float hz = running ? audio.pitch.detectedHz.load() : 0;
        const double midi = hz > 0 ? pitchlab::midiFromHz(hz) : -1;
        const double target = running ? audio.pitch.targetMidi.load() : -1;
        if (target >= 0 && midi >= 0) {
            const float angle = float(std::clamp((midi-target)*100.0,-50.0,50.0)/50.0*2.3);
            const auto centre = juce::Point<float>(cx,cy);
            g.setColour(theme::accent); g.drawLine({centre.getPointOnCircumference(83,angle),centre.getPointOnCircumference(102,angle)},4);
        }
        text(g,"TARGET NOTE",460,237,180,20,10,theme::muted,true,juce::Justification::centred);
        text(g,noteName(target),451,266,198,66,49,theme::accent,true,juce::Justification::centred);
        text(g,midi>=0 ? noteName(midi)+" / "+juce::String(hz,1)+" Hz" : juce::String::fromUTF8("목소리를 기다리는 중"),448,341,204,24,12,theme::muted,false,juce::Justification::centred);
        text(g,target>=0 ? juce::String(audio.pitch.correction.load()*100,0)+" cents" : "INPUT  /  TARGET",439,417,222,21,11,theme::muted,false,juce::Justification::centred);
        card(g,28,461,1044,102); text(g,"PITCH TRACE",50,466,200,18,10,theme::muted,true); drawHistory(g);
        card(g,28,577,1044,125); text(g,juce::String::fromUTF8("보정할 음 선택"),50,582,280,21,12,theme::text,true);
        text(g,juce::String::fromUTF8("건반을 눌러 음을 켜고 끄세요"),800,612,244,25,12,theme::text);
        text(g,juce::String::fromUTF8("초록 표시 = 보정에 사용할 음"),800,645,244,24,11,theme::muted);
        card(g,28,714,1044,86);
        const int style=audio.pitch.parameters.toneStyle.load();
        text(g,style>0 ? juce::String::fromUTF8(pitchlab::styleProfiles[size_t(style-1)].description)
                      : juce::String::fromUTF8("기본 음정 보정 · 아티스트 스타일을 고르면 음색과 공간 효과도 적용돼요"),48,716,995,20,11,theme::muted);
        text(g,"STYLE FX",48,736,240,18,10,theme::muted,true);
        text(g,"SONG BPM",323,736,170,18,10,theme::muted,true);
        text(g,juce::String::fromUTF8("녹음 크기 · 파일만 증폭"),534,736,260,18,11,theme::accent,true);
        card(g,28,814,1044,94);
        text(g,"IN",377,877,23,18,9,theme::muted,true); meter(g,404,883,132,displayedInput,theme::green);
        text(g,"PHONES",551,877,44,18,9,theme::muted,true); meter(g,598,883,121,displayedOutput,theme::accent);
        text(g,recording?"REC "+timeString(audio.recorder.seconds()):running?juce::String(audio.bufferSamples.load())+" samples / DELAY "+juce::String(audio.totalLatencySamples.load()*1000.0/audio.currentRate.load(),0)+" ms / XRUN "+juce::String(driverXruns):"WAV 24-bit / LOCAL",737,873,316,25,11,recording?theme::red:theme::muted);
        text(g,status,30,914,875,22,11,statusError?theme::red:theme::muted);
        text(g,"STUDY 0.4.0",929,914,140,22,10,theme::muted,false,juce::Justification::right);
    }

private:
    theme::Look look;
    AudioEngine audio;
    juce::TextButton deviceButton, startButton, recordButton, folderButton, locationButton, fileButton, diagnosticButton, levelButton;
    juce::ToggleButton monitor, bypass, formants;
    juce::ComboBox key, scale, preset, voice;
    VocalKeyboard keyboard;
    std::unique_ptr<OfflineRenderer> renderJob;
    int driverXruns = 0, xrunBaseline = 0;
    juce::String lastDiagnostic;
    juce::Slider speed, amount, humanize, volume, fx, tempo, recordGain;
    juce::TooltipWindow tooltips{this, 700};
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Component::SafePointer<juce::DialogWindow> deviceWindow;
    juce::File recordings;
    juce::String status, lastTake, deviceDescription{juce::String::fromUTF8("기본 마이크 / 기본 출력 장치")};
    bool running = false, recording = false, statusError = false, applyingPreset = false;
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
        const juce::Rectangle<float> plot(50, 490, 1000, 62);
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
        if (!running) text(g, juce::String::fromUTF8("마이크를 시작하면 음높이가 여기에 표시돼요"), 260, 506, 580, 28, 12, theme::muted, false, juce::Justification::centred);
        text(g, juce::String::fromUTF8("● 입력"), 895, 466, 66, 18, 10, theme::green); text(g, juce::String::fromUTF8("● 목표"), 972, 466, 76, 18, 10, theme::accent);
    }
    void markCustom() { if (!applyingPreset && preset.getNumItems() > 0) preset.setSelectedId(VocalSettings::customId, juce::dontSendNotification); }
    void applyPreset(int index) {
        applyingPreset = true;
        auto settings = VocalSettings::preset(index);
        speed.setValue(settings.speed,juce::sendNotificationSync); amount.setValue(settings.amount*100,juce::sendNotificationSync);
        humanize.setValue(settings.humanize*100,juce::sendNotificationSync);
        audio.pitch.parameters.retuneMs = settings.speed; audio.pitch.parameters.amount = settings.amount; audio.pitch.parameters.humanize = settings.humanize;
        audio.pitch.parameters.toneStyle=settings.style; audio.pitch.parameters.fxMix=settings.fx;
        fx.setValue(settings.fx*100,juce::dontSendNotification);
        fx.setEnabled(settings.style>0);
        applyingPreset = false;
        // A preset changes tone controls, never output volume or monitoring.
    }
    void updateScale() {
        audio.pitch.parameters.key=key.getSelectedId()-1;
        if (scale.getSelectedId()==5) { audio.pitch.parameters.noteMask=keyboard.mask; return; }
        audio.pitch.parameters.scale=scale.getSelectedId()-1; audio.pitch.parameters.noteMask=0;
        keyboard.mask=pitchlab::scaleMask(key.getSelectedId()-1,scale.getSelectedId()-1); keyboard.repaint();
    }
    juce::String diagnosticText() {
        auto* device=audio.devices.getCurrentAudioDevice();
        juce::String result="PitchLab 0.4.0 device diagnostic\n";
        result+="Device: "+deviceDescription+"\nBackend: "+audio.devices.getCurrentAudioDeviceType();
        result+="\nRate: "+juce::String(audio.currentRate.load())+"\nBuffer: "+juce::String(audio.bufferSamples.load());
        result+="\nEstimated latency ms: "+juce::String(audio.totalLatencySamples.load()*1000.0/audio.currentRate.load(),2);
        result+="\nDevice-reported IO latency ms: "+juce::String(audio.deviceLatencySamples.load()*1000.0/audio.currentRate.load(),2);
        result+="\nLive engine: Signalsmith short window / 128-frame worker / 1 ms limiters";
        result+="\nStream fault: "+juce::String(audio.streamFault.load())+"\nCapture overflow: "+juce::String(audio.captureOverflows.load());
        result+="\nPlayback underrun: "+juce::String(audio.outputUnderruns.load())+"\nDSP late blocks: "+juce::String(audio.dspOverruns.load());
        result+="\nLongest DSP block ms: "+juce::String(audio.maxDspMs.load(),3)+"\nDevice xruns since start: "+juce::String(driverXruns);
        result+="\nProcessed frames: "+juce::String(audio.processedSamples.load())+"\nMonitoring: "+juce::String(int(audio.monitoring.load()));
        result+="\nMonitor gain dB: "+juce::String(audio.monitorDb.load())+"\nPost-guard monitor peak: "+juce::String(audio.monitorPeak.load(),6);
        result+="\nCurrent device open: "+juce::String(int(device!=nullptr))+"\nNo microphone audio included.\n";
        return result;
    }
    void saveDiagnostics(bool announce=true) {
        if (recordings.createDirectory().failed()) { if(announce)showError(juce::String::fromUTF8("진단 저장 폴더를 만들지 못했어요.")); return; }
        auto file=recordings.getNonexistentChildFile("diagnostic_"+juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S"),".txt");
        const bool ok=file.replaceWithText(running||lastDiagnostic.isEmpty()?diagnosticText():lastDiagnostic);
        if(announce) { statusError=!ok; status=ok?juce::String::fromUTF8("녹음 폴더에 장치 진단을 저장했어요."):juce::String::fromUTF8("진단 파일을 저장하지 못했어요."); }
    }
    void chooseFile(bool levelOnly=false) {
        if(running || renderJob) return;
        chooser=std::make_unique<juce::FileChooser>(juce::String::fromUTF8(levelOnly?"음량을 키울 WAV 선택":"보정할 보컬 WAV 선택"),juce::File(),"*.wav");
        juce::Component::SafePointer<MainComponent> safe(this);
        chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe,levelOnly](const juce::FileChooser& fc){
            if(!safe || !fc.getResult().existsAsFile())return;
            safe->renderJob=std::make_unique<OfflineRenderer>();
            auto settings=VocalSettings::capture(safe->audio.pitch.parameters);
            settings.recordDb=float(safe->recordGain.getValue()); settings.levelOnly=levelOnly;
            if(!safe->renderJob->start(fc.getResult(),safe->recordings,settings)) {
                safe->renderJob.reset(); safe->showError(juce::String::fromUTF8("파일 처리 스레드를 시작하지 못했어요.")); return;
            }
            safe->startButton.setEnabled(false); safe->fileButton.setEnabled(false); safe->levelButton.setEnabled(false); safe->recordGain.setEnabled(false); safe->statusError=false;
        });
    }
    void showError(const juce::String& error) { status = error; statusError = true; repaint(); }
    void toggleAudio() {
        monitor.setToggleState(false, juce::dontSendNotification); audio.monitoring = false;
        if (running) {
            if (recording) finishRecording();
            lastDiagnostic = diagnosticText(); audio.stop(); running = false; startButton.setButtonText(juce::String::fromUTF8("마이크 시작")); recordButton.setEnabled(false);
            deviceButton.setEnabled(true); status = juce::String::fromUTF8("마이크를 멈췄어요."); statusError = false;
        } else {
            if (deviceWindow) { deviceWindow->exitModalState(0); deviceWindow = nullptr; }
            auto error = audio.start();
            if (error.isNotEmpty()) { showError(error); return; }
            running = true; startButton.setButtonText(juce::String::fromUTF8("마이크 멈추기")); recordButton.setEnabled(true); deviceButton.setEnabled(false);
            historyInput.fill(-1); historyTarget.fill(-1);
            status = juce::String::fromUTF8("노래해 보세요. 헤드폰을 연결했다면 ‘헤드폰으로 듣기’를 켜 주세요."); statusError = false;
            updateDeviceDescription(); xrunBaseline = audio.devices.getXRunCount(); driverXruns = 0;
        }
        monitor.setEnabled(running); fileButton.setEnabled(!running); levelButton.setEnabled(!running); tempo.setEnabled(!running);
        repaint();
    }
    void updateDeviceDescription() {
        auto setup = audio.devices.getAudioDeviceSetup();
        deviceDescription = setup.inputDeviceName + juce::String::fromUTF8(" → ") + setup.outputDeviceName;
    }
    void toggleRecording() {
        if (recording) { finishRecording(); return; }
        if (!running || !audio.ready.load()) { showError(juce::String::fromUTF8("먼저 마이크를 시작해 주세요.")); return; }
        auto error = audio.recorder.start(recordings, audio.currentRate.load(),float(recordGain.getValue()));
        if (error.isNotEmpty()) { showError(error); return; }
        recording = true; recordButton.setButtonText(juce::String::fromUTF8("■  녹음 끝내기")); locationButton.setEnabled(false);
        recordGain.setEnabled(false);
        status = juce::String::fromUTF8("녹음 중 · 듣기를 꺼도 보정된 소리는 저장돼요."); statusError = false; repaint();
    }
    void finishRecording() {
        audio.recorder.stop(); lastSeconds = audio.recorder.seconds(); lastTake = audio.recorder.wetFile.getFileName();
        recording = false; recordButton.setButtonText(juce::String::fromUTF8("●  녹음 시작")); locationButton.setEnabled(true);
        recordGain.setEnabled(true);
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
        if (renderJob) {
            if (renderJob->done.load()) {
                renderJob->waitForCompletion();
                statusError = renderJob->error.isNotEmpty();
                status = statusError ? renderJob->error : juce::String::fromUTF8("파일 처리 완료 · ")+renderJob->wetFile.getFileName();
                renderJob.reset(); startButton.setEnabled(true); fileButton.setEnabled(true); levelButton.setEnabled(true); recordGain.setEnabled(true);
            } else status = juce::String::fromUTF8("WAV 파일 처리 중 · ") + juce::String(int(renderJob->progress.load() * 100)) + "%";
        }
        if (running) {
            const int latest = std::max(0, audio.devices.getXRunCount() - xrunBaseline);
            if (latest > driverXruns && audio.processedSamples.load() > audio.currentRate.load() * 0.5) {
                audio.monitoring = false; audio.streamFault = 5;
            }
            driverXruns = latest;
        }
        keyboard.activeNote = running && audio.pitch.targetMidi.load() >= 0 ? int(audio.pitch.targetMidi.load()) : -1; keyboard.repaint();
        if (running && (audio.deviceStopped.load() || audio.streamFault.load() != 0)) {
            const int fault = audio.streamFault.load();
            if (recording) finishRecording();
            lastDiagnostic = diagnosticText(); audio.stop(); running = false; startButton.setButtonText(juce::String::fromUTF8("마이크 시작")); recordButton.setEnabled(false); deviceButton.setEnabled(true);
            monitor.setToggleState(false, juce::dontSendNotification); monitor.setEnabled(false); fileButton.setEnabled(true);
            levelButton.setEnabled(true); tempo.setEnabled(true);
            saveDiagnostics(false);
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
        displayedOutput = running && audio.monitoring.load() ? std::max(displayedOutput * 0.85f, audio.monitorPeak.load()) : 0;
        if (audio.pitch.clipped.exchange(false)) clipFrames = 45; else clipFrames = std::max(0, clipFrames - 1);
        if (deviceWindow) updateDeviceDescription();
        repaint();
    }
};

// Exercise the actual callback and DSP worker with memory-only buffers.
int runStreamTests(std::ostream& log) {
    try {
        auto ownedEngine = std::make_unique<AudioEngine>(); auto& engine = *ownedEngine;
        std::vector<float> input(32768), left(32768), right(32768);
        const float* ins[] = {input.data()}; float* outs[] = {left.data(), right.data()};
        juce::AudioIODeviceCallbackContext context;
        for (double rate : {44100., 48000.}) {
            engine.prepareStream(rate, 1024);
            require(engine.ready.load() && !engine.monitoring.load(), "stream starts with monitor off");
            engine.monitoring = true; engine.monitorDb = -18;
            int supplied = 0, consumed = 0;
            auto feed = [&](int count, bool consume) {
                for (int i = 0; i < count; ++i)
                    input[size_t(i)] = float(0.8 * std::sin(2 * pitchlab::pi * 432 * (supplied + i) / rate));
                engine.audioDeviceIOCallbackWithContext(ins, 1, outs, 2, count, context);
                supplied += count; if (consume) consumed += count;
                const int expectedReady = supplied / AudioEngine::processingBlockSize() * AudioEngine::processingBlockSize() - consumed;
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
    try { testWasapiFormats(log); }
    catch (const std::exception& e) { log << "FAIL WASAPI format: " << e.what() << '\n'; return 16; }
    if (runStreamTests(log) != 0) return 10;
    try { testLiveStreamLatency(log); testQuietMonitorStream(log); }
    catch (const std::exception& e) { log << "FAIL live stream: " << e.what() << '\n'; return 15; }
    try { auto panel = std::make_unique<MainComponent>(); panel->verifyControls(); log << "PASS preset/control/keyboard state without a window or audio device\n"; }
    catch (const std::exception& e) { log << "FAIL controls: " << e.what() << '\n'; return 14; }
    auto ownedEngine = std::make_unique<AudioEngine>(); auto& engine = *ownedEngine; engine.pitch.prepare(48000); engine.pitch.parameters.retuneMs = 0;
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
    auto gainError=engine.recorder.start(folder,48000,18);
    if(gainError.isNotEmpty()) return 17;
    in.fill(.01f); dry.fill(.02f); engine.recorder.push(in.data(),dry.data(),256); engine.recorder.stop();
    std::unique_ptr<juce::AudioFormatReader> boosted(format.createReaderFor(engine.recorder.wetFile.createInputStream().release(),true));
    std::unique_ptr<juce::AudioFormatReader> original(format.createReaderFor(engine.recorder.dryFile.createInputStream().release(),true));
    if(!boosted || !original || engine.recorder.hasFailed() || boosted->lengthInSamples!=256 || original->lengthInSamples!=256) return 18;
    juce::AudioBuffer<float> a(1,256),b(1,256); boosted->read(&a,0,256,0,true,false); original->read(&b,0,256,0,true,false);
    for(int i=0;i<256;++i) if(std::abs(a.getSample(0,i)-.01f*std::pow(10.f,.9f))>1e-6f || std::abs(b.getSample(0,i)-.02f)>1e-6f) return 19;
    log<<"PASS actual recorder +18 dB saved wet gain, unchanged original, no added samples\n";
    return 0;
}

class PitchLabApplication : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "PitchLab"; }
    const juce::String getApplicationVersion() override { return "0.4.0"; }
    void initialise(const juce::String& commandLine) override {
        auto args = juce::StringArray::fromTokens(commandLine, true); args.removeEmptyStrings();
        if(args.size()>=2 && args[0]=="--render-ui") {
            auto panel = std::make_unique<MainComponent>();
            auto rendered=panel->createComponentSnapshot(panel->getLocalBounds(),true,1.0f);
            juce::File target(args[1].unquoted()); target.getParentDirectory().createDirectory();
            auto stream=target.createOutputStream(); juce::PNGImageFormat png;
            if (stream) { stream->setPosition(0); stream->truncate(); }
            setApplicationReturnValue(stream && png.writeImageToStream(rendered,*stream)?0:12); quit(); return;
        }
        if(args.size()>=3 && (args[0]=="--render" || args[0]=="--render-live" || args[0]=="--boost-wav")) {
            OfflineRenderer job;
            const int selected=args.size()>3?std::clamp(args[3].getIntValue(),0,VocalSettings::presetCount-1):0;
            auto settings=VocalSettings::preset(selected); settings.lowLatency=args[0]=="--render-live";
            settings.levelOnly=args[0]=="--boost-wav";
            settings.recordDb=settings.levelOnly ? (args.size()>3?std::clamp(args[3].getFloatValue(),0.f,30.f):18.f)
                                                : (args.size()>4?std::clamp(args[4].getFloatValue(),0.f,30.f):0.f);
            if(args.size()>5) settings.bpm=std::clamp(args[5].getFloatValue(),60.f,200.f);
            const auto error=OfflineRenderer::render(juce::File(args[1].unquoted()),juce::File(args[2].unquoted()),settings,job);
            if(error.isNotEmpty()) juce::File(args[2].unquoted()).getChildFile("render-error.txt").replaceWithText(error);
            setApplicationReturnValue(error.isEmpty()?0:13); quit(); return;
        }
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
            setUsingNativeTitleBar(true);
            auto* content = new MainComponent();
            float factor = 1.0f;
            if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()) {
                factor = std::max(0.5f,std::min({1.0f, (display->userArea.getWidth()-60)/1100.0f, (display->userArea.getHeight()-80)/940.0f}));
            }
            auto* host = new juce::Component(); host->addAndMakeVisible(content);
            content->setTransform(juce::AffineTransform::scale(factor));
            host->setSize(int(std::ceil(1100*factor)),int(std::ceil(940*factor)));
            scaledContent.reset(content); setContentOwned(host, true);
            setResizable(false, false); centreWithSize(getWidth(), getHeight()); setVisible(true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
        std::unique_ptr<MainComponent> scaledContent;
    };
    std::unique_ptr<Window> window;
};
START_JUCE_APPLICATION(PitchLabApplication)
