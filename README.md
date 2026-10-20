# PitchLab

[English](README.md), [Korean](README.ko.md), [Japanese](README.ja.md)

A real-time microphone and pitch-processing tool, built around an audio callback and DSP worker.

| Project | Details |
|---|---|
| Status | Supporting engineering project |
| Stack | C++, JUCE, WASAPI, DSP |

[Explore the case](https://ysl-ryudo.github.io/en/work/pitchlab/), [Portfolio](https://ysl-ryudo.github.io/en/)

## My contribution

Separated pitch analysis/processing and recording from the device callback through a dedicated DSP worker and bounded FIFOs.

Investigated WASAPI input-format handling and implemented input-level calibration, output protection and diagnostics.

Added regression checks for DSP, streaming, input calibration, noise gate and WAV output behavior.

## Contribution evidence

[Audio callback / 오디오 콜백 / 音声コールバック](https://github.com/YSL-RyuDo/pitchlab/blob/5bcbac396627611e5ec12c8384fdc0733d636f8c/Source/AudioEngine.h)

[Development history / 개발 이력 / 開発履歴](https://github.com/YSL-RyuDo/pitchlab/commits/main/?author=YSL-RyuDo)

Commits are evidence of the stated changes, not a percentage-of-authorship score. Third-party assets and teammates' work remain separately credited.

## Implementation and design

Microphone processing combines real-time device input with heavier pitch analysis, correction and recording. The callback and processing worker need a bounded handoff.

I separated processing and recording from the callback with a DSP worker and bounded FIFOs, investigated WASAPI input formats, and added calibration, diagnostics and WAV regression checks.

| Area | Implementation |
|---|---|
| Device callback | Input/output handoff with bounded FIFOs |
| DSP worker | Pitch analysis, processing and recording |
| Validation | Input calibration, WASAPI formats, diagnostics and WAV regression checks |

## Open and inspect

Use [CMakeLists.txt](CMakeLists.txt) for the C++/JUCE build and the existing development reference for device and recording setup.

## Development and verification scope

A supporting C++ engineering project, not a game-server benchmark. Recorded synthetic/file-based checks do not prove quality on every physical microphone or headphone. Artist-named presets are approximations, not official artist settings. User voice recordings are not included.

## Further documentation

[Existing detailed development reference (Korean)](docs/DEVELOPMENT_REFERENCE.ko.md)

[TEST-RESULTS.txt](TEST-RESULTS.txt)

[LIVE-VOICE-TEST-RESULTS.txt](LIVE-VOICE-TEST-RESULTS.txt)

[ThirdParty/JUCE-LICENSE.md](ThirdParty/JUCE-LICENSE.md)

## Related repositories

[pitchlab](https://github.com/YSL-RyuDo/pitchlab), Private, access required
