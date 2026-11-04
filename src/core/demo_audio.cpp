#include "core/demo_audio.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace locus::core {
namespace {
constexpr double kPi = 3.14159265358979323846;

void Append16(std::vector<std::uint8_t>& wav, std::uint16_t value) {
    wav.push_back(static_cast<std::uint8_t>(value & 0xffU));
    wav.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffU));
}

void Append32(std::vector<std::uint8_t>& wav, std::uint32_t value) {
    Append16(wav, static_cast<std::uint16_t>(value & 0xffffU));
    Append16(wav, static_cast<std::uint16_t>(value >> 16));
}

} // namespace

std::vector<std::uint8_t> MakeDemoClickWav(double bpm, int beats) {
    if (!std::isfinite(bpm) || bpm <= 0.0 || beats <= 0) {
        throw std::invalid_argument("demo click track requires positive BPM and beats");
    }
    const double framesExact = beats * 60.0 / bpm * kDemoSampleRate;
    if (framesExact > (std::numeric_limits<std::uint32_t>::max() - 44U) / 2.0) {
        throw std::length_error("demo click track too long");
    }
    const auto frames = static_cast<std::uint32_t>(std::llround(framesExact));
    if (frames == 0) {
        throw std::invalid_argument("demo click track shorter than one sample");
    }
    const auto bytes = frames * 2U;
    std::vector<std::uint8_t> wav;
    wav.reserve(44U + bytes);

    wav.insert(wav.end(), {'R', 'I', 'F', 'F'});
    Append32(wav, 36U + bytes);
    wav.insert(wav.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    Append32(wav, 16U);
    Append16(wav, 1U); // Linear PCM
    Append16(wav, 1U); // Mono
    Append32(wav, kDemoSampleRate);
    Append32(wav, kDemoSampleRate * 2U);
    Append16(wav, 2U);
    Append16(wav, 16U);
    wav.insert(wav.end(), {'d', 'a', 't', 'a'});
    Append32(wav, bytes);

    // All clicks are synthesized on exact musical beat boundaries.
    // A brighter accent on each fourth beat makes the demo easy to follow.
    const double samplesPerBeat = 60.0 / bpm * kDemoSampleRate;
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        const double beatPosition = static_cast<double>(frame) / samplesPerBeat;
        const int beat = static_cast<int>(std::floor(beatPosition));
        const double tickSeconds = (beatPosition - beat) * 60.0 / bpm;
        double sample = 0.0;
        if (tickSeconds < 0.055) {
            const double frequency = (beat % 4 == 0) ? 1100.0 : 800.0;
            sample = 0.72 * std::exp(-85.0 * tickSeconds) *
                     std::sin(2.0 * kPi * frequency * tickSeconds);
        }
        const auto signedSample = static_cast<std::int16_t>(
            std::lround(std::clamp(sample, -1.0, 1.0) * 32767.0));
        Append16(wav, static_cast<std::uint16_t>(signedSample));
    }
    return wav;
}

} // namespace locus::core
