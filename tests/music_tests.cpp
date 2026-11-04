#include "core/demo_audio.h"
#include "core/song_clock.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

bool Expect(bool condition, const char* description) {
    if (!condition) std::cerr << "DEMO_AUDIO_TEST_FAIL: " << description << '\n';
    return condition;
}

std::uint32_t Read32(const std::vector<std::uint8_t>& bytes, std::size_t at) {
    return static_cast<std::uint32_t>(bytes.at(at)) |
           (static_cast<std::uint32_t>(bytes.at(at + 1)) << 8U) |
           (static_cast<std::uint32_t>(bytes.at(at + 2)) << 16U) |
           (static_cast<std::uint32_t>(bytes.at(at + 3)) << 24U);
}

std::int16_t ReadSample(const std::vector<std::uint8_t>& bytes,
                        std::size_t sampleIndex) {
    const std::size_t offset = 44U + sampleIndex * 2U;
    const std::uint16_t raw = static_cast<std::uint16_t>(bytes.at(offset)) |
                              (static_cast<std::uint16_t>(bytes.at(offset + 1)) << 8U);
    return static_cast<std::int16_t>(raw);
}

} // namespace

int main() {
    using locus::core::SongClock;
    using locus::core::MakeDemoClickWav;
    bool ok = true;

    SongClock clock;
    ok &= Expect(!clock.Running() && clock.SecondsAt(42.0) == 0.0,
                 "stopped song clock must not advance");
    clock.Start(100.0);
    ok &= Expect(clock.Running() && clock.SecondsAt(100.0) == 0.0,
                 "song starts at zero on its monotonic anchor");
    ok &= Expect(std::abs(clock.SecondsAt(100.25) - 0.25) < 1e-10,
                 "song seconds use monotonically sampled time");
    ok &= Expect(std::abs(clock.BeatAt(100.25, 120.0) - 0.5) < 1e-10,
                 "120 BPM is two quarter-note beats per second");
    ok &= Expect(std::abs(clock.BeatAt(102.0, 120.0, 0.25) - 3.5) < 1e-10,
                 "positive offset places beat zero after audio start");
    ok &= Expect(clock.SecondsAt(99.5) == 0.0,
                 "clock never exposes a negative song time");
    clock.Stop();
    ok &= Expect(!clock.Running() && clock.SecondsAt(10000.0) == 0.0,
                 "stopping transport freezes and resets the clock");
    clock.Start(200.0);
    ok &= Expect(std::abs(clock.BeatAt(201.0, 120.0) - 2.0) < 1e-10,
                 "restarting establishes a new independent monotonic anchor");

    // G-06: pausing and resuming must freeze the same clock used for route
    // animation and A/D judgements; wall-clock pause time is excluded.
    clock.Pause(201.25);
    ok &= Expect(clock.Paused() && clock.Running() &&
                     std::abs(clock.SecondsAt(250.0) - 1.25) < 1e-10 &&
                     std::abs(clock.BeatAt(250.0, 120.0) - 2.5) < 1e-10,
                 "music time must not advance during pause");
    clock.Pause(250.0);
    ok &= Expect(std::abs(clock.SecondsAt(300.0) - 1.25) < 1e-10,
                 "pausing twice must not double-count elapsed time");
    clock.Resume(300.0);
    ok &= Expect(!clock.Paused() &&
                     std::abs(clock.SecondsAt(300.25) - 1.5) < 1e-10 &&
                     std::abs(clock.BeatAt(300.25, 120.0) - 3.0) < 1e-10,
                 "resuming excludes wall-clock pause time");
    clock.Resume(301.0);
    ok &= Expect(std::abs(clock.SecondsAt(301.25) - 2.5) < 1e-10,
                 "resuming twice must not shift the song clock");
    clock.Pause(301.25);
    clock.Stop();
    ok &= Expect(!clock.Running() && !clock.Paused() &&
                     clock.SecondsAt(400.0) == 0.0,
                 "stop clears paused state and accumulated music time");
    clock.Start(500.0);
    ok &= Expect(std::abs(clock.SecondsAt(500.5) - 0.5) < 1e-10,
                 "retry starts a fresh clock, independent of the previous pause");

    constexpr std::size_t frames = 48000U * 16U;
    const auto wav = MakeDemoClickWav(120.0, 32);
    ok &= Expect(wav.size() == 44U + frames * 2U,
                 "32-beat 120 BPM WAV must last exactly sixteen seconds");
    ok &= Expect(std::memcmp(wav.data(), "RIFF", 4) == 0 &&
                     std::memcmp(wav.data() + 8, "WAVE", 4) == 0 &&
                     std::memcmp(wav.data() + 12, "fmt ", 4) == 0 &&
                     std::memcmp(wav.data() + 36, "data", 4) == 0,
                 "valid in-memory RIFF/WAVE/PCM header");
    ok &= Expect(Read32(wav, 4) == wav.size() - 8U &&
                     Read32(wav, 24) == 48000U &&
                     Read32(wav, 28) == 96000U &&
                     Read32(wav, 40) == frames * 2U,
                 "PCM sample rate, byte rate and data length");
    ok &= Expect(ReadSample(wav, 100U) != 0 &&
                     ReadSample(wav, 24100U) != 0 &&
                     ReadSample(wav, 48100U) != 0,
                 "synthesized clicks at the first three quarter-note beats");
    ok &= Expect(ReadSample(wav, 12000U) == 0 &&
                     ReadSample(wav, 36000U) == 0,
                 "silence between synthesized beat clicks");

    try {
        (void)MakeDemoClickWav(0.0, 32);
        ok &= Expect(false, "zero BPM must be rejected");
    } catch (const std::invalid_argument&) {}
    try {
        (void)MakeDemoClickWav(std::numeric_limits<double>::infinity(), 32);
        ok &= Expect(false, "non-finite BPM must be rejected");
    } catch (const std::invalid_argument&) {}
    try {
        (void)MakeDemoClickWav(120.0, 0);
        ok &= Expect(false, "zero beats must be rejected");
    } catch (const std::invalid_argument&) {}

    if (!ok) return 1;
    std::cout << "DEMO_AUDIO_CLOCK_PASS bpm=120 beats=32 duration=16s\n";
    return 0;
}
