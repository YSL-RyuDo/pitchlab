#pragma once
inline void testInputCalibration(std::ostream& log) {
    for(double rate:{44100.,48000.,96000.}) {
        pitchlab::InputCalibration c; c.start(rate); float db=0;
        for(int i=0;i<int(rate*3);++i) c.push(0);
        require(c.done() && !c.result(db),"silent calibration does not apply maximum gain");
        c.start(rate);
        for(int i=0;i<int(rate*3);++i) c.push(float(.0035*std::sin(2*pitchlab::pi*213*i/rate)));
        require(c.result(db) && db>29 && db<31,"quiet mic receives measured gain rather than a fixed default");
        c.start(rate);
        for(int i=0;i<int(rate*3);++i) c.push(float(.7*std::sin(2*pitchlab::pi*213*i/rate)));
        require(c.result(db) && db==0,"loud mic is not boosted");
    }
    pitchlab::PitchEngine engine; engine.prepare(48000,true); engine.parameters.amount=0;
    engine.calibrationRequest=true;
    std::array<float,128> in{},wet{},dry{};
    for(int offset=0;offset<144128;offset+=128) {
        for(int i=0;i<128;++i) in[i]=float(.0035*std::sin(2*pitchlab::pi*213*(offset+i)/48000));
        engine.process(in.data(),wet.data(),dry.data(),128);
    }
    require(engine.calibrationState.load()==2 && engine.calibratedDb.load()>29,"live worker publishes completed calibration");
    engine.parameters.inputDb=engine.calibratedDb.load();
    double rawEnergy=0,wetEnergy=0;
    for(int offset=0;offset<48000;offset+=128) {
        for(int i=0;i<128;++i) in[i]=float(.0035*std::sin(2*pitchlab::pi*213*(offset+i)/48000));
        engine.process(in.data(),wet.data(),dry.data(),128);
        if(offset>24000) for(int i=0;i<128;++i) {rawEnergy+=dry[i]*dry[i];wetEnergy+=wet[i]*wet[i];}
    }
    const double ratioDb=10*std::log10(wetEnergy/rawEnergy);
    require(std::abs(ratioDb-(engine.calibratedDb.load()-6))<.2,"preamp reaches wet path while raw recording remains unchanged");
    require(engine.detectedHz.load()>200 && engine.detectedHz.load()<230,"boosted quiet mic reaches pitch detector");
    engine.parameters.inputDb=36;
    in.fill(.9f); engine.process(in.data(),wet.data(),dry.data(),128);
    require(engine.clipped.load(),"excessive preamp lights clip warning");
    for(float x:wet) require(std::isfinite(x)&&std::abs(x)<=.500001f,"overboost output remains bounded");
    engine.calibrationRequest=true;
    in.fill(0);
    for(int offset=0;offset<144128;offset+=128) engine.process(in.data(),wet.data(),dry.data(),128);
    require(engine.calibrationState.load()==3,"repeat calibration on silence does not reuse old success");
    log<<"PASS 3-second input calibration, silence/loud/quiet levels, repeated request, pre-detector gain, raw preservation; wet boost dB="<<ratioDb<<'\n';
}
