#include "app/song_archive.h"
#include "app/song_set_storage.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace locus::app {
namespace {
namespace fs = std::filesystem;

constexpr std::uint32_t kLocal = 0x04034b50;
constexpr std::uint32_t kCentral = 0x02014b50;
constexpr std::uint32_t kEnd = 0x06054b50;
constexpr std::uint16_t kUtf8 = 0x0800;
constexpr std::uint64_t kMaxFile = 256ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t kMaxArchive = 320ULL * 1024ULL * 1024ULL;
constexpr std::size_t kMaxEntries = 256;

struct ZipEntry {
    fs::path file;
    std::string name;
    std::uint32_t size = 0;
    std::uint32_t crc = 0;
    std::uint32_t offset = 0;
};

bool FlatName(const std::string& s) {
    if (s.empty() || s.size() > 255 || s == "." || s == "..") return false;
    for (unsigned char c : s)
        if (c < 32 || c == 127 || c == '/' || c == '\\' || c == ':')
            return false;
    return true;
}

void Put16(std::ostream& out, std::uint16_t n) {
    out.put(static_cast<char>(n & 255));
    out.put(static_cast<char>((n >> 8) & 255));
}
void Put32(std::ostream& out, std::uint32_t n) {
    Put16(out, static_cast<std::uint16_t>(n));
    Put16(out, static_cast<std::uint16_t>(n >> 16));
}
std::uint16_t Get16(std::istream& in) {
    unsigned char s[2]{};
    if (!in.read(reinterpret_cast<char*>(s), 2))
        throw std::runtime_error("Truncated archive header.");
    return static_cast<std::uint16_t>(s[0] | (s[1] << 8));
}
std::uint32_t Get32(std::istream& in) {
    const std::uint32_t a = Get16(in), b = Get16(in);
    return a | (b << 16);
}
std::uint32_t CrcUpdate(std::uint32_t crc, const char* bytes,
                        std::size_t length) {
    for (std::size_t i = 0; i < length; ++i) {
        crc ^= static_cast<unsigned char>(bytes[i]);
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0u);
    }
    return crc;
}
std::uint32_t CrcFile(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot read export file.");
    std::array<char, 65536> block{};
    std::uint32_t crc = 0xffffffffu;
    while (input) {
        input.read(block.data(), static_cast<std::streamsize>(block.size()));
        const auto count = input.gcount();
        if (count > 0)
            crc = CrcUpdate(crc, block.data(),
                            static_cast<std::size_t>(count));
    }
    if (input.bad()) throw std::runtime_error("Cannot hash export file.");
    return ~crc;
}
void Transfer(std::istream& input, std::ostream& output, std::uint32_t bytes,
              std::uint32_t expectedCrc) {
    std::array<char, 65536> block{};
    std::uint32_t crc = 0xffffffffu;
    std::uint32_t remaining = bytes;
    while (remaining) {
        const auto count = static_cast<std::streamsize>(
            std::min<std::uint32_t>(remaining,
                                   static_cast<std::uint32_t>(block.size())));
        if (!input.read(block.data(), count))
            throw std::runtime_error("Archive payload is truncated.");
        output.write(block.data(), count);
        if (!output) throw std::runtime_error("Cannot write archive payload.");
        crc = CrcUpdate(crc, block.data(), static_cast<std::size_t>(count));
        remaining -= static_cast<std::uint32_t>(count);
    }
    if ((~crc) != expectedCrc)
        throw std::runtime_error("Archive CRC mismatch or changed source bytes.");
}
std::wstring Suffix() {
    return L".tmp-" + std::to_wstring(
        std::chrono::steady_clock::now().time_since_epoch().count());
}
bool IsWithin(const fs::path& root, const fs::path& file) {
    auto a = root.begin(), b = file.begin();
    for (; a != root.end(); ++a, ++b)
        if (b == file.end() || *a != *b) return false;
    return b != file.end();
}
std::string FileUtf8Name(const fs::path& path) {
    return path.filename().u8string();
}
} // namespace

