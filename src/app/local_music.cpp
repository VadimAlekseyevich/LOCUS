#include "app/local_music.h"

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

LocalMusic::~LocalMusic() {
    Shutdown();
}

void LocalMusic::SwapWith(LocalMusic& other) noexcept {
    using std::swap;
    swap(clock_, other.clock_);
    swap(fileBytes_, other.fileBytes_);
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
    SetMusicPitch(candidate, static_cast<float>(speed_));

    // Only replace the previous working song after successful decoding.
    Shutdown();
    fileBytes_.swap(newBytes);
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
    SetMusicPitch(music_, static_cast<float>(speed_));
    PlayMusicStream(music_);
    // Seek the actual audio stream; moving just the marker would desync it.
    if (audioSeconds > 0.0)
        SeekMusicStream(music_, static_cast<float>(audioSeconds));
    clock_.Start(monotonicSeconds);
    playing_ = true;
}

void LocalMusic::Pause(double monotonicSeconds) {
    if (!playing_ || paused_) return;
    PauseMusicStream(music_);
    clock_.Pause(monotonicSeconds);
    paused_ = true;
}

void LocalMusic::Resume(double monotonicSeconds) {
    if (!playing_ || !paused_) return;
    ResumeMusicStream(music_);
    clock_.Resume(monotonicSeconds);
    paused_ = false;
}

void LocalMusic::SetVolume(float level) {
    if (!std::isfinite(level)) return;
    volume_ = std::clamp(level, 0.0f, 1.0f);
    if (ready_) SetMusicVolume(music_, volume_);
}

void LocalMusic::SetSpeed(double fraction, double monotonicSeconds) {
    if (!std::isfinite(fraction) || fraction < 0.1 || fraction > 1.0) return;
    speed_ = fraction;
    clock_.SetRate(fraction, monotonicSeconds);
    if (ready_) SetMusicPitch(music_, static_cast<float>(fraction));
}

void LocalMusic::Stop() {
    if (ready_ && playing_) StopMusicStream(music_);
    playing_ = false;
    paused_ = false;
    clock_.Stop();
    startPositionSeconds_ = 0.0;
}

void LocalMusic::Update(double monotonicSeconds) {
    if (!playing_ || paused_) return;
    UpdateMusicStream(music_);
    // At reduced pitch the hardware may consume source frames more slowly
    // than an ideal wall-clock prediction (buffering, stalls, device jitter).
    // Let the actual stream determine completion instead of cutting off the
    // last part of a slow practice preview.
    if (!IsMusicStreamPlaying(music_) ||
        SongSeconds(monotonicSeconds) >= durationSeconds_) {
        Stop();
        finished_ = true;
    }
}

void LocalMusic::Shutdown() {
    Stop();
    if (ready_) {
        UnloadMusicStream(music_);
        ready_ = false;
    }
    music_ = Music{};
    fileBytes_.clear();
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
