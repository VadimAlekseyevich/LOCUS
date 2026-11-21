#pragma once

#include "raylib.h"
#include <soundtouch/SoundTouch.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace locus::app {

// Editor/listener transport at practice rates. Raylib's SetMusicPitch changes
// both tempo and pitch; SoundTouch changes tempo only. PCM is decoded lazily
// (on the first sub-100% playback) and reused for seeking and rate changes.
// The source audio clock remains the single authority in chart seconds.
class PitchPreservingStream {
public:
    PitchPreservingStream() = default;
    PitchPreservingStream(const PitchPreservingStream&) = delete;
    PitchPreservingStream& operator=(const PitchPreservingStream&) = delete;
    ~PitchPreservingStream();

    bool Load(const std::vector<std::uint8_t>& encoded,
              const char* extension, std::string& error);
    void StartAt(double songSeconds, double tempo, float volume);
    void Pause();
    void Resume();
    void Stop();
    void Update();
    void SetVolume(float volume);
    void Close() noexcept;
    bool Ready() const noexcept { return ready_; }
    bool Playing() const noexcept { return playing_; }
private:
    static constexpr unsigned kBlockFrames = 2048;
    void FillBlock();
    void Prime();

    soundtouch::SoundTouch processor_;
    AudioStream stream_{};
    std::vector<float> samples_; // Interleaved, 32-bit float, mono/stereo.
    std::vector<float> output_;
    std::size_t cursorFrames_ = 0;
    unsigned sampleRate_ = 0;
    unsigned channels_ = 0;
    bool flushed_ = false;
    bool ready_ = false;
    bool playing_ = false;
};
} // namespace locus::app
