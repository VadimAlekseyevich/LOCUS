#include "app/music_picker.h"

// This compilation unit deliberately never includes raylib.h: Windows SDK
// declares functions with names shared by raylib.
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#endif

#include <vector>

namespace locus::app {

MusicPickResult ChooseLocalMusicFile() {
#ifdef _WIN32
    // Wide Win32 paths preserve song names outside the active Windows codepage.
    std::vector<wchar_t> file(32768, L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = GetActiveWindow();
    dialog.lpstrFile = file.data();
    dialog.nMaxFile = static_cast<DWORD>(file.size());
    dialog.lpstrTitle = L"LOCUS - Select a WAV or MP3";
    dialog.lpstrFilter =
        L"Audio (*.wav;*.mp3)\0*.wav;*.mp3\0WAV (*.wav)\0*.wav\0MP3 (*.mp3)\0*.mp3\0\0";
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
                   OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
    if (GetOpenFileNameW(&dialog) != 0) {
        return MusicPickResult{std::wstring(file.data()), "", false};
    }
    if (CommDlgExtendedError() == 0) return MusicPickResult{L"", "", true};
    return MusicPickResult{L"", "Windows could not open the music picker.", false};
#else
    return MusicPickResult{L"", "Local music picker requires Windows.", false};
#endif
}

} // namespace locus::app
