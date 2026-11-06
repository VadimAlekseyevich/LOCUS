#include "app/song_library.h"

#include "app/chart_file.h"
#include "app/audio_revision.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

namespace locus::app {
namespace {
namespace fs = std::filesystem;

bool Inside(const fs::path& root, const fs::path& file) {
    auto a = root.begin(), b = file.begin();
    for (; a != root.end(); ++a, ++b) {
        if (b == file.end()) return false;
#ifdef _WIN32
        if (_wcsicmp(a->c_str(), b->c_str()) != 0) return false;
#else
        if (*a != *b) return false;
#endif
    }
    return b != file.end();
}

std::uint64_t PathHash(const fs::path& path) {
    std::uint64_t value = 14695981039346656037ull;
    const std::string name = path.generic_u8string();
    for (unsigned char byte : name) {
        value ^= byte;
        value *= 1099511628211ull;
    }
    return value;
}

std::string HashText(const fs::path& path) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16) << PathHash(path);
    return stream.str();
}

std::string FilenamePart(std::string raw) {
    std::string output;
    for (unsigned char c : raw) {
        if (std::isalnum(c) && c < 128) output += static_cast<char>(c);
        else if (c == '-' || c == '_') output += static_cast<char>(c);
        else if (!output.empty() && output.back() != '-') output += '-';
        if (output.size() >= 45) break;
    }
    while (!output.empty() && output.back() == '-') output.pop_back();
    return output.empty() ? "song" : output;
}

std::string SongDisplayTitle(const fs::path& audio) {
    const auto name = audio.stem().u8string();
    return name.empty() ? std::string("Unknown song") : name;
}

} // namespace

std::filesystem::path UserDataDirectory() {
#ifdef _WIN32
    const DWORD count = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
    if (count > 1 && count < 32768) {
        std::wstring value(count, L'\0');
        const DWORD copied = GetEnvironmentVariableW(L"LOCALAPPDATA",
                                                     value.data(), count);
        if (copied > 0 && copied < count) {
            value.resize(copied);
            return fs::path(value) / L"LOCUS";
        }
    }
#endif
    // Portable fallback if LOCALAPPDATA is unavailable.
    std::error_code ec;
    const auto dir = fs::current_path(ec);
    return (ec ? fs::path(".") : dir) / "LOCUS_Data";
}

std::size_t SongLibrary::ChartCount() const noexcept {
    std::size_t count = 0;
    for (const auto& song : songs_) count += song.charts.size();
    return count;
}

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
        if (path.extension() != L".json") continue;
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

        const fs::path logicalAudio = LogicalSongAudioPath(audioFile);
        const auto found = byAudio.find(logicalAudio);
        std::size_t index = 0;
        if (found == byAudio.end()) {
            index = next.size();
            byAudio.emplace(logicalAudio, index);
            next.push_back({audioFile, SongDisplayTitle(logicalAudio), {}});
        } else {
            index = found->second;
        }
        next[index].charts.push_back({path, std::move(chart), audioFile});
    }
    if (scanned >= 8192) ++skipped;
    for (auto& song : next) {
        std::sort(song.charts.begin(), song.charts.end(),
                  [](const LibraryChart& a, const LibraryChart& b) {
                      if (a.data.title != b.data.title)
                          return a.data.title < b.data.title;
                      return a.file < b.file;
                  });
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

bool SongLibrary::FindChart(const fs::path& path,
                            int& songIndex, int& chartIndex) const {
    for (std::size_t s = 0; s < songs_.size(); ++s)
        for (std::size_t c = 0; c < songs_[s].charts.size(); ++c) {
            if (songs_[s].charts[c].file == path) {
                songIndex = static_cast<int>(s);
                chartIndex = static_cast<int>(c);
                return true;
            }
        }
    return false;
}

bool SongLibrary::Import(const fs::path& sourceChart,
                         fs::path& installedChart, std::string& error) {
    core::LoadedChart source;
    std::wstring audio;
    if (!OpenChartWithAudioPath(sourceChart.wstring(), source, audio, error))
        return false;
    std::error_code ec;
    fs::create_directories(root_, ec);
    if (ec) { error = "Cannot create Songs folder: " + ec.message(); return false; }
    const auto root = fs::canonical(root_, ec);
    const auto actualChart = fs::canonical(sourceChart, ec);
    if (ec) { error = "Cannot resolve source chart path."; return false; }
    if (Inside(root, actualChart)) {
        std::string notice;
        if (!Refresh(notice)) { error = notice; return false; }
        int s = -1, c = -1;
        if (!FindChart(actualChart, s, c)) {
            error = "Existing library chart could not be indexed.";
            return false;
        }
        installedChart = actualChart;
        error.clear();
        return true;
    }

    const auto sourceAudio = fs::canonical(fs::path(audio), ec);
    if (ec || !fs::is_regular_file(sourceAudio, ec)) {
        error = "Cannot access song audio for importing.";
        return false;
    }
    const auto folder = root /
        (FilenamePart(sourceAudio.stem().u8string()) + "-" + HashText(sourceAudio));
    fs::create_directories(folder, ec);
    if (ec) { error = "Cannot create song folder: " + ec.message(); return false; }

    // Stage an immutable audio revision first, then atomically publish the
    // chart reference. An interrupted import cannot rewrite another
    // difficulty's existing audio file.
    fs::path destinationAudio;
    if (!InstallAudioRevision(sourceAudio, folder, destinationAudio, error))
        return false;

    // Stable source filename; changing a chart title updates the same library
    // difficulty rather than creating an accidental duplicate.
    const std::string difficulty =
        FilenamePart(actualChart.stem().u8string());
    installedChart = folder /
        (difficulty + "-" + HashText(actualChart) + ".locus.json");
    if (!SaveChartWithAudioPath(installedChart.wstring(),
                                destinationAudio.wstring(), source, error))
        return false;
    std::string notice;
    if (!Refresh(notice)) { error = notice; return false; }
    error.clear();
    return true;
}

} // namespace locus::app
