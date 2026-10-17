#pragma once
inline void testLiveStreamLatency(std::ostream& log) {
    for (int block : {64,128,256,512}) {
        auto owned = std::make_unique<AudioEngine>(); auto& engine = *owned;
        engine.prepareStream(48000,block);
        engine.monitoring=true; engine.monitorDb=-18;
        engine.pitch.parameters.amount=0;
        std::array<float,512> input{}, output{};
        const float* ins[]={input.data()}; float* outs[]={output.data()};
        juce::AudioIODeviceCallbackContext context;
        std::vector<float> rendered;
        bool primed=false; int supplied=0, consumed=0;
        double maxCallbackMs=0;
        constexpr int impulseAt=16384;
        for(int offset=0;offset<24576;offset+=block) {
            input.fill(0);
            if(offset<=impulseAt && impulseAt<offset+block) input[size_t(impulseAt-offset)]=.2f;
            if(engine.bufferedOutputSamples()>=engine.startupBufferSamples()) primed=true;
            const auto started=std::chrono::steady_clock::now();
            engine.audioDeviceIOCallbackWithContext(ins,1,outs,1,block,context);
            maxCallbackMs=std::max(maxCallbackMs,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count());
            supplied+=block; if(primed) consumed+=block;
            const int expected=supplied/128*128-consumed;
            const auto deadline=juce::Time::getMillisecondCounter()+2000;
            while(engine.bufferedOutputSamples()<expected && engine.streamFault.load()==0
                && juce::Time::getMillisecondCounter()<deadline) juce::Thread::sleep(1);
            require(engine.streamFault.load()==0 && engine.bufferedOutputSamples()>=expected,"low-buffer callback/worker stable in memory simulation");
            rendered.insert(rendered.end(),output.begin(),output.begin()+block);
        }
        const int peak=int(std::max_element(rendered.begin(),rendered.end())-rendered.begin());
        const int measured=peak-impulseAt;
        require(rendered[size_t(peak)]>.001f,"live impulse signal exists in memory buffer");
        require(std::abs(measured-engine.totalLatencySamples.load())<=1,"whole app measured impulse latency matches UI");
        if(block<=128) require(measured/48.0<30,"whole live path below 30 ms at 48 kHz / <=128 frames");
        log<<"PASS LIVE STREAM block="<<block<<" measured_app_ms="<<measured/48.0<<" max_callback_ms="<<maxCallbackMs
           <<" max_worker_ms="<<engine.maxDspMs.load()<<" (memory simulation, no device/driver latency)\n";
        engine.stop();
    }
}

inline void testQuietMonitorStream(std::ostream& log) {
    auto owned = std::make_unique<AudioEngine>(); auto& engine = *owned;
    engine.prepareStream(48000,128); engine.pitch.parameters.amount=0;
    require(engine.monitorDb.load()==0 && !engine.monitoring.load(), "default monitor is off at unity gain");
    engine.monitoring=true;
    std::array<float,128> input{}, output{};
    const float* ins[]={input.data()}; float* outs[]={output.data()};
    juce::AudioIODeviceCallbackContext context;
    int supplied=0, consumed=0; bool primed=false;
    double sum[2]{}; int counts[2]{};
    for(int offset=0;offset<96000;offset+=128) {
        for(int i=0;i<128;++i) input[i]=float(.0054*std::sin(2*pitchlab::pi*220*(offset+i)/48000));
        engine.monitorDb=offset<48000 ? 0.f : 12.f;
        if(engine.bufferedOutputSamples()>=engine.startupBufferSamples()) primed=true;
        engine.audioDeviceIOCallbackWithContext(ins,1,outs,1,128,context);
        supplied+=128; if(primed) consumed+=128;
        const auto deadline=juce::Time::getMillisecondCounter()+2000;
        while(engine.bufferedOutputSamples()<supplied-consumed && engine.streamFault.load()==0
            && juce::Time::getMillisecondCounter()<deadline) juce::Thread::sleep(1);
        require(engine.streamFault.load()==0 && engine.bufferedOutputSamples()>=supplied-consumed, "quiet monitor worker drains");
        const int segment=offset<48000?0:1;
        if(offset%48000>24000) {
            float peak=0;
            for(float x:output) { sum[segment]+=x*x; ++counts[segment]; peak=std::max(peak,std::abs(x)); }
            require(std::abs(peak-engine.monitorPeak.load())<1e-7f, "PHONES meter measures actual callback output");
        }
    }
    const double unity=std::sqrt(sum[0]/counts[0]), boosted=std::sqrt(sum[1]/counts[1]);
    require(unity>.0018 && unity<.0021, "quiet microphone is not attenuated by another 36 dB");
    require(boosted/unity>3.9 && boosted/unity<4.1, "quiet callback output follows monitor boost");
    engine.monitoring=false;
    engine.audioDeviceIOCallbackWithContext(ins,1,outs,1,128,context);
    for(float x:output) require(x==0,"disabled callback monitor is silent");
    require(engine.monitorPeak.load()==0, "disabled PHONES meter is zero");
    engine.stop();
    log << "PASS quiet callback monitor unity RMS=" << unity << ", +12 dB RMS=" << boosted
        << "; output meter and immediate mute (no device opened)\n";
}
