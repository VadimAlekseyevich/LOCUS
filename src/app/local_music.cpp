#include "app/local_music.h"
#include "app/pitch_preserving_stream.h"

#include "core/music_file.h"
#include "app/local_file_io.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace locus::app {

// Out-of-line construction keeps unique_ptr<PitchPreservingStream>'s
// exception-unwind destructor in a TU with a complete definition (MSVC).
LocalMusic::LocalMusic() noexcept = default;

LocalMusic::~LocalMusic() {
    Shutdown();
}

void LocalMusic::SwapWith(LocalMusic& other) noexcept {
    using std::swap;
    swap(clock_, other.clock_);
    swap(fileBytes_, other.fileBytes_);
    swap(stretch_, other.stretch_);
    swap(sourceFormat_, other.sourceFormat_);
    swap(activeStretch_, other.activeStretch_);
    swap(music_, other.music_);
    swap(displayName_, other.displayName_);
    swap(error_, other.error_);
    swap(durationSeconds_, other.durationSeconds_);
    swap(startPositionSeconds_, other.startPositionSeconds_);
    swap(speed_, other.speed_);
    swap(volume_, other.volume_);
    swap(ready_, other.ready_);
    swap(playing_, other.playing_);
    swap(paused_, other.paused_);
    swap(finished_, other.finished_);
}

bool LocalMusic::LoadFromFile(const std::wstring& path) {
    const core::MusicFileType type = core::DetectMusicFileType(path);
    if (type == core::MusicFileType::Unsupported) {
        error_ = "Unsupported audio format. Choose a WAV or MP3.";
        return false;
    }
    if (!IsAudioDeviceReady()) {
        error_ = "No audio output device is ready. Check Windows sound settings.";
        return false;
    }
    std::vector<std::uint8_t> newBytes;
    if (!ReadLocalFileBytes(path, newBytes, error_)) return false;

    const char* format = (type == core::MusicFileType::Wav) ? ".wav" : ".mp3";
    Music candidate = LoadMusicStreamFromMemory(
        format, newBytes.data(), static_cast<int>(newBytes.size()));
    if (!IsMusicValid(candidate)) {
        error_ = "Audio decoder could not open this file. Try a different WAV or MP3.";
        return false;
    }
    const double length = static_cast<double>(GetMusicTimeLength(candidate));
    if (!std::isfinite(length) || length <= 0.0) {
        UnloadMusicStream(candidate);
        error_ = "This audio file has no valid playback duration.";
        return false;
    }
    candidate.looping = false;
    SetMusicVolume(candidate, volume_);
    SetMusicPitch(candidate, 1.0f);

    // Only replace the previous working song after successful decoding.
    Shutdown();
    fileBytes_.swap(newBytes);
    sourceFormat_ = format;
    music_ = candidate;
    durationSeconds_ = length;
    displayName_ = DisplayLocalFileName(path);
    ready_ = true;
    error_.clear();
    std::cout << "LOCAL_MUSIC_READY duration=" << durationSeconds_ << "s\n";
    return true;
}

void LocalMusic::Start(double monotonicSeconds) {
    StartAt(monotonicSeconds, 0.0);
}

void LocalMusic::StartAt(double monotonicSeconds, double audioSeconds) {
    if (!ready_ || !std::isfinite(audioSeconds) ||
        audioSeconds < 0.0 || audioSeconds >= durationSeconds_) return;
    Stop();
    finished_ = false;
    startPositionSeconds_ = audioSeconds;
    activeStretch_ = false;
    if (speed_ < 0.999) {
        if (!stretch_) stretch_ = std::make_unique<PitchPreservingStream>();
        if (!stretch_->Ready()) {
            std::string error;
            if (!stretch_->Load(fileBytes_, sourceFormat_.c_str(), error))
                std::cerr << "LOCAL_MUSIC_STRETCH_UNAVAILABLE: " << error << '\n';
        }
        if (stretch_->Ready()) {
            stretch_->StartAt(audioSeconds, speed_, volume_);
            activeStretch_ = true;
        }
    }
    if (!activeStretch_) {
        // Normal-speed playback uses raylib's existing streaming decoder.
        // A decode failure in the optional DSP path falls back to audio,
        // rather than silently dropping preview sound.
        SetMusicPitch(music_, static_cast<float>(speed_));
        PlayMusicStream(music_);
        if (audioSeconds > 0.0)
            SeekMusicStream(music_, static_cast<float>(audioSeconds));
    }
    clock_.Start(monotonicSeconds);
    playing_ = true;
}

