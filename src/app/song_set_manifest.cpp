#include "app/song_set_manifest.h"
#include "app/image_asset_file.h"
#include "core/utf8.h"
#include "core/strict_json.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <utility>

namespace locus::app {
namespace {
bool SafeAudioFilename(const std::string& name) {
    if (name.empty() || name == "." || name == ".." || name.size() > 255 ||
        !core::utf8::Valid(name) ||
        name.find('/') != std::string::npos ||
        name.find('\\') != std::string::npos ||
        name.find(':') != std::string::npos) return false;
    for (unsigned char c : name) if (c < 32 || c == 127) return false;
    // Song Set metadata describes a real audio file, never a chart or the
    // manifest itself. Keep extension checks independent of locale.
    const auto dot = name.find_last_of('.');
    if (dot == std::string::npos) return false;
    std::string extension = name.substr(dot);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(
                       c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c); });
    return extension == ".wav" || extension == ".mp3";
}

} // namespace

bool ValidSongSetMapper(const std::string& mapper) noexcept {
    return mapper.empty() || core::utf8::ValidName(mapper, 100, 512);
}

bool ValidSongSetTags(const std::vector<std::string>& tags) {
    if (tags.size() > 16) return false;
    std::set<std::string> unique;
    for (const auto& tag : tags) {
        if (!core::utf8::ValidName(tag, 32, 128) ||
            tag.find(',') != std::string::npos) return false;
        // Preserve UTF-8 tags as authored. Ignore ASCII case for duplicates.
        std::string key = tag;
        for (char& c : key) {
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        }
        if (!unique.insert(std::move(key)).second) return false;
    }
    return true;
}

bool LoadSongSetManifest(const fs::path& path, SongSetManifest& manifest,
                         std::string& error) {
    std::error_code ec;
    const auto size=std::filesystem::file_size(path,ec);
    if (ec || size==0 || size>kMaxSongSetManifestBytes) {
        error="Song Set metadata is missing or exceeds 64 KiB."; return false;
    }
    std::ifstream file(path,std::ios::binary);
    std::string bytes(static_cast<std::size_t>(size),'\0');
    if (!file.read(bytes.data(),static_cast<std::streamsize>(bytes.size())) ||
        file.peek()!=std::char_traits<char>::eof()) {
        error="Cannot read Song Set metadata."; return false;
    }
    if (!core::detail::ValidateJsonStructure(bytes,error)) return false;
    const auto data = nlohmann::json::parse(bytes, nullptr, false);
    if (data.is_discarded() || !data.is_object() ||
        !data.contains("formatVersion") || !data["formatVersion"].is_number_integer() ||
        data["formatVersion"] != 2) {
        error = "Invalid Song Set metadata version or JSON.";
        return false;
    }
    const auto field = [&](const char* key, std::string& target) {
        if (!data.contains(key) || !data[key].is_string()) return false;
        target = data[key].get<std::string>();
        return true;
    };
    SongSetManifest loaded;
    if (!field("setId", loaded.id) || !field("title", loaded.title) ||
        !field("artist", loaded.artist) || !field("audioFile", loaded.audioFile) ||
        loaded.id.empty() || loaded.id.size() > 128 ||
        !core::utf8::ValidName(loaded.title, 256, 1024) ||
        (!loaded.artist.empty() &&
         !core::utf8::ValidName(loaded.artist, 256, 1024)) ||
        !SafeAudioFilename(loaded.audioFile)) {
        error = "Song Set metadata has missing or invalid fields.";
        return false;
    }
    if (data.contains("mapper")) {
        if (!data["mapper"].is_string()) {
            error = "Song Set mapper must be a string.";
            return false;
        }
        loaded.mapper = data["mapper"].get<std::string>();
    }
    if (data.contains("tags")) {
        if (!data["tags"].is_array()) {
            error = "Song Set tags must be an array.";
            return false;
        }
        if (data["tags"].size() > 16) {
            error = "Song Set tags exceed the supported limit.";
            return false;
        }
        for (const auto& tag : data["tags"]) {
            if (!tag.is_string()) {
                error = "Song Set tags must contain only strings.";
                return false;
            }
            loaded.tags.push_back(tag.get<std::string>());
        }
    }
    if (!ValidSongSetMapper(loaded.mapper) || !ValidSongSetTags(loaded.tags)) {
        error = "Song Set mapper or tags are invalid.";
        return false;
    }
    if (!data.contains("previewStartMs") ||
        !data["previewStartMs"].is_number_integer() ||
        data["previewStartMs"].get<double>()<0 || data["previewStartMs"].get<double>()>kMaxSongPreviewStartMs) {
        error = "Song Set preview position must be an integer.";
        return false;
    }
    try { loaded.previewStartMs = data["previewStartMs"].get<int>(); }
    catch (const std::exception&) {
        error = "Song Set preview position is out of range.";
        return false;
    }
    if (loaded.previewStartMs < 0 ||
        loaded.previewStartMs > kMaxSongPreviewStartMs) {
        error = "Song Set preview position is out of range."; return false;
    }
    // Current Song Set manifests always own one canonical song-wide map.
    if (!data.contains("bpm") || !data.contains("offsetMs") ||
        !data["bpm"].is_number() || !data["offsetMs"].is_number_integer() ||
        data["offsetMs"].get<double>() < -3600000 || data["offsetMs"].get<double>() > 3600000) {
        error = "Song Set BPM and offset are required with valid types.";
        return false;
    }
    try {
        loaded.bpm = data["bpm"].get<double>();
        loaded.offsetMs = data["offsetMs"].get<int>();
    } catch (const std::exception&) {
        error = "Song Set BPM/offset are out of range.";
        return false;
    }
    if (!std::isfinite(*loaded.bpm) || *loaded.bpm <= 0.0 ||
        *loaded.offsetMs < -3600000 || *loaded.offsetMs > 3600000) {
        error = "Song Set BPM/offset are invalid.";
        return false;
    }
    if (!data.contains("tempoMap") || !data["tempoMap"].is_array() ||
        data["tempoMap"].empty() || data["tempoMap"].size() > 256) {
        error = "Song Set tempoMap must contain 1 to 256 markers.";
        return false;
    }
    for (const auto& item : data["tempoMap"]) {
        if (!item.is_object() || item.size() != 2 ||
            !item.contains("timeMs") || !item.contains("bpm") ||
            !item["timeMs"].is_number_integer() || !item["bpm"].is_number() ||
            item["timeMs"].get<double>() < -3600000 || item["timeMs"].get<double>() > 86400000) {
            error = "Song Set tempoMap entry must contain timeMs and bpm.";
            return false;
        }
        loaded.tempoMap.push_back({item["timeMs"].get<std::int64_t>(),
                                   item["bpm"].get<double>()});
    }
    if (!core::ValidTempoMap(loaded.tempoMap, error)) return false;
    if (loaded.tempoMap.front().bpm != *loaded.bpm ||
        loaded.tempoMap.front().timeMs != *loaded.offsetMs) {
        error = "Song Set tempoMap first row disagrees with bpm/offsetMs.";
        return false;
    }

    if (!data.contains("backgroundFile") ||
        !data["backgroundFile"].is_string()) {
        error = "Song Set backgroundFile must be a string.";
        return false;
    }
    loaded.backgroundFile = data["backgroundFile"].get<std::string>();
    if (!loaded.backgroundFile.empty() &&
        !SafeImageFilename(loaded.backgroundFile)) {
        error = "Song Set backgroundFile must be a safe PNG/JPEG filename.";
        return false;
    }
    manifest = std::move(loaded);
    error.clear();
    return true;
}

