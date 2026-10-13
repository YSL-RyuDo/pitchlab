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
