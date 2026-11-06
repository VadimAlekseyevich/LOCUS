#include "app/editor_document.h"

#include "app/chart_file.h"
#include "app/chart_filename.h"
#include "app/local_music.h"
#include "app/music_picker.h"
#include "core/chart_editor.h"
#include "core/chart_json.h"

#include <filesystem>
#include <string>
#include <utility>

namespace locus::app {

// Editor document lifecycle is independent of screen transitions and scoring.
// M-04: reuse the same validation and local-music stream as external chart
// playback. Saving/opening never silently changes the active Play session.
bool LoadDraftFromDisk(const std::wstring& file, locus::core::ChartEditor& editor,
                       locus::app::LocalMusic& song, std::wstring& chartPath,
                       std::wstring& audioPath, std::string& error) {
    locus::core::LoadedChart candidate;
    std::wstring selectedAudio;
    if (!locus::app::OpenChartWithAudioPath(file, candidate, selectedAudio, error))
        return false;
    // Decode and validate before modifying the open document or its audio.
    locus::app::LocalMusic replacement;
    replacement.SetVolume(song.Volume());
    replacement.SetSpeed(song.Speed(), 0.0);
    if (!replacement.LoadFromFile(selectedAudio)) {
        error = replacement.Error();
        return false;
    }
    if (!locus::core::PrepareChartForSong(
            candidate, replacement.DurationSeconds(), error))
        return false;
    locus::core::ChartEditor preparedEditor;
    preparedEditor.Load(candidate);
    if (!preparedEditor.SetSongDuration(replacement.DurationSeconds())) {
        error = "Chart timeline is incompatible with its audio.";
        return false;
    }
    song.SwapWith(replacement);
    editor = std::move(preparedEditor);
    chartPath = file;
    audioPath = selectedAudio;
    error.clear();
    return true;
}

bool SaveDraftToDisk(locus::core::ChartEditor& editor,
                     const locus::app::LocalMusic& song,
                     std::wstring& chartPath, const std::wstring& audioPath,
                     bool saveAs, std::string& error) {
    if (audioPath.empty() || !song.Ready()) {
        error = "PRESS O TO ATTACH YOUR WAV / MP3 FIRST";
        return false;
    }
    locus::core::LoadedChart checked = editor.Data();
    if (!locus::core::PrepareChartForSong(checked, song.DurationSeconds(), error))
        return false;

    std::wstring destination = chartPath;
    if (destination.empty() || saveAs) {
        // Use the authored chart title as the filename suggestion, not
        // the unrelated audio filename or a perpetual "Untitled" default.
        // Ctrl+S for a previously saved chart still uses its existing path.
        const auto folder = destination.empty()
            ? std::filesystem::path(audioPath).parent_path()
            : std::filesystem::path(destination).parent_path();
        const std::wstring suggestion =
            (folder / locus::app::SuggestedChartFileName(checked.title)).wstring();
        const auto selected = locus::app::ChooseChartSaveFile(suggestion);
        if (selected.path.empty()) {
            error = selected.canceled ? "" : selected.error;
            return false;
        }
        destination = selected.path;
    }
    if (!locus::app::SaveChartWithAudioPath(destination, audioPath, checked, error))
        return false;

    // Reopen exactly the bytes that were saved, preserving editor semantics
    // including audioPath relative to the new location and authored length.
    locus::core::LoadedChart reopened;
    std::wstring resolvedSong;
    if (!locus::app::OpenChartWithAudioPath(
            destination, reopened, resolvedSong, error) ||
        !locus::core::PrepareChartForSong(reopened, song.DurationSeconds(), error))
        return false;
    // Saving must not rewind the timeline or wipe the user's undo history.
    // Load() is for opening another chart; this is the same editing session.
    editor.ApplySavedChart(std::move(reopened));
    chartPath = destination;
    error.clear();
    return true;
}

} // namespace locus::app
