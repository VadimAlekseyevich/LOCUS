#include "app/song_library.h"
#include "app/song_library_internal.h"
#include "app/chart_file.h"
#include "app/song_set_storage.h"
#include "app/song_timing.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <system_error>
#include <utility>
namespace locus::app {
namespace fs = std::filesystem;
using namespace detail;
bool SongLibrary::Refresh(std::string& message) {
    std::error_code ec;
    fs::create_directories(root_, ec);
    if (ec) {
        message = "Cannot create Songs library: " + ec.message();
        return false;
    }
    const fs::path root = fs::canonical(root_, ec);
    if (ec) {
        message = "Cannot open Songs library: " + ec.message();
        return false;
    }

    std::vector<LibrarySong> next;
    std::map<fs::path, std::size_t> byAudio; // Logical song, not specific revision.
    std::size_t skipped = 0;
    std::size_t scanned = 0;
    fs::recursive_directory_iterator entry(
        root, fs::directory_options::skip_permission_denied, ec);
    const fs::recursive_directory_iterator end;
    if (ec) {
        message = "Cannot enumerate Songs folder: " + ec.message();
        return false;
    }
    while (entry != end && scanned < 8192) {
        const fs::directory_entry current = *entry;
        entry.increment(ec);
        if (ec) {
            ++skipped;
            ec.clear();
        }
        const fs::path path = current.path();
        if (path.extension() != L".json" ||
            path.filename() == L"set.locus.json") continue;
        ++scanned;
        if (current.is_symlink(ec) || !current.is_regular_file(ec)) {
            ++skipped;
            ec.clear();
            continue;
        }
        core::LoadedChart chart;
        std::wstring audio;
        std::string problem;
        if (!OpenChartWithAudioPath(path.wstring(), chart, audio, problem)) {
            ++skipped;
            continue;
        }
        const fs::path audioFile = fs::canonical(fs::path(audio), ec);
        if (ec || !Inside(root, audioFile)) {
            ++skipped;
            ec.clear();
            continue;
        }

        // Managed Song Sets are grouped by their folder, not by incidental
        // audio-revision paths. Existing legacy folders remain untouched.
        const fs::path manifestPath = path.parent_path() / L"set.locus.json";
        const bool managed = fs::exists(manifestPath, ec);
        if (ec) { ++skipped; ec.clear(); continue; }
        SongSetManifest metadata;
        if (managed) {
            if (!LoadSongSetManifest(manifestPath, metadata, problem)) {
                ++skipped;
                continue;
            }
            const fs::path expected = path.parent_path() /
                fs::u8path(metadata.audioFile);
            if (!fs::equivalent(expected, audioFile, ec) || ec) {
                ++skipped;
                ec.clear();
                continue;
            }
        }
        const fs::path logicalAudio = managed
            ? path.parent_path() : LogicalSongAudioPath(audioFile);
        const auto found = byAudio.find(logicalAudio);
        std::size_t index = 0;
        if (found == byAudio.end()) {
            index = next.size();
            byAudio.emplace(logicalAudio, index);
            const std::string title = managed
                ? (metadata.artist.empty() ? metadata.title
                    : metadata.artist + " - " + metadata.title)
                : SongDisplayTitle(logicalAudio);
            next.push_back({audioFile, title, {}});
            next.back().id = managed ? metadata.id : HashText(logicalAudio);
            next.back().artist = managed ? metadata.artist : "";
            next.back().folder = path.parent_path();
            next.back().managed = managed;
            next.back().previewStartMs = managed ? metadata.previewStartMs : 0;
            next.back().backgroundImage = managed && !metadata.backgroundFile.empty()
                ? path.parent_path() / fs::u8path(metadata.backgroundFile) : fs::path{};
        } else {
            index = found->second;
        }
        next[index].charts.push_back({path, std::move(chart), audioFile});
    }
    if (scanned >= 8192) ++skipped;
    for (auto& song : next) {
        if (song.managed) {
            SongSetManifest timing;
            std::string problem;
            if (!LoadSongSetManifest(song.folder / L"set.locus.json",
                                     timing, problem)) {
                message = "Song Set timing could not be read: " + problem;
                return false;
            }
            // For older sets, choose a stable source chart by filename.
            // Existing chart files and their absolute timestamps are intact.
            const auto first = std::min_element(song.charts.begin(),
                song.charts.end(), [](const LibraryChart& a, const LibraryChart& b) {
                    return a.file < b.file;
                });
            if (first != song.charts.end()) {
                song.bpm = timing.bpm.value_or(first->data.route.bpm);
                song.offsetMs = timing.offsetMs.value_or(
                    static_cast<int>(std::llround(
                        first->data.offsetSeconds * 1000.0)));
                for (auto& difficulty : song.charts) {
                    if (!ApplySongTiming(difficulty.data, song.bpm,
                                         song.offsetMs, problem)) {
                        message = "Song Set " + song.title + ": " + problem;
                        return false;
                    }
                }
            }
        }
        for (auto& difficulty : song.charts)
            difficulty.difficulty = core::CalculateChartDifficulty(
                difficulty.data.route, difficulty.data.offsetSeconds);
        SortChartDifficulties(song.charts);
    }
    std::sort(next.begin(), next.end(),
              [](const LibrarySong& a, const LibrarySong& b) {
                  if (a.title != b.title) return a.title < b.title;
                  return a.audioFile < b.audioFile;
              });
    songs_ = std::move(next);
    message = "LIBRARY: " + std::to_string(songs_.size()) + " SONGS / " +
              std::to_string(ChartCount()) + " CHARTS";
    if (skipped) message += " (" + std::to_string(skipped) + " SKIPPED)";
    return true;
}

} // namespace locus::app
