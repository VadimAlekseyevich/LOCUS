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
    // Both sections hit the same full-scale peak. Their energy is very
    // different; the cache must retain that distinction at every zoom.
    const auto loud=AudioWaveform::Build(1000,1000,2,
        [](std::uint32_t i,unsigned c) {
            const float amplitude=i<500 || i%5==0 ? 1.0f : 0.2f;
            return c ? -amplitude : amplitude;
        });
    const auto dense=loud.Range(0,0.5);
    const auto sparse=loud.Range(0.5,1);
    const auto merged=loud.Range(0,1);
    if (dense.maximum!=sparse.maximum || dense.minimum!=sparse.minimum ||
        dense.Rms()!=1.0f || std::abs(sparse.Rms()-std::sqrt(0.232))>1e-6 ||
        merged.sampleCount!=2000 ||
        std::abs(merged.Rms()-std::sqrt(0.616))>1e-6 ||
        quiet.Rms()!=0.0f) {
        std::cerr<<"WAVEFORM_FAIL RMS aggregation\n"; return 1;
    }
    return 0;
}
