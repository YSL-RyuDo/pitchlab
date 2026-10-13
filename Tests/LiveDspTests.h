#pragma once
inline void testLiveDsp(std::ostream& log) {
    for (double rate : {44100.,48000.,96000.}) {
        for (bool formants : {false,true}) for(double frequency : {67.,75.,90.,140.,213.,432.,452.,865.,987.}) {
            pitchlab::PitchEngine engine; engine.prepare(rate,true);
            engine.parameters.retuneMs=0; engine.parameters.preserveFormants=formants;
            std::array<float,128> input{}, wet{}, dry{};
            std::vector<float> signal;
            double maximumMs=0;
            for(int offset=0;offset<int(rate);offset+=128) {
                for(int i=0;i<128;++i) {
                    const auto phase=2*pitchlab::pi*frequency*(offset+i)/rate;
                    input[i]=float(.2*std::sin(phase)+.07*std::sin(2*phase)+.035*std::sin(3*phase));
                }
                const auto start=std::chrono::steady_clock::now();
                engine.process(input.data(),wet.data(),dry.data(),128);
                maximumMs=std::max(maximumMs,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
                signal.insert(signal.end(),wet.begin(),wet.end());
                for(float x:wet) require(std::isfinite(x)&&std::abs(x)<=.500001f,"live finite bounded audio");
            }
            const double target=pitchlab::hzFromMidi(pitchlab::nearestNote(pitchlab::midiFromHz(frequency),0,0));
            const double measured=estimate(signal,rate), cents=1200*std::log2(measured/target);
            log<<"LIVE rate="<<rate<<" formants="<<formants<<" input="<<frequency<<" output="<<measured<<" cents="<<cents<<" engine_ms="<<engine.latencySamples()*1000.0/rate<<" max_block_ms="<<maximumMs<<'\n';
            // The shorter spectral window trades some pitch precision for
            // latency. Keep a separate, explicit live tolerance of 25 cents;
            // the existing file-engine accuracy requirement remains unchanged.
            require(std::isfinite(cents)&&std::abs(cents)<25,"live harmonic pitch accuracy < 25 cents");
            require(engine.latencySamples()*1000.0/rate<24,"live engine latency under 24 ms");
        }
        // Measure the waveform delay, not just the latency getter. Unity pitch
        // must place an isolated impulse at the reported sample offset.
        pitchlab::PitchEngine engine; engine.prepare(rate,true); engine.parameters.amount=0;
        std::vector<float> wet(8192), dry(8192), input(8192);
        input[512]=0.2f;
        for(int offset=0;offset<8192;offset+=128) engine.process(input.data()+offset,wet.data()+offset,dry.data()+offset,128);
        const int peak=int(std::max_element(wet.begin(),wet.end())-wet.begin());
        require(std::abs(peak-512-engine.latencySamples())<=1,"measured live impulse delay agrees with reported latency");
        log<<"PASS live impulse latency measured="<<(peak-512)*1000.0/rate<<" ms at "<<rate<<" Hz\n";
    }
}
