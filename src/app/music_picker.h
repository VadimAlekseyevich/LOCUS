#pragma once

#include <string>
#include <filesystem>
#include <vector>

namespace locus::app {

struct MusicPickResult {
    std::wstring path;
    std::string error;
    bool canceled = false;
};

// Native Windows file dialog. Canceling leaves the current song untouched.
MusicPickResult ChooseLocalMusicFile();
MusicPickResult ChooseChartFile();
struct ImportPickResult {
    std::vector<std::filesystem::path> paths;
    std::string error;
    bool canceled = false;
};
ImportPickResult ChooseLibraryImportFiles();
MusicPickResult ChooseSongSetExportFile(const std::wstring& suggestedPath);
MusicPickResult ChooseChartSaveFile(const std::wstring& suggestedPath);
MusicPickResult ChooseSkinFile();
MusicPickResult ChooseHitSoundFile();
MusicPickResult ChooseSongBackgroundFile();
MusicPickResult ChooseSkinSaveFile(const std::wstring& suggestedPath);

} // namespace locus::app
