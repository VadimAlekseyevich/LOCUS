#pragma once

namespace locus::app {

// A source audio cursor sampled at a known monotonic instant. Some audio
// decoders expose only the *current* consumed cursor: requesting it at two
// past timestamps does not recover the audio position of either key press.
//
// One sample per game update, then project EACH queued input back along the
// rate-scaled time axis. OS event order and millisecond spacing survive frame
// batching and audio-buffer quantization without moving the authoritative
// cursor or changing the configured judgement windows.
struct SampledSongTime {
    double sampledAt = 0.0;
    double songSeconds = 0.0;
    double rate = 1.0;

    double At(double capturedAt) const noexcept {
        return songSeconds + (capturedAt - sampledAt) * rate;
    }
};

} // namespace locus::app
