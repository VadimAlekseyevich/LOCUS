#include "app/song_set_storage.h"
#include "core/tempo_map.h"
#include "core/utf8.h"
#include "app/image_asset_file.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace locus::app {
namespace {
namespace fs = std::filesystem;

bool SafeAudioFilename(const std::string& name) {
    if (name.empty() || name == "." || name == ".." || name.size() > 255 ||
        !core::utf8::Valid(name) ||
        name.find('/') != std::string::npos ||
        name.find('\\') != std::string::npos ||
        name.find(':') != std::string::npos) return false;
    for (unsigned char c : name) if (c < 32 || c == 127) return false;
    // Song Set metadata describes a real audio file, never a chart or the
    // manifest itself. Keep extension checks independent of locale.
    const auto dot = name.find_last_of('.');
    if (dot == std::string::npos) return false;
    std::string extension = name.substr(dot);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(
                       c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c); });
    return extension == ".wav" || extension == ".mp3";
}

std::wstring UniqueSuffix() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
#ifdef _WIN32
    return L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
           std::to_wstring(stamp);
#else
    return L".tmp-" + std::to_wstring(stamp);
#endif
}

bool PublishNewFile(const fs::path& staged, const fs::path& dest,
                    std::string& error) {
#ifdef _WIN32
    if (MoveFileExW(staged.c_str(), dest.c_str(), MOVEFILE_WRITE_THROUGH))
        return true;
    error = "Cannot publish the new file (Windows error " +
            std::to_string(GetLastError()) + ").";
#else
    std::error_code ec;
    fs::rename(staged, dest, ec);
    if (!ec) return true;
    error = "Cannot publish the new file: " + ec.message();
#endif
    return false;
}

} // namespace

FolderWriteLock::~FolderWriteLock() {
#ifdef _WIN32
    if (handle_) CloseHandle(static_cast<HANDLE>(handle_));
#else
    if (fd_ >= 0) { flock(fd_, LOCK_UN); close(fd_); }
#endif
}

bool FolderWriteLock::Acquire(const fs::path& folder, std::string& error) {
#ifdef _WIN32
    if (handle_) { error = "Folder lock is already owned."; return false; }
    const fs::path lockFile = folder / L".locus-lock";
    HANDLE handle = CreateFileW(lockFile.c_str(), GENERIC_READ | GENERIC_WRITE,
                                0, nullptr, OPEN_ALWAYS,
                                FILE_ATTRIBUTE_HIDDEN, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        error = "This song folder is being edited elsewhere or is not writable.";
        return false;
    }
    handle_ = handle;
#else
    if (fd_ >= 0) { error = "Folder lock is already owned."; return false; }
    const auto lockFile = folder / ".locus-lock";
    fd_ = open(lockFile.c_str(), O_CREAT | O_RDWR, 0600);
    if (fd_ < 0 || flock(fd_, LOCK_EX | LOCK_NB) != 0) {
        if (fd_ >= 0) { close(fd_); fd_ = -1; }
        error = "This song folder is being edited elsewhere or is not writable.";
        return false;
    }
#endif
    return true;
}

