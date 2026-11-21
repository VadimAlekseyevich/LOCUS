#pragma once

#include "app/run_results.h"
#include "app/song_library.h"

#include <filesystem>
#include <vector>

namespace locus::app {

// Saved results store stable Song Set identity, not image filenames. Resolve
// artwork from the current library index only when a result is selected so
// renames and changed covers work without modifying old result records.
// A deleted/moved Song Set has no artwork; never show another song's cover.
inline std::filesystem::path BackgroundForSavedResult(
    const std::vector<LibrarySong>& songs, const RunResult& result) {
    if (result.songSetId.empty()) return {};
    for (const auto& song : songs)
        if (song.id == result.songSetId)
            return song.backgroundImage;
    return {};
}

} // namespace locus::app
