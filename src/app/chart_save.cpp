#include "app/chart_file.h"

#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace locus::app {
namespace {

bool IsDescendant(const std::filesystem::path& root,
                  const std::filesystem::path& file) {
    auto parent = root.begin();
    auto child = file.begin();
    for (; parent != root.end(); ++parent, ++child) {
        if (child == file.end()) return false;
#ifdef _WIN32
        const std::wstring a = parent->wstring();
        const std::wstring b = child->wstring();
        if (_wcsicmp(a.c_str(), b.c_str()) != 0) return false;
#else
        if (*parent != *child) return false;
#endif
    }
    return child != file.end();
}

} // namespace

bool SaveChartWithAudioPath(const std::wstring& chartFile,
                            const std::wstring& audioFile,
                            const core::LoadedChart& draft,
                            std::string& error) {
#ifdef _WIN32
    namespace fs = std::filesystem;
    const fs::path destination(chartFile);
    if (destination.extension() != L".json") {
        error = "Save as a .locus.json or .json chart file.";
        return false;
    }
    std::error_code ec;
    const fs::path folder = fs::canonical(destination.parent_path(), ec);
    if (ec) {
        error = "The chosen chart folder does not exist or is unavailable.";
        return false;
    }
    const fs::path song = fs::canonical(fs::path(audioFile), ec);
    if (ec || !fs::is_regular_file(song)) {
        error = "Choose an existing WAV/MP3 song before saving.";
        return false;
    }
    if (!IsDescendant(folder, song)) {
        error = "Save the chart in the song's folder or a parent folder.";
        return false;
    }
    core::LoadedChart withPath = draft;
    withPath.audioPath = song.lexically_relative(folder).generic_u8string();
    if (!core::ValidChartAudioPath(withPath.audioPath)) {
        error = "The selected audio path cannot be written as a relative reference.";
        return false;
    }

    std::string encoded;
    if (!core::SerializeChartJson(withPath, encoded, error)) return false;
    const std::wstring temporary = chartFile + L".tmp-" +
        std::to_wstring(GetCurrentProcessId()) + L"-" +
        std::to_wstring(GetTickCount64());
    HANDLE handle = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0,
                                nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        error = "Cannot create the temporary chart file. Check folder permissions.";
        return false;
    }
    DWORD count = 0;
    const bool wrote = WriteFile(handle, encoded.data(),
                                 static_cast<DWORD>(encoded.size()),
                                 &count, nullptr) != 0 &&
                       count == encoded.size() &&
                       FlushFileBuffers(handle) != 0;
    CloseHandle(handle);
    if (!wrote) {
        DeleteFileW(temporary.c_str());
        error = "Could not finish writing the chart. Existing file was not changed.";
        return false;
    }
    if (!MoveFileExW(temporary.c_str(), chartFile.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary.c_str());
        error = "Could not replace the chart file. Existing file was not changed.";
        return false;
    }
    error.clear();
    return true;
#else
    (void)chartFile;
    (void)audioFile;
    (void)draft;
    error = "Saving charts requires Windows.";
    return false;
#endif
}

} // namespace locus::app
