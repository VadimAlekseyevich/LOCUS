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
    // Retained decoded PCM, not transient decoder memory or process RSS.
    std::size_t DecodedPcmBytes() const noexcept {
        return static_cast<std::size_t>(decoded_.frameCount) *
            decoded_.channels * sizeof(float);
    }
    double DecodeMilliseconds() const noexcept { return decodeMilliseconds_; }
private:
    static constexpr unsigned kBlockFrames = 2048;
    void FillBlock();
    void Prime();

    soundtouch::SoundTouch processor_;
    AudioStream stream_{};
    // Raylib's Wave already owns converted float PCM. Retain that buffer
    // directly: LoadWaveSamples() plus vector::assign() previously created
    // TWO additional full-length PCM copies at first slowdown.
    Wave decoded_{};
    std::vector<float> output_;
    std::size_t cursorFrames_ = 0;
    unsigned sampleRate_ = 0;
    unsigned channels_ = 0;
    double decodeMilliseconds_ = 0.0;
    bool flushed_ = false;
    bool ready_ = false;
    bool playing_ = false;
};
} // namespace locus::app
