#include "app/library_playback.h"

#include "app/chart_playback.h"
#include "app/game_session.h"
#include "app/local_music.h"
#include "core/chart_json.h"
#include "core/judgement.h"

#include <utility>

namespace locus::app {

bool PrepareLibraryDifficulty(const LibraryChart& selected, double audioDuration,
                              core::LoadedChart& prepared, std::string& error) {
    if (selected.audioFile.empty()) {
        error = "Selected difficulty does not reference an audio file.";
        return false;
    }
    core::LoadedChart candidate = selected.data;
    if (!core::PrepareChartForSong(candidate, audioDuration, error))
        return false;
    prepared = std::move(candidate);
    error.clear();
    return true;
}

bool RequiresLibraryAudioLoad(const LibraryChart& selected,
                              const std::wstring& loadedAudioPath,
                              bool audioReady) noexcept {
    return !audioReady || loadedAudioPath != selected.audioFile.wstring();
}

bool ActivateLibraryDifficulty(const LibraryChart& selected,
                               const LibraryPlaybackContext& context,
                               double now, std::string& error) {
    if (selected.audioFile.empty()) {
        error = "Selected difficulty does not reference an audio file.";
        return false;
    }
    const std::wstring audioPath = selected.audioFile.wstring();
    const bool loadAudio = RequiresLibraryAudioLoad(
        selected, context.loadedAudioPath, context.song.Ready());

    // Prepare the decoder independently: failed decoding cannot tear down
    // the previous song or invalidate a preview currently bound to it.
    LocalMusic replacement;
    double duration = context.song.DurationSeconds();
    if (loadAudio) {
        replacement.SetVolume(context.song.Volume());
        if (!replacement.LoadFromFile(audioPath)) {
            error = replacement.Error();
            return false;
        }
        duration = replacement.DurationSeconds();
    }

    core::LoadedChart prepared;
    if (!PrepareLibraryDifficulty(selected, duration, prepared, error))
        return false;

    // No visible state is changed until both audio and chart are valid.
    // Keep the Chart object's ADDRESS stable for TurnJudge and GameSession.
    if (loadAudio) {
        context.song.SwapWith(replacement);
        context.loadedAudioPath = audioPath;
    }
    context.activeChart = std::move(prepared.route);
    context.chartTitle = std::move(prepared.title);
    context.playback.Configure(
        context.activeChart.bpm * context.activeChart.ticksPerBeat,
        prepared.offsetSeconds);
    context.playback.SetSpeed(1.0, now);
    context.session.ClearTestOrigin();
    context.judge.SetOffsetSeconds(prepared.offsetSeconds);
    context.customChart = true;
    error.clear();
    return true;
}

} // namespace locus::app
