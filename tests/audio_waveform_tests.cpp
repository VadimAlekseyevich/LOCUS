#include "core/audio_waveform.h"
#include <cmath>
#include <iostream>
int main() {
    using namespace locus::core;
    // Opposite stereo polarity must not disappear in the displayed envelope.
    const auto waveform=AudioWaveform::Build(1000,1000,2,
        [](std::uint32_t i,unsigned c) {
            return i==410 ? (c==0 ? 0.8f : -0.6f) : 0.0f;
        });
    const auto whole=waveform.Range(0,1);
    const auto detail=waveform.Range(0.410,0.415);
    const auto quiet=waveform.Range(0,0.2);
    if (waveform.Empty() || waveform.Duration()!=1 ||
        whole.maximum!=0.8f || whole.minimum!=-0.6f ||
        detail.maximum!=whole.maximum || detail.minimum!=whole.minimum ||
        quiet.maximum!=0 || quiet.minimum!=0 ||
        waveform.Range(1.01,2).maximum!=0) {
        std::cerr<<"WAVEFORM_FAIL\n"; return 1;
    }
    return 0;
}
