#include "app/song_library.h"
#include "app/chart_file.h"
#include "app/song_set_storage.h"
#include "app/song_timing.h"
#include "core/chart_revision.h"
#include <filesystem>
#include <utility>

namespace locus::app {

bool LibraryChart::CopyPayload(core::LoadedChart& result,std::string& error) const {
    // Standalone in-memory charts have no catalog identity. They are used
    // for isolated preparation/tests, never by the published Song Library.
    if (songSetId.empty()) {
        if (!data) {
            error = "Selected difficulty has no validated chart payload.";
            return false;
        }
        result = *data;
        error.clear();
        return true;
    }
    if (!HasPayload()) {
        error = "Selected difficulty is not indexed; refresh the library.";
        return false;
    }
    core::LoadedChart candidate;
    std::wstring resolvedAudio;
    if (!OpenChartWithAudioPath(file.wstring(),candidate,resolvedAudio,error,
                                &indexedContentFingerprint))
        return false;
    SongSetManifest manifest;
    if (!LoadSongSetManifest(file.parent_path()/L"set.locus.json",manifest,error))
        return false;
    const auto stale = [&]() {
        error = "Song Set or difficulty changed on disk; refresh the library.";
        return false;
    };
    namespace fs = std::filesystem;
    std::error_code ec;
    const auto sameFile = [&](const fs::path& a,const fs::path& b) {
        if (a == b) return true;
        ec.clear();
        return fs::equivalent(a,b,ec) && !ec;
    };
    if (manifest.id != songSetId ||
        candidate.difficultyId != metadata.difficultyId ||
        candidate.title != metadata.title ||
        candidate.route.turns.size() != metadata.noteCount ||
        !sameFile(fs::path(resolvedAudio),audioFile) ||
        !sameFile(file.parent_path()/fs::u8path(manifest.audioFile),audioFile))
        return stale();
    if (!ApplySongTiming(candidate,manifest.tempoMap,error)) return false;
    if (core::ChartGameplayRevision(candidate) != gameplayRevision) return stale();
    result = std::move(candidate);
    error.clear();
    return true;
}

} // namespace locus::app
