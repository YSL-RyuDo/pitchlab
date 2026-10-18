#pragma once
#include <array>
#include <algorithm>
#include <cmath>

namespace pitchlab {
// User-triggered three-second level estimate; no automatic listening or I/O.
class InputCalibration {
public:
    void start(double rate) { frameLength=std::max(1,int(rate*.02)); frames=count=0; energy=0; peak=0; active=true; }
    void push(float x) {
        if(!active) return;
        if(!std::isfinite(x)) x=0;
        peak=std::max(peak,std::abs(x)); energy+=double(x)*x;
        if(++count<frameLength) return;
        levels[size_t(frames++)]=float(std::sqrt(energy/count)); count=0; energy=0;
        if(frames==int(levels.size())) active=false;
    }
    bool done() const { return !active && frames==int(levels.size()); }
    float progress() const { return frames/float(levels.size()); }
    bool result(float& db) const {
        if(!done()) return false;
        auto sorted=levels; std::sort(sorted.begin(),sorted.end());
        const float speech=sorted[105]; // 70th percentile of 20 ms RMS windows
        if(speech<.00031623f || peak<=0) return false;
        const float rmsGain=20*std::log10(.0794328f/speech); // -22 dBFS active-window RMS before FX
        const float peakGain=20*std::log10(.5f/peak); // retain measured peak headroom
        db=std::clamp(std::min(rmsGain,peakGain),0.f,36.f); return true;
    }
private:
    std::array<float,150> levels{};
    int frameLength=960,frames=0,count=0; double energy=0; float peak=0; bool active=false;
};
}
