#pragma once

#include <filesystem>
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
#elif defined(__linux__)
#include <cerrno>
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace locus::app {

// Publish a staged path without ever replacing another file or directory.
// Unlike exists() + std::filesystem::rename(), this is one atomic namespace
// operation against competing processes. The caller still owns source on
// failure; both existing destinations and recovery entries remain untouched.
inline bool MovePathNoReplace(const std::filesystem::path& source,
                              const std::filesystem::path& destination,
                              std::error_code& error) noexcept {
#ifdef _WIN32
    if (MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH)) {
        error.clear();
        return true;
    }
    error = std::error_code(static_cast<int>(GetLastError()),
                            std::system_category());
    return false;
#elif defined(__linux__) && defined(SYS_renameat2)
    // Linux renameat2 also handles directories, unlike hard-link publication.
    if (::syscall(SYS_renameat2, AT_FDCWD, source.c_str(),
                  AT_FDCWD, destination.c_str(), RENAME_NOREPLACE) == 0) {
        error.clear();
        return true;
    }
    error = std::error_code(errno, std::generic_category());
    return false;
#else
    // Never fall back to rename() silently: its overwrite semantics violate
    // the recovery contract. Callers can report an unsupported filesystem.
    (void)source;
    (void)destination;
    error = std::make_error_code(std::errc::operation_not_supported);
    return false;
#endif
}

} // namespace locus::app
