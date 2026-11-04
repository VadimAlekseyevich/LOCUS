// A-07 disposable raylib audio-time experiment. NOT LOCUS game architecture.
#include "raylib.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

namespace {
constexpr int kSampleRate = 48000;
constexpr int kDurationSeconds = 16;
constexpr int kBpm = 120;
constexpr double kSecondsPerBeat = 60.0 / kBpm;
constexpr double kPi = 3.14159265358979323846;
constexpr char kWavPath[] = "metronome.wav";
constexpr char kCsvPath[] = "raylib_probe_log.csv";

double MonotonicSeconds() {
    using Clock = std::chrono::steady_clock;
    static const auto origin = Clock::now();
    return std::chrono::duration<double>(Clock::now() - origin).count();
}

void WriteU16(std::ofstream& out, std::uint16_t value) {
    const char bytes[2] = {
        static_cast<char>(value & 0xffu),
        static_cast<char>((value >> 8) & 0xffu)
    };
    out.write(bytes, 2);
}

void WriteU32(std::ofstream& out, std::uint32_t value) {
    const char bytes[4] = {
        static_cast<char>(value & 0xffu),
        static_cast<char>((value >> 8) & 0xffu),
        static_cast<char>((value >> 16) & 0xffu),
        static_cast<char>((value >> 24) & 0xffu)
    };
    out.write(bytes, 4);
}

bool GenerateMetronome(const std::string& path) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;

    const auto frames = static_cast<std::uint32_t>(kSampleRate * kDurationSeconds);
    const auto bytes = frames * sizeof(std::int16_t);

    out.write("RIFF", 4);
    WriteU32(out, 36u + bytes);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    WriteU32(out, 16);                 // PCM header
    WriteU16(out, 1);                  // Linear PCM
    WriteU16(out, 1);                  // Mono
    WriteU32(out, kSampleRate);
    WriteU32(out, kSampleRate * 2);
    WriteU16(out, 2);
    WriteU16(out, 16);
    out.write("data", 4);
    WriteU32(out, bytes);

    for (std::uint32_t i = 0; i < frames; ++i) {
        const double absolute = static_cast<double>(i) / kSampleRate;
        const double sinceTick = std::fmod(absolute, kSecondsPerBeat);
        double wave = 0.0;
        if (sinceTick < 0.035) {
            const int tick = static_cast<int>(absolute / kSecondsPerBeat);
            const double frequency = (tick % 4 == 0) ? 1100.0 : 830.0;
            const double envelope = std::exp(-100.0 * sinceTick);
            wave = 0.65 * envelope * std::sin(2.0 * kPi * frequency * sinceTick);
        }
        const auto pcm = static_cast<std::int16_t>(std::lround(
            std::clamp(wave, -1.0, 1.0) * 32767.0));
        WriteU16(out, static_cast<std::uint16_t>(pcm));
    }
    out.flush();
    return out.good();
}

bool ValidateGeneratedWav(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    char riff[4]{}, wave[4]{};
    in.read(riff, 4);
    in.seekg(8, std::ios::beg);
    in.read(wave, 4);
    std::error_code ec;
    const auto bytes = std::filesystem::file_size(path, ec);
    return in.good() && std::string(riff, 4) == "RIFF" &&
        std::string(wave, 4) == "WAVE" &&
        !ec && bytes == 44u + static_cast<std::uintmax_t>(kSampleRate) *
            kDurationSeconds * 2u;
}

struct MonotonicAnchor {
    double positionSeconds = 0.0;
    double anchorMonotonic = 0.0;
    bool paused = false;

    double ExpectedAt(double capturedMonotonic, double duration) const {
        const double elapsed = paused ? 0.0 :
            std::max(0.0, capturedMonotonic - anchorMonotonic);
        return std::clamp(positionSeconds + elapsed, 0.0, duration);
    }

    void Reanchor(double position, double now) {
        positionSeconds = position;
        anchorMonotonic = now;
    }
};

void LogSnapshot(std::ofstream& csv, const char* event, double capturedMonotonic,
                 Music music, const MonotonicAnchor& anchor, double duration) {
    const double reported = GetMusicTimePlayed(music);
    const double expected = anchor.ExpectedAt(capturedMonotonic, duration);
    const double delta = reported - expected;
    if (csv) {
        csv << std::fixed << std::setprecision(6)
            << capturedMonotonic << ',' << reported << ','
            << expected << ',' << delta << ',' << event << '\n';
        csv.flush();
    }
    if (std::string(event) != "sample") {
        std::cout << std::fixed << std::setprecision(3)
                  << event << " captured_mono=" << capturedMonotonic
                  << " music_reported=" << reported
                  << " wall_anchored=" << expected
                  << " reported_minus_wall=" << delta << '\n';
    }
}

