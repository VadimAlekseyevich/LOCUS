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

    // Verify a normalized traversal attempt is rejected before any file access.
    const bool blocked = !locus::core::ValidChartAudioPath("../another-song.wav");
    fs::remove_all(root, errorCode);
    if (!correct || !blocked) {
        std::cerr << "CHART_FILE_FAIL " << message << '\n';
        return 1;
    }
    std::cout << "CHART_FILE_PASS relative_audio=1 turns=2\n";
#else
    std::cout << "CHART_FILE_PASS Windows filesystem test skipped\n";
#endif
    return 0;
}
