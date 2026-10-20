# PitchLab

[English](README.md), [Korean](README.ko.md), [Japanese](README.ja.md)

오디오 콜백과 DSP 워커를 중심으로 만든 실시간 마이크, 피치 처리 도구입니다.

| 프로젝트 정보 | 내용 |
|---|---|
| 개발 상태 | C++ 시스템 개발 사례 |
| 사용 기술 | C++, JUCE, WASAPI, DSP |

[개발 사례 보기](https://ysl-ryudo.github.io/ko/work/pitchlab/), [Portfolio](https://ysl-ryudo.github.io/ko/)

## 내 기여

전용 DSP 워커와 크기가 제한된 FIFO를 사용해 음정 분석, 처리, 녹음을 장치 콜백에서 분리했습니다.

WASAPI 입력 형식 처리를 분석하고 입력 음량 보정, 출력 보호와 진단 기능을 구현했습니다.

DSP, 스트리밍, 입력 보정, 잡음 게이트, WAV 출력 동작의 회귀 검사를 추가했습니다.

## 기여 근거

[Audio callback / 오디오 콜백 / 音声コールバック](https://github.com/YSL-RyuDo/pitchlab/blob/5bcbac396627611e5ec12c8384fdc0733d636f8c/Source/AudioEngine.h)

[Development history / 개발 이력 / 開発履歴](https://github.com/YSL-RyuDo/pitchlab/commits/main/?author=YSL-RyuDo)

커밋은 설명한 변경의 근거이며 기여율 점수가 아닙니다. 외부 에셋과 팀원 작업의 크레딧은 구분합니다.

## 구현과 설계

마이크 처리는 실시간 장치 입력과 피치 분석, 보정, 녹음을 함께 다룹니다. 콜백과 처리 워커 사이에 크기가 제한된 전달 구조를 두었습니다.

DSP 워커와 bounded FIFO로 콜백에서 처리, 녹음을 분리하고 WASAPI 입력 형식을 조사했습니다. 입력 보정, 진단과 WAV 회귀 검증도 추가했습니다.

| 영역 | 구현 |
|---|---|
| 장치 콜백 | 용량 제한 FIFO를 통한 입출력 전달 |
| DSP 워커 | 피치 분석, 처리, 녹음 |
| 검증 | 입력 보정, WASAPI 형식, 진단, WAV 회귀 확인 |

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

[pitchlab](https://github.com/YSL-RyuDo/pitchlab), 비공개, 접근 권한 필요
