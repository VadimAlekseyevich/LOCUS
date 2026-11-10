#include "app/song_library.h"
#include "app/song_set_storage.h"
#include "app/chart_file.h"

#include <algorithm>
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
        "\"formatVersion\":3,"
        "\"title\":\"" + name + "\","
        "\"audioPath\":\"song.wav\","
        "\"bpm\":120,\"offsetMs\":0,"
        "\"ticksPerBeat\":1,\"snapDivision\":1,"
        "\"grid\":{\"width\":8,\"height\":8},"
        "\"start\":{\"x\":3,\"y\":3,\"heading\":\"r\"},"
        "\"previewLeadBeats\":4,"
        "\"turns\":[{\"timeMs\":1000,\"heading\":\"u\"}]"
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
                metadata.bpm && *metadata.bpm == 120.0 &&
                metadata.offsetMs && *metadata.offsetMs == 0 &&
                sharedAudio == library.Songs()[0].charts[0].audioFile &&
                sharedAudio == library.Songs()[0].charts[1].audioFile &&
                Read(sharedAudio) == firstBytes,
                "manifest and all difficulties refer to one immutable audio");
    SongSetManifest unicodeMetadata = metadata;
    unicodeMetadata.title = u8"Песня — Ночной город";
    unicodeMetadata.artist = u8"Исполнитель Ёж";
    const fs::path unicodeManifest = folder / "unicode-set.json";
    SongSetManifest unicodeLoaded;
    ok &= Check(WriteNewSongSetManifest(unicodeManifest, unicodeMetadata, error) &&
                LoadSongSetManifest(unicodeManifest, unicodeLoaded, error) &&
                unicodeLoaded.title == unicodeMetadata.title &&
                unicodeLoaded.artist == unicodeMetadata.artist,
                "Song Set title and artist round-trip UTF-8 without corruption");
    fs::remove(unicodeManifest, ec);
    // Menu listening start is SONG SET metadata, not a difficulty's
    // BPM/offset or note timestamps. It persists and survives ZIP export.
    const auto originalChartBytes = Read(easyInstalled);
    ok &= Check(metadata.previewStartMs == 0 &&
                library.Songs()[0].previewStartMs == 0,
                "old/new Song Sets default preview start to time zero");
    ok &= Check(library.SetPreviewStartMs(0, 12500, error) &&
                library.Songs()[0].previewStartMs == 12500 &&
                LoadSongSetManifest(
                    easyInstalled.parent_path() / "set.locus.json",
                    metadata, error) &&
                metadata.previewStartMs == 12500 &&
                Read(easyInstalled) == originalChartBytes,
                "preview timestamp is atomic and does not rewrite chart timing");
    // A song's BPM/offset are single manifest properties. All difficulties
    // are reprojected in memory by exact note timestamps, and raw old chart
    // files remain byte-for-byte unchanged (recoverable if migration fails).
    ok &= Check(library.SetSongTiming(0, 150.0, 175, nullptr, error) &&
                library.Songs()[0].bpm == 150.0 &&
                library.Songs()[0].offsetMs == 175 &&
                library.Songs()[0].charts[0].data.route.bpm == 150.0 &&
                library.Songs()[0].charts[1].data.route.bpm == 150.0 &&
                library.Songs()[0].charts[0].data.offsetSeconds == 0.175 &&
                library.Songs()[0].charts[1].data.offsetSeconds == 0.175 &&
                locus::core::EventTimeMs(
                    library.Songs()[0].charts[0].data.route,
                    library.Songs()[0].charts[0].data.route.turns[0],
                    library.Songs()[0].charts[0].data.offsetSeconds) == 1000 &&
                Read(easyInstalled) == originalChartBytes,
                "shared BPM and offset preserve note times and authored files");
    ok &= Check(LoadSongSetManifest(
                    easyInstalled.parent_path() / "set.locus.json",
                    metadata, error) && metadata.bpm &&
                *metadata.bpm == 150.0 && metadata.offsetMs &&
                *metadata.offsetMs == 175,
                "Song Set manifest persists canonical timing once");
    SongLibrary reopened(root);
    ok &= Check(reopened.Refresh(note) &&
                reopened.Songs()[0].bpm == 150.0 &&
                reopened.Songs()[0].offsetMs == 175 &&
                reopened.Songs()[0].previewStartMs == 12500,
                "preview position survives app restart / library rescan");
    ok &= Check(!library.SetPreviewStartMs(0, -100, error) &&
                library.Songs()[0].previewStartMs == 12500 &&
                !library.SetPreviewStartMs(0, kMaxSongPreviewStartMs + 1, error),
                "out of range preview time does not mutate metadata");
    // Invalid preview metadata is rejected without modifying output object.
    const fs::path badPreviewFile = folder / "bad-preview.json";
    Write(badPreviewFile,
          R"({"formatVersion":1,"setId":"one","title":"Test",)"
          R"("artist":"","audioFile":"song.wav","previewStartMs":-1})");
    SongSetManifest sentinel = metadata;
    ok &= Check(!LoadSongSetManifest(badPreviewFile, sentinel, error) &&
                sentinel.previewStartMs == 12500,
                "invalid preview position rejected atomically");
    fs::remove(badPreviewFile, ec);

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

    const auto beforeArtworkBytes = Read(easyInstalled);
    // Image assets are independent of chart JSONs, shared by all difficulties,
    // and never permitted to overwrite their source audio.
    const fs::path sourceCover = source / "cover.png";
    const unsigned char imageHeader[] = {
        137,80,78,71,13,10,26,10,0,0,0,13,
        73,72,68,82,0,0,0,2,0,0,0,2,8,2,0,0,0,253,212,154,115,
        0,0,0,22,73,68,65,84,120,156,99,108,144,75,97,96,96,96,
        98,96,96,96,96,96,0,0,12,94,1,6,140,139,168,168,
        0,0,0,0,73,69,78,68,174,66,96,130
    };
    Write(sourceCover, std::string(
        reinterpret_cast<const char*>(imageHeader), sizeof imageHeader));
    ok &= Check(library.SetBackgroundImage(0, sourceCover, error) &&
                !library.Songs()[0].backgroundImage.empty() &&
                fs::exists(library.Songs()[0].backgroundImage) &&
                Read(library.Songs()[0].backgroundImage) == Read(sourceCover) &&
                LoadSongSetManifest(easyInstalled.parent_path() /
                                    "set.locus.json", metadata, error) &&
                !metadata.backgroundFile.empty() &&
                Read(easyInstalled) == beforeArtworkBytes,
                "background image installs without touching chart timing");
    SongLibrary artReloaded(root);
    ok &= Check(artReloaded.Refresh(note) &&
                artReloaded.Songs()[0].backgroundImage ==
                  library.Songs()[0].backgroundImage,
                "image reference persists across library scan");
    const auto activeArtwork = library.Songs()[0].backgroundImage;
    ok &= Check(!library.SetBackgroundImage(0, audio, error) &&
                library.Songs()[0].backgroundImage == activeArtwork,
                "invalid image leaves existing artwork unchanged");
    const fs::path forgedManifest = folder / "unsafe-art.json";
    Write(forgedManifest,
          R"({"formatVersion":1,"setId":"one","title":"Test",)"
          R"("artist":"","audioFile":"song.wav","backgroundFile":"../secret.png"})");
    SongSetManifest safe = metadata;
    ok &= Check(!LoadSongSetManifest(forgedManifest, safe, error) &&
                safe.backgroundFile == metadata.backgroundFile,
                "manifest rejects path traversal without modifying output");
    fs::remove(forgedManifest, ec);

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
                copiedLibrary.Songs()[0].previewStartMs == 12500 &&
                fs::exists(copiedLibrary.Songs()[0].backgroundImage) &&
                Read(copiedLibrary.Songs()[0].backgroundImage) == Read(sourceCover) &&
                Read(importedFolder / "song.wav") == firstBytes,
                "ZIP import restores manifest, audio and four difficulties");
    ok &= Check(copiedLibrary.SetBackgroundImage(0, {}, error) &&
                copiedLibrary.Songs()[0].backgroundImage.empty() &&
                !fs::exists(importedFolder / fs::u8path(metadata.backgroundFile)) &&
                copiedLibrary.ChartCount() == 4 &&
                Read(importedFolder / "song.wav") == firstBytes,
                "removing background cleans its owned copy without affecting the song");
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
    // Alphabetical order would put AAA first. Difficulty order instead
    // puts the much denser pattern last in both PLAY and EDIT -> SET.
    const fs::path denseSource = source / "dense.locus.json";
    std::string denseJson = Read(easy);
    const auto replaceDense = [&](const std::string& before,
                                  const std::string& after) {
        const auto at = denseJson.find(before);
        if (at != std::string::npos) denseJson.replace(at, before.size(), after);
    };
    replaceDense("\"title\":\"Easy\"", "\"title\":\"AAA Dense\"");
    replaceDense("\"ticksPerBeat\":1", "\"ticksPerBeat\":24");
    replaceDense("\"snapDivision\":1", "\"snapDivision\":24");
    replaceDense("\"turns\":[{\"timeMs\":1000,\"heading\":\"u\"}]",
                 "\"turns\":[{\"timeMs\":1000,\"heading\":\"u\"},"
                 "{\"timeMs\":1100,\"heading\":\"r\"},"
                 "{\"timeMs\":1200,\"heading\":\"d\"},"
                 "{\"timeMs\":1300,\"heading\":\"l\"}]");
    Write(denseSource, denseJson);
    fs::path denseInstalled;
    const bool denseAdded = library.Import(denseSource, denseInstalled, error);
    if (!denseAdded) std::cerr << "DENSE_IMPORT_ERROR " << error << '\n';
    const auto& sorted = library.Songs()[0].charts;
    ok &= Check(denseAdded && sorted.size() == 5 &&
                sorted.front().file != denseInstalled &&
                sorted.back().file == denseInstalled &&
                sorted.back().difficulty.overall >
                    sorted.front().difficulty.overall,
                "Song Set difficulties default to ascending calculated difficulty");

    // A no-op rescan must reuse the cached difficulty and chart parse.
    const std::size_t chartTotal = library.ChartCount();
    ok &= Check(library.Refresh(note) &&
                library.LastRefreshStats().parsedCharts == 0 &&
                library.LastRefreshStats().reusedCharts == chartTotal &&
                library.LastRefreshStats().calculatedDifficulties == 0,
                "unchanged library refresh reuses every chart and difficulty");

    ok &= Check(library.SetPreviewStartMs(0, 21000, error) &&
                library.Refresh(note) &&
                library.Songs()[0].previewStartMs == 21000 &&
                library.LastRefreshStats().calculatedDifficulties == 0,
                "manifest-only preview change does not re-rate unchanged charts");

    // External modification of one difficulty invalidates exactly that
    // cached file; the other songs/difficulties remain cached.
    const auto densePosition = std::find_if(
        library.Songs()[0].charts.begin(), library.Songs()[0].charts.end(),
        [&](const LibraryChart& item) { return item.file == denseInstalled; });
    if (densePosition == library.Songs()[0].charts.end()) {
        ok &= Check(false, "dense chart remains indexed before cache test");
    } else {
        core::LoadedChart editedDense = densePosition->data;
        editedDense.title += " Renamed Externally";
        ok &= Check(SaveChartWithAudioPath(
                        denseInstalled.wstring(), densePosition->audioFile.wstring(),
                        editedDense, error, true) &&
                    library.Refresh(note) &&
                    library.LastRefreshStats().parsedCharts == 1 &&
                    library.LastRefreshStats().reusedCharts == chartTotal - 1 &&
                    library.LastRefreshStats().calculatedDifficulties == 1,
                    "editing a single chart invalidates only that chart cache");
    }

    fs::remove_all(folder, ec);
    if (!ok) return 1;
    std::cout << "SONG_SET_PASS shared-audio=1 copies=1 backups=1 trash=1\n";
    return 0;
}
