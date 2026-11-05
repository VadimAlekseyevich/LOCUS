#include "app/music_picker.h"

// No raylib header here: Windows SDK has conflicting C symbols.
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#endif

#include <algorithm>
#include <vector>

namespace locus::app {
namespace {

MusicPickResult PickFile(const wchar_t* title, const wchar_t* filter) {
#ifdef _WIN32
    std::vector<wchar_t> buffer(32768, L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = GetActiveWindow();
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.lpstrTitle = title;
    dialog.lpstrFilter = filter;
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
                   OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
    if (GetOpenFileNameW(&dialog)) {
        return MusicPickResult{std::wstring(buffer.data()), "", false};
    }
    if (CommDlgExtendedError() == 0) return MusicPickResult{L"", "", true};
    return MusicPickResult{L"", "Windows could not open the selected file dialog.", false};
#else
    (void)title;
    (void)filter;
    return MusicPickResult{L"", "File picker requires Windows.", false};
#endif
}

} // namespace

MusicPickResult ChooseLocalMusicFile() {
    return PickFile(L"LOCUS - Select a WAV or MP3",
                    L"Audio (*.wav;*.mp3)\0*.wav;*.mp3\0"
                    L"WAV (*.wav)\0*.wav\0MP3 (*.mp3)\0*.mp3\0\0");
}

MusicPickResult ChooseChartFile() {
    return PickFile(L"LOCUS - Open a Chart v1 JSON",
                    L"LOCUS charts (*.locus.json;*.json)\0*.locus.json;*.json\0"
                    L"JSON (*.json)\0*.json\0\0");
}


MusicPickResult ChooseChartSaveFile(const std::wstring& suggestedPath) {
#ifdef _WIN32
    std::vector<wchar_t> buffer(32768, L'\0');
    std::wstring suggestion = suggestedPath.empty()
        ? L"Untitled.locus.json" : suggestedPath;
    if (suggestion.size() >= buffer.size()) {
        return MusicPickResult{L"", "Suggested chart path is too long.", false};
    }
    std::copy(suggestion.begin(), suggestion.end(), buffer.begin());
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = GetActiveWindow();
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.lpstrTitle = L"LOCUS - Save Chart As";
    dialog.lpstrFilter = L"LOCUS charts (*.locus.json;*.json)\0*.locus.json;*.json\0\0";
    dialog.lpstrDefExt = L"json";
    dialog.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST |
                   OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
    if (GetSaveFileNameW(&dialog)) {
        return MusicPickResult{std::wstring(buffer.data()), "", false};
    }
    if (CommDlgExtendedError() == 0) return MusicPickResult{L"", "", true};
    return MusicPickResult{L"", "Windows could not open the Save As dialog.", false};
#else
    (void)suggestedPath;
    return MusicPickResult{L"", "Saving charts requires Windows.", false};
#endif
}

} // namespace locus::app