bool LoadSongSetManifest(const fs::path& path, SongSetManifest& manifest,
                         std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) { error = "Cannot open Song Set metadata."; return false; }
    const auto data = nlohmann::json::parse(file, nullptr, false);
    if (data.is_discarded() || !data.is_object() ||
        !data.contains("formatVersion") || !data["formatVersion"].is_number_integer() ||
        data["formatVersion"] != 2) {
        error = "Invalid Song Set metadata version or JSON.";
        return false;
    }
    const auto field = [&](const char* key, std::string& target) {
        if (!data.contains(key) || !data[key].is_string()) return false;
        target = data[key].get<std::string>();
        return true;
    };
    SongSetManifest loaded;
    if (!field("setId", loaded.id) || !field("title", loaded.title) ||
        !field("artist", loaded.artist) || !field("audioFile", loaded.audioFile) ||
        loaded.id.empty() || loaded.id.size() > 128 ||
        !core::utf8::ValidName(loaded.title, 256, 1024) ||
        (!loaded.artist.empty() &&
         !core::utf8::ValidName(loaded.artist, 256, 1024)) ||
        !SafeAudioFilename(loaded.audioFile)) {
        error = "Song Set metadata has missing or invalid fields.";
        return false;
    }
    if (data.contains("previewStartMs")) {
        if (!data["previewStartMs"].is_number_integer()) {
            error = "Song Set preview position must be an integer."; return false;
        }
        try { loaded.previewStartMs = data["previewStartMs"].get<int>(); }
        catch (const std::exception&) {
            error = "Song Set preview position is out of range."; return false;
        }
    }
    if (loaded.previewStartMs < 0 ||
        loaded.previewStartMs > kMaxSongPreviewStartMs) {
        error = "Song Set preview position is out of range."; return false;
    }
    // Current Song Set manifests always own one canonical song-wide map.
    if (!data.contains("bpm") || !data.contains("offsetMs") ||
        !data["bpm"].is_number() || !data["offsetMs"].is_number_integer()) {
        error = "Song Set BPM and offset are required with valid types.";
        return false;
    }
    try {
        loaded.bpm = data["bpm"].get<double>();
        loaded.offsetMs = data["offsetMs"].get<int>();
    } catch (const std::exception&) {
        error = "Song Set BPM/offset are out of range.";
        return false;
    }
    if (!std::isfinite(*loaded.bpm) || *loaded.bpm <= 0.0 ||
        *loaded.offsetMs < -3600000 || *loaded.offsetMs > 3600000) {
        error = "Song Set BPM/offset are invalid.";
        return false;
    }
    if (!data.contains("tempoMap") || !data["tempoMap"].is_array() ||
        data["tempoMap"].empty() || data["tempoMap"].size() > 256) {
        error = "Song Set tempoMap must contain 1 to 256 markers.";
        return false;
    }
    for (const auto& item : data["tempoMap"]) {
        if (!item.is_object() || item.size() != 2 ||
            !item.contains("timeMs") || !item.contains("bpm") ||
            !item["timeMs"].is_number_integer() || !item["bpm"].is_number()) {
            error = "Song Set tempoMap entry must contain timeMs and bpm.";
            return false;
        }
        loaded.tempoMap.push_back({item["timeMs"].get<std::int64_t>(),
                                   item["bpm"].get<double>()});
    }
    if (!core::ValidTempoMap(loaded.tempoMap, error)) return false;
    if (loaded.tempoMap.front().bpm != *loaded.bpm ||
        loaded.tempoMap.front().timeMs != *loaded.offsetMs) {
        error = "Song Set tempoMap first row disagrees with bpm/offsetMs.";
        return false;
    }

    if (data.contains("backgroundFile")) {
        if (!data["backgroundFile"].is_string()) {
            error = "Song Set backgroundFile must be a string.";
            return false;
        }
        loaded.backgroundFile = data["backgroundFile"].get<std::string>();
        if (!loaded.backgroundFile.empty() &&
            !SafeImageFilename(loaded.backgroundFile)) {
            error = "Song Set backgroundFile must be a safe PNG/JPEG filename.";
            return false;
        }
    }
    manifest = std::move(loaded);
    error.clear();
    return true;
}

