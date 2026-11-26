#include "app/song_set_storage.h"
#include "app/atomic_move.h"
#include "app/path_containment.h"
#include "app/atomic_text_file.h"
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
#include <set>
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
    std::error_code ec;
    if (MovePathNoReplace(staged, dest, ec)) return true;
    error = "Cannot publish the new file without overwrite: " + ec.message();
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

// Call while holding the Song Set folder writer lock.
bool WriteNewSongSetManifest(const fs::path& path,
                             const SongSetManifest& manifest, std::string& error) {
    std::string encoded;
    if (!EncodeSongSetManifest(manifest, encoded, error)) return false;
    const auto published = AtomicCreateTextFile(path, encoded, error);
    if (published == AtomicTextWriteStatus::AlreadyExists) {
        error = "Song Set metadata already exists.";
        return false;
    }
    return published == AtomicTextWriteStatus::Published;
}

// Call while holding the Song Set folder writer lock.
bool ReplaceSongSetManifest(const fs::path& path,
                            const SongSetManifest& manifest, std::string& error) {
    std::string encoded;
    if (!EncodeSongSetManifest(manifest, encoded, error)) return false;
    std::error_code ec;
    if (!fs::is_regular_file(path, ec) || ec) {
        error = "Song Set manifest is missing.";
        return false;
    }
    return AtomicReplaceTextFile(path, encoded, error);
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
    // Require actual canonical containment, not a textual '..' prefix.
    // Reparse points/junctions and symlinks may resolve outside this root.
    if (!IsPathWithinDirectory(libraryRoot, item)) {
        error = "Cannot trash an item outside the library.";
        return false;
    }
    const fs::path relative = item.lexically_relative(libraryRoot);
    if (relative.empty() || relative.is_absolute()) {
        error = "Cannot construct a safe Trash recovery path.";
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
    // The recovery journal must be durable BEFORE payload is moved.
    // A plain ofstream::flush only reaches the OS cache, not necessarily
    // stable storage; reuse the same flushed, exclusive atomic writer.
    const nlohmann::json record = {
        {"formatVersion", 1},
        {"original", relative.generic_u8string()}
    };
    std::string journalError;
    if (AtomicCreateTextFile(entry / "origin.json",
                             record.dump(2) + "\n", journalError) !=
        AtomicTextWriteStatus::Published) {
        fs::remove_all(entry, ec);
        error = "Cannot persist Trash recovery metadata: " + journalError;
        return false;
    }
    MovePathNoReplace(item, entry / L"payload", ec);
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
        if (it->is_symlink(ec)) continue;
        if (ec) break;
        const fs::path payload = it->path() / L"payload";
        if (it->is_directory(ec) &&
            it->path().filename().wstring().rfind(L"item-", 0) == 0 &&
            fs::exists(payload, ec) && !fs::is_symlink(payload, ec))
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
        if (!IsPathWithinDirectory(libraryRoot, destination)) {
            error = "Trash restore destination resolves outside the library.";
            return false;
        }
        MovePathNoReplace(entry / L"payload", destination, ec);
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
