#pragma once

#include <cstdint>
#include <vector>

namespace locus::core {

// Generates an original mono 16-bit PCM WAV click track in memory.
// There is no runtime file, copyrighted audio, or external fixture.
constexpr int kDemoSampleRate = 48000;

std::vector<std::uint8_t> MakeDemoClickWav(double bpm, int beats);

} // namespace locus::core