bool WriteNewSongSetManifest(const fs::path& path,
                             const SongSetManifest& manifest, std::string& error) {
    if (manifest.id.empty() ||
        !core::utf8::ValidName(manifest.title, 256, 1024) ||
        (!manifest.artist.empty() &&
         !core::utf8::ValidName(manifest.artist, 256, 1024)) ||
        (!SafeAudioFilename(manifest.audioFile) ||
         manifest.previewStartMs < 0 ||
         manifest.previewStartMs > kMaxSongPreviewStartMs ||
         (!manifest.backgroundFile.empty() &&
          !SafeImageFilename(manifest.backgroundFile))) ||
        (manifest.bpm.has_value() != manifest.offsetMs.has_value()) ||
        (manifest.bpm && (!std::isfinite(*manifest.bpm) ||
                          *manifest.bpm <= 0.0 ||
                          *manifest.offsetMs < -3600000 ||
                          *manifest.offsetMs > 3600000)) ||
        (!manifest.tempoMap.empty() &&
         (manifest.tempoMap.front().bpm != manifest.bpm.value_or(-1.0) ||
          manifest.tempoMap.front().timeMs != manifest.offsetMs.value_or(-1)))) {
        error = "Invalid new Song Set metadata.";
        return false;
    }
    const std::vector<core::TempoMarker> tempo =
        manifest.tempoMap.empty()
            ? std::vector<core::TempoMarker>{{manifest.offsetMs.value_or(0),
                                               manifest.bpm.value_or(120.0)}}
            : manifest.tempoMap;
    if (!core::ValidTempoMap(tempo, error)) return false;
    std::error_code ec;
    if (fs::exists(path, ec)) {
        error = "Song Set metadata already exists.";
        return false;
    }
    const fs::path stage = path.wstring() + UniqueSuffix();
    nlohmann::json data = {
        {"formatVersion", 2}, {"setId", manifest.id}, {"title", manifest.title},
        {"artist", manifest.artist}, {"audioFile", manifest.audioFile},
        {"previewStartMs", manifest.previewStartMs},
        {"backgroundFile", manifest.backgroundFile}
    };
    data["bpm"] = tempo.front().bpm;
    data["offsetMs"] = tempo.front().timeMs;
    data["tempoMap"] = nlohmann::json::array();
    for (const auto& marker : tempo)
        data["tempoMap"].push_back({
            {"timeMs", marker.timeMs}, {"bpm", marker.bpm}});

    {
        std::ofstream out(stage, std::ios::binary | std::ios::trunc);
        if (!out) { error = "Cannot stage Song Set metadata."; return false; }
        out << data.dump(2) << '\n';
        out.flush();
        if (!out) {
            fs::remove(stage, ec);
            error = "Cannot write Song Set metadata.";
            return false;
        }
    }
    if (!PublishNewFile(stage, path, error)) {
        fs::remove(stage, ec);
        return false;
    }
    error.clear();
    return true;
}

