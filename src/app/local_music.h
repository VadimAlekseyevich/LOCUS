#pragma once

#include "core/song_clock.h"

#include "raylib.h"

#include <cstdint>
#include <string>
#include <vector>

namespace locus::app {

// Listens to a user-selected local WAV/MP3 without inventing a chart.
// The existing DemoTransport owns the shared raylib audio device.
class LocalMusic {
public:
    LocalMusic() = default;
    LocalMusic(const LocalMusic&) = delete;
    LocalMusic& operator=(const LocalMusic&) = delete;
    ~LocalMusic();

    // On failure, the previous loaded song remains usable.
    bool LoadFromFile(const std::wstring& path);
    void Start(double monotonicSeconds);
    void Pause(double monotonicSeconds);
    void Resume(double monotonicSeconds);
    void Stop();
    void Update(double monotonicSeconds);
    void Shutdown();

    bool Ready() const noexcept { return ready_; }
    bool Playing() const noexcept { return playing_; }
    bool Paused() const noexcept { return paused_; }
    bool Finished() const noexcept { return finished_; }
    double DurationSeconds() const noexcept { return durationSeconds_; }
    double SongSeconds(double monotonicSeconds) const noexcept;
    const std::string& DisplayName() const noexcept { return displayName_; }
    const std::string& Error() const noexcept { return error_; }

private:
    core::SongClock clock_;
    // Raylib's memory stream borrows these bytes until UnloadMusicStream().
    std::vector<std::uint8_t> fileBytes_;
    Music music_{};
    std::string displayName_;
    std::string error_;
    double durationSeconds_ = 0.0;
    bool ready_ = false;
    bool playing_ = false;
    bool paused_ = false;
    bool finished_ = false;
};

} // namespace locus::app
