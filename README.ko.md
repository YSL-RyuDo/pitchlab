# PitchLab

<!-- portfolio-languages -->
[![English](https://ysl-ryudo.github.io/assets/diagrams/language-en.svg)](README.md) [![한국어 (현재 언어)](https://ysl-ryudo.github.io/assets/diagrams/language-ko-current.svg)](README.ko.md) [![日本語](https://ysl-ryudo.github.io/assets/diagrams/language-ja.svg)](README.ja.md)
<!-- /portfolio-languages -->

오디오 콜백과 DSP 워커를 중심으로 만든 실시간 마이크, 피치 처리 도구입니다.

| 프로젝트 정보 | 내용 |
|---|---|
| 개발 상태 | C++ 시스템 개발 사례 |
| 사용 기술 | C++, JUCE, WASAPI, DSP |

[개발 사례 보기](https://ysl-ryudo.github.io/ko/work/pitchlab/), [Portfolio](https://ysl-ryudo.github.io/ko/)

## 게임에서 보는 구현

[![현재 소스로 빌드한 PitchLab 실행 화면. 키와 스케일, 보정 속도, 포먼트 유지, WAV 보정을 한 화면에서 조절합니다.](https://ysl-ryudo.github.io/assets/gameplay/pitchlab-controls.webp)](https://ysl-ryudo.github.io/assets/gameplay/pitchlab-controls.webp)

현재 소스로 빌드한 PitchLab 실행 화면. 키와 스케일, 보정 속도, 포먼트 유지, WAV 보정을 한 화면에서 조절합니다.

## 게임 전체에서 맡은 구현

JUCE와 Signalsmith DSP를 이용해 오디오 경로, 입력 보정과 게이트, 목표 음 선택, 작업자 버퍼링, 녹음과 오프라인 렌더를 구현했습니다.

[![담당 구현 흐름](https://ysl-ryudo.github.io/assets/diagrams/pitchlab-flow-ko.svg)](https://ysl-ryudo.github.io/assets/diagrams/pitchlab-flow-ko.svg)

### 입력 보정과 신호 제어

입력 보정과 노이즈 게이트, 녹음 레벨과 출력 보호 처리를 구현했습니다. 피치 변환 라이브러리 호출 외에도 처리 전후의 신호를 다루는 마이크 애플리케이션의 흐름을 만들었습니다.

[InputCalibration.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/InputCalibration.h), [내 변경 5bcbac3](https://github.com/YSL-RyuDo/pitchlab/commit/5bcbac396627611e5ec12c8384fdc0733d636f8c), [NoiseGate.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/NoiseGate.h), [내 변경 ecaf34c](https://github.com/YSL-RyuDo/pitchlab/commit/ecaf34c27b0b65439bd64ca7b47dad27d001ac0b)

### 목표 피치와 음 선택

검출 피치와 MIDI 음 변환, 허용 음 마스크와 가까운 음 선택을 이용한 목표 음 로직을 구현했습니다. 보컬 키보드로 사용자가 음을 선택하도록 연결했습니다. 실제 피치 변환은 연동한 DSP 의존성을 사용합니다.

[PitchEngine.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/PitchEngine.h), [내 변경 5bcbac3](https://github.com/YSL-RyuDo/pitchlab/commit/5bcbac396627611e5ec12c8384fdc0733d636f8c), [VocalKeyboard.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/VocalKeyboard.h), [내 변경 f37751d](https://github.com/YSL-RyuDo/pitchlab/commit/f37751d1d22e348f90ad6a1ef9437526ff157709)

### 스트리밍, 녹음과 오프라인 렌더

콜백과 작업자 사이의 버퍼 처리, 녹음과 파일 기반 오프라인 렌더를 구현했습니다. 실시간 모니터링은 오디오 마감을 지켜야 하고 오프라인 렌더는 그 콜백 밖에서 파일을 처리할 수 있습니다. 아래에서 버퍼에 따른 지연과 동기화 비용을 설명합니다.

[AudioEngine.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/AudioEngine.h), [내 변경 fe484c8](https://github.com/YSL-RyuDo/pitchlab/commit/fe484c8be966d0f91203373fd5f9eddd060ce5d0), [Recorder.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/Recorder.h), [내 변경 a8b3d93](https://github.com/YSL-RyuDo/pitchlab/commit/a8b3d93b8bba5cc75ee4124d7b143c226a4f4e50), [OfflineRenderer.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/OfflineRenderer.h), [내 변경 5bcbac3](https://github.com/YSL-RyuDo/pitchlab/commit/5bcbac396627611e5ec12c8384fdc0733d636f8c)

## 기여 근거

[Audio callback / 오디오 콜백 / 音声コールバック](https://github.com/YSL-RyuDo/pitchlab/blob/5bcbac396627611e5ec12c8384fdc0733d636f8c/Source/AudioEngine.h)

[Development history / 개발 이력 / 開発履歴](https://github.com/YSL-RyuDo/pitchlab/commits/main/?author=YSL-RyuDo)

## 구현과 설계

마이크 처리는 실시간 장치 입력과 피치 분석, 보정, 녹음을 함께 다룹니다. 콜백과 처리 워커 사이에 크기가 제한된 전달 구조를 두었습니다.

DSP 워커와 bounded FIFO로 콜백에서 처리, 녹음을 분리하고 WASAPI 입력 형식을 조사했습니다. 입력 보정, 진단과 WAV 회귀 검증도 추가했습니다.

## 코드의 동작과 구현 방식을 택한 이유

### 오디오 콜백에서 DSP 작업을 분리한 이유

**구현 흐름**

AudioEngine은 고정 용량의 입력 및 출력 FIFO로 오디오 콜백과 DSP 작업자 사이의 샘플을 전달합니다. 콜백은 입력값 검사, 샘플 전달, 준비된 출력 기록을 수행합니다. 넘침, 출력 부족, 유한하지 않은 값이 생기면 스트림 오류를 표시하고 모니터링 출력을 중지합니다.

**이 구조가 필요한 이유**

장치는 주기적인 마감 안에 출력을 받아야 하지만 피치 처리는 작업량이 달라질 수 있습니다. 분리하면 콜백이 해야 할 일을 좁히고, 버퍼 부족을 잘못된 소리를 계속 내보내는 상황 대신 명시적인 오류 상태로 다룰 수 있습니다. 고정 용량은 메모리 사용과 넘침 처리도 드러냅니다.

**대안과 현재 구현의 절충**

콜백 안에서 DSP를 처리하면 스레드 사이 버퍼와 그 지연이 없지만 가변적인 연산이 오디오 마감에 직접 걸립니다. 별도 작업자는 버퍼와 동기화가 추가되는 대신 콜백의 일을 좁힙니다. 적합한 선택은 스레드 수 자체보다 측정한 처리 시간과 출력 부족 발생에 따라 판단해야 합니다.

**장단점과 검증 범위**

버퍼는 실행 시점의 흔들림을 흡수하는 대신 지연을 더합니다. 오류 시 무음으로 전환하면 잘못된 데이터의 출력을 막지만 모니터링이 끊기므로 복구 경로가 필요합니다. 장치별 지연과 출력 부족은 측정해야 합니다.

[pitchlab/AudioEngine.h](https://github.com/YSL-RyuDo/pitchlab/blob/e191ed10d7d2cc8fc7dabd6192936b1ee3d4ea19/Source/AudioEngine.h)

### 피치 보정에서 목표 음을 정하는 방법

**구현 흐름**

PitchEngine은 주파수와 MIDI 피치를 변환하고 허용 음을 12비트 마스크로 표현한 뒤 인접한 음에서 가장 가까운 허용 목표를 찾습니다. 사용자는 음계를 고르거나 보컬 키보드에서 허용 음을 바꿀 수 있습니다. 변환 단계에는 연동한 DSP 라이브러리를 사용합니다.

**이 구조가 필요한 이유**

고정된 반음 수만큼 옮기는 작업과 음계에 맞는 음으로 보정하는 작업은 다릅니다. 목표 선택을 순수 함수로 두면 그 규칙을 마이크 입출력과 분리하고 오디오 장치 없이 경계 조건을 검사할 수 있습니다.

**대안과 현재 구현의 절충**

반음 단위 반올림은 단순하지만 모든 반음을 허용합니다. 음계로 제한하면 조성에 맞춰 제어할 수 있고 사용자 마스크는 특정 음을 제외할 수 있습니다. 가장 가까운 음 선택은 설명하기 쉬운 대신 음 경계에서 흔들리지 않도록 시간에 따른 처리도 필요합니다.

**장단점과 검증 범위**

사용자 마스크가 비어 있으면 반올림한 음으로 돌아갑니다. 목표 음 선택 규칙과 함께 검출 신뢰도, 무성음 입력, 음 전환과 청각적 왜곡을 다뤄야 합니다.

[pitchlab/PitchEngine.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/PitchEngine.h), [pitchlab/VocalKeyboard.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/VocalKeyboard.h)

## 실행, 코드 확인

[CMakeLists.txt](CMakeLists.txt)으로 C++/JUCE 빌드를 구성하고 장치, 녹음 설정은 기존 상세 안내를 확인하세요.

## 개발, 검증 범위

게임 서버 벤치마크가 아닌 보조 C++ 기술 프로젝트입니다. 합성 신호, 파일 기반 검사가 모든 실제 마이크, 헤드폰의 품질을 입증하지는 않습니다. 아티스트 이름 프리셋은 공식 설정이 아닌 근사치이며 개인 음성 녹음은 포함하지 않습니다.

## 상세 문서

[기존 개발, 사용, 검증 상세 안내](docs/DEVELOPMENT_REFERENCE.ko.md)

[TEST-RESULTS.txt](TEST-RESULTS.txt)

[LIVE-VOICE-TEST-RESULTS.txt](LIVE-VOICE-TEST-RESULTS.txt)

[ThirdParty/JUCE-LICENSE.md](ThirdParty/JUCE-LICENSE.md)

## 관련 저장소

[pitchlab](https://github.com/YSL-RyuDo/pitchlab), 공개 저장소
