#include "app/pitch_preserving_stream.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace locus::app {

PitchPreservingStream::~PitchPreservingStream() { Close(); }

bool PitchPreservingStream::Load(const std::vector<std::uint8_t>& encoded,
                                const char* extension, std::string& error) {
    Close();
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
    // SoundTouch's optimized path processes float mono/stereo PCM. Convert
    // once, not during every Update or inside the audio output callback.
    WaveFormat(&wave, static_cast<int>(sampleRate_), 32,
               static_cast<int>(channels_));
    const std::size_t count = static_cast<std::size_t>(wave.frameCount) * channels_;
    float* decoded = LoadWaveSamples(wave);
    if (!decoded || !count) {
        if (decoded) UnloadWaveSamples(decoded);
        UnloadWave(wave);
        error = "Could not load float PCM for time stretching";
        return false;
    }
    samples_.assign(decoded, decoded + count);
    UnloadWaveSamples(decoded);
    UnloadWave(wave);

    // The normal Music stream remains untouched and is used at 100%.
    // Dedicated, prebuffered AudioStream is used only for slower preview.
    // Use blocks the same size as raylib's subbuffers. With its default
    // auto-sizing, UpdateAudioStream pads every small write with silence.
    SetAudioStreamBufferSizeDefault(kBlockFrames);
    stream_ = LoadAudioStream(sampleRate_, 32, channels_);
    SetAudioStreamBufferSizeDefault(0);
    if (!IsAudioStreamValid(stream_)) {
        samples_.clear();
        error = "Could not create pitch-preserving audio output";
        return false;
    }
    processor_.setSampleRate(sampleRate_);
    processor_.setChannels(channels_);
    processor_.setPitch(1.0);
    output_.resize(kBlockFrames * channels_);
    ready_ = true;
    error.clear();
    return true;
}

void PitchPreservingStream::FillBlock() {
    if (!ready_) return;
    const std::size_t frameCount = samples_.size() / channels_;
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
            processor_.putSamples(samples_.data() + cursorFrames_ * channels_, put);
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
    processor_.setTempo(std::clamp(tempo, 0.01, 1.0));
    processor_.setPitch(1.0);
    cursorFrames_ = std::min<std::size_t>(
        static_cast<std::size_t>(std::max(0.0, songSeconds) * sampleRate_),
        samples_.size() / channels_);
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
    samples_.clear();
    samples_.shrink_to_fit();
    output_.clear();
    cursorFrames_ = 0;
    sampleRate_ = channels_ = 0;
    flushed_ = false;
}
} // namespace locus::app
