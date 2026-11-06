#include "app/demo_transport.h"

#include "core/demo_audio.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace locus::app {

DemoTransport::~DemoTransport() {
    Shutdown();
}

bool DemoTransport::Initialize(const core::Chart& chart) {
    Shutdown();
    bpm_ = chart.bpm;
    durationSeconds_ = chart.lengthBeats * 60.0 / bpm_;

    // AudioDevice initializes the shared output before any transport.
    // A failed demo decoder must not close LocalMusic's audio device.
    if (!IsAudioDeviceReady()) {
        std::cerr << "DEMO_AUDIO_NO_DEVICE\n";
        return false;
    }

    wavData_ = core::MakeDemoClickWav(bpm_, chart.lengthBeats);
    if (wavData_.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        std::cerr << "DEMO_AUDIO_TOO_LARGE\n";
        Shutdown();
        return false;
    }

    // raylib's WAV decoder borrows these memory bytes: keep wavData_ alive
    // through UnloadMusicStream() instead of saving an audio file to disk.
    music_ = LoadMusicStreamFromMemory(
        ".wav", wavData_.data(), static_cast<int>(wavData_.size()));
    musicLoaded_ = IsMusicValid(music_);
    if (!musicLoaded_) {
        std::cerr << "DEMO_AUDIO_LOAD_FAILED\n";
        Shutdown();
        return false;
    }
    music_.looping = false;
    SetMusicVolume(music_, volume_);
    std::cout << "DEMO_AUDIO_READY bpm=" << bpm_
              << " beats=" << chart.lengthBeats << '\n';
    return true;
}

void DemoTransport::SetVolume(float level) {
    if (!std::isfinite(level)) return;
    volume_ = std::clamp(level, 0.0f, 1.0f);
    if (musicLoaded_) SetMusicVolume(music_, volume_);
}

void DemoTransport::Start(double monotonicSeconds) {
    if (!musicLoaded_) {
        return;
    }
    Stop();
    finished_ = false;
    PlayMusicStream(music_);
    clock_.Start(monotonicSeconds);
    playing_ = true;
    std::cout << "DEMO_AUDIO_PLAY\n";
}

void DemoTransport::Pause(double monotonicSeconds) {
    if (!playing_ || paused_) return;
    PauseMusicStream(music_);
    clock_.Pause(monotonicSeconds);
    paused_ = true;
    std::cout << "DEMO_AUDIO_PAUSE\n";
}

void DemoTransport::Resume(double monotonicSeconds) {
    if (!playing_ || !paused_) return;
    ResumeMusicStream(music_);
    clock_.Resume(monotonicSeconds);
    paused_ = false;
    std::cout << "DEMO_AUDIO_RESUME\n";
}

void DemoTransport::Stop() {
    if (musicLoaded_ && playing_) {
        StopMusicStream(music_);
    }
    playing_ = false;
    paused_ = false;
    clock_.Stop();
}

void DemoTransport::Update(double monotonicSeconds) {
    if (!playing_ || paused_) {
        return;
    }
    UpdateMusicStream(music_);
    if (clock_.SecondsAt(monotonicSeconds) >= durationSeconds_) {
        Stop();
        finished_ = true;
        std::cout << "DEMO_AUDIO_FINISHED\n";
    }
}

void DemoTransport::Shutdown() {
    Stop();
    if (musicLoaded_) {
        UnloadMusicStream(music_);
        musicLoaded_ = false;
        music_ = Music{};
    }
    // The raylib memory decoder no longer references this buffer.
    wavData_.clear();
    finished_ = false;
}

double DemoTransport::SongSeconds(double monotonicSeconds) const noexcept {
    return finished_ ? durationSeconds_ : clock_.SecondsAt(monotonicSeconds);
}

double DemoTransport::Beat(double monotonicSeconds) const noexcept {
    return SongSeconds(monotonicSeconds) * bpm_ / 60.0;
}

const char* DemoTransport::Status() const noexcept {
    if (!musicLoaded_) return "Audio unavailable - check output device";
    if (finished_) return "Demo complete - see Results";
    if (paused_) return "PAUSED - SEE SETTINGS FOR CONTROLS";
    if (playing_) return "BUILT-IN DEMO - PRESS YOUR BOUND DIRECTION KEYS";
    return "BUILT-IN CLICK TRACK READY";
}

} // namespace locus::app
