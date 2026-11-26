#include "core/audio_tempo_analysis.h"
#include <iostream>
#include <cmath>
#include <vector>

int main() {
    using namespace locus::core;
    std::vector<float> flux(4000);
    const auto pulses=[&](double from,double until,double step) {
        for(double t=from;t<until;t+=step) {
            const auto i=static_cast<std::size_t>(std::lround(t));
            if (i+1<flux.size()) {flux[i]=1; flux[i+1]=0.35f;}
        }
    };
    pulses(25,1225,50);        // 120 BPM
    pulses(1225,2425,40);      // 150 BPM
    pulses(2425,3825,66.6667); // 90 BPM
    const auto result=SuggestTempoMap(flux);
    bool a=false,b=false,c=false;
    for (const auto& s:result) {
        std::cout<<"TEMPO "<<s.timeMs<<" "<<s.bpm<<" "<<s.confidence<<"\n";
        a|=std::abs(s.bpm-120)<2 && s.timeMs<3000;
        b|=std::abs(s.bpm-150)<2 && s.timeMs>8500 && s.timeMs<16000;
        c|=std::abs(s.bpm-90)<2 && s.timeMs>21000 && s.timeMs<28000;
    }
    if (!a || !b || !c || !SuggestTempoMap(std::vector<float>(1000)).empty()) {
        std::cerr<<"TEMPO_ANALYSIS_FAIL multi-tempo pulse fixture or silence\n";return 1;
    }
    return 0;
}
