#include "app/chart_file.h"
#include "core/chart_json.h"
#include "core/demo_audio.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main() {
#ifdef _WIN32
    namespace fs = std::filesystem;
    const auto unique = std::to_string(std::chrono::steady_clock::now()
                                      .time_since_epoch().count());
    const fs::path root = fs::temp_directory_path() / ("locus-v1-" + unique);
    const fs::path audioDirectory = root / "audio";
    std::error_code errorCode;
    fs::create_directories(audioDirectory, errorCode);
    if (errorCode) return 1;

    const fs::path audio = audioDirectory / "sample.wav";
    const auto wav = locus::core::MakeDemoClickWav(120.0, 8);
    {
        std::ofstream file(audio, std::ios::binary);
        file.write(reinterpret_cast<const char*>(wav.data()),
                   static_cast<std::streamsize>(wav.size()));
    }

    const fs::path chartFile = root / "sample.locus.json";
    {
        std::ofstream file(chartFile, std::ios::binary);
        file << R"({"formatVersion":1,"title":"Test File",
                     "audioPath":"audio/sample.wav",
                     "bpm":120,"offsetSeconds":0.25,
                     "grid":{"width":4,"height":4},
                     "start":{"x":0,"y":2,"heading":"left"},
                     "previewLeadBeats":3.5,
                     "turns":[{"beat":2,"turn":"right"},
                              {"beat":5,"turn":"left"}]})";
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
        parsed.route.turns.push_back({7, locus::core::Turn::Right});
        parsed.authoredLengthBeats = 8;
    }
    const bool savedOnce = opened && locus::app::SaveChartWithAudioPath(
        saved.wstring(), resolved, parsed, message);
    locus::core::LoadedChart reopened;
    std::wstring songAgain;
    const bool reloaded = savedOnce && locus::app::OpenChartWithAudioPath(
        saved.wstring(), reopened, songAgain, message) &&
        locus::core::PrepareChartForSong(reopened, 4.0, message);
    const bool roundTrip = reloaded &&
        reopened.title == "My edited chart" &&
        reopened.route.turns.size() == 3 &&
        reopened.route.lengthBeats == 8 &&
        reopened.route.turns[2].beat == 7 &&
        reopened.authoredLengthBeats == 8 &&
        reopened.offsetSeconds == -0.125 &&
        fs::equivalent(fs::path(songAgain), audio);

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
    if (!correct || !blocked || !roundTrip || !refusedOutside || !existedAfterError) {
        std::cerr << "CHART_FILE_FAIL " << message
                  << " opened=" << opened << " correct=" << correct
                  << " saved=" << savedOnce << " reloaded=" << reloaded
                  << " roundTrip=" << roundTrip
                  << " refusedOutside=" << refusedOutside
                  << " existedAfterError=" << existedAfterError
                  << " blocked=" << blocked << '\n';
        return 1;
    }
    std::cout << "CHART_FILE_PASS relative_audio=1 turns=3 save_reopen=1\n";
#else
    std::cout << "CHART_FILE_PASS Windows filesystem test skipped\n";
#endif
    return 0;
}
