#pragma once
#include "core/audio_waveform.h"
#include "core/audio_tempo_analysis.h"

namespace locus::core {
struct EditorAudioAnalysis {
    AudioWaveform waveform;
    std::vector<TempoProposal> tempo;
};
} // namespace locus::core
