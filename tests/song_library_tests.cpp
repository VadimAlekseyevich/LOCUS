#include "app/song_library.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

using namespace locus::app;

namespace {
namespace fs = std::filesystem;
bool Check(bool condition, const char* label) {
    if (!condition) std::cerr << "SONG_LIBRARY_FAIL " << label << '\n';
    return condition;
}
std::string ReadBytes(const fs::path& file) {
    std::ifstream stream(file, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream),
                       std::istreambuf_iterator<char>());
}
std::string SourcePathHash(const fs::path& path) {
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : fs::canonical(path).generic_u8string()) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
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

    // Every difficulty initially references the same immutable revision.
    const auto selectedAudio = [&](const fs::path& chartPath) {
        int songIndex = -1, chartIndex = -1;
        if (!library.FindChart(chartPath, songIndex, chartIndex)) return fs::path{};
        return library.Songs()[static_cast<std::size_t>(songIndex)]
            .charts[static_cast<std::size_t>(chartIndex)].audioFile;
    };
    const fs::path initialAudio = selectedAudio(easyInstalled);
    ok &= Check(!initialAudio.empty() &&
                initialAudio == selectedAudio(hardInstalled) &&
                library.Songs().size() == 1 &&
                library.Songs()[0].title == "song",
                "identical imports share one revision and one song browser entry");

    // Changed song bytes produce a NEW version and rebind only the updated
    // difficulty. Other charts cannot silently change during this operation.
    const std::string changedAudio = "RIFFtest-library-reimported-new-bytes";
    {
        std::ofstream audio(source / "song.wav", std::ios::binary | std::ios::trunc);
        audio << changedAudio;
    }
    const bool replaced = library.Import(hard, again, error);
    const fs::path hardRevision = selectedAudio(hardInstalled);
    ok &= Check(replaced && again == hardInstalled &&
                hardRevision != initialAudio &&
                library.Songs().size() == 1 && library.ChartCount() == 2 &&
                selectedAudio(easyInstalled) == initialAudio &&
                ReadBytes(initialAudio) == "RIFFtest-library-file" &&
                ReadBytes(hardRevision) == changedAudio,
                "changed audio publishes a new revision while old difficulty is stable");
    const auto repeatRevision = hardRevision;
    ok &= Check(library.Import(hard, again, error) &&
                selectedAudio(hardInstalled) == repeatRevision,
                "repeated import of identical bytes reuses the existing revision");

    // Deterministically simulate a failed JSON publication AFTER audio staging:
    // replace the installed chart file with a directory, then restore it.
    // A failure must not alter the old audio referenced by either difficulty.
    const std::string thirdAudio = "RIFFtest-library-third-version";
    {
        std::ofstream audio(source / "song.wav", std::ios::binary | std::ios::trunc);
        audio << thirdAudio;
    }
    const fs::path backupChart = hardInstalled.wstring() + L".test-backup";
    fs::rename(hardInstalled, backupChart, ec);
    const bool renamed = !ec;
    if (renamed) fs::create_directory(hardInstalled, ec);
    const bool blocked = renamed && !ec;
    bool importFailed = false;
    if (blocked) importFailed = !library.Import(hard, again, error);
    if (blocked) fs::remove(hardInstalled, ec);
    if (renamed) fs::rename(backupChart, hardInstalled, ec);
    ok &= Check(blocked && importFailed && !ec &&
                ReadBytes(hardRevision) == changedAudio &&
                ReadBytes(initialAudio) == "RIFFtest-library-file",
                "failed chart publication preserves all existing audio revisions");
    ok &= Check(library.Refresh(notice) &&
                selectedAudio(hardInstalled) == hardRevision &&
                selectedAudio(easyInstalled) == initialAudio &&
                library.ChartCount() == 2,
                "library refresh recovers unchanged difficulty references after failure");

    // Retry after the failed commit should reuse staged bytes safely.
    ok &= Check(library.Import(hard, again, error) &&
                ReadBytes(selectedAudio(hardInstalled)) == thirdAudio &&
                selectedAudio(easyInstalled) == initialAudio &&
                library.Songs().size() == 1,
                "retry publishes new audio only with a successful chart update");
    bool noTemporary = true;
    for (const auto& entry : fs::directory_iterator(
             selectedAudio(hardInstalled).parent_path().parent_path())) {
        if (entry.path().filename().wstring().find(L".stage-") != std::wstring::npos)
            noTemporary = false;
    }
    ok &= Check(noTemporary, "normal and failed imports leave no staging files");
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
                !afterRestart.Songs()[0].charts[0].audioFile.empty() &&
                !afterRestart.Songs()[0].charts[1].audioFile.empty() &&
                afterRestart.Songs()[0].charts[0].audioFile !=
                    afterRestart.Songs()[0].charts[1].audioFile,
                "library restart retains different difficulty/audio revisions");

    fs::remove_all(folder, ec);
    if (!ok) return 1;
    std::cout << "SONG_LIBRARY_PASS atomic-chart-version=1 grouped-revisions=1\n";
    return 0;
}