void LocalMusic::Pause(double monotonicSeconds) {
    if (!playing_ || paused_) return;
    if (activeStretch_) stretch_->Pause();
    else PauseMusicStream(music_);
    clock_.Pause(monotonicSeconds);
    paused_ = true;
}

void LocalMusic::Resume(double monotonicSeconds) {
    if (!playing_ || !paused_) return;
    if (activeStretch_) stretch_->Resume();
    else ResumeMusicStream(music_);
    clock_.Resume(monotonicSeconds);
    paused_ = false;
}

void LocalMusic::SetVolume(float level) {
    if (!std::isfinite(level)) return;
    volume_ = std::clamp(level, 0.0f, 1.0f);
    if (ready_) SetMusicVolume(music_, volume_);
    if (stretch_ && stretch_->Ready()) stretch_->SetVolume(volume_);
}

void LocalMusic::SetSpeed(double fraction, double monotonicSeconds) {
    if (!std::isfinite(fraction) || fraction < 0.01 || fraction > 1.0 ||
        std::abs(fraction - speed_) < 0.000001) return;
    const bool wasPlaying = playing_, wasPaused = paused_;
    const double position = wasPlaying
        ? std::clamp(SongSeconds(monotonicSeconds), 0.0, durationSeconds_)
        : 0.0;
    if (wasPlaying) Stop();
    speed_ = fraction;
    clock_.SetRate(fraction, monotonicSeconds);
    if (wasPlaying && position < durationSeconds_) {
        StartAt(monotonicSeconds, position);
        if (wasPaused) Pause(monotonicSeconds);
    }
}

void LocalMusic::Stop() {
    if (stretch_ && stretch_->Playing()) stretch_->Stop();
    if (ready_ && playing_ && !activeStretch_) StopMusicStream(music_);
    activeStretch_ = false;
    playing_ = false;
    paused_ = false;
    clock_.Stop();
    startPositionSeconds_ = 0.0;
}

void LocalMusic::Update(double monotonicSeconds) {
    if (!playing_ || paused_) return;
    if (activeStretch_) stretch_->Update();
    else UpdateMusicStream(music_);
    if ((!activeStretch_ && !IsMusicStreamPlaying(music_)) ||
        SongSeconds(monotonicSeconds) >= durationSeconds_) {
        Stop();
        finished_ = true;
    }
}

void LocalMusic::Shutdown() {
    Stop();
    stretch_.reset();
    if (ready_) {
        UnloadMusicStream(music_);
        ready_ = false;
    }
    music_ = Music{};
    fileBytes_.clear();
    sourceFormat_.clear();
    displayName_.clear();
    durationSeconds_ = 0.0;
    finished_ = false;
}

double LocalMusic::SongSeconds(double monotonicSeconds) const noexcept {
    if (finished_) return durationSeconds_;
    // The raylib stream's consumed PCM frame position is the source of truth
    // for slowed authoring playback. A second synthetic rate clock drifts
    // ahead when audio output stalls, making authored turns seem late.
    if (ready_ && playing_ && speed_ < 0.999) {
        // The SoundTouch output is written at fixed hardware sample rate;
        // its chart-time advances at the requested musical tempo.
        if (activeStretch_)
            return std::clamp(startPositionSeconds_ +
                clock_.SecondsAt(monotonicSeconds), 0.0, durationSeconds_);
        const double cursor = static_cast<double>(GetMusicTimePlayed(music_));
        if (std::isfinite(cursor) &&
            cursor >= startPositionSeconds_ - 0.05 &&
            cursor <= durationSeconds_ + 0.05)
            return std::clamp(cursor, 0.0, durationSeconds_);
    }
    // Keep the monotonic clock for normal-speed scoring and for the brief
    // period before a seek has populated raylib's streaming buffers.
    return startPositionSeconds_ + clock_.SecondsAt(monotonicSeconds);
}

} // namespace locus::app
