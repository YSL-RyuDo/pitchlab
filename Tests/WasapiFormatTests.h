#pragma once
#include "../ThirdParty/JUCE/pitchlab_WasapiFormat.h"
#include <cstring>

// Exercises the same format selector used by the patched Windows device code
// and JUCE's actual capture/playback byte converters. No audio device is opened.
inline void testWasapiFormats(std::ostream& log) {
    require(pitchlabWasapiIsFloatFormat(3, false), "plain IEEE_FLOAT selects float converter");
    require(pitchlabWasapiIsFloatFormat(0xfffe, true), "extensible IEEE_FLOAT selects float converter");
    require(!pitchlabWasapiIsFloatFormat(1, true), "plain PCM ignores stale float subtype");
    require(!pitchlabWasapiIsFloatFormat(0xfffe, false), "extensible PCM remains integer");
    using Float = juce::AudioData::Pointer<juce::AudioData::Float32, juce::AudioData::LittleEndian, juce::AudioData::NonInterleaved, juce::AudioData::NonConst>;
    using Int = juce::AudioData::Pointer<juce::AudioData::Int32, juce::AudioData::LittleEndian, juce::AudioData::NonInterleaved, juce::AudioData::NonConst>;
    double oldMaxStep = 0, fixedMaxError = 0;
    float previousWrong = 0;
    for (int i = 0; i < 48000; ++i) {
        float input = float(0.02 * std::sin(2 * pitchlab::pi * 213 * i / 48000));
        alignas(float) unsigned char bytes[4]; std::memcpy(bytes, &input, 4);
        const float wrong = Int(bytes).getAsFloat();
        const float captured = pitchlabWasapiIsFloatFormat(3, false) ? Float(bytes).getAsFloat() : Int(bytes).getAsFloat();
        oldMaxStep = std::max(oldMaxStep, double(std::abs(wrong - previousWrong))); previousWrong = wrong;
        fixedMaxError = std::max(fixedMaxError, double(std::abs(captured - input)));
        if (pitchlabWasapiIsFloatFormat(3, false)) Float(bytes).setAsFloat(captured); else Int(bytes).setAsFloat(captured);
        float played = 0; std::memcpy(&played, bytes, 4);
        require(std::isfinite(played) && played == input, "plain float playback retains IEEE float bytes");
    }
    require(oldMaxStep > 0.85, "old plain-float interpretation reproduces large near-zero jumps");
    require(fixedMaxError == 0, "fixed float capture preserves samples exactly");
    log << "PASS WASAPI plain/extensible float and PCM selection; actual JUCE capture/playback converters; old max step="
        << oldMaxStep << ", fixed capture error=" << fixedMaxError << " (no device opened)\n";
}
