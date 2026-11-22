#pragma once

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#include <windows.h>
#undef CloseWindow
#undef ShowCursor
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace locus::app {

// Publish one complete UTF-8 document, or leave the previous document intact.
// Each writer owns an exclusive staging file, even across processes. The caller
// creates the parent directory and validates the serialized content first.
inline bool AtomicReplaceTextFile(const std::filesystem::path& destination,
                                  std::string_view bytes, std::string& error) {
    namespace fs = std::filesystem;
    static std::atomic<std::uint64_t> sequence{0};
#ifdef _WIN32
    const auto pid = static_cast<unsigned long long>(GetCurrentProcessId());
#else
    const auto pid = static_cast<unsigned long long>(getpid());
#endif
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path stage;
    bool created = false;
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;
#else
    int handle = -1;
#endif
    for (int attempt = 0; attempt < 32; ++attempt) {
        stage = destination;
        const std::string suffix = ".tmp-" + std::to_string(pid) + "-" +
            std::to_string(stamp) + "-" + std::to_string(sequence.fetch_add(1));
        stage += fs::u8path(suffix).native();
#ifdef _WIN32
        handle = CreateFileW(stage.c_str(), GENERIC_WRITE, 0, nullptr,
                             CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle != INVALID_HANDLE_VALUE) { created = true; break; }
        if (GetLastError() != ERROR_FILE_EXISTS &&
            GetLastError() != ERROR_ALREADY_EXISTS) break;
#else
        handle = ::open(stage.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (handle >= 0) { created = true; break; }
        if (errno != EEXIST) break;
#endif
    }
    if (!created) {
        error = "Cannot exclusively create a temporary file for " +
                destination.filename().u8string() + ".";
        return false;
    }

    bool complete = true;
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        // Bounded writes also avoid narrowing a large buffer to DWORD/ssize_t.
        const std::size_t chunk = std::min<std::size_t>(bytes.size() - offset, 1 << 20);
#ifdef _WIN32
        DWORD written = 0;
        if (!WriteFile(handle, bytes.data() + offset,
                       static_cast<DWORD>(chunk), &written, nullptr) ||
            written == 0) { complete = false; break; }
        offset += written;
#else
        const ssize_t written = ::write(handle, bytes.data() + offset, chunk);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) { complete = false; break; }
        offset += static_cast<std::size_t>(written);
#endif
    }
#ifdef _WIN32
    if (complete && !FlushFileBuffers(handle)) complete = false;
    if (!CloseHandle(handle)) complete = false;
#else
    if (complete && ::fsync(handle) != 0) complete = false;
    if (::close(handle) != 0) complete = false;
#endif
    if (!complete) {
        std::error_code cleanup;
        fs::remove(stage, cleanup);
        error = "Cannot finish writing " + destination.filename().u8string() +
                "; the previous file was not changed.";
        return false;
    }

#ifdef _WIN32
    const bool published = MoveFileExW(stage.c_str(), destination.c_str(),
                             MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::error_code ec;
    fs::rename(stage, destination, ec);
    const bool published = !ec;
#endif
    if (!published) {
        std::error_code cleanup;
        fs::remove(stage, cleanup);
        error = "Cannot replace " + destination.filename().u8string() +
                "; the previous file was not changed.";
        return false;
    }
    error.clear();
    return true;
}

} // namespace locus::app
