#pragma once

#include <string>

namespace locus::app {

struct MusicPickResult {
    std::wstring path;
    std::string error;
    bool canceled = false;
};

// Native Windows file dialog. Canceling leaves the current song untouched.
MusicPickResult ChooseLocalMusicFile();
MusicPickResult ChooseChartFile();
MusicPickResult ChooseChartSaveFile(const std::wstring& suggestedPath);

} // namespace locus::app
