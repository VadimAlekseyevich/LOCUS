#pragma once

#include "core/route.h"
#include "core/song_clock.h"

#include "raylib.h"

#include <cstdint>
#include <vector>

namespace locus::app {

// Owns the generated WAV bytes for as long as raylib's memory decoder uses
// them. One monotonic music-time anchor drives all future game consumers.
class DemoTransport {
public:
    DemoTransport() = default;
    DemoTransport(const DemoTransport&) = delete;
    DemoTransport& operator=(const DemoTransport&) = delete;
    ~DemoTransport();

    bool Initialize(const core::Chart& chart);
    void Start(double monotonicSeconds);
    void Pause(double monotonicSeconds);
    void Resume(double monotonicSeconds);
    void Stop();
    void Update(double monotonicSeconds);
    void SetVolume(float level);
    void Shutdown();

    bool Ready() const noexcept { return musicLoaded_; }
    bool Playing() const noexcept { return playing_; }
    bool Paused() const noexcept { return paused_; }
    bool Finished() const noexcept { return finished_; }
    double SongSeconds(double monotonicSeconds) const noexcept;
    double Beat(double monotonicSeconds) const noexcept;
    const char* Status() const noexcept;

private:
    core::SongClock clock_;
    std::vector<std::uint8_t> wavData_;
    Music music_{};
    double bpm_ = 120.0;
    float volume_ = 0.7f;
    double durationSeconds_ = 0.0;
    bool audioInitialized_ = false;
    bool musicLoaded_ = false;
    bool playing_ = false;
    bool paused_ = false;
    bool finished_ = false;
};

} // namespace locus::app
