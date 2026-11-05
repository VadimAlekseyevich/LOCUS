#include "core/music_file.h"

namespace locus::core {
namespace {

bool EndsWithAsciiIgnoreCase(std::wstring_view path, std::wstring_view suffix) noexcept {
    if (path.size() < suffix.size()) return false;
    const auto ending = path.substr(path.size() - suffix.size());
    for (std::size_t i = 0; i < suffix.size(); ++i) {
        wchar_t value = ending[i];
        if (value >= L'A' && value <= L'Z') value += (L'a' - L'A');
        if (value != suffix[i]) return false;
    }
    return true;
}

} // namespace

MusicFileType DetectMusicFileType(std::wstring_view path) noexcept {
    if (EndsWithAsciiIgnoreCase(path, L".wav")) return MusicFileType::Wav;
    if (EndsWithAsciiIgnoreCase(path, L".mp3")) return MusicFileType::Mp3;
    return MusicFileType::Unsupported;
}

} // namespace locus::core
