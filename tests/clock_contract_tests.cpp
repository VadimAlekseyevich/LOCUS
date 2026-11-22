#include "app/chart_playback.h"
#include "app/global_offset_calibrator.h"
#include "app/local_music.h"
#include "app/play_intro_skip.h"
#include "core/tempo_map.h"
#include "song_fixture.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace {
using namespace locus;
namespace fs = std::filesystem;

bool Check(bool good, const char* scenario) {
    if (!good) std::cerr << "CLOCK_CONTRACT_FAIL " << scenario << '\n';
    return good;
}
bool Close(double x, double y, double tolerance = 1e-7) {
    return std::abs(x - y) <= tolerance;
}
std::uint32_t ReadLe32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) |
        (static_cast<std::uint32_t>(p[1]) << 8) |
        (static_cast<std::uint32_t>(p[2]) << 16) |
        (static_cast<std::uint32_t>(p[3]) << 24);
}

// The ONLY replaced layer is hardware output/decoder. Production LocalMusic
// still validates the actual WAV from disk; ChartPlayback owns all song time.
class SilentStream final : public app::MusicOutputStream {
public:
    explicit SilentStream(double duration) : duration_(duration) {}
    double DurationSeconds() const override { return duration_; }
    double PlayedSeconds() const override { return position_; }
    bool Playing() const override { return started_ && !paused_; }
    void SetVolume(float) override {}
    void SetPitch(float) override {}
    void Play() override { started_ = true; paused_ = false; }
    void Seek(float seconds) override { position_ = seconds; }
    void Pause() override { paused_ = true; }
    void Resume() override { paused_ = false; }
    void Stop() override { started_ = false; paused_ = false; position_ = 0.0; }
    void Update() override {}
private:
    double duration_;
    double position_ = 0.0;
    bool started_ = false;
    bool paused_ = false;
};
class SilentOutput final : public app::MusicOutputBackend {
public:
    bool Ready() const override { return true; }
    bool SupportsPitchPreserving() const noexcept override { return false; }
    bool HasConsumedAudioCursor() const noexcept override { return false; }
    std::unique_ptr<app::MusicOutputStream> Open(
        const char* format, const std::uint8_t* data, int size) override {
        if (size < 44 || std::strcmp(format, ".wav") != 0 ||
            std::memcmp(data, "RIFF", 4) != 0 ||
            std::memcmp(data + 8, "WAVE", 4) != 0 ||
            ReadLe32(data + 28) == 0) return nullptr;
        return std::make_unique<SilentStream>(
            static_cast<double>(ReadLe32(data + 40)) /
            ReadLe32(data + 28));
    }
};
class MissingOutput final : public app::MusicOutputBackend {
public:
    bool Ready() const override { return false; }
    std::unique_ptr<app::MusicOutputStream> Open(
        const char*, const std::uint8_t*, int) override { return nullptr; }
};

struct TempSong {
    fs::path folder;
    fs::path wav;
    TempSong() {
        folder = fs::temp_directory_path() /
            ("locus-clock-contract-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(folder);
        wav = folder / "song.wav";
    }
    ~TempSong() {
        std::error_code ec;
        fs::remove_all(folder, ec);
    }
};
} // namespace

int main() {
    bool ok = true;
    TempSong file;
    ok &= Check(test::WriteSilentWav(file.wav),
                "fixture is a real four-second PCM WAV");
    SilentOutput silentDevice;
    app::LocalMusic song(silentDevice);
    app::ChartPlayback play(song);
    ok &= Check(song.LoadFromFile(file.wav.wstring()) &&
                Close(song.DurationSeconds(), 4.0),
                "real LocalMusic loads a four-second song");

    core::Chart firstAtZero;
    firstAtZero.tempoMap = {{0, 120.0}};
    firstAtZero.turns = {{0, core::Heading::Up, 0}};
    const double lead = app::ScoredChartStartSeconds(firstAtZero, 0.0);
    ok &= Check(lead <= -app::kMinimumFirstNoteLeadSeconds &&
                std::isfinite(lead),
                "first note at zero gets a negative silent movement lead-in");

    play.Configure(120.0, 0.0, {{0, 120.0}, {2000, 180.0}}, 1);
    constexpr double t0 = 100.0;
    ok &= Check(play.StartAtAudioSeconds(-2.0, t0) &&
                play.Playing() && !song.Playing() &&
                Close(play.Snapshot(t0).judgeSeconds, -2.0) &&
                Close(play.Snapshot(t0 + 0.5).judgeSeconds, -1.5),
                "silent preroll moves continuously while audio is not started");

    play.Update(t0 + 2.0);
    ok &= Check(song.Playing() &&
                Close(play.AudioSeconds(t0 + 2.0), 0.0) &&
                Close(play.AudioSeconds(t0 + 2.5), 0.5),
                "silent preroll crosses song zero once, without a clock jump");
    ok &= Check(Close(play.AudioSeconds(t0 + 3.0), 1.0) &&
                Close(play.BeatAtAudioSeconds(1.0), 2.0),
                "100% song clock and first tempo segment");

    play.Pause(t0 + 3.0);
    ok &= Check(play.Paused() &&
                Close(play.AudioSeconds(t0 + 10.0), 1.0),
                "pause excludes wall-clock time");
    play.Resume(t0 + 10.0);
    ok &= Check(Close(play.AudioSeconds(t0 + 11.0), 2.0) &&
                Close(play.Beat(t0 + 11.0), 4.0),
                "resume preserves exact time across the BPM change");

    play.SetSpeed(0.5, t0 + 11.0);
    ok &= Check(Close(play.Speed(), 0.5) &&
                Close(play.AudioSeconds(t0 + 13.0), 3.0) &&
                Close(play.Beat(t0 + 13.0), 7.0),
                "50% practice rate advances one song second in two wall seconds");

    play.Pause(t0 + 13.0);
    play.SetSpeed(0.1, t0 + 13.0);
    ok &= Check(play.Paused() &&
                Close(play.AudioSeconds(t0 + 20.0), 3.0),
                "changing to 10% while paused never moves the playhead");
    play.Resume(t0 + 20.0);
    ok &= Check(Close(play.AudioSeconds(t0 + 21.0), 3.1) &&
                Close(play.Beat(t0 + 21.0), 7.3),
                "resume at 10% without resuming the old rate");

    ok &= Check(Close(app::CalibratedSongSeconds(2.0, 200, 1.0), 1.8) &&
                Close(app::CalibratedSongSeconds(2.0, 200, 0.5), 1.9) &&
                Close(app::CalibratedSongSeconds(2.0, -200, 0.1), 2.02),
                "global output latency has one explicit sign and practice-rate scale");

    ok &= Check(play.StartAtAudioSeconds(1.5, t0 + 30.0) &&
                Close(play.AudioSeconds(t0 + 30.0), 1.5) &&
                Close(play.AudioSeconds(t0 + 32.0), 1.7),
                "explicit seek is one discontinuity followed by a continuous clock");
    play.SetSpeed(1.0, t0 + 32.0);
    play.Update(t0 + 35.0);
    ok &= Check(play.Finished() &&
                Close(play.AudioSeconds(t0 + 35.0), 4.0),
                "song end transitions to finished clock at exact duration");

    MissingOutput missingDevice;
    app::LocalMusic missing(missingDevice);
    ok &= Check(!missing.LoadFromFile(file.wav.wstring()) &&
                !missing.Ready() && !missing.Error().empty(),
                "no audio output device fails cleanly, not with a phantom clock");

    return ok ? 0 : 1;
}