// Call while holding the Song Set folder writer lock.
bool ReplaceSongSetManifest(const fs::path& path,
                            const SongSetManifest& manifest, std::string& error) {
    if (manifest.id.empty() ||
        !core::utf8::ValidName(manifest.title, 256, 1024) ||
        (!manifest.artist.empty() &&
         !core::utf8::ValidName(manifest.artist, 256, 1024)) ||
        !SafeAudioFilename(manifest.audioFile) ||
        manifest.previewStartMs < 0 ||
        manifest.previewStartMs > kMaxSongPreviewStartMs ||
        (!manifest.backgroundFile.empty() &&
         !SafeImageFilename(manifest.backgroundFile)) ||
        (manifest.bpm.has_value() != manifest.offsetMs.has_value()) ||
        (manifest.bpm && (!std::isfinite(*manifest.bpm) ||
                          *manifest.bpm <= 0.0 ||
                          *manifest.offsetMs < -3600000 ||
                          *manifest.offsetMs > 3600000)) ||
        (!manifest.tempoMap.empty() &&
         (manifest.tempoMap.front().bpm != manifest.bpm.value_or(-1.0) ||
          manifest.tempoMap.front().timeMs != manifest.offsetMs.value_or(-1)))) {
        error = "Invalid Song Set metadata."; return false;
    }
    const std::vector<core::TempoMarker> tempo =
        manifest.tempoMap.empty()
            ? std::vector<core::TempoMarker>{{manifest.offsetMs.value_or(0),
                                               manifest.bpm.value_or(120.0)}}
            : manifest.tempoMap;
    if (!core::ValidTempoMap(tempo, error)) return false;
    std::error_code ec;
    if (!fs::is_regular_file(path, ec) || ec) {
        error = "Song Set manifest is missing."; return false;
    }
    const fs::path stage = path.wstring() + UniqueSuffix();
    nlohmann::json data = {
        {"formatVersion", 2}, {"setId", manifest.id},
        {"title", manifest.title}, {"artist", manifest.artist},
        {"audioFile", manifest.audioFile},
        {"previewStartMs", manifest.previewStartMs},
        {"backgroundFile", manifest.backgroundFile}
    };
    data["bpm"] = tempo.front().bpm;
    data["offsetMs"] = tempo.front().timeMs;
    data["tempoMap"] = nlohmann::json::array();
    for (const auto& marker : tempo)
        data["tempoMap"].push_back({
            {"timeMs", marker.timeMs}, {"bpm", marker.bpm}});

    {
        std::ofstream out(stage, std::ios::binary | std::ios::trunc);
        if (!out) { error = "Cannot stage Song Set metadata."; return false; }
        out << data.dump(2) << '\n';
        out.flush();
        if (!out) {
            fs::remove(stage, ec);
            error = "Cannot write Song Set metadata."; return false;
        }
    }
#ifdef _WIN32
    if (!MoveFileExW(stage.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        fs::remove(stage, ec);
        error = "Cannot replace Song Set metadata."; return false;
    }
#else
    fs::rename(stage, path, ec);
    if (ec) {
        fs::remove(stage, ec);
        error = "Cannot replace Song Set metadata: " + ec.message();
        return false;
    }
#endif
    error.clear();
    return true;
}

bool SameFileBytes(const fs::path& a, const fs::path& b, bool& identical,
                   std::string& error) {
    identical = false;
    std::error_code ec;
    const auto aSize = fs::file_size(a, ec);
    if (ec) { error = "Cannot inspect source audio."; return false; }
    const auto bSize = fs::file_size(b, ec);
    if (ec) { error = "Cannot inspect installed audio."; return false; }
    if (aSize != bSize) return true;
    std::ifstream left(a, std::ios::binary), right(b, std::ios::binary);
    if (!left || !right) {
        error = "Cannot compare shared audio files.";
        return false;
    }
    std::array<char, 65536> aBlock{}, bBlock{};
    do {
        left.read(aBlock.data(), static_cast<std::streamsize>(aBlock.size()));
        right.read(bBlock.data(), static_cast<std::streamsize>(bBlock.size()));
        const auto count = left.gcount();
        if (count != right.gcount() ||
            !std::equal(aBlock.begin(), aBlock.begin() + count, bBlock.begin()))
            return true;
        if (count == 0) break;
    } while (left && right);
    if (left.bad() || right.bad()) {
        error = "Audio comparison failed.";
        return false;
    }
    identical = true;
    return true;
}

bool InstallSharedSongAudio(const fs::path& source, const fs::path& dest,
                            std::string& error) {
    std::error_code ec;
    if (fs::exists(dest, ec)) {
        bool identical = false;
        if (!SameFileBytes(source, dest, identical, error)) return false;
        if (!identical) {
            error = "This Song Set already has different audio. Import was stopped "
                    "to protect other difficulties.";
            return false;
        }
        error.clear();
        return true;
    }
    if (ec) { error = "Cannot inspect destination audio."; return false; }
    const fs::path stage = dest.wstring() + UniqueSuffix();
    if (!fs::copy_file(source, stage, fs::copy_options::none, ec) || ec) {
        fs::remove(stage, ec);
        error = "Cannot stage shared audio.";
        return false;
    }
    if (!PublishNewFile(stage, dest, error)) {
        // The other process may have published identical bytes in the meantime.
        bool same = false;
        std::string compareError;
        if (fs::exists(dest, ec) &&
            SameFileBytes(stage, dest, same, compareError) && same) {
            fs::remove(stage, ec);
            error.clear();
            return true;
        }
        fs::remove(stage, ec);
        return false;
    }
    error.clear();
    return true;
}

bool BackUpChartBeforeReplace(const fs::path& chart, std::string& error) {
    std::error_code ec;
    if (!fs::exists(chart, ec)) return !ec;
    if (!fs::is_regular_file(chart, ec) || ec) {
        error = "The existing chart is not a regular file.";
        return false;
    }
    const fs::path history = chart.parent_path() / L".locus-history";
    fs::create_directories(history, ec);
    if (ec) { error = "Cannot create chart backup directory."; return false; }
    const fs::path backup = history / (chart.filename().wstring() + UniqueSuffix() + L".bak");
    if (!fs::copy_file(chart, backup, fs::copy_options::none, ec) || ec) {
        error = "Cannot back up the previous chart; overwrite was canceled.";
        return false;
    }
    error.clear();
    return true;
}

bool TrashLibraryItem(const fs::path& libraryRoot,
                      const fs::path& item, std::string& error) {
    std::error_code ec;
    if (!fs::exists(item, ec) || ec) {
        error = "The selected library item no longer exists.";
        return false;
    }
    const fs::path relative = item.lexically_relative(libraryRoot);
    if (relative.empty() || relative.is_absolute() ||
        relative.native().rfind(L"..", 0) == 0) {
        error = "Cannot trash an item outside the library.";
        return false;
    }
    const fs::path trash = libraryRoot.parent_path() / L"Trash";
    fs::create_directories(trash, ec);
    if (ec) { error = "Cannot create recovery Trash folder."; return false; }
    // Human-readable, stable across restarts and lexically sortable newest-first.
    static std::atomic<unsigned> sequence{0};
    const auto stamp = std::chrono::system_clock::now().time_since_epoch();
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(stamp).count();
    const fs::path entry = trash /
        (L"item-" + std::to_wstring(milliseconds) + L"-" +
         std::to_wstring(sequence.fetch_add(1)) + UniqueSuffix());
    fs::create_directory(entry, ec);
    if (ec) { error = "Cannot allocate a Trash entry."; return false; }
    {
        const nlohmann::json record = {
            {"formatVersion", 1},
            {"original", relative.generic_u8string()}
        };
        std::ofstream journal(entry / "origin.json", std::ios::binary);
        if (!journal) {
            fs::remove_all(entry, ec);
            error = "Cannot write Trash recovery metadata.";
            return false;
        }
        journal << record.dump(2) << '\n';
        journal.flush();
        if (!journal) {
            journal.close();
            fs::remove_all(entry, ec);
            error = "Cannot finish Trash recovery metadata.";
            return false;
        }
    }
    fs::rename(item, entry / L"payload", ec);
    if (ec) {
        const auto why = ec.message();
        fs::remove_all(entry, ec);
        error = "Cannot move item to recoverable Trash: " + why;
        return false;
    }
    error.clear();
    return true;
}

bool RestoreLatestTrashItem(const fs::path& libraryRoot,
                            std::string& error) {
    std::error_code ec;
    const fs::path trash = libraryRoot.parent_path() / L"Trash";
    if (!fs::exists(trash, ec) || ec) {
        error = "Trash is empty.";
        return false;
    }
    std::vector<fs::path> entries;
    for (fs::directory_iterator it(trash, ec), end; !ec && it != end;
         it.increment(ec)) {
        if (it->is_directory(ec) &&
            it->path().filename().wstring().rfind(L"item-", 0) == 0 &&
            fs::exists(it->path() / L"payload", ec))
            entries.push_back(it->path());
    }
    if (ec) { error = "Cannot inspect Trash."; return false; }
    if (entries.empty()) { error = "Trash has no recoverable items."; return false; }
    std::sort(entries.rbegin(), entries.rend());
    for (const fs::path& entry : entries) {
        std::ifstream stream(entry / L"origin.json", std::ios::binary);
        if (!stream) continue;
        const auto data = nlohmann::json::parse(stream, nullptr, false);
        if (data.is_discarded() || !data.is_object() ||
            !data.contains("original") || !data["original"].is_string() ||
            !data.contains("formatVersion") || data["formatVersion"] != 1)
            continue;
        const fs::path relative = fs::u8path(data["original"].get<std::string>());
        if (relative.empty() || relative.is_absolute()) continue;
        bool unsafe = false;
        for (const auto& part : relative)
            if (part == L".." || part == L"." || part.empty()) unsafe = true;
        if (unsafe) continue;
        const fs::path destination = libraryRoot / relative;
        if (fs::exists(destination, ec)) {
            error = "Original file already exists; Trash is preserved.";
            return false;
        }
        if (ec) { error = "Cannot inspect restore destination."; return false; }
        fs::create_directories(destination.parent_path(), ec);
        if (ec) { error = "Cannot prepare restore folder."; return false; }
        fs::rename(entry / L"payload", destination, ec);
        if (ec) {
            error = "Cannot restore from Trash: " + ec.message();
            return false;
        }
        fs::remove_all(entry, ec);
        error.clear();
        return true;
    }
    error = "Trash metadata is invalid; no files were changed.";
    return false;
}

} // namespace locus::app
