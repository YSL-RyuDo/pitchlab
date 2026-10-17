#pragma once

// WASAPI may return either a plain WAVEFORMATEX (tag 3, IEEE float) or
// WAVEFORMATEXTENSIBLE (tag 0xfffe with the IEEE float subtype). Both must
// select the float converter, on capture AND playback. PCM remains integer.
// No includes: JUCE's native implementation includes this within namespace juce.
static constexpr bool pitchlabWasapiIsFloatFormat(unsigned int tag, bool ieeeFloatSubtype) noexcept {
    return tag == 0x0003 || (tag == 0xfffe && ieeeFloatSubtype);
}
