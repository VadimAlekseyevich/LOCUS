#pragma once
#include "core/editor_audio_analysis.h"
#include "raylib.h"
#include <memory>
#include <string>
#include <vector>

namespace locus::app {

// CPU-only decoder; no access to the output device, chart or UI.
// Called by the editor analysis worker, never in a render callback.
inline std::shared_ptr<const core::EditorAudioAnalysis> DecodeEditorAudio(
    const std::vector<std::uint8_t>& bytes,const std::string& format) {
    if (bytes.empty() || format.empty()) return {};
    struct OwnedWave {
        Wave value;
        ~OwnedWave() { if (value.data) UnloadWave(value); }
    } decoded{LoadWaveFromMemory(format.c_str(),bytes.data(),
                                  static_cast<int>(bytes.size()))};
    const auto& w=decoded.value;
    if (!IsWaveValid(w) || !w.frameCount || !w.sampleRate || !w.channels ||
        w.channels>8 || (w.sampleSize!=8 && w.sampleSize!=16 && w.sampleSize!=32))
        return {};
    const auto sample=[&](std::uint32_t frame,unsigned channel) -> float {
        const auto i=std::size_t(frame)*w.channels+channel;
        if (w.sampleSize==8)
            return (static_cast<const unsigned char*>(w.data)[i]-128)/128.0f;
        if (w.sampleSize==16)
            return static_cast<const short*>(w.data)[i]/32768.0f;
        return static_cast<const float*>(w.data)[i];
    };
    auto waveform=core::AudioWaveform::Build(w.frameCount,w.sampleRate,w.channels,sample);
    if (waveform.Empty()) return {};
    auto analysis=std::make_shared<core::EditorAudioAnalysis>();
    analysis->waveform=std::move(waveform);
    analysis->tempo=core::SuggestTempoMap(
        core::BuildOnsetEnvelope(w.frameCount,w.sampleRate,w.channels,sample));
    return analysis;
}
} // namespace locus::app