bool ExportSongSetArchive(const fs::path& setFolder,
                           const std::vector<fs::path>& chartFiles,
                           const fs::path& archiveFile, std::string& error) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path folder = fs::canonical(setFolder, ec);
    if (ec) { error = "Cannot open Song Set for export."; return false; }
    SongSetManifest metadata;
    if (!LoadSongSetManifest(folder / L"set.locus.json", metadata, error))
        return false;
    if (chartFiles.empty() || chartFiles.size() + 2 > kMaxEntries) {
        error = "Song Set has no difficulties or too many files.";
        return false;
    }
    std::vector<fs::path> files{folder / L"set.locus.json",
                                folder / fs::u8path(metadata.audioFile)};
    files.insert(files.end(), chartFiles.begin(), chartFiles.end());
    std::vector<ZipEntry> entries;
    std::set<std::string> names;
    std::uint64_t combined = 0;
    for (const fs::path& file : files) {
        const fs::path absolute = fs::canonical(file, ec);
        if (ec || !IsWithin(folder, absolute) ||
            !fs::is_regular_file(absolute, ec) || ec) {
            error = "Export contains an inaccessible or escaping file.";
            return false;
        }
        const std::string name = FileUtf8Name(file);
        if (!FlatName(name) || !names.insert(name).second) {
            error = "Export has an unsafe or duplicate filename.";
            return false;
        }
        const auto size = fs::file_size(file, ec);
        if (ec || size > kMaxFile || size > 0xffffffffu ||
            (combined += size) > kMaxArchive) {
            error = "Song Set exceeds LOCUS package limits.";
            return false;
        }
        entries.push_back({absolute, name, static_cast<std::uint32_t>(size),
                           0, 0});
    }
    // Reject damaged destination directories before starting an export.
    if (!fs::exists(archiveFile.parent_path(), ec) || ec) {
        error = "Export folder does not exist.";
        return false;
    }
    const fs::path staged = archiveFile.wstring() + Suffix();
    try {
        std::ofstream out(staged, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("Cannot open temporary archive.");
        for (auto& entry : entries) {
            const auto pos = out.tellp();
            if (pos < 0 || static_cast<std::uint64_t>(pos) > 0xffffffffu)
                throw std::runtime_error("Archive exceeds ZIP size limit.");
            entry.offset = static_cast<std::uint32_t>(pos);
            entry.crc = CrcFile(entry.file);
            Put32(out, kLocal);
            Put16(out, 20); Put16(out, kUtf8); Put16(out, 0);
            Put16(out, 0); Put16(out, 0);
            Put32(out, entry.crc);
            Put32(out, entry.size); Put32(out, entry.size);
            Put16(out, static_cast<std::uint16_t>(entry.name.size()));
            Put16(out, 0);
            out.write(entry.name.data(),
                      static_cast<std::streamsize>(entry.name.size()));
            std::ifstream input(entry.file, std::ios::binary);
            if (!input) throw std::runtime_error("Cannot reopen export file.");
            Transfer(input, out, entry.size, entry.crc);
        }
        const auto dirOffsetPos = out.tellp();
        if (dirOffsetPos < 0 ||
            static_cast<std::uint64_t>(dirOffsetPos) > 0xffffffffu)
            throw std::runtime_error("Archive central directory exceeds ZIP limit.");
        const auto dirOffset = static_cast<std::uint32_t>(dirOffsetPos);
        for (const auto& entry : entries) {
            Put32(out, kCentral);
            Put16(out, 20); Put16(out, 20);
            Put16(out, kUtf8); Put16(out, 0);
            Put16(out, 0); Put16(out, 0);
            Put32(out, entry.crc);
            Put32(out, entry.size); Put32(out, entry.size);
            Put16(out, static_cast<std::uint16_t>(entry.name.size()));
            Put16(out, 0); Put16(out, 0);
            Put16(out, 0); Put16(out, 0);
            Put32(out, 0); Put32(out, entry.offset);
            out.write(entry.name.data(),
                      static_cast<std::streamsize>(entry.name.size()));
        }
        const auto endPos = out.tellp();
        if (endPos < 0 || static_cast<std::uint64_t>(endPos) > 0xffffffffu)
            throw std::runtime_error("Archive exceeds ZIP size limit.");
        Put32(out, kEnd);
        Put16(out, 0); Put16(out, 0);
        Put16(out, static_cast<std::uint16_t>(entries.size()));
        Put16(out, static_cast<std::uint16_t>(entries.size()));
        Put32(out, static_cast<std::uint32_t>(endPos) - dirOffset);
        Put32(out, dirOffset); Put16(out, 0);
        out.flush();
        if (!out) throw std::runtime_error("Cannot finish writing archive.");
        out.close();
        if (!out) throw std::runtime_error("Cannot close archive.");
#ifdef _WIN32
        if (!MoveFileExW(staged.c_str(), archiveFile.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot publish exported archive.");
#else
        fs::rename(staged, archiveFile, ec);
        if (ec) throw std::runtime_error("Cannot publish exported archive.");
#endif
        error.clear();
        return true;
    } catch (const std::exception& e) {
        fs::remove(staged, ec);
        error = e.what();
        return false;
    }
}

bool ExtractSongSetArchive(const fs::path& archiveFile,
                           const fs::path& stagingFolder, std::string& error) {
    std::error_code ec;
    const auto totalBytes = fs::file_size(archiveFile, ec);
    if (ec || totalBytes > kMaxArchive || totalBytes < 22) {
        error = "Archive is missing or exceeds the package size limit.";
        return false;
    }
    if (!fs::is_directory(stagingFolder, ec) || ec ||
        !fs::is_empty(stagingFolder, ec)) {
        error = "Extraction folder is invalid or not empty.";
        return false;
    }
    try {
        std::ifstream input(archiveFile, std::ios::binary);
        if (!input) throw std::runtime_error("Cannot open Song Set archive.");
        std::vector<ZipEntry> entries;
        std::set<std::string> names;
        std::uint64_t expanded = 0;
        std::uint32_t marker = Get32(input);
        while (marker == kLocal) {
            if (entries.size() >= kMaxEntries)
                throw std::runtime_error("Too many archive entries.");
            const auto version = Get16(input), flags = Get16(input);
            const auto method = Get16(input);
            (void)Get16(input); (void)Get16(input); // DOS date/time
            const auto crc = Get32(input);
            const auto packed = Get32(input), unpacked = Get32(input);
            const auto nameLength = Get16(input), extraLength = Get16(input);
            if (version > 20 || flags != kUtf8 || method != 0 ||
                packed != unpacked || unpacked > kMaxFile ||
                nameLength == 0 || nameLength > 255 || extraLength > 4096 ||
                (expanded += unpacked) > kMaxArchive)
                throw std::runtime_error("Unsupported or oversized ZIP entry.");
            std::string name(nameLength, '\0');
            if (!input.read(name.data(), nameLength))
                throw std::runtime_error("Truncated archive filename.");
            if (!FlatName(name) || !names.insert(name).second)
                throw std::runtime_error("Unsafe or duplicate archive filename.");
            input.seekg(extraLength, std::ios::cur);
            if (!input) throw std::runtime_error("Truncated archive extra data.");
            const auto afterHeader = input.tellg();
            const std::uint64_t off = static_cast<std::uint64_t>(afterHeader) -
                                     nameLength - extraLength - 30u;
            if (off > 0xffffffffu)
                throw std::runtime_error("Invalid ZIP offset.");
            const fs::path extracted = stagingFolder / fs::u8path(name);
            std::error_code existsError;
            const bool occupied = fs::exists(extracted, existsError);
            if (existsError || occupied)
                throw std::runtime_error(
                    "ZIP contains duplicate filesystem names or invalid paths.");
            std::ofstream output(extracted, std::ios::binary | std::ios::trunc);
            if (!output) throw std::runtime_error("Cannot extract ZIP file.");
            Transfer(input, output, unpacked, crc);
            output.flush();
            if (!output) throw std::runtime_error("Cannot flush extracted file.");
            entries.push_back({extracted, name, unpacked, crc,
                               static_cast<std::uint32_t>(off)});
            marker = Get32(input);
        }
        if (marker != kCentral || entries.empty())
            throw std::runtime_error("Missing archive central directory.");
        const auto dirOffsetPos = input.tellg();
        if (dirOffsetPos < 4)
            throw std::runtime_error("Invalid ZIP central directory offset.");
        const auto dirOffset = static_cast<std::uint32_t>(
            static_cast<std::streamoff>(dirOffsetPos) - 4);
        for (std::size_t i = 0; i < entries.size(); ++i) {
            if (i != 0 && Get32(input) != kCentral)
                throw std::runtime_error("Unexpected central directory entry.");
            (void)Get16(input); // made by
            const auto version = Get16(input), flags = Get16(input);
            const auto method = Get16(input);
            (void)Get16(input); (void)Get16(input);
            const auto crc = Get32(input);
            const auto packed = Get32(input), unpacked = Get32(input);
            const auto nameLength = Get16(input), extraLength = Get16(input);
            const auto commentLength = Get16(input);
            const auto disk = Get16(input);
            (void)Get16(input); (void)Get32(input); // attrs
            const auto offset = Get32(input);
            if (version > 20 || flags != kUtf8 || method != 0 ||
                disk != 0 || nameLength == 0 || nameLength > 255 ||
                extraLength > 4096 || commentLength > 4096)
                throw std::runtime_error("Unsupported ZIP central directory.");
            std::string name(nameLength, '\0');
            if (!input.read(name.data(), nameLength))
                throw std::runtime_error("Truncated central filename.");
            input.seekg(extraLength + commentLength, std::ios::cur);
            if (!input) throw std::runtime_error("Truncated central ZIP extras.");
            const auto& expected = entries[i];
            if (name != expected.name || packed != expected.size ||
                unpacked != expected.size || crc != expected.crc ||
                offset != expected.offset)
                throw std::runtime_error("ZIP directory mismatches payload.");
        }
        const auto centralEnd = input.tellg();
        if (Get32(input) != kEnd)
            throw std::runtime_error("Missing ZIP end record.");
        const auto disk = Get16(input), startDisk = Get16(input);
        const auto onDisk = Get16(input), total = Get16(input);
        const auto centralSize = Get32(input), centralOffset = Get32(input);
        const auto comment = Get16(input);
        if (disk != 0 || startDisk != 0 || comment != 0 ||
            onDisk != entries.size() || total != entries.size() ||
            centralOffset != dirOffset ||
            centralSize != static_cast<std::uint32_t>(
                static_cast<std::streamoff>(centralEnd) -
                static_cast<std::streamoff>(dirOffset)) ||
            input.tellg() != static_cast<std::streamoff>(totalBytes))
            throw std::runtime_error("ZIP end record is inconsistent.");
        error.clear();
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

} // namespace locus::app
