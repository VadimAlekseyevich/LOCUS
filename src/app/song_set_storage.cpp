#include "app/song_set_storage.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
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
        name.find('/') != std::string::npos ||
        name.find('\\') != std::string::npos ||
        name.find(':') != std::string::npos) return false;
    for (unsigned char c : name) if (c < 32 || c == 127) return false;
    return true;
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
        data["formatVersion"] != 1) {
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
        loaded.title.empty() || loaded.title.size() > 1024 ||
        loaded.artist.size() > 1024 || !SafeAudioFilename(loaded.audioFile)) {
        error = "Song Set metadata has missing or invalid fields.";
        return false;
    }
    manifest = std::move(loaded);
    error.clear();
    return true;
}

bool WriteNewSongSetManifest(const fs::path& path,
                             const SongSetManifest& manifest, std::string& error) {
    if (manifest.id.empty() || manifest.title.empty() ||
        !SafeAudioFilename(manifest.audioFile)) {
        error = "Invalid new Song Set metadata.";
        return false;
    }
    std::error_code ec;
    if (fs::exists(path, ec)) {
        error = "Song Set metadata already exists.";
        return false;
    }
    const fs::path stage = path.wstring() + UniqueSuffix();
    const nlohmann::json data = {
        {"formatVersion", 1}, {"setId", manifest.id}, {"title", manifest.title},
        {"artist", manifest.artist}, {"audioFile", manifest.audioFile}
    };
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

bool RecycleSongFile(const fs::path& file, std::string& error) {
#ifdef _WIN32
    std::error_code ec;
    const auto full = fs::canonical(file, ec);
    if (ec) { error = "Cannot resolve selected item."; return false; }
    std::wstring names = full.wstring();
    names.push_back(L'\0');
    names.push_back(L'\0');
    SHFILEOPSTRUCTW operation{};
    operation.wFunc = FO_DELETE;
    operation.pFrom = names.c_str();
    operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION |
                       FOF_SILENT | FOF_NOERRORUI;
    const int code = SHFileOperationW(&operation);
    if (code != 0 || operation.fAnyOperationsAborted) {
        error = "Windows could not move the item to the Recycle Bin.";
        return false;
    }
    error.clear();
    return true;
#else
    (void)file;
    error = "Recycle Bin deletion requires Windows.";
    return false;
#endif
}

} // namespace locus::app
