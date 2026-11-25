#include "app/song_library.h"
#include "app/song_library_internal.h"
#include "app/chart_file.h"
#include "app/song_set_storage.h"
#include "app/song_timing.h"
#include "core/chart_revision.h"
#include <filesystem>
#include <utility>

namespace locus::app {

bool LibraryChart::CopyPayload(core::LoadedChart& result,std::string& error) const {
    if (!HasPayload()) {
        error = "Selected difficulty is not indexed; refresh the library.";
        return false;
    }
    SongSetManifest manifest;
    if (!LoadSongSetManifest(file.parent_path()/L"set.locus.json",manifest,error))
        return false;
    return detail::MaterializeIndexedChart(*this,manifest,result,error);
}

bool detail::MaterializeIndexedChart(const LibraryChart& indexed,
        const SongSetManifest& manifest,core::LoadedChart& result,std::string& error) {
    if (!indexed.HasPayload()) {
        error = "Selected difficulty is not indexed; refresh the library.";
        return false;
    }
    core::LoadedChart candidate;
    std::wstring resolvedAudio;
    if (!OpenChartWithAudioPath(indexed.file.wstring(),candidate,resolvedAudio,error,
                                &indexed.indexedContentFingerprint)) return false;
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
    if (manifest.id != indexed.songSetId ||
        candidate.difficultyId != indexed.metadata.difficultyId ||
        candidate.title != indexed.metadata.title ||
        candidate.route.turns.size() != indexed.metadata.noteCount ||
        !sameFile(fs::path(resolvedAudio),indexed.audioFile) ||
        !sameFile(indexed.file.parent_path()/fs::u8path(manifest.audioFile),indexed.audioFile))
        return stale();
    if (!ApplySongTiming(candidate,manifest.tempoMap,error)) return false;
    if (core::ChartGameplayRevision(candidate) != indexed.gameplayRevision) return stale();
    result = std::move(candidate);
    error.clear();
    return true;
}

} // namespace locus::app
