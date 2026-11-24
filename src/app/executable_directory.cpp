#include "app/executable_directory.h"
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include "raylib.h"
#endif

namespace locus::app {
std::filesystem::path ExecutableDirectory() {
#ifdef _WIN32
    std::vector<wchar_t> buffer(512);
    while (buffer.size() <= 32768) {
        const DWORD count = GetModuleFileNameW(nullptr, buffer.data(),
                                               static_cast<DWORD>(buffer.size()));
        if (count == 0) break;
        if (count < buffer.size())
            return std::filesystem::path(std::wstring(buffer.data(), count)).parent_path();
        buffer.resize(buffer.size() * 2);
    }
    throw std::runtime_error("Cannot resolve LOCUS executable directory.");
#else
    return std::filesystem::u8path(GetApplicationDirectory());
#endif
}
}
