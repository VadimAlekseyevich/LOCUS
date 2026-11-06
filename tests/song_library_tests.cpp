#include "app/song_library.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

using namespace locus::app;

namespace {
namespace fs = std::filesystem;
bool Check(bool condition, const char* label) {
    if (!condition) std::cerr << "SONG_LIBRARY_FAIL " << label << '\n';
    return condition;
}
bool WriteChart(const fs::path& file, const std::string& name) {
    const std::string json =
        "{\n"
        "  \"formatVersion\": 1,\n"
        "  \"title\": \"" + name + "\",\n"
        "  \"audioPath\": \"song.wav\",\n"
        "  \"bpm\": 120,\n"
        "  \"offsetMs\": 0,\n"
        "  \"grid\": {\"width\": 4, \"height\": 4},\n"
        "  \"start\": {\"x\": 1, \"y\": 1, \"heading\": \"right\"},\n"
        "  \"previewLeadBeats\": 4,\n"
        "  \"turns\": [{\"beat\": 2, \"turn\": \"left\"}]\n"
        "}\n";
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    stream << json;
    return static_cast<bool>(stream);
}
} // namespace

int main() {
    bool ok = true;
    const auto unique = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path folder = fs::temp_directory_path() /
                            ("locus-library-test-" + unique);
    const fs::path source = folder / "source" / "My Song";
    const fs::path installedRoot = folder / "user-data" / "Songs";
    std::error_code ec;
    fs::create_directories(source, ec);
    if (ec) {
        std::cerr << "Cannot create temporary source fixture\n";
        return 1;
    }
    {
        std::ofstream audio(source / "song.wav", std::ios::binary);
        audio << "RIFFtest-library-file"; // Scanner only checks file paths, not decoding.
    }
    const fs::path easy = source / "easy.locus.json";
    const fs::path hard = source / "hard.locus.json";
    ok &= Check(WriteChart(easy, "Easy") && WriteChart(hard, "Hard"),
                "create two chart files for the same audio");
    SongLibrary library(installedRoot);
    std::string notice;
    ok &= Check(library.Refresh(notice) && library.Songs().empty(),
                "empty persistent library indexes successfully");
    fs::path easyInstalled, hardInstalled;
    std::string error;
    const bool importedEasy = library.Import(easy, easyInstalled, error);
    if (!importedEasy) std::cerr << "IMPORT_EASY_ERROR " << error << '\n';
    ok &= Check(importedEasy && library.Songs().size() == 1 &&
                library.ChartCount() == 1 &&
                fs::exists(easyInstalled) && fs::exists(library.Songs()[0].audioFile),
                "import installs both chart and audio inside persistent Songs");
    const bool importedHard = library.Import(hard, hardInstalled, error);
    if (!importedHard) std::cerr << "IMPORT_HARD_ERROR " << error << '\n';
    ok &= Check(importedHard && library.Songs().size() == 1 &&
                library.ChartCount() == 2 &&
                library.Songs()[0].charts[0].data.title == "Easy" &&
                library.Songs()[0].charts[1].data.title == "Hard",
                "two difficulties sharing song audio are grouped together");
    fs::path again;
    ok &= Check(library.Import(easy, again, error) &&
                easyInstalled == again && library.ChartCount() == 2,
                "repeat import of the same chart does not create duplicates");

    ok &= Check(WriteChart(easy, "Expert") &&
                library.Import(easy, again, error) &&
                easyInstalled == again && library.ChartCount() == 2,
                "changing chart title updates the same installed difficulty");

    // A re-import of modified audio must replace the file after a successful
    // complete copy, without leaving temporary artifacts or losing charts.
    const fs::path installedAudio = library.Songs()[0].audioFile;
    {
        std::ofstream audio(source / "song.wav", std::ios::binary | std::ios::trunc);
        audio << "RIFFtest-library-file-reimported-with-new-content";
    }
    bool replaced = library.Import(hard, again, error);
    std::string bytes;
    {
        std::ifstream stream(installedAudio, std::ios::binary);
        bytes.assign(std::istreambuf_iterator<char>(stream),
                     std::istreambuf_iterator<char>());
    }
    bool noTemporary = true;
    for (const auto& entry : fs::directory_iterator(installedAudio.parent_path())) {
        if (entry.path().filename().wstring().find(L".tmp-") != std::wstring::npos)
            noTemporary = false;
    }
    ok &= Check(replaced && library.ChartCount() == 2 &&
                bytes == "RIFFtest-library-file-reimported-with-new-content" &&
                noTemporary,
                "changed audio reimport replaces full file without leftover temporary files");
    int songIndex = -1, chartIndex = -1;
    ok &= Check(library.FindChart(easyInstalled, songIndex, chartIndex) &&
                songIndex == 0 &&
                library.Songs()[songIndex].charts[chartIndex].data.title == "Expert",
                "in-memory chart index returns updated parsed chart data");
    fs::remove_all(source, ec);
    SongLibrary afterRestart(installedRoot);
    ok &= Check(afterRestart.Refresh(notice) &&
                afterRestart.Songs().size() == 1 &&
                afterRestart.ChartCount() == 2 &&
                fs::exists(afterRestart.Songs()[0].audioFile),
                "library survives process restart and deletion of imported originals");

    fs::remove_all(folder, ec);
    if (!ok) return 1;
    std::cout << "SONG_LIBRARY_PASS on-disk albums, grouped charts, cache, reimport\n";
    return 0;
}
