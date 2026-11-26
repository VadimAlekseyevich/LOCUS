#pragma once
#include "core/tempo_map.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace locus::app {
struct SongSetManifest {
    std::string id;
    std::string title;
    std::string artist;
    std::string mapper; // Authorship credit for the whole Song Set.
    std::vector<std::string> tags; // Set-wide, not chart metadata.
    std::string audioFile;
    int previewStartMs = 0; // Song menu playback only; not chart timing.
    std::optional<double> bpm; // Authoritative Song Set tempo.
    std::optional<int> offsetMs; // First tempo-map marker.
    std::vector<core::TempoMarker> tempoMap; // Authoritative shared timing.
    std::string backgroundFile; // Optional flat PNG/JPEG filename.
};
inline constexpr int kMaxSongPreviewStartMs = 3600000;

// Optional UTF-8 pack metadata, shared by all difficulties.
bool ValidSongSetMapper(const std::string& mapper) noexcept;
bool ValidSongSetTags(const std::vector<std::string>& tags);

inline constexpr std::size_t kMaxSongSetManifestBytes = 64U*1024U;
bool LoadSongSetManifest(const std::filesystem::path& path,
                         SongSetManifest& manifest, std::string& error);
bool EncodeSongSetManifest(const SongSetManifest& manifest,
                           std::string& encoded, std::string& error);
} // namespace locus::app
