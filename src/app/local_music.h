#pragma once

#include "core/song_clock.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace locus::app {
class PitchPreservingStream;

// Device and decoder boundary: tests provide in-memory sound output while
// application controllers continue using the real LocalMusic API.
class MusicOutputStream {
public:
    virtual ~MusicOutputStream() = default;
    virtual double DurationSeconds() const = 0;
    virtual double PlayedSeconds() const = 0;
    virtual bool Playing() const = 0;
    virtual void SetVolume(float) = 0;
    virtual void SetPitch(float) = 0;
    virtual void Play() = 0;
    virtual void Seek(float) = 0;
    virtual void Pause() = 0;
    virtual void Resume() = 0;
    virtual void Stop() = 0;
    virtual void Update() = 0;
};

class MusicOutputBackend {
public:
    virtual ~MusicOutputBackend() = default;
    virtual bool Ready() const = 0;
    virtual std::unique_ptr<MusicOutputStream> Open(
        const char* format, const std::uint8_t* bytes, int count) = 0;
    virtual bool SupportsPitchPreserving() const noexcept { return true; }
    virtual bool HasConsumedAudioCursor() const noexcept { return true; }
};

// Plays a user-selected local WAV/MP3 without inventing a chart.
// Borrows the raylib audio device from AudioDevice and never closes it.
class LocalMusic {
public:
    LocalMusic() noexcept;
    explicit LocalMusic(MusicOutputBackend& backend) noexcept;
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
    MusicOutputBackend& Backend() const noexcept { return *backend_; }
    double DurationSeconds() const noexcept { return durationSeconds_; }
    double SongSeconds(double monotonicSeconds) const noexcept;
    const std::string& DisplayName() const noexcept { return displayName_; }
    const std::string& Error() const noexcept { return error_; }

private:
    core::SongClock clock_;
    // Raylib's memory stream borrows these bytes until UnloadMusicStream().
    std::vector<std::uint8_t> fileBytes_;
    // Lazily decoded on sub-100% playback, keeps pitch at 1.0.
    std::unique_ptr<PitchPreservingStream> stretch_;
    std::string sourceFormat_;
    bool activeStretch_ = false;
    std::unique_ptr<MusicOutputStream> stream_;
    MusicOutputBackend* backend_ = nullptr;
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
