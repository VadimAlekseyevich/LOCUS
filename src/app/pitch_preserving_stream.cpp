#include "app/pitch_preserving_stream.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

namespace locus::app {

PitchPreservingStream::~PitchPreservingStream() { Close(); }

bool PitchPreservingStream::Load(const std::vector<std::uint8_t>& encoded,
                                const char* extension, std::string& error) {
    Close();
    const auto startedAt = std::chrono::steady_clock::now();
    if (encoded.empty() || encoded.size() >
        static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        error = "Invalid or oversized audio file for time stretching";
        return false;
    }
    Wave wave = LoadWaveFromMemory(extension, encoded.data(),
                                   static_cast<int>(encoded.size()));
    if (!IsWaveValid(wave)) {
        error = "Could not decode the source for pitch-preserved playback";
        return false;
    }
    sampleRate_ = wave.sampleRate;
    channels_ = std::min(wave.channels, 2u);
    if (!sampleRate_ || !channels_) {
        UnloadWave(wave);
        error = "Invalid PCM format for time stretching";
        return false;
    }
    // SoundTouch consumes float mono/stereo PCM directly. WaveFormat()
    // produces a 32-bit float Wave owned by raylib, so retain that buffer
    // instead of allocating a second float array (LoadWaveSamples) and
    // copying it into a third array (std::vector<float>).
    // WaveFormat allocates and copies the entire track, even for an
    // already-float mono/stereo source. Reuse that decoded buffer as-is.
    if (wave.sampleSize != 32 || wave.channels != channels_)
        WaveFormat(&wave, static_cast<int>(sampleRate_), 32,
                   static_cast<int>(channels_));
    if (!IsWaveValid(wave) || !wave.data || !wave.frameCount ||
        wave.sampleSize != 32 || wave.channels != channels_) {
        UnloadWave(wave);
        error = "Could not convert source audio to 32-bit float PCM";
        return false;
    }
    decoded_ = wave; // Transfer ownership; freed once in Close().

    // The normal Music stream remains untouched and is used at 100%.
    // Dedicated, prebuffered AudioStream is used only for slower preview.
    // Use blocks the same size as raylib's subbuffers. With its default
    // auto-sizing, UpdateAudioStream pads every small write with silence.
    SetAudioStreamBufferSizeDefault(kBlockFrames);
    stream_ = LoadAudioStream(sampleRate_, 32, channels_);
    SetAudioStreamBufferSizeDefault(0);
    if (!IsAudioStreamValid(stream_)) {
        UnloadWave(decoded_);
        decoded_ = {};
        error = "Could not create pitch-preserving audio output";
        return false;
    }
    processor_.setSampleRate(sampleRate_);
    processor_.setChannels(channels_);
    processor_.setPitch(1.0);
    output_.resize(kBlockFrames * channels_);
    decodeMilliseconds_ = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - startedAt).count();
    ready_ = true;
    error.clear();
    return true;
}

void PitchPreservingStream::FillBlock() {
    if (!ready_) return;
    const std::size_t frameCount = decoded_.frameCount;
    const auto* const samples = static_cast<const float*>(decoded_.data);
    unsigned produced = 0;
    while (produced < kBlockFrames) {
        const unsigned got = processor_.receiveSamples(
            output_.data() + static_cast<std::size_t>(produced) * channels_,
            kBlockFrames - produced);
        produced += got;
        if (produced == kBlockFrames) break;
        if (cursorFrames_ < frameCount) {
            const unsigned put = static_cast<unsigned>(
                std::min<std::size_t>(4096, frameCount - cursorFrames_));
            processor_.putSamples(samples + cursorFrames_ * channels_, put);
            cursorFrames_ += put;
        } else if (!flushed_) {
            processor_.flush();
            flushed_ = true;
        } else {
            break;
        }
    }
    std::fill(output_.begin() + static_cast<std::size_t>(produced) * channels_,
              output_.end(), 0.0f);
    UpdateAudioStream(stream_, output_.data(), kBlockFrames);
}

void PitchPreservingStream::Prime() {
    // raylib uses two sub-buffers; feed at most two to keep seek latency
    // bounded without pre-rendering the entire song at low tempo.
    for (int i = 0; i < 2 && IsAudioStreamProcessed(stream_); ++i)
        FillBlock();
}

void PitchPreservingStream::StartAt(double songSeconds, double tempo,
                                    float volume) {
    if (!ready_) return;
    Stop();
    processor_.clear();
    processor_.setTempo(std::clamp(tempo, 0.01, 1.5));
    processor_.setPitch(1.0);
    cursorFrames_ = std::min<std::size_t>(
        static_cast<std::size_t>(std::max(0.0, songSeconds) * sampleRate_),
        static_cast<std::size_t>(decoded_.frameCount));
    flushed_ = false;
    SetVolume(volume);
    Prime();
    PlayAudioStream(stream_);
    playing_ = true;
}
void PitchPreservingStream::Pause() {
    if (playing_) PauseAudioStream(stream_);
}
void PitchPreservingStream::Resume() {
    if (playing_) ResumeAudioStream(stream_);
}
void PitchPreservingStream::Stop() {
    if (ready_ && playing_) StopAudioStream(stream_);
    playing_ = false;
}
void PitchPreservingStream::Update() {
    if (!playing_) return;
    // No decoding/allocations on the hardware callback thread.
    Prime();
}
void PitchPreservingStream::SetVolume(float volume) {
    if (ready_) SetAudioStreamVolume(stream_, volume);
}
void PitchPreservingStream::Close() noexcept {
    Stop();
    if (ready_) {
        UnloadAudioStream(stream_);
        ready_ = false;
    }
    stream_ = {};
    if (decoded_.data) UnloadWave(decoded_);
    decoded_ = {};
    output_.clear();
    cursorFrames_ = 0;
    sampleRate_ = channels_ = 0;
    decodeMilliseconds_ = 0.0;
    flushed_ = false;
}
} // namespace locus::app
