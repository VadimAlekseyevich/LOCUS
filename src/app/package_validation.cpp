#include "app/package_validation.h"
#include "app/chart_file.h"
#include "app/image_asset_file.h"
#include "app/song_timing.h"
#include <fstream>
#include <utility>

namespace locus::app {
namespace fs=std::filesystem;
bool ValidateStagedPackage(const fs::path& folder,ValidatedPackage& package,std::string& error) {
    error.clear();
    ValidatedPackage result;
    std::error_code ec;
    const auto manifest=folder/"set.locus.json";
    if (fs::symlink_status(manifest,ec).type()!=fs::file_type::regular || ec ||
        !LoadSongSetManifest(manifest,result.manifest,error)) {
        if (error.empty()) error="Package is missing regular Song Set metadata.";
        return false;
    }
    result.audio=folder/fs::u8path(result.manifest.audioFile);
    if (fs::symlink_status(result.audio,ec).type()!=fs::file_type::regular || ec) {
        error="Package is missing the shared audio file."; return false;
    }
    if (!result.manifest.backgroundFile.empty()) {
        const auto art=folder/fs::u8path(result.manifest.backgroundFile);
        if (fs::symlink_status(art,ec).type()!=fs::file_type::regular || ec ||
            !ValidateImageAssetFile(art,error)) {
            if (error.empty()) error="Package background is missing or invalid.";
            return false;
        }
    }
    bool foundAudio=false;
    bool foundArt=result.manifest.backgroundFile.empty();
    for (fs::directory_iterator it(folder,ec),end; !ec && it!=end; it.increment(ec)) {
        const auto file=it->path();
        if (it->symlink_status(ec).type()!=fs::file_type::regular || ec) {
            error="Package contains an unexpected directory or file type."; return false;
        }
        const auto name=file.filename().u8string();
        if (name=="set.locus.json") continue;
        if (name==result.manifest.audioFile) { foundAudio=true; continue; }
        if (name==result.manifest.backgroundFile) { foundArt=true; continue; }
        if (file.extension()!=".json") {
            error="Package contains unsupported files."; return false;
        }
        const auto size=fs::file_size(file,ec);
        if (ec || size==0 || size>kMaxChartFileBytes) {
            error="Package difficulty is empty or exceeds 8 MiB."; return false;
        }
        std::ifstream input(file,std::ios::binary);
        std::string bytes(static_cast<std::size_t>(size),'\0');
        if (!input.read(bytes.data(),static_cast<std::streamsize>(bytes.size())) ||
            input.peek()!=std::char_traits<char>::eof()) {
            error="Cannot read package difficulty."; return false;
        }
        core::LoadedChart parsed;
        if (!core::ParseChartJson(bytes,parsed,error)) return false;
        // Exact flat references make acceptance independent of filesystem case rules.
        if (parsed.audioPath!=result.manifest.audioFile) {
            error="Package difficulty does not use shared audio."; return false;
        }
        if (!core::ValidDifficultyId(parsed.difficultyId) ||
            result.difficulties.count(parsed.difficultyId)) {
            error="Package has a missing or duplicate difficultyId."; return false;
        }
        auto projected=parsed;
        if (!ApplySongTiming(projected,result.manifest.tempoMap,error)) return false;
        const auto id=parsed.difficultyId;
        result.difficulties.emplace(id,PackageDifficulty{file,std::move(parsed)});
    }
    if (ec || result.difficulties.empty() || !foundAudio || !foundArt) {
        error="Package lacks valid difficulties or exact manifest asset filenames."; return false;
    }
    package=std::move(result);
    error.clear();
    return true;
}
} // namespace locus::app
