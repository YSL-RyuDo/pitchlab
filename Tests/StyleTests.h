#pragma once
#include "../Source/RecordLevel.h"
inline void testStyleProcessing(std::ostream& log) {
    pitchlab::RecordLevel level; level.setGain(18);
    require(std::abs(level.process(.01f)-.01f*std::pow(10.f,.9f))<1e-6f,"file gain increases quiet audio by 18 dB");
    for(float db : {0.f,18.f,30.f}) {
        level.setGain(db);
        require(level.process(0)==0,"file gain preserves silence");
        for(int i=-10000;i<=10000;++i) require(std::abs(level.process(i*.001f))<=pitchlab::RecordLevel::ceiling+.000001f,"file boost ceiling under overload");
        require(level.process(std::numeric_limits<float>::quiet_NaN())==0,"file boost rejects nonfinite samples");
    }
    for(double rate : {44100.,48000.,96000.}) {
        constexpr int styles=int(pitchlab::styleProfiles.size());
        std::array<double,styles> signatures{};
        for(int style=1;style<=styles;++style) {
            pitchlab::PitchEngine engine; engine.prepare(rate,true);
            engine.parameters.toneStyle=style; engine.parameters.fxMix=1; engine.parameters.bpm=140;
            engine.parameters.retuneMs=pitchlab::styleProfiles[size_t(style-1)].speed;
            std::array<float,128> in{},wet{},dry{};
            float peak=0; double tail=0;
            const auto started=std::chrono::steady_clock::now();
            for(int offset=0;offset<int(rate*2);offset+=128) {
                for(int i=0;i<128;++i) {
                    double t=(offset+i)/rate;
                    double envelope=t<1?std::min(1.,t/.02)*std::min(1.,(1-t)/.02):0;
                    in[i]=float(envelope*(.16*std::sin(2*pitchlab::pi*213*t)+.04*std::sin(2*pitchlab::pi*426*t)));
                }
                engine.process(in.data(),wet.data(),dry.data(),128);
                for(int i=0;i<128;++i) {
                    require(std::isfinite(wet[i])&&std::abs(wet[i])<=.500001f,"style output is finite and bounded");
                    peak=std::max(peak,std::abs(wet[i]));
                    if(offset>rate*1.15) tail+=wet[i]*wet[i];
                    signatures[size_t(style-1)]+=wet[i]*wet[i];
                }
            }
            require(peak>.01 && tail>1e-7,"style retains vocal and produces delay/room tail");
            const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
            require(seconds<2,"style DSP processes faster than realtime");
            log<<"PASS style="<<style<<" rate="<<rate<<" peak="<<peak<<" tail_energy="<<tail<<" render_seconds="<<seconds<<'\n';
        }
        for(int a=0;a<styles;++a)for(int b=a+1;b<styles;++b)
            require(std::abs(signatures[a]-signatures[b])>.001,"style chains produce distinct output");
        pitchlab::VocalTone tone; tone.prepare(rate); tone.select(0,0,140);
        for(int i=0;i<2000;++i) require(tone.process(.0123f)==.0123f,"legacy style bypass is exact");
        float previous=0,maxStep=0;
        for(int i=0;i<int(rate*2);++i) {
            if(i%int(rate*.2)==0) tone.select((i/int(rate*.2))%(styles+1),1,140);
            const float y=tone.process(.01f); maxStep=std::max(maxStep,std::abs(y-previous)); previous=y;
            require(std::isfinite(y)&&std::abs(y)<.1f,"switching styles remains bounded");
        }
        require(maxStep<.011f,"style switching crossfades rather than jumps");
    }
    log<<"PASS file gain/ceiling and eight distinct live style chains, bypass, tails and switching\n";
}
