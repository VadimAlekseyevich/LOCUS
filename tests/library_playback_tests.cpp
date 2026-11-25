#include "app/library_playback.h"

#include "app/chart_playback.h"
#include "app/demo_transport.h"
#include "app/game_session.h"
#include "app/local_music.h"
#include "app/chart_file.h"
#include "app/song_set_storage.h"
#include "app/song_timing.h"
#include "core/chart_revision.h"
#include "core/judgement.h"
#include "core/route.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <chrono>
#include <fstream>
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
    app::LibraryChart incomplete;
    incomplete.audioFile = L"song/song.wav";
    core::LoadedChart untouched;
    untouched.title = "UNCHANGED";
    std::string badError;
    ok &= Check(!app::PrepareLibraryDifficulty(incomplete, 30.0,
                                               untouched, badError) &&
                untouched.title == "UNCHANGED" && !badError.empty(),
                "unpublished metadata cannot be prepared as a playable chart");
    // Actual compact indexed fixture; invalid WAV bytes are intentional
    // to exercise transactional failure without a physical audio device.
    namespace fs=std::filesystem;
    const auto root=fs::temp_directory_path()/("locus-playback-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code ec;
    fs::create_directories(root,ec);
    if (ec) return 1;
    struct Cleanup { fs::path root; ~Cleanup() { std::error_code e; fs::remove_all(root,e); } } cleanup{root};
    const auto audio=root/"song.wav";
    { std::ofstream out(audio,std::ios::binary); out<<"invalid wav"; }
    const auto chartPath=root/"easy.locus.json";
    { std::ofstream out(chartPath,std::ios::binary);
      out<<R"({"formatVersion":4,"difficultyId":"0123456789abcdef0123456789abcdef","title":"Selected difficulty","audioPath":"song.wav","bpm":120,"offsetMs":0,"tempoMap":[{"timeMs":0,"bpm":120}],"ticksPerBeat":1,"snapDivision":1,"grid":{"width":8,"height":8},"start":{"x":3,"y":3,"heading":"r"},"previewLeadBeats":4,"turns":[{"timeMs":1000,"heading":"u"}]})";
    }
    app::SongSetManifest manifest;
    manifest.id="playback-fixture";
    manifest.title="Song";
    manifest.audioFile="song.wav";
    manifest.bpm=120.0;
    manifest.offsetMs=0;
    manifest.tempoMap={{0,120.0}};
    std::string error;
    if (!Check(app::WriteNewSongSetManifest(root/"set.locus.json",manifest,error),
               "fixture manifest saved")) return 1;
    app::LibraryChart difficulty;
    difficulty.file=chartPath;
    difficulty.songSetId=manifest.id;
    core::LoadedChart authored;
    std::wstring resolvedAudio;
    if (!Check(app::OpenChartWithAudioPath(chartPath.wstring(),authored,resolvedAudio,error,
                    nullptr,&difficulty.indexedContentFingerprint) &&
               app::ApplySongTiming(authored,manifest.tempoMap,error),
               "fixture file verified")) return 1;
    difficulty.audioFile=fs::path(resolvedAudio);
    difficulty.indexedFingerprintValid=true;
    difficulty.RefreshMetadata(authored);
    difficulty.gameplayRevision=core::ChartGameplayRevision(authored);

    core::LoadedChart prepared;
    prepared.title = "UNCHANGED";
    ok &= Check(app::PrepareLibraryDifficulty(difficulty, 30.0, prepared, error) &&
                error.empty() && prepared.title == "Selected difficulty" &&
                prepared.route.lengthBeats >= 32,
                "valid difficulty prepares independently of the audio device");

    const std::string previousTitle = prepared.title;
    ok &= Check(!app::PrepareLibraryDifficulty(difficulty, 0.0, prepared, error) &&
                !error.empty() && prepared.title == previousTitle,
                "invalid duration cannot partly replace prepared chart");

    ok &= Check(!app::PrepareLibraryDifficulty(difficulty, 0.5, prepared, error) &&
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
