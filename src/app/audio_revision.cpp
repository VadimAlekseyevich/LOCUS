#include "app/audio_revision.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <system_error>

namespace locus::app {
namespace {
namespace fs = std::filesystem;

// Keep the original filename for Unicode and song identity. The 16-hex-digit
// directory identifies the exact staged bytes; files within it are immutable.
constexpr const wchar_t* kVersions = L".locus-versions";
constexpr std::size_t kBlockSize = 65536;

bool HashStagedAudio(const fs::path& file, std::string& hex, std::string& error) {
    std::ifstream stream(file, std::ios::binary);
    if (!stream) { error = "Cannot read staged audio."; return false; }
    std::uint64_t hash = 14695981039346656037ull;
    std::array<char, kBlockSize> buffer{};
    while (stream) {
        stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto received = stream.gcount();
        for (std::streamsize i = 0; i < received; ++i) {
            hash ^= static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);
            hash *= 1099511628211ull;
        }
    }
    if (stream.bad()) { error = "Cannot finish reading staged audio."; return false; }
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << hash;
    hex = out.str();
    return true;
}

bool CompareBytes(const fs::path& staged, const fs::path& existing,
                  bool& identical, std::string& error) {
    identical = false;
    std::error_code ec;
    const auto firstSize = fs::file_size(staged, ec);
    if (ec) { error = "Cannot inspect staged audio."; return false; }
    const auto secondSize = fs::file_size(existing, ec);
    if (ec) { error = "Cannot inspect existing audio revision."; return false; }
    if (firstSize != secondSize) return true;
    std::ifstream a(staged, std::ios::binary), b(existing, std::ios::binary);
    if (!a || !b) { error = "Cannot compare audio revision bytes."; return false; }
    std::array<char, kBlockSize> left{}, right{};
    for (;;) {
        a.read(left.data(), static_cast<std::streamsize>(left.size()));
        b.read(right.data(), static_cast<std::streamsize>(right.size()));
        const auto count = a.gcount();
        if (count != b.gcount() ||
            std::memcmp(left.data(), right.data(),
                        static_cast<std::size_t>(count)) != 0) return true;
        if (count == 0) break;
    }
    if (a.bad() || b.bad()) { error = "Audio revision comparison failed."; return false; }
    identical = true;
    return true;
}

bool ValidFingerprintDirectory(const std::wstring& name) noexcept {
    if (name.size() != 16) return false;
    for (wchar_t c : name)
        if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f')))
            return false;
    return true;
}

} // namespace

fs::path LogicalSongAudioPath(const fs::path& actual) {
    const fs::path fingerprint = actual.parent_path();
    if (fingerprint.parent_path().filename() == kVersions &&
        ValidFingerprintDirectory(fingerprint.filename().wstring()))
        return fingerprint.parent_path().parent_path() / actual.filename();
    return actual;
}

bool InstallAudioRevision(const fs::path& source, const fs::path& songFolder,
                          fs::path& installed, std::string& error) {
    std::error_code ec;
    const fs::path root = songFolder / kVersions;
    fs::create_directories(root, ec);
    if (ec) {
        error = "Cannot create audio revisions folder: " + ec.message();
        return false;
    }

    // Copy before publishing. Hash the staged copy rather than the source:
    // changes to the source during import cannot mislabel the stored revision.
    static std::atomic<unsigned long long> sequence{0};
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    std::wstring suffix = L".stage-" + std::to_wstring(nonce) + L"-" +
                          std::to_wstring(sequence.fetch_add(1));
#ifdef _WIN32
    suffix += L"-" + std::to_wstring(GetCurrentProcessId());
#endif
    const fs::path staged = root / suffix;
    if (!fs::copy_file(source, staged, fs::copy_options::none, ec) || ec) {
        const std::string problem = ec ? ec.message() : "copy did not complete";
        std::error_code cleanup;
        fs::remove(staged, cleanup);
        error = "Failed to stage song audio: " + problem;
        return false;
    }
    const auto sourceTime = fs::last_write_time(source, ec);
    if (!ec) fs::last_write_time(staged, sourceTime, ec);
    if (ec) {
        const std::string problem = ec.message();
        std::error_code cleanup;
        fs::remove(staged, cleanup);
        error = "Cannot preserve song file timestamp: " + problem;
        return false;
    }

    std::string fingerprint;
    if (!HashStagedAudio(staged, fingerprint, error)) {
        std::error_code cleanup;
        fs::remove(staged, cleanup);
        return false;
    }
    const fs::path revisionFolder = root / fingerprint;
    fs::create_directories(revisionFolder, ec);
    if (ec) {
        const std::string problem = ec.message();
        std::error_code cleanup;
        fs::remove(staged, cleanup);
        error = "Cannot create revision folder: " + problem;
        return false;
    }
    const fs::path destination = revisionFolder / source.filename();

    // If a concurrent or repeated import published identical bytes, reuse
    // them; a (very unlikely) fingerprint collision must not overwrite audio.
    const auto reuse = [&]() -> bool {
        bool identical = false;
        const bool checked = CompareBytes(staged, destination, identical, error);
        std::error_code cleanup;
        fs::remove(staged, cleanup);
        if (!checked) return false;
        if (!identical) {
            error = "Audio fingerprint collision; existing audio preserved.";
            return false;
        }
        installed = destination;
        return true;
    };
    const bool exists = fs::exists(destination, ec);
    if (ec) {
        std::error_code cleanup;
        fs::remove(staged, cleanup);
        error = "Cannot inspect existing audio revision: " + ec.message();
        return false;
    }
    if (exists) return reuse();

#ifdef _WIN32
    // Deliberately omit MOVEFILE_REPLACE_EXISTING to preserve immutability.
    if (!MoveFileExW(staged.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH)) {
        const DWORD code = GetLastError();
        const bool published = fs::exists(destination, ec);
        if (published && !ec) return reuse();
        std::error_code cleanup;
        fs::remove(staged, cleanup);
        error = "Cannot publish audio revision (Windows error " +
                std::to_string(code) + ").";
        return false;
    }
#else
    // Same-volume link creation is no-replace; plain rename could overwrite.
    fs::create_hard_link(staged, destination, ec);
    if (ec) {
        const bool published = fs::exists(destination);
        if (published) return reuse();
        const std::string problem = ec.message();
        std::error_code cleanup;
        fs::remove(staged, cleanup);
        error = "Cannot publish audio revision: " + problem;
        return false;
    }
    fs::remove(staged, ec);
#endif
    installed = destination;
    return true;
}

} // namespace locus::app
