#include "app/song_library.h"
#include "app/song_set_storage.h"
#include "app/chart_file.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

using namespace locus::app;

namespace {
namespace fs = std::filesystem;

bool Check(bool condition, const char* label) {
    if (!condition) std::cerr << "SONG_SET_FAIL " << label << '\n';
    return condition;
}
std::string Read(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream),
                       std::istreambuf_iterator<char>());
}
void Write(const fs::path& file, const std::string& data) {
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    stream << data;
}
bool WriteChart(const fs::path& file, const std::string& name) {
    const std::string json =
        "{"
        "\"formatVersion\":2,"
        "\"title\":\"" + name + "\","
        "\"audioPath\":\"song.wav\","
        "\"bpm\":120,\"offsetMs\":0,"
        "\"ticksPerBeat\":1,\"snapDivision\":1,"
        "\"grid\":{\"width\":8,\"height\":8},"
        "\"start\":{\"x\":3,\"y\":3,\"heading\":\"right\"},"
        "\"previewLeadBeats\":4,"
        "\"turns\":[{\"timeMs\":1000,\"turn\":\"left\"}]"
        "}";
    Write(file, json);
    return fs::exists(file) && Read(file) == json;
}
} // namespace

int main() {
    bool ok = true;
    const auto unique = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path folder = fs::temp_directory_path() /
                            ("locus-song-sets-test-" + unique);
    const fs::path source = folder / "sources" / "My Song";
    const fs::path root = folder / "user" / "Songs";
    std::error_code ec;
    fs::create_directories(source, ec);
    if (ec) return 1;
    const fs::path audio = source / "song.wav";
    const std::string firstBytes = "RIFF-song-set-test-audio-v1";
    Write(audio, firstBytes);
    const fs::path easy = source / "easy.locus.json";
    const fs::path hard = source / "hard.locus.json";
    ok &= Check(WriteChart(easy, "Easy") && WriteChart(hard, "Hard"),
                "create two difficulties referencing one audio");

    SongLibrary library(root);
    std::string note, error;
    ok &= Check(library.Refresh(note) && library.Songs().empty(),
                "empty library can be indexed");

    fs::path easyInstalled, hardInstalled;
    const bool importedEasy = library.Import(easy, easyInstalled, error);
    if (!importedEasy) std::cerr << "IMPORT_ERROR " << error << '\n';
    ok &= Check(importedEasy && fs::exists(easyInstalled) &&
                library.ChartCount() == 1 && library.Songs().size() == 1 &&
                library.Songs()[0].managed &&
                fs::exists(easyInstalled.parent_path() / "set.locus.json"),
                "import creates a managed Song Set with metadata");
    const bool importedHard = library.Import(hard, hardInstalled, error);
    ok &= Check(importedHard && library.Songs().size() == 1 &&
                library.ChartCount() == 2 &&
                library.Songs()[0].charts[0].data.title == "Easy" &&
                library.Songs()[0].charts[1].data.title == "Hard",
                "two difficulties are grouped in one Song Set");
    const fs::path sharedAudio = library.Songs()[0].audioFile;
    SongSetManifest metadata;
    ok &= Check(LoadSongSetManifest(
                    easyInstalled.parent_path() / "set.locus.json", metadata, error) &&
                !metadata.id.empty() && metadata.title == "song" &&
                metadata.audioFile == "song.wav" &&
                sharedAudio == library.Songs()[0].charts[0].audioFile &&
                sharedAudio == library.Songs()[0].charts[1].audioFile &&
                Read(sharedAudio) == firstBytes,
                "manifest and all difficulties refer to one immutable audio");
    // A hostile or malformed package manifest cannot use a chart JSON as
    // its alleged shared audio file.
    const fs::path invalidMetadata = folder / "unsafe-set.json";
    Write(invalidMetadata, R"({"formatVersion":1,"setId":"test",)"
        R"("title":"Hostile","artist":"","audioFile":"set.locus.json"})");
    SongSetManifest invalidSet;
    ok &= Check(!LoadSongSetManifest(invalidMetadata, invalidSet, error),
                "manifest cannot point its audioFile at JSON");
    fs::remove(invalidMetadata, ec);

    fs::path repeated;
    ok &= Check(library.Import(easy, repeated, error) &&
                repeated == easyInstalled && library.ChartCount() == 2,
                "repeated import updates same difficulty instead of duplicating");

    // A change to source audio must NOT silently replace bytes referenced
    // by any existing difficulty.
    Write(audio, "RIFF-different-audio-never-overwrite");
    ok &= Check(!library.Import(hard, repeated, error) &&
                error.find("different audio") != std::string::npos &&
                Read(sharedAudio) == firstBytes &&
                library.ChartCount() == 2,
                "changed audio is rejected without touching existing set");
    Write(audio, firstBytes);

    const locus::core::LoadedChart copiedChart = library.Songs()[0].charts[0].data;
    locus::core::LoadedChart newChart = copiedChart;
    newChart.title = "Insane";
    fs::path thirdInstalled;
    ok &= Check(library.CreateDifficulty(
                    audio, newChart, thirdInstalled, error,
                    easyInstalled.parent_path()) &&
                library.ChartCount() == 3 &&
                fs::exists(thirdInstalled),
                "new difficulty is written directly to existing Song Set");

    fs::path duplicate;
    ok &= Check(library.DuplicateDifficulty(easyInstalled, duplicate, error) &&
                library.ChartCount() == 4 &&
                duplicate != easyInstalled &&
                Read(duplicate).find("Easy Copy") != std::string::npos,
                "duplicate difficulty uses shared audio and a separate JSON");

    // Portable .locuspack is a real ZIP file containing manifest, shared
    // audio and two independent chart JSONs. Import into a fresh library
    // must not depend on the original source path or existing installation.
    const fs::path packageFile = folder / "song.locuspack";
    const fs::path copiedRoot = folder / "second-user" / "Songs";
    const fs::path corruptFile = folder / "corrupt.locuspack";
    const fs::path traversalFile = folder / "traversal.locuspack";
    fs::path importedFolder;
    SongLibrary copiedLibrary(copiedRoot);
    ok &= Check(library.ExportPackage(easyInstalled.parent_path(),
                                     packageFile, error) &&
                Read(packageFile).substr(0, 4) == std::string("PK\x03\x04", 4),
                "Song Set exported as portable ZIP");
    ok &= Check(copiedLibrary.Refresh(note) &&
                copiedLibrary.ImportPackage(packageFile, importedFolder, error) &&
                copiedLibrary.Songs().size() == 1 &&
                copiedLibrary.ChartCount() == 4 &&
                fs::exists(importedFolder / "set.locus.json") &&
                Read(importedFolder / "song.wav") == firstBytes,
                "ZIP import restores manifest, audio and four difficulties");
    fs::path duplicatePackage;
    ok &= Check(!copiedLibrary.ImportPackage(packageFile, duplicatePackage, error) &&
                copiedLibrary.ChartCount() == 4,
                "same package cannot overwrite already installed Song Set");
    std::string zipBytes = Read(packageFile);
    const auto payload = zipBytes.find(firstBytes);
    if (payload != std::string::npos) zipBytes[payload] ^= 1;
    Write(corruptFile, zipBytes);
    ok &= Check(payload != std::string::npos &&
                !copiedLibrary.ImportPackage(corruptFile, duplicatePackage, error) &&
                copiedLibrary.ChartCount() == 4,
                "bad ZIP payload CRC is rejected without changing library");
    zipBytes = Read(packageFile);
    const auto secondHeader = zipBytes.find(std::string("PK\x03\x04", 4), 4);
    const auto filename = secondHeader == std::string::npos
        ? std::string::npos : secondHeader + 30;
    const bool validFilename = filename != std::string::npos &&
        filename + 8 <= zipBytes.size() &&
        zipBytes.substr(filename, 8) == "song.wav";
    if (validFilename) zipBytes.replace(filename, 8, "../g.wav");
    Write(traversalFile, zipBytes);
    ok &= Check(validFilename &&
                !copiedLibrary.ImportPackage(traversalFile, duplicatePackage, error) &&
                copiedLibrary.ChartCount() == 4,
                "path traversal in ZIP entry names is rejected");

    // Normal Ctrl+S equivalent: overwrite chart and verify its previous bytes
    // were backed up before atomic publication.
    const auto before = Read(easyInstalled);
    newChart.title = "Easy Updated";
    ok &= Check(SaveChartWithAudioPath(easyInstalled.wstring(),
                                       sharedAudio.wstring(), newChart, error) &&
                Read(easyInstalled) != before &&
                Read(easyInstalled).find("Easy Updated") != std::string::npos,
                "overwriting a difficulty changes only its own file");
    bool backupFound = false;
    for (fs::directory_iterator it(easyInstalled.parent_path() /
                                    ".locus-history", ec), end;
         !ec && it != end; it.increment(ec)) {
        if (Read(it->path()) == before) backupFound = true;
    }
    ok &= Check(!ec && backupFound, "previous chart can be recovered from backup");

    // Writer lock must be exclusive across simultaneous editors/importers.
    {
        FolderWriteLock firstLock, secondLock;
        ok &= Check(firstLock.Acquire(easyInstalled.parent_path(), error) &&
                    !secondLock.Acquire(easyInstalled.parent_path(), error),
                    "a concurrent writer cannot acquire the same Song Set lock");
    }

    ok &= Check(library.TrashDifficulty(duplicate, error) &&
                !fs::exists(duplicate) && library.ChartCount() == 3,
                "deleting a difficulty preserves other difficulties and audio");
    ok &= Check(library.RestoreLastDeleted(error) &&
                fs::exists(duplicate) && library.ChartCount() == 4,
                "recover deleted difficulty from durable local Trash");

    const fs::path managedFolder = easyInstalled.parent_path();
    ok &= Check(library.TrashSongSet(managedFolder, error) &&
                !fs::exists(managedFolder) && library.Songs().empty(),
                "deleting Song Set moves entire folder to Trash");
    ok &= Check(library.RestoreLastDeleted(error) &&
                fs::exists(managedFolder) && library.ChartCount() == 4,
                "restore complete Song Set with all shared content");

    // Restart preserves a managed Song Set; deletion/restore did not rewrite
    // the audio or require any old legacy revision directories.
    SongLibrary restarted(root);
    ok &= Check(restarted.Refresh(note) && restarted.ChartCount() == 4 &&
                restarted.Songs().size() == 1 && restarted.Songs()[0].managed &&
                Read(sharedAudio) == firstBytes,
                "restart reconstructs Song Set and retains audio bytes");
    fs::remove_all(folder, ec);
    if (!ok) return 1;
    std::cout << "SONG_SET_PASS shared-audio=1 copies=1 backups=1 trash=1\n";
    return 0;
}
