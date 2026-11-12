#include "app/chart_file.h"
#include "app/chart_filename.h"
#include "core/chart_json.h"
#include "core/demo_audio.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main() {
    // Save As suggests the authored chart title, not the audio filename.
    const bool filenames =
        locus::app::SuggestedChartFileName("rip me apart") ==
            L"rip me apart.locus.json" &&
        locus::app::SuggestedChartFileName("  New:Chart/Part?  ") ==
            L"New_Chart_Part_.locus.json" &&
        locus::app::SuggestedChartFileName("CON") == L"_CON.locus.json" &&
        locus::app::SuggestedChartFileName(" ... ") == L"Untitled.locus.json" &&
        locus::app::SuggestedChartFileName(
            u8"\u041C\u043E\u044F \u043F\u0435\u0441\u043D\u044F") ==
            L"\u041C\u043E\u044F \u043F\u0435\u0441\u043D\u044F.locus.json";
    if (!filenames) {
        std::cerr << "CHART_FILE_FAIL unsafe chart filename suggestion\n";
        return 1;
    }
#ifdef _WIN32
    namespace fs = std::filesystem;
    const auto unique = std::to_string(std::chrono::steady_clock::now()
                                      .time_since_epoch().count());
    const fs::path root = fs::temp_directory_path() / ("locus-v4-" + unique);
    const fs::path audioDirectory = root / "audio";
    std::error_code errorCode;
    fs::create_directories(audioDirectory, errorCode);
    if (errorCode) return 1;

    const fs::path audio = audioDirectory / "sample.wav";
    const auto wav = locus::core::MakeDemoClickWav(120.0, 64); // 32-second song
    {
        std::ofstream file(audio, std::ios::binary);
        file.write(reinterpret_cast<const char*>(wav.data()),
                   static_cast<std::streamsize>(wav.size()));
    }

    const fs::path chartFile = root / "sample.locus.json";
    {
        std::ofstream file(chartFile, std::ios::binary);
        file << R"({"formatVersion":4,"title":"Test File",
                     "audioPath":"audio/sample.wav",
                     "bpm":120,"offsetMs":250,
                     "tempoMap":[{"timeMs":250,"bpm":120}],
                     "grid":{"width":4,"height":4},
                     "start":{"x":0,"y":2,"heading":"l"},
                     "previewLeadBeats":3.5,
                     "turns":[{"timeMs":1250,"heading":"u"},
                              {"timeMs":2750,"heading":"r"}]})";
    }

    locus::core::LoadedChart parsed;
    std::wstring resolved;
    std::string message;
    const bool opened = locus::app::OpenChartWithAudioPath(
        chartFile.wstring(), parsed, resolved, message);
    const bool correct = opened &&
                         fs::equivalent(fs::path(resolved), audio) &&
                         locus::core::PrepareChartForSong(parsed, 4.0, message) &&
                         parsed.route.lengthBeats == 8 &&
                         parsed.route.turns.size() == 2 &&
                         parsed.route.start.x == 0;

    // M-04 real disk round-trip: save a changed chart next to the song,
    // overwrite it atomically, reopen and validate its duration and turns.
    const fs::path saved = root / "my-created.locus.json";
    if (opened) {
        parsed.title = "My edited chart";
        parsed.offsetSeconds = -0.125;
        // v4 charts keep the first tempo marker and offsetMs synchronized.
        // Moving the song offset must shift the authoritative tempo map too.
        parsed.route.tempoMap.front().timeMs = -125;
        parsed.route.turns.push_back({7, locus::core::Heading::Down});
    }
    const bool savedOnce = opened && locus::app::SaveChartWithAudioPath(
        saved.wstring(), resolved, parsed, message);
    const std::string saveError = savedOnce ? std::string{} : message;
    locus::core::LoadedChart reopened;
    std::wstring songAgain;
    const bool reloaded = savedOnce && locus::app::OpenChartWithAudioPath(
        saved.wstring(), reopened, songAgain, message) &&
        locus::core::PrepareChartForSong(reopened, 4.0, message);
    const bool roundTrip = reloaded &&
        reopened.title == "My edited chart" &&
        reopened.route.turns.size() == 3 &&
        reopened.route.firstBeat == 0 &&
        reopened.route.lengthBeats == 9 &&
        reopened.route.turns[2].beat == 7 &&
        reopened.offsetSeconds == -0.125 &&
        fs::equivalent(fs::path(songAgain), audio);

    // A dense but valid chart must not be writable and then impossible to
    // reload just because its pretty-printed JSON exceeds the old 1 MiB cap.
    locus::core::LoadedChart dense;
    dense.title = "Dense file round-trip";
    dense.route.bpm = 2000.0;
    dense.route.ticksPerBeat = 48;
    dense.route.lengthBeats = 52000;
    dense.route.start = {3, 3};
    for (int i = 1; i <= 20000; ++i)
        dense.route.turns.push_back({
            static_cast<int>(std::llround(i * 1.6)),
            locus::core::Heading::Up, i});
    const fs::path densePath = root / "dense.locus.json";
    const bool denseSaved = locus::app::SaveChartWithAudioPath(
        densePath.wstring(), audio.wstring(), dense, message);
    std::error_code sizeError;
    const auto denseBytes = fs::file_size(densePath, sizeError);
    locus::core::LoadedChart denseReopened;
    std::wstring denseAudio;
    const bool denseRoundTrip = denseSaved && !sizeError &&
        denseBytes > 1024U * 1024U &&
        denseBytes <= locus::app::kMaxChartFileBytes &&
        locus::app::OpenChartWithAudioPath(
            densePath.wstring(), denseReopened, denseAudio, message) &&
        denseReopened.route.turns.size() == 20000 &&
        locus::core::PrepareChartForSong(denseReopened, 32.0, message) &&
        fs::equivalent(fs::path(denseAudio), audio);

    // An impossible save location must not modify the original JSON.
    const fs::path sibling = fs::temp_directory_path() / ("locus-sibling-" + unique);
    fs::create_directories(sibling, errorCode);
    const fs::path outside = sibling / "outside.locus.json";
    const bool refusedOutside = !locus::app::SaveChartWithAudioPath(
        outside.wstring(), resolved, parsed, message) &&
        !fs::exists(outside);
    const bool existedAfterError = fs::exists(saved);

    // Verify a normalized traversal attempt is rejected before any file access.
    const bool blocked = !locus::core::ValidChartAudioPath("../another-song.wav");
    fs::remove_all(root, errorCode);
    fs::remove_all(sibling, errorCode);
    if (!correct || !blocked || !roundTrip || !denseRoundTrip ||
        !refusedOutside || !existedAfterError) {
        std::cerr << "CHART_FILE_FAIL save_error=" << saveError
                  << " last_error=" << message
                  << " opened=" << opened << " correct=" << correct
                  << " saved=" << savedOnce << " reloaded=" << reloaded
                  << " roundTrip=" << roundTrip
                  << " denseRoundTrip=" << denseRoundTrip
                  << " refusedOutside=" << refusedOutside
                  << " existedAfterError=" << existedAfterError
                  << " blocked=" << blocked << '\n';
        return 1;
    }
    std::cout << "CHART_FILE_PASS relative_audio=1 turns=3 save_reopen=1 dense_json=1\n";
#else
    std::cout << "CHART_FILE_PASS Windows filesystem test skipped\n";
#endif
    return 0;
}