int SelfTest(const char* executablePath) {
    if (!GenerateMetronome(kWavPath) || !ValidateGeneratedWav(kWavPath)) {
        std::cerr << "SELF_TEST_FAIL: generated WAV header/size invalid\n";
        return 2;
    }
    std::error_code ec;
    const auto exeBytes = std::filesystem::file_size(executablePath, ec);
    if (ec) {
        std::cerr << "SELF_TEST_FAIL: cannot measure executable: " << ec.message() << '\n';
        return 3;
    }
    std::cout << "SELF_TEST_PASS\n"
              << "probe_executable_bytes=" << exeBytes << '\n'
              << "generated_wav_bytes=" << std::filesystem::file_size(kWavPath) << '\n'
              << "generated_bpm=" << kBpm << '\n'
              << "generated_sample_rate=" << kSampleRate << '\n'
              << "generated_duration_seconds=" << kDurationSeconds << '\n'
              << "audio_playback_tested=false\n"
              << "note=Headless self-test does not initialize an audio device\n";
    return 0;
}
}  // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--self-test") {
        return SelfTest(argv[0]);
    }
    if (!GenerateMetronome(kWavPath) || !ValidateGeneratedWav(kWavPath)) {
        std::cerr << "Could not generate original metronome WAV\n";
        return 2;
    }

    InitWindow(1080, 620, "LOCUS - A-07 raylib audio timing SPIKE (not the game)");
    SetTargetFPS(120);
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        std::cerr << "Audio device unavailable; interactive timing test cannot run\n";
        CloseAudioDevice();
        CloseWindow();
        return 3;
    }

    Music music = LoadMusicStream(kWavPath);
    if (!IsMusicValid(music)) {
        std::cerr << "raylib cannot decode generated WAV\n";
        CloseAudioDevice();
        CloseWindow();
        return 4;
    }
    music.looping = false;
    const double duration = GetMusicTimeLength(music);
    std::ofstream csv(kCsvPath, std::ios::trunc);
    if (csv) csv << "monotonic_seconds,music_reported_seconds,wall_anchored_seconds,"
                    "reported_minus_wall_seconds,event\n";

    PlayMusicStream(music);
    MonotonicAnchor anchor;
    anchor.Reanchor(0.0, MonotonicSeconds());
    double lastSample = -1.0;
    double lastPressed = -1.0;
    const char* lastAction = "(none)";
    bool paused = false;

    while (!WindowShouldClose()) {
        UpdateMusicStream(music);
        for (int key = GetKeyPressed(); key != 0; key = GetKeyPressed()) {
            const double capture = MonotonicSeconds();  // Poll timestamp, NOT device timestamp
            if (key == KEY_A || key == KEY_D) {
                lastPressed = capture;
                lastAction = (key == KEY_A) ? "LEFT_TEST" : "RIGHT_TEST";
                LogSnapshot(csv, lastAction, capture, music, anchor, duration);
            } else if (key == KEY_SPACE || key == KEY_P) {
                if (!paused) {
                    anchor.Reanchor(anchor.ExpectedAt(capture, duration), capture);
                    PauseMusicStream(music);
                    anchor.paused = true;
                    paused = true;
                    LogSnapshot(csv, "PAUSE", capture, music, anchor, duration);
                } else {
                    ResumeMusicStream(music);
                    anchor.paused = false;
                    anchor.Reanchor(GetMusicTimePlayed(music), capture);
                    paused = false;
                    LogSnapshot(csv, "RESUME", capture, music, anchor, duration);
                }
            } else if (key == KEY_J || key == KEY_L) {
                const double current = GetMusicTimePlayed(music);
                const double target = std::clamp(
                    current + ((key == KEY_J) ? -2.0 : 2.0),
                    0.0, std::max(0.0, duration - 0.05));
                SeekMusicStream(music, static_cast<float>(target));
                UpdateMusicStream(music);
                anchor.Reanchor(target, capture);
                LogSnapshot(csv, "SEEK", capture, music, anchor, duration);
            } else if (key == KEY_R) {
                StopMusicStream(music);
                SeekMusicStream(music, 0.0f);
                PlayMusicStream(music);
                paused = false;
                anchor.paused = false;
                anchor.Reanchor(0.0, capture);
                LogSnapshot(csv, "RESTART", capture, music, anchor, duration);
            }
        }

        const double now = MonotonicSeconds();
        if (now - lastSample >= 0.1) {
            LogSnapshot(csv, "sample", now, music, anchor, duration);
            lastSample = now;
        }
        const double reported = GetMusicTimePlayed(music);
        const double expected = anchor.ExpectedAt(now, duration);

        BeginDrawing();
        ClearBackground(Color{21, 23, 33, 255});
        DrawText("LOCUS A-07 / disposable raylib music-time probe", 30, 28, 24, RAYWHITE);
        DrawText("A/D: input timestamp | SPACE/P: pause | J/L: seek -/+2s | R: restart",
                 30, 80, 19, LIGHTGRAY);
        DrawText("ESC: close | Original generated WAV: 120 BPM, 16s, 48kHz mono",
                 30, 110, 18, LIGHTGRAY);
        DrawText(TextFormat("GetMusicTimePlayed:  %.6f seconds", reported),
                 30, 172, 24, SKYBLUE);
        DrawText(TextFormat("Steady-clock anchor:   %.6f seconds", expected),
                 30, 210, 24, GREEN);
        DrawText(TextFormat("Difference (NOT acoustic latency):  %+.6f seconds",
                            reported - expected), 30, 248, 21, YELLOW);
        DrawText(TextFormat("Stream length: %.3f s | Paused: %s",
                            duration, paused ? "yes" : "no"), 30, 288, 20, LIGHTGRAY);
        DrawText(TextFormat("Last input: %s | capture monotonic: %.6f s",
                            lastAction, lastPressed), 30, 338, 20, RAYWHITE);
        DrawText("Every 100ms and every key-down is recorded in raylib_probe_log.csv",
                 30, 396, 18, LIGHTGRAY);
        DrawText("GetMusicTimePlayed is a streaming-position estimate, not a DAC callback.",
                 30, 448, 18, ORANGE);
        DrawText("Keyboard time is captured when raylib polls the event (frame-limited).",
                 30, 480, 18, ORANGE);
        DrawText("Do NOT infer hit-judgement accuracy from this probe alone.",
                 30, 530, 18, RAYWHITE);
        EndDrawing();
    }

    LogSnapshot(csv, "EXIT", MonotonicSeconds(), music, anchor, duration);
    UnloadMusicStream(music);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
