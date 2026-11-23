#include "app/chart_file.h"
#include "app/song_set_storage.h"
#include "app/path_containment.h"
#include "app/atomic_text_file.h"

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
bool SaveChartWithAudioPath(const std::wstring& chartFile,
                            const std::wstring& audioFile,
                            const core::LoadedChart& draft,
                            std::string& error,
                            bool folderLockAlreadyHeld,
                            ChartWriteMode mode) {
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
    if (!IsPathWithinDirectory(folder, song)) {
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
    if (encoded.size() > kMaxChartFileBytes) {
        error = "Chart JSON exceeds the supported 8 MiB file limit.";
        return false;
    }
    FolderWriteLock folderLock;
    if (!folderLockAlreadyHeld && !folderLock.Acquire(folder, error))
        return false;
    if (mode == ChartWriteMode::Replace &&
        !BackUpChartBeforeReplace(destination, error))
        return false;
    const auto status = AtomicWriteTextFile(
        destination, encoded,
        mode == ChartWriteMode::CreateNew
            ? AtomicTextWriteMode::CreateNew
            : AtomicTextWriteMode::Replace,
        error);
    if (status == AtomicTextWriteStatus::AlreadyExists) {
        error = "Chart filename already exists; another difficulty was not overwritten.";
        return false;
    }
    if (status != AtomicTextWriteStatus::Published) return false;
    error.clear();
    return true;
#else
    (void)chartFile;
    (void)audioFile;
    (void)draft;
    (void)folderLockAlreadyHeld;
    (void)mode;
    error = "Saving charts requires Windows.";
    return false;
#endif
}

} // namespace locus::app
