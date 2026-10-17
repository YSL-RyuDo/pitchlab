#pragma once
inline void testNoiseGate(std::ostream& log) {
    for(double rate : {44100.,48000.,96000.}) {
        pitchlab::NoiseGate gate; gate.prepare(rate); gate.configure(true,-60);
        float last=0,step=0; double quiet=0,voice=0,inputVoice=0,tail=0;
        for(int i=0;i<int(rate*3);++i) {
            const double t=i/rate;
            // Small electrical hum + high frequency hiss, then quiet speech-sized tone.
            float x=float(.0004*std::sin(2*pitchlab::pi*60*t)+.0001*std::sin(2*pitchlab::pi*7013*t));
            if(t>=1 && t<2) x+=float(.005*std::sin(2*pitchlab::pi*220*t));
            const float y=gate.process(x); step=std::max(step,std::abs(y-last)); last=y;
            if(t<.9) quiet+=y*y;
            if(t>1.2 && t<1.8) {voice+=y*y;inputVoice+=x*x;}
            if(t>2.9) tail+=y*y;
            require(std::isfinite(y),"gate remains finite");
        }
        require(quiet<1e-12 && tail<1e-12,"gate blocks idle hum/hiss and recloses after speech");
        require(voice/inputVoice>.99,"quiet voice passes after attack");
        require(step<.001,"gate opening/closing does not create large discontinuities");
        gate.configure(false,-60);
        for(int i=0;i<int(rate*.1);++i) gate.process(.0002f);
        require(gate.process(.0002f)==.0002f,"disabled gate passes quiet input exactly after transition");
        gate.configure(true,-78);
        for(int i=0;i<int(rate*.1);++i) gate.process(.0002f);
        require(gate.currentGain()>.99f,"lower threshold admits quiet voices");
        log<<"PASS noise gate rate="<<rate<<" quiet_energy="<<quiet<<" voice_power_ratio="<<voice/inputVoice<<" tail_energy="<<tail<<'\n';
    }
    // Exercise both pitch engines and every style, with cleanup off and on.
    // A gate must not disguise an engine that generates sound from digital zero.
    for(bool live : {false,true}) for(bool cleanup : {false,true})
        for(int style=0;style<=int(pitchlab::styleProfiles.size());++style) {
            pitchlab::PitchEngine engine;
            engine.parameters.noiseGate=cleanup; engine.parameters.toneStyle=style; engine.parameters.fxMix=1;
            engine.prepare(48000,live);
            std::array<float,128> in{},wet{},dry{};
            float peak=0;
            for(int offset=0;offset<96000;offset+=128) {
                engine.process(in.data(),wet.data(),dry.data(),128);
                for(float x:wet) { require(std::isfinite(x),"idle engine output finite"); peak=std::max(peak,std::abs(x)); }
                for(float x:dry) require(x==0,"idle original stays zero");
            }
            require(peak<1e-8f,"digital silence does not generate crackle in any style");
        }
    pitchlab::PitchEngine engine; engine.parameters.noiseGate=true; engine.prepare(48000,true);
    std::array<float,128> in{},wet{},dry{}; double outputEnergy=0,originalEnergy=0;
    for(int offset=0;offset<96000;offset+=128) {
        for(int i=0;i<128;++i) in[i]=float(.0004*std::sin(2*pitchlab::pi*60*(offset+i)/48000));
        engine.process(in.data(),wet.data(),dry.data(),128);
        if(offset>48000) for(int i=0;i<128;++i) {outputEnergy+=wet[i]*wet[i];originalEnergy+=dry[i]*dry[i];}
    }
    require(outputEnergy<1e-12 && originalEnergy>.001,"live gate quiets wet signal without erasing raw recording");
    log<<"PASS 36 idle engine/style combinations, cleanup off/on; raw preservation and gated live hum\n";
}
