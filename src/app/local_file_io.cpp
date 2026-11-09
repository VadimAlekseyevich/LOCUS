#include "app/local_file_io.h"
#include "core/utf8.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <algorithm>
#include <cstdint>
#include <new>
#include <string>
#include <vector>

namespace locus::app {

std::string DisplayLocalFileName(const std::wstring& path) {
    const auto slash = path.find_last_of(L"\\/");
    const std::wstring name = path.substr(
        slash == std::wstring::npos ? 0 : slash + 1);
    if (name.empty()) return "Local song";
#ifdef _WIN32
    // Convert the whole UTF-16 name before truncating. A 36-wchar cut
    // previously could split a surrogate pair into a replacement character.
    const int needed = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, name.data(),
        static_cast<int>(name.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return "Local song";
    std::string result(static_cast<std::size_t>(needed), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, name.data(),
                            static_cast<int>(name.size()), result.data(),
                            needed, nullptr, nullptr) != needed)
        return "Local song";
#else
    // wchar_t is UTF-32 on the supported Unix targets.
    std::string result;
    for (const wchar_t wc : name) {
        const auto part = core::utf8::Encode(static_cast<std::uint32_t>(wc));
        if (part.empty()) return "Local song";
        result += part;
    }
#endif
    return core::utf8::Truncate(result, 36);
}

bool ReadLocalFileBytes(const std::wstring& path, std::vector<std::uint8_t>& bytes,
                        std::string& error, std::size_t maxBytes) {
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
                                 static_cast<unsigned long long>(maxBytes);
    if (!validLength) {
        CloseHandle(handle);
        error = "Selected file is empty or exceeds the allowed file size.";
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
    (void)maxBytes;
    error = "Local file reading requires Windows.";
    return false;
#endif
}

} // namespace locus::app
