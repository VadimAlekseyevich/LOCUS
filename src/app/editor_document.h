#pragma once

#include "app/music_picker.h"

#include <string>

namespace locus::core { class ChartEditor; }
namespace locus::app {
class LocalMusic;

// Open a draft only after its chart, audio decoder and editor timeline all
// validate. On failure, preserve the entire existing document and song.
bool LoadDraftFromDisk(const std::wstring& file, core::ChartEditor& editor,
                       LocalMusic& song, std::wstring& chartPath,
                       std::wstring& audioPath, std::string& error);

// Atomically persist the chart while retaining selection and Undo/Redo.
// Save As may request a Windows file dialog; cancellation changes nothing.
// Inject a picker in tests and editor composition; the persistence logic
// does not need to know who owns the native UI dialog.
using ChartSavePicker = MusicPickResult (*)(const std::wstring&);
bool SaveDraftToDisk(core::ChartEditor& editor, const LocalMusic& song,
                     std::wstring& chartPath, const std::wstring& audioPath,
                     bool saveAs, std::string& error,
                     ChartSavePicker chooseFile = ChooseChartSaveFile);

} // namespace locus::app
