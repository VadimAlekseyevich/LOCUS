#pragma once

#include <string_view>

namespace locus::core {

enum class MusicFileType { Wav, Mp3, Unsupported };

// Only WAV and MP3 belong to the M-01 user-facing picker.
// Windows paths are Unicode; extension matching is ASCII case-insensitive.
MusicFileType DetectMusicFileType(std::wstring_view path) noexcept;

} // namespace locus::core
