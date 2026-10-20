# PitchLab — Real-Time Vocal Pitch Processing

[English](README.md) · [Korean](README.ko.md) · [Japanese](README.ja.md)

**C++ · JUCE · WASAPI · DSP** · Supporting engineering project

[Explore the case](https://ysl-ryudo.github.io/en/work/pitchlab/) · [Portfolio](https://ysl-ryudo.github.io/en/)


A Windows C++/JUCE application for microphone pitch correction, recording and offline WAV processing.

![PitchLab — Real-Time Vocal Pitch Processing](PREVIEW.png)

## The engineering problem

Microphone processing combines real-time device input with heavier pitch analysis, correction and recording. The callback and processing worker need a bounded handoff.

## My implementation

I separated processing and recording from the callback with a DSP worker and bounded FIFOs, investigated WASAPI input formats, and added calibration, diagnostics and WAV regression checks.

### Implementation overview · conceptual flow

```mermaid
flowchart LR
    N0["WASAPI input"] --> N1
    N1["Bounded FIFO"] --> N2
    N2["DSP worker"] --> N3
    N3["Output / WAV recording"]
```

## My contribution

- Separated pitch analysis/processing and recording from the device callback through a dedicated DSP worker and bounded FIFOs.
- Investigated WASAPI input-format handling and implemented input-level calibration, output protection and diagnostics.
- Added regression checks for DSP, streaming, input calibration, noise gate and WAV output behavior.

## Contribution evidence

- [Audio callback / 오디오 콜백 / 音声コールバック](https://github.com/YSL-RyuDo/pitchlab/blob/5bcbac396627611e5ec12c8384fdc0733d636f8c/Source/AudioEngine.h)
- [Development history / 개발 이력 / 開発履歴](https://github.com/YSL-RyuDo/pitchlab/commits/main/?author=YSL-RyuDo)

Commits are evidence of the stated changes, not a percentage-of-authorship score. Third-party assets and teammates' work remain separately credited.

## Open and inspect

Use [CMakeLists.txt](CMakeLists.txt) for the C++/JUCE build and the existing development reference for device and recording setup.

## Development and verification scope

A supporting C++ engineering project, not a game-server benchmark. Recorded synthetic/file-based checks do not prove quality on every physical microphone or headphone. Artist-named presets are approximations, not official artist settings. User voice recordings are not included.

## Further documentation

- [Existing detailed development reference (Korean)](docs/DEVELOPMENT_REFERENCE.ko.md)
- [TEST-RESULTS.txt](TEST-RESULTS.txt)
- [LIVE-VOICE-TEST-RESULTS.txt](LIVE-VOICE-TEST-RESULTS.txt)
- [ThirdParty/JUCE-LICENSE.md](ThirdParty/JUCE-LICENSE.md)

## How to evaluate the work

Inspect AudioEngine.h and the recorded synthetic/file checks. They demonstrate a validation approach, not uniform quality across every physical microphone.
