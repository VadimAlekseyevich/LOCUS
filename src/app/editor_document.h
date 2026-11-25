#pragma once

#include "app/music_picker.h"

#include "core/route.h"

#include <string>
#include <vector>

namespace locus::core { class ChartEditor; struct LoadedChart; }
namespace locus::app {
class LocalMusic;

// Construct the entire new-difficulty authoring document before replacing
// the current draft. The Song Set's complete tempo map, not just its first
// BPM/offset, determines snapping immediately. No audio device is touched.
bool PrepareNewDifficultyDraft(core::ChartEditor& result,
                               double songDurationSeconds,
                               const std::string& audioFilename,
                               const std::vector<core::TempoMarker>& tempoMap,
                               std::string& error);

// Open a draft only after its chart, audio decoder and editor timeline all
// validate. On failure, preserve the entire existing document and song.
bool LoadDraftFromDisk(const std::wstring& file, core::ChartEditor& editor,
                       LocalMusic& song, std::wstring& chartPath,
                       std::wstring& audioPath, std::string& error);
// Managed library open already verified the exact indexed bytes and audio.
// Decode and publish that payload without reopening or reparsing the chart.
bool LoadDraftFromPayload(const std::wstring& file,core::LoadedChart candidate,
                          const std::wstring& selectedAudio,core::ChartEditor& editor,
                          LocalMusic& song,std::wstring& chartPath,
                          std::wstring& audioPath,std::string& error);

// Atomically persist the chart while retaining selection and Undo/Redo.
// Save As may request a Windows file dialog; cancellation changes nothing.
// Inject a picker in tests and editor composition; the persistence logic
// does not need to know who owns the native UI dialog.
using ChartSavePicker = MusicPickResult (*)(const std::wstring&);
bool SaveDraftToDisk(core::ChartEditor& editor, const LocalMusic& song,
                     std::wstring& chartPath, const std::wstring& audioPath,
                     bool saveAs, std::string& error,
                     ChartSavePicker chooseFile = ChooseChartSaveFile,
                     bool folderLockAlreadyHeld = false);

} // namespace locus::app
