#pragma once
#include <algorithm>
#include <cmath>

namespace pitchlab {
// Quiet-input gate before the pitch/effect chain. No look-ahead, allocation or
// device access. Raw recordings remain independent. Hysteresis and a hold keep
// consonants and the ends of words from being chopped at each zero crossing.
class NoiseGate {
public:
    void prepare(double rate, bool initiallyEnabled=true) {
        detectorStep=float(1-std::exp(-1/(rate*.003)));
        attack=float(1-std::exp(-1/(rate*.001)));
        release=float(1-std::exp(-1/(rate*.06)));
        holdLength=int(rate*.1); hold=0; energy=0; gain=initiallyEnabled?0.f:1.f; open=false;
    }
    void configure(bool enabled, float thresholdDb) {
        enabledGate=enabled;
        threshold=std::pow(10.f,std::clamp(std::isfinite(thresholdDb)?thresholdDb:-60.f,-78.f,-36.f)/20.f);
    }
    float process(float x) {
        if(!std::isfinite(x)) x=0;
        energy+=detectorStep*(x*x-energy);
        const float level=std::sqrt(std::max(0.f,energy));
        if(level>=threshold) { open=true; hold=holdLength; }
        else if(level>=threshold*.63f && open) hold=holdLength;
        else if(hold>0) --hold;
        else open=false;
        const float wanted=(!enabledGate || open)?1.f:0.f;
        gain+=(wanted>gain?attack:release)*(wanted-gain);
        if(gain<1e-7f) gain=0;
        if(gain>0.99999f) gain=1;
        return x*gain;
    }
    float currentGain() const { return gain; }
private:
    float detectorStep=0,attack=0,release=0,energy=0,gain=0,threshold=.001f;
    int holdLength=0,hold=0;
    bool open=false,enabledGate=true;
};
}
