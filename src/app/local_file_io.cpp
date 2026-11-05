#include "app/local_file_io.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <algorithm>
#include <new>
#include <string>
#include <vector>

namespace locus::app {
namespace {
constexpr std::size_t kMaxLocalSongBytes = 256U * 1024U * 1024U;
}

std::string DisplayLocalFileName(const std::wstring& path) {
    const std::size_t slash = path.find_last_of(L"\\/");
    std::wstring name = path.substr(slash == std::wstring::npos ? 0 : slash + 1);
    if (name.empty()) return "Local song";
    if (name.size() > 36) name = name.substr(0, 33) + L"...";
#ifdef _WIN32
    const int count = WideCharToMultiByte(CP_UTF8, 0, name.c_str(),
                                         static_cast<int>(name.size()), nullptr, 0,
                                         nullptr, nullptr);
    if (count <= 0) return "Local song";
    std::string utf8(static_cast<std::size_t>(count), '\0');
    const int converted = WideCharToMultiByte(CP_UTF8, 0, name.c_str(),
                                              static_cast<int>(name.size()),
                                              utf8.data(), count, nullptr, nullptr);
    return (converted == count) ? utf8 : "Local song";
#else
    std::string display;
    for (wchar_t c : name) {
        display.push_back((c >= 32 && c <= 126) ? static_cast<char>(c) : '?');
    }
    return display;
#endif
}

bool ReadLocalFileBytes(const std::wstring& path, std::vector<std::uint8_t>& bytes,
                        std::string& error) {
#ifdef _WIN32
    // CreateFileW retains full Unicode paths without relying on the ANSI locale.
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        error = "Cannot open that audio file. Check its location and permissions.";
        return false;
    }
    LARGE_INTEGER length{};
    const bool validLength = GetFileSizeEx(handle, &length) != 0 &&
                             length.QuadPart > 0 &&
                             static_cast<unsigned long long>(length.QuadPart) <=
                                 static_cast<unsigned long long>(kMaxLocalSongBytes);
    if (!validLength) {
        CloseHandle(handle);
        error = "File is empty or over 256 MiB. Try a smaller WAV/MP3.";
        return false;
    }
    try {
        bytes.resize(static_cast<std::size_t>(length.QuadPart));
    } catch (const std::bad_alloc&) {
        CloseHandle(handle);
        error = "Not enough memory to load that song.";
        return false;
    }
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const DWORD request = static_cast<DWORD>(
            std::min<std::size_t>(bytes.size() - offset, 1024U * 1024U));
        DWORD received = 0;
        if (!ReadFile(handle, bytes.data() + offset, request, &received, nullptr) ||
            received == 0) {
            CloseHandle(handle);
            error = "Could not finish reading the selected music file.";
            return false;
        }
        offset += received;
    }
    CloseHandle(handle);
    return true;
#else
    (void)path;
    (void)bytes;
    error = "Local music playback requires Windows.";
    return false;
#endif
}

} // namespace locus::app
