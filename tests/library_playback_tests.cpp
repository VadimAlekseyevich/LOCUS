#include "app/library_playback.h"

#include "app/chart_playback.h"
#include "app/demo_transport.h"
#include "app/game_session.h"
#include "app/local_music.h"
#include "core/judgement.h"
#include "core/route.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

using namespace locus;

namespace {
bool Check(bool condition, const char* message) {
    if (!condition) std::cerr << "LIBRARY_PLAYBACK_FAIL " << message << '\n';
    return condition;
}
}

int main() {
    bool ok = true;
    app::LibraryChart difficulty;
    difficulty.file = L"song/easy.locus.json";
    difficulty.audioFile = L"song/song.wav";
    difficulty.data.route = core::MakeDemoChart();
    difficulty.data.title = "Selected difficulty";
    difficulty.data.audioPath = "song.wav";

    core::LoadedChart prepared;
    prepared.title = "UNCHANGED";
    std::string error;
    ok &= Check(app::PrepareLibraryDifficulty(difficulty, 30.0, prepared, error) &&
                error.empty() && prepared.title == "Selected difficulty" &&
                prepared.route.lengthBeats >= 32,
                "valid difficulty prepares independently of the audio device");

    const std::string previousTitle = prepared.title;
    ok &= Check(!app::PrepareLibraryDifficulty(difficulty, 0.0, prepared, error) &&
                !error.empty() && prepared.title == previousTitle,
                "invalid duration cannot partly replace prepared chart");

    app::LibraryChart outOfRange = difficulty;
    outOfRange.data.route.turns.push_back({500000, core::Heading::UpRight});
    ok &= Check(!app::PrepareLibraryDifficulty(outOfRange, 30.0, prepared, error) &&
                prepared.title == previousTitle && !error.empty(),
                "events outside actual song duration are rejected without mutation");

    app::LibraryChart missingAudio = difficulty;
    missingAudio.audioFile.clear();
    ok &= Check(!app::PrepareLibraryDifficulty(
                    missingAudio, 30.0, prepared, error) &&
                prepared.title == previousTitle && !error.empty(),
                "difficulty with no audio file cannot be published");

    ok &= Check(app::RequiresLibraryAudioLoad(difficulty, L"another.wav", true) &&
                app::RequiresLibraryAudioLoad(
                    difficulty, difficulty.audioFile.wstring(), false) &&
                !app::RequiresLibraryAudioLoad(
                    difficulty, difficulty.audioFile.wstring(), true),
                "decoded audio reuse is keyed to the selected Song Set audio");

    // The CTest runner deliberately does not create a physical audio device.
    // Decode failure must preserve previous game/editor session state.
    core::Chart activeChart = core::MakeDemoChart();
    const auto originalTurns = activeChart.turns.size();
    core::TurnJudge judge(activeChart);
    app::DemoTransport demo;
    app::LocalMusic song;
    app::ChartPlayback playback(song);
    const auto initialClock = playback.Snapshot(2.0);
    ok &= Check(!initialClock.playing && !initialClock.paused &&
                !initialClock.finished &&
                initialClock.judgeSeconds == initialClock.visualSeconds &&
                initialClock.practiceRate == 1.0,
                "stopped audio exposes one coherent judge/visual clock sample");
    playback.SetSpeed(0.5, 2.0);
    const auto slowerClock = playback.Snapshot(2.1);
    ok &= Check(!slowerClock.playing && slowerClock.practiceRate == 0.5 &&
                slowerClock.judgeSeconds == slowerClock.visualSeconds,
                "stopped slow transport has no phantom visual smoothing");
    playback.SetSpeed(1.0, 2.2);
    app::GameSession session(activeChart, judge, demo, playback);
    std::wstring loadedAudioPath = L"old-audio.wav";
    std::string chartTitle = "OLD CHART";
    bool customChart = false;
    session.SetTestOrigin({2.5, 5});
    const app::LibraryPlaybackContext context{
        song, loadedAudioPath, activeChart, chartTitle,
        playback, judge, session, customChart
    };
    ok &= Check(!app::ActivateLibraryDifficulty(
                    difficulty, context, 1.0, error) &&
                !error.empty() &&
                chartTitle == "OLD CHART" &&
                loadedAudioPath == L"old-audio.wav" &&
                activeChart.turns.size() == originalTurns &&
                !customChart && session.IsEditorTest() && !song.Ready(),
                "failed audio load preserves chart, playback and TEST origin");

    ok &= Check(!app::ActivateLibraryDifficulty(
                    missingAudio, context, 2.0, error) &&
                !error.empty() && chartTitle == "OLD CHART" &&
                loadedAudioPath == L"old-audio.wav" &&
                !customChart && session.IsEditorTest(),
                "missing revision cannot mutate current session");

    if (!ok) return 1;
    std::cout << "LIBRARY_PLAYBACK_PASS validation=1 audio-reuse=1 failure-atomic=1\n";
    return 0;
}
