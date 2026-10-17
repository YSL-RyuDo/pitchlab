#pragma once
inline void testMonitorGain(std::ostream& log) {
    for (double rate : {44100., 48000., 96000.}) for (bool fast : {false, true}) {
        double rms[3]{};
        int level = 0;
        for (float db : {0.f, 12.f, 24.f}) {
            pitchlab::HeadphoneGuard guard; guard.prepare(rate, fast);
            double sum = 0; int count = 0;
            for (int i = 0; i < int(rate); ++i) {
                float x = float(.001 * std::sin(2 * pitchlab::pi * 220 * i / rate));
                float y = guard.process(x, true, db);
                require(std::isfinite(y) && std::abs(y) <= pitchlab::HeadphoneGuard::peakCeiling+0.000001f, "boost keeps headphone ceiling");
                if (i > rate * .5) { sum += y*y; ++count; }
            }
            rms[level++] = std::sqrt(sum/count);
            require(guard.process(.5f, false, db) == 0, "boosted monitor mutes immediately");
            for (int i = 0; i < int(rate); ++i)
                require(std::abs(guard.process(.5f, true, db)) <= pitchlab::HeadphoneGuard::peakCeiling+0.000001f, "overload at each gain stays bounded");
        }
        require(rms[0] > .00070 && rms[0] < .00072, "0 dB passes quiet waveform at unity");
        require(std::abs(rms[1]/rms[0] - std::pow(10.,12./20)) < .01, "+12 dB works for quiet voices");
        require(std::abs(rms[2]/rms[0] - std::pow(10.,24./20)) < .02, "+24 dB works below limiter ceiling");
        pitchlab::HeadphoneGuard guard; guard.prepare(rate, fast);
        float previous = 0, maxStep = 0;
        for (int i = 0; i < int(rate); ++i) {
            const float y = guard.process(.0005f, true, i < rate*.5 ? -60.f : 24.f);
            maxStep = std::max(maxStep, std::abs(y-previous)); previous = y;
        }
        require(maxStep < .00001f, "large monitor knob change is smoothed");
        pitchlab::HeadphoneGuard audible; audible.prepare(rate,fast);
        double normalEnergy=0; int normalCount=0;
        for(int i=0;i<int(rate);++i) {
            float x=float(.02*std::sin(2*pitchlab::pi*220*i/rate));
            float y=audible.process(x,true,pitchlab::HeadphoneGuard::defaultDb);
            if(i>rate*.5) {normalEnergy+=y*y; ++normalCount;}
        }
        const double normalRms=std::sqrt(normalEnergy/normalCount);
        require(normalRms>.055 && normalRms<.057,"normal monitor waveform no longer crushed by old -30 dBFS ceiling");
        log << "PASS monitor gain rate=" << rate << " fast=" << fast << " quiet RMS=" << rms[0]
            << ", +12=" << rms[1] << ", +24=" << rms[2] << ", normal_default_RMS="<<normalRms<<"; ceiling/mute/gain-ramp preserved\n";
    }
}
