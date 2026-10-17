#pragma once
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

namespace pitchlab {
// Original study presets, not artist session files. See PRESET-GUIDE.txt.
struct StyleProfile {
    const char* name; const char* description;
    float speed, amount, humanize;
    float lowpass, air, drive, echo, beats, room;
};
inline constexpr std::array<StyleProfile,6> styleProfiles{{
    {"T-Pain / Snap", "즉시 붙는 음정 · 짧은 메아리 / 스타일 참고",0,1,0,11000,.12f,1,.07f,.5f,.05f},
    {"Travis Scott / Space", "빠른 보정 · 어두운 톤 · 긴 메아리 / 스타일 참고",3,1,.08f,4800,-.12f,1.8f,.22f,1,.22f},
    {"Future / Room", "빠른 보정 · 4분음표 딜레이 · 룸 / 스타일 참고",7,1,.15f,8500,.08f,1,.15f,1,.13f},
    {"Sik-K / Air Hook", "밝은 훅 · 가벼운 메아리 / 공개 수치 없는 근사 프리셋",12,.96f,.25f,11000,.3f,1,.13f,.5f,.16f},
    {"CHANGMO / Upfront", "앞으로 나오는 싱잉랩 · 절제된 공간 / 근사 프리셋",8,1,.2f,9500,.18f,1.3f,.08f,.5f,.08f},
    {"ASH ISLAND / Emo", "긴 음을 남기는 보정 · 잔향 / 공개 수치 없는 근사 프리셋",20,.92f,.45f,7500,.08f,1.2f,.16f,1,.24f}
}};

class VoiceColour {
public:
    void prepare(double sampleRate, const StyleProfile& p) {
        rate=sampleRate; profile=p;
        delay.assign(size_t(std::ceil(rate*1.1))+1,0);
        for(size_t i=0;i<room.size();++i) room[i].assign(size_t(rate*(.0297+.0071*i))+1,0);
        hpA=float(1-std::exp(-6.28318530718*80/rate));
        lpA=float(1-std::exp(-6.28318530718*profile.lowpass/rate));
        airA=float(1-std::exp(-6.28318530718*1500/rate));
        attack=float(1-std::exp(-1/(rate*.005))); release=float(1-std::exp(-1/(rate*.12)));
        reset();
    }
    void reset() {
        std::fill(delay.begin(),delay.end(),0.f); for(auto& r:room)std::fill(r.begin(),r.end(),0.f);
        positions.fill(0); pos=0; hp=lp=low=envelope=echoLow=0;
    }
    float process(float x, float bpm) {
        hp+=hpA*(x-hp); x-=hp;
        lp+=lpA*(x-lp); low+=airA*(lp-low); x=lp+profile.air*(lp-low);
        const float a=std::abs(x); envelope+=(a>envelope?attack:release)*(a-envelope);
        if(envelope>.1f) x*=std::pow(envelope/.1f,-2.f/3.f); // 3:1, no automatic makeup
        const float drive=4*profile.drive;
        x=std::tanh(x*drive)/drive;
        const size_t lag=std::clamp(size_t(rate*60/std::clamp(bpm,60.f,200.f)*profile.beats),size_t(1),delay.size()-1);
        const float echo=delay[(pos+delay.size()-lag)%delay.size()];
        echoLow+=.2f*(echo-echoLow);
        delay[pos]=x+.24f*echoLow; pos=(pos+1)%delay.size();
        float ambience=0;
        for(size_t i=0;i<room.size();++i) {
            const float r=room[i][positions[i]];
            room[i][positions[i]]=x*.25f+r*(.58f+.025f*float(i));
            positions[i]=(positions[i]+1)%room[i].size(); ambience+=r;
        }
        return x+profile.echo*echo+profile.room*ambience;
    }
private:
    StyleProfile profile=styleProfiles[0]; double rate=48000;
    std::vector<float> delay; std::array<std::vector<float>,4> room;
    std::array<size_t,4> positions{}; size_t pos=0;
    float hpA=0,lpA=0,airA=0,attack=0,release=0,hp=0,lp=0,low=0,envelope=0,echoLow=0;
};

class VocalTone {
public:
    void prepare(double rate) {
        for(size_t i=0;i<voices.size();++i) voices[i].prepare(rate,styleProfiles[i]);
        weights.fill(0); active=0; smooth=float(1-std::exp(-1/(rate*.02)));
    }
    void select(int style, float mix, float tempo) {
        style=std::clamp(style,0,6);
        if(style!=active && style>0 && weights[size_t(style-1)]<.000001f) voices[size_t(style-1)].reset();
        active=style; desiredMix=std::clamp(std::isfinite(mix)?mix:0.f,0.f,1.f);
        bpm=std::clamp(std::isfinite(tempo)?tempo:140.f,60.f,200.f);
    }
    float process(float x) {
        float y=x;
        for(size_t i=0;i<voices.size();++i) {
            const float wanted=active==int(i+1)?desiredMix:0;
            weights[i]+=smooth*(wanted-weights[i]);
            if(weights[i]>.000001f) y+=weights[i]*(voices[i].process(x,bpm)-x);
            else weights[i]=0;
        }
        return y;
    }
private:
    std::array<VoiceColour,6> voices; std::array<float,6> weights{};
    float smooth=0,desiredMix=0,bpm=140; int active=0;
};
}
