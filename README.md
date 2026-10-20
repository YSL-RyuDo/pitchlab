# PitchLab

[English](README.md), [Korean](README.ko.md), [Japanese](README.ja.md)

A real-time microphone and pitch-processing tool, built around an audio callback and DSP worker.

| Project | Details |
|---|---|
| Status | Supporting engineering project |
| Stack | C++, JUCE, WASAPI, DSP |

[Explore the case](https://ysl-ryudo.github.io/en/work/pitchlab/), [Portfolio](https://ysl-ryudo.github.io/en/)

## What I built across the game

I built the application's audio flow, control logic and UI around JUCE and third-party DSP components. My contribution includes target-note selection, calibration/gating, buffering, recording and offline processing. JUCE and Signalsmith algorithms remain credited dependencies, not claimed as algorithms I invented.

**Player flow**

Select audio devices → calibrate input → monitor and process pitch → choose notes/presets → record or render offline

Source links pin the inspected revision; contribution links identify my non-merge changes. Shared-file links do not imply sole authorship. Private source requires repository access.

### Input calibration and signal control

Implemented input calibration and noise-gate behavior alongside recording-level and output-guard handling. These components manage the signal entering and leaving pitch processing rather than treating a pitch-shift library as a complete microphone application.

[InputCalibration.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/InputCalibration.h), [My change 5bcbac3](https://github.com/YSL-RyuDo/pitchlab/commit/5bcbac396627611e5ec12c8384fdc0733d636f8c), [NoiseGate.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/NoiseGate.h), [My change ecaf34c](https://github.com/YSL-RyuDo/pitchlab/commit/ecaf34c27b0b65439bd64ca7b47dad27d001ac0b)

### Target pitch and note controls

Implemented the target-note logic around detected pitch, MIDI note conversion, allowed-note masks and nearest-note selection. The vocal keyboard exposes pitch-class choices to the user. Pitch shifting itself uses the integrated DSP dependency.

[PitchEngine.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/PitchEngine.h), [My change 5bcbac3](https://github.com/YSL-RyuDo/pitchlab/commit/5bcbac396627611e5ec12c8384fdc0733d636f8c), [VocalKeyboard.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/VocalKeyboard.h), [My change f37751d](https://github.com/YSL-RyuDo/pitchlab/commit/f37751d1d22e348f90ad6a1ef9437526ff157709)

### Streaming, recording and offline rendering

Implemented the callback/worker buffering path, recording and file-based offline rendering. Live monitoring has an audio deadline; offline rendering can process a captured file outside that callback. The detailed buffering note explains the latency and synchronization tradeoff.

[AudioEngine.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/AudioEngine.h), [My change fe484c8](https://github.com/YSL-RyuDo/pitchlab/commit/fe484c8be966d0f91203373fd5f9eddd060ce5d0), [Recorder.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/Recorder.h), [My change a8b3d93](https://github.com/YSL-RyuDo/pitchlab/commit/a8b3d93b8bba5cc75ee4124d7b143c226a4f4e50), [OfflineRenderer.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/OfflineRenderer.h), [My change 5bcbac3](https://github.com/YSL-RyuDo/pitchlab/commit/5bcbac396627611e5ec12c8384fdc0733d636f8c)

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

## How the code works and why it is structured this way

These notes connect the inspected implementation to the constraints it addresses and its tradeoffs. Explanations of design consequences are code analysis, not a claim that every alternative was prototyped. Contribution links identify my changes within team projects; source links retain their original access permissions.

### Audio callbacks and DSP have different deadlines

**Implementation**

AudioEngine moves samples through fixed-capacity input/output FIFOs between the audio callback and a DSP worker. The callback validates input, transfers samples and writes available output. Overflow, underrun or non-finite data marks a stream fault and disables monitoring output.

**Design rationale**

The device needs output on a recurring deadline; pitch processing can have variable work. Separating them limits what the callback must do and makes buffer exhaustion an explicit state rather than silently returning invalid audio. Fixed capacity also makes memory use and overflow behavior visible.

**Alternatives and the current tradeoff**

Doing DSP inside the callback avoids an inter-thread buffer and its delay, but puts variable processing work on the audio deadline. The separate worker introduces buffering and synchronization while narrowing callback work. The useful choice depends on measured processing time and underruns, not on thread count alone.

**Tradeoffs and verification**

Buffering absorbs scheduling variation at the cost of latency. Faulting to silence avoids continuing with invalid data but interrupts monitoring and needs a clear recovery path. This architecture is not a measured latency claim or a hard real-time guarantee on Windows.

[pitchlab/AudioEngine.h](https://github.com/YSL-RyuDo/pitchlab/blob/e191ed10d7d2cc8fc7dabd6192936b1ee3d4ea19/Source/AudioEngine.h)

### Pitch correction needs a target-selection policy

**Implementation**

PitchEngine converts frequency and MIDI pitch, represents allowed pitch classes with a 12-bit mask, and searches neighboring notes for the nearest allowed target. The user can choose a scale or edit allowed classes through the vocal keyboard. DSP libraries perform the integrated shifting stage.

**Design rationale**

Shifting by a fixed number of semitones and correcting toward a musical scale solve different problems. Making target selection a pure function separates that policy from microphone I/O and makes edge cases testable without an audio device.

**Alternatives and the current tradeoff**

Chromatic rounding is simple but permits every semitone. Restricting targets to a scale gives musical control, while a custom mask supports intentional exclusions. A nearest-note policy is easy to explain but needs temporal handling elsewhere to avoid unstable changes around note boundaries.

**Tradeoffs and verification**

An empty custom mask falls back to the rounded note in the inspected function. Detection confidence, unvoiced input, transitions and audible artifacts require separate tests. This policy does not establish original authorship of the underlying pitch-shifting algorithm.

[pitchlab/PitchEngine.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/PitchEngine.h), [pitchlab/VocalKeyboard.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/VocalKeyboard.h)

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
