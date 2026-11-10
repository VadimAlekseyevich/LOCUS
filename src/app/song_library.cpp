#include "app/song_library_internal.h"
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
#include <sstream>
#include <system_error>
namespace locus::app {
namespace detail {
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

std::string HashSetId(const std::string& id) {
    // Manifest IDs are untrusted data. Hash raw UTF-8 bytes rather than
    // interpreting an arbitrary ID as a filesystem path on Windows.
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : id) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16) << hash;
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

void SortChartDifficulties(std::vector<LibraryChart>& charts) {
    std::sort(charts.begin(), charts.end(),
              [](const LibraryChart& a, const LibraryChart& b) {
                  if (a.difficulty.overall != b.difficulty.overall)
                      return a.difficulty.overall < b.difficulty.overall;
                  if (a.data.title != b.data.title)
                      return a.data.title < b.data.title;
                  return a.file < b.file;
              });
}


} // namespace detail
using namespace detail;
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



} // namespace locus::app
