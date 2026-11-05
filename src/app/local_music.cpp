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
    SetMusicVolume(candidate, 0.70f);
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
    if (!ready_) return;
    Stop();
    finished_ = false;
    SetMusicPitch(music_, static_cast<float>(speed_));
    PlayMusicStream(music_);
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
}

void LocalMusic::Update(double monotonicSeconds) {
    if (!playing_ || paused_) return;
    UpdateMusicStream(music_);
    if (clock_.SecondsAt(monotonicSeconds) >= durationSeconds_) {
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
    return finished_ ? durationSeconds_ : clock_.SecondsAt(monotonicSeconds);
}

} // namespace locus::app
