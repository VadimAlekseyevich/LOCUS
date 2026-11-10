#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#ifdef _MSC_VER
#pragma comment(lib, "shell32.lib")
#endif
#endif

namespace locus::app {

// Open a local directory without invoking a command interpreter.
// ShellExecuteW supports Unicode filenames on Windows.
inline bool OpenFolderInExplorer(const std::filesystem::path& folder,
                                 std::string& error) {
    std::error_code ec;
    if (!std::filesystem::is_directory(folder, ec) || ec) {
        error = "Folder does not exist or is inaccessible.";
        return false;
    }
#ifdef _WIN32
    const auto result = reinterpret_cast<std::intptr_t>(
        ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr,
                      SW_SHOWNORMAL));
    if (result <= 32) {
        error = "Windows could not open the folder (ShellExecute code " +
                std::to_string(result) + ").";
        return false;
    }
    error.clear();
    return true;
#else
    error = "Opening folders requires Windows.";
    return false;
#endif
}

} // namespace locus::app