// A single validator/encoder for create and replace: they must never
// accept different Song Set schemas or serialize different JSON shapes.
bool EncodeSongSetManifest(const SongSetManifest& manifest,
                           std::string& encoded, std::string& error) {
    if (manifest.id.empty() || manifest.id.size() > 128 ||
        !core::utf8::ValidName(manifest.title, 256, 1024) ||
        (!manifest.artist.empty() &&
         !core::utf8::ValidName(manifest.artist, 256, 1024)) ||
        !ValidSongSetMapper(manifest.mapper) ||
        !ValidSongSetTags(manifest.tags) ||
        !SafeAudioFilename(manifest.audioFile) ||
        manifest.previewStartMs < 0 ||
        manifest.previewStartMs > kMaxSongPreviewStartMs ||
        (!manifest.backgroundFile.empty() &&
         !SafeImageFilename(manifest.backgroundFile)) ||
        !manifest.bpm.has_value() || !manifest.offsetMs.has_value() ||
        !std::isfinite(*manifest.bpm) || *manifest.bpm <= 0.0 ||
        *manifest.offsetMs < -3600000 || *manifest.offsetMs > 3600000 ||
        (!manifest.tempoMap.empty() &&
         (manifest.tempoMap.front().bpm != *manifest.bpm ||
          manifest.tempoMap.front().timeMs != *manifest.offsetMs))) {
        error = "Invalid Song Set metadata.";
        return false;
    }
    const std::vector<core::TempoMarker> tempo =
        manifest.tempoMap.empty()
            ? std::vector<core::TempoMarker>{{*manifest.offsetMs, *manifest.bpm}}
            : manifest.tempoMap;
    if (!core::ValidTempoMap(tempo, error)) return false;
    nlohmann::json data = {
        {"formatVersion", 2}, {"setId", manifest.id},
        {"title", manifest.title}, {"artist", manifest.artist},
        {"mapper", manifest.mapper}, {"tags", manifest.tags},
        {"audioFile", manifest.audioFile},
        {"previewStartMs", manifest.previewStartMs},
        {"backgroundFile", manifest.backgroundFile},
        {"bpm", tempo.front().bpm}, {"offsetMs", tempo.front().timeMs}
    };
    data["tempoMap"] = nlohmann::json::array();
    for (const auto& marker : tempo)
        data["tempoMap"].push_back({
            {"timeMs", marker.timeMs}, {"bpm", marker.bpm}});
    encoded = data.dump(2) + '\n';
    error.clear();
    return true;
}

} // namespace locus::app
