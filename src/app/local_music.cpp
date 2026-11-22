#include "app/local_music.h"
#include "raylib.h"
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

namespace {
class RaylibOutputStream final : public MusicOutputStream {
public:
    explicit RaylibOutputStream(Music music) : music_(music) {
        music_.looping = false;
    }
    ~RaylibOutputStream() override { UnloadMusicStream(music_); }
    double DurationSeconds() const override {
        return static_cast<double>(GetMusicTimeLength(music_));
    }
    double PlayedSeconds() const override {
        return static_cast<double>(GetMusicTimePlayed(music_));
    }
    bool Playing() const override { return IsMusicStreamPlaying(music_); }
    void SetVolume(float v) override { SetMusicVolume(music_, v); }
    void SetPitch(float p) override { SetMusicPitch(music_, p); }
    void Play() override { PlayMusicStream(music_); }
    void Seek(float at) override { SeekMusicStream(music_, at); }
    void Pause() override { PauseMusicStream(music_); }
    void Resume() override { ResumeMusicStream(music_); }
    void Stop() override { StopMusicStream(music_); }
    void Update() override { UpdateMusicStream(music_); }
private:
    Music music_{};
};
class RaylibOutputBackend final : public MusicOutputBackend {
public:
    bool Ready() const override { return IsAudioDeviceReady(); }
    std::unique_ptr<MusicOutputStream> Open(
        const char* fmt, const std::uint8_t* bytes, int count) override {
        Music music = LoadMusicStreamFromMemory(fmt, bytes, count);
        if (!IsMusicValid(music)) return {};
        return std::make_unique<RaylibOutputStream>(music);
    }
};
MusicOutputBackend& DefaultOutputBackend() {
    static RaylibOutputBackend backend;
    return backend;
}
} // namespace

// Out-of-line construction keeps unique_ptr<PitchPreservingStream>'s
// exception-unwind destructor in a TU with a complete definition (MSVC).
LocalMusic::LocalMusic() noexcept : backend_(&DefaultOutputBackend()) {}
LocalMusic::LocalMusic(MusicOutputBackend& backend) noexcept : backend_(&backend) {}

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
    swap(stream_, other.stream_);
    swap(backend_, other.backend_);
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
    if (!backend_->Ready()) {
        error_ = "No audio output device is ready. Check Windows sound settings.";
        return false;
    }
    std::vector<std::uint8_t> newBytes;
    if (!ReadLocalFileBytes(path, newBytes, error_)) return false;

    const char* format = (type == core::MusicFileType::Wav) ? ".wav" : ".mp3";
    auto candidate = backend_->Open(
        format, newBytes.data(), static_cast<int>(newBytes.size()));
    if (!candidate) {
        error_ = "Audio decoder could not open this file. Try a different WAV or MP3.";
        return false;
    }
    const double length = candidate->DurationSeconds();
    if (!std::isfinite(length) || length <= 0.0) {
        error_ = "This audio file has no valid playback duration.";
        return false;
    }
    candidate->SetVolume(volume_);
    candidate->SetPitch(1.0f);

    // Stream may borrow memory: swap its backing bytes into this object
    // before adopting the decoder, after releasing the previous stream.
    Shutdown();
    fileBytes_.swap(newBytes);
    sourceFormat_ = format;
    stream_ = std::move(candidate);
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
    if (speed_ < 0.999 && backend_->SupportsPitchPreserving()) {
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
        stream_->SetPitch(static_cast<float>(speed_));
        stream_->Play();
        if (audioSeconds > 0.0)
            stream_->Seek(static_cast<float>(audioSeconds));
    }
    clock_.Start(monotonicSeconds);
    playing_ = true;
}

void LocalMusic::Pause(double monotonicSeconds) {
    if (!playing_ || paused_) return;
    if (activeStretch_) stretch_->Pause();
    else stream_->Pause();
    clock_.Pause(monotonicSeconds);
    paused_ = true;
}

void LocalMusic::Resume(double monotonicSeconds) {
    if (!playing_ || !paused_) return;
    if (activeStretch_) stretch_->Resume();
    else stream_->Resume();
    clock_.Resume(monotonicSeconds);
    paused_ = false;
}

void LocalMusic::SetVolume(float level) {
    if (!std::isfinite(level)) return;
    volume_ = std::clamp(level, 0.0f, 1.0f);
    if (ready_) stream_->SetVolume(volume_);
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
    if (ready_ && playing_ && !activeStretch_) stream_->Stop();
    activeStretch_ = false;
    playing_ = false;
    paused_ = false;
    clock_.Stop();
    startPositionSeconds_ = 0.0;
}

void LocalMusic::Update(double monotonicSeconds) {
    if (!playing_ || paused_) return;
    if (activeStretch_) stretch_->Update();
    else stream_->Update();
    if ((!activeStretch_ && !stream_->Playing()) ||
        SongSeconds(monotonicSeconds) >= durationSeconds_) {
        Stop();
        finished_ = true;
    }
}

void LocalMusic::Shutdown() {
    Stop();
    stretch_.reset();
    if (ready_) {
        stream_.reset();
        ready_ = false;
    }
    stream_.reset();
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
    if (ready_ && playing_ && speed_ < 0.999 &&
        backend_->HasConsumedAudioCursor()) {
        // The SoundTouch output is written at fixed hardware sample rate;
        // its chart-time advances at the requested musical tempo.
        if (activeStretch_)
            return std::clamp(startPositionSeconds_ +
                clock_.SecondsAt(monotonicSeconds), 0.0, durationSeconds_);
        const double cursor = stream_->PlayedSeconds();
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
