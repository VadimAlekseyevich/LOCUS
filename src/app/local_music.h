#pragma once

#include "core/song_clock.h"

#include "raylib.h"

#include <cstdint>
#include <string>
#include <vector>

namespace locus::app {

// Plays a user-selected local WAV/MP3 without inventing a chart.
// Borrows the raylib audio device from AudioDevice and never closes it.
class LocalMusic {
public:
    LocalMusic() = default;
    LocalMusic(const LocalMusic&) = delete;
    LocalMusic& operator=(const LocalMusic&) = delete;
    ~LocalMusic();

    // On failure, the previous loaded song remains usable.
    bool LoadFromFile(const std::wstring& path);
    void Start(double monotonicSeconds);
    // Play from a real WAV/MP3 position; the visual clock follows that position.
    void StartAt(double monotonicSeconds, double audioSeconds);
    void Pause(double monotonicSeconds);
    void Resume(double monotonicSeconds);
    void Stop();
    void Update(double monotonicSeconds);
    void SetSpeed(double fraction, double monotonicSeconds);
    void SetVolume(float level);
    void Shutdown();

    bool Ready() const noexcept { return ready_; }
    bool Playing() const noexcept { return playing_; }
    bool Paused() const noexcept { return paused_; }
    bool Finished() const noexcept { return finished_; }
    double Speed() const noexcept { return speed_; }
    float Volume() const noexcept { return volume_; }
    // Commit a fully decoded and validated replacement without losing the old
    // song on failure. Stop playback before swapping.
    void SwapWith(LocalMusic& other) noexcept;
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
    double startPositionSeconds_ = 0.0;
    double speed_ = 1.0;
    float volume_ = 0.7f;
    bool ready_ = false;
    bool playing_ = false;
    bool paused_ = false;
    bool finished_ = false;
};

} // namespace locus::app
