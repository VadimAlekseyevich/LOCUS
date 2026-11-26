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

MusicPickResult ChooseSongBackgroundFile() {
    return PickFile(L"LOCUS - Choose Song Set Background Image",
                    L"PNG / JPEG Images (*.png;*.jpg;*.jpeg)\0"
                    L"*.png;*.jpg;*.jpeg\0"
                    L"PNG (*.png)\0*.png\0"
                    L"JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0\0");
}

MusicPickResult ChooseLocalMusicFile() {
    return PickFile(L"LOCUS - Select a WAV or MP3",
                    L"Audio (*.wav;*.mp3)\0*.wav;*.mp3\0"
                    L"WAV (*.wav)\0*.wav\0MP3 (*.mp3)\0*.mp3\0\0");
}

ImportPickResult ChooseLibraryImportFiles() {
#ifdef _WIN32
    // Explorer returns either one full path or a directory followed by a
    // NUL-delimited list of filenames. Other pickers remain single-select.
    std::vector<wchar_t> buffer(1024*1024,L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize=sizeof(dialog);
    dialog.hwndOwner=GetActiveWindow();
    dialog.lpstrFile=buffer.data();
    dialog.nMaxFile=static_cast<DWORD>(buffer.size());
    dialog.lpstrTitle=L"LOCUS - Import Charts or Song Sets";
    dialog.lpstrFilter=L"LOCUS files (*.locuspack;*.locus.json;*.json)\0"
                       L"*.locuspack;*.locus.json;*.json\0"
                       L"Song Set (*.locuspack)\0*.locuspack\0"
                       L"Chart (*.locus.json;*.json)\0*.locus.json;*.json\0\0";
    dialog.Flags=OFN_EXPLORER|OFN_ALLOWMULTISELECT|OFN_FILEMUSTEXIST|
                 OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_HIDEREADONLY;
    if (!GetOpenFileNameW(&dialog)) {
        const auto error=CommDlgExtendedError();
        if (error==0) return {{},{},true};
        if (error==FNERR_BUFFERTOOSMALL)
            return {{},"Selection is too large; choose fewer files at once.",false};
        return {{},"Windows could not open the import dialog.",false};
    }
    ImportPickResult result;
    const std::filesystem::path first(buffer.data());
    const wchar_t* item=buffer.data()+std::wstring(buffer.data()).size()+1;
    if (!*item) result.paths.push_back(first);
    else while (*item) {
        const std::wstring name(item);
        result.paths.push_back(first/name);
        item+=name.size()+1;
    }
    return result;
#else
    return {{},"File picker requires Windows.",false};
#endif
}

MusicPickResult ChooseChartFile() {
    return PickFile(L"LOCUS - Open a Chart JSON",
                    L"LOCUS charts (*.locus.json;*.json)\0*.locus.json;*.json\0"
                    L"JSON (*.json)\0*.json\0\0");
}


MusicPickResult ChooseHitSoundFile() {
    return PickFile(L"LOCUS - Select hitsound",
                    L"Audio (*.wav;*.mp3;*.ogg)\0*.wav;*.mp3;*.ogg\0"
                    L"WAV (*.wav)\0*.wav\0MP3 (*.mp3)\0*.mp3\0"
                    L"OGG (*.ogg)\0*.ogg\0\0");
}

MusicPickResult ChooseSkinFile() {
    return PickFile(L"LOCUS - Load Gameplay Skin",
                    L"LOCUS skins (*.locus-skin.json;*.json)\0*.locus-skin.json;*.json\0"
                    L"JSON (*.json)\0*.json\0\0");
}

MusicPickResult ChooseSkinSaveFile(const std::wstring& suggestedPath) {
#ifdef _WIN32
    std::vector<wchar_t> buffer(32768, L'\0');
    const std::wstring suggestion = suggestedPath.empty()
        ? L"My-Skin.locus-skin.json" : suggestedPath;
    if (suggestion.size() >= buffer.size()) {
        return MusicPickResult{L"", "Skin filename is too long.", false};
    }
    std::copy(suggestion.begin(), suggestion.end(), buffer.begin());
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = GetActiveWindow();
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.lpstrTitle = L"LOCUS - Export Gameplay Skin";
    dialog.lpstrFilter = L"LOCUS skins (*.locus-skin.json;*.json)\0*.locus-skin.json;*.json\0\0";
    dialog.lpstrDefExt = L"json";
    dialog.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST |
                   OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
    if (GetSaveFileNameW(&dialog))
        return MusicPickResult{std::wstring(buffer.data()), "", false};
    if (CommDlgExtendedError() == 0) return MusicPickResult{L"", "", true};
    return MusicPickResult{L"", "Windows could not open the skin Save As dialog.", false};
#else
    (void)suggestedPath;
    return MusicPickResult{L"", "Skin export requires Windows.", false};
#endif
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

MusicPickResult ChooseSongSetExportFile(const std::wstring& suggestedPath) {
#ifdef _WIN32
    std::vector<wchar_t> buffer(32768, L'\0');
    const std::wstring suggestion = suggestedPath.empty()
        ? L"Song.locuspack" : suggestedPath;
    if (suggestion.size() >= buffer.size())
        return {L"", "Export filename is too long.", false};
    std::copy(suggestion.begin(), suggestion.end(), buffer.begin());
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = GetActiveWindow();
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.lpstrTitle = L"LOCUS - Export Complete Song Set";
    dialog.lpstrFilter =
        L"LOCUS Song Sets (*.locuspack)\0*.locuspack\0\0";
    dialog.lpstrDefExt = L"locuspack";
    dialog.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST |
                   OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
    if (GetSaveFileNameW(&dialog))
        return {std::wstring(buffer.data()), "", false};
    if (CommDlgExtendedError() == 0)
        return {L"", "", true};
    return {L"", "Cannot open the Song Set export dialog.", false};
#else
    (void)suggestedPath;
    return {L"", "Song Set export requires Windows.", false};
#endif
}

} // namespace locus::app
