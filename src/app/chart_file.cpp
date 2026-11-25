#include "app/chart_file.h"
#include "app/chart_file_fingerprint.h"

#include "app/local_file_io.h"
#include "app/path_containment.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <cctype>
#include <cwctype>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace locus::app {
namespace {

#ifdef _WIN32
bool Utf8ToWide(const std::string& utf8, std::wstring& wide) {
    const int count = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, utf8.c_str(),
        static_cast<int>(utf8.size()), nullptr, 0);
    if (count <= 0) return false;
    wide.resize(static_cast<std::size_t>(count));
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.c_str(),
                               static_cast<int>(utf8.size()), wide.data(), count) == count;
}
#endif


} // namespace

bool OpenChartWithAudioPath(const std::wstring& chartFile,
                            core::LoadedChart& chart,
                            std::wstring& absoluteAudioPath,
                            std::string& error,
                            const std::uint64_t* expectedFingerprint,
                            std::uint64_t* actualFingerprint,
                            std::size_t* actualBytes) {
    if (actualBytes) *actualBytes = 0;
#ifdef _WIN32
    std::vector<std::uint8_t> bytes;
    if (!ReadLocalFileBytes(chartFile, bytes, error, kMaxChartFileBytes)) {
        error = "Chart file: " + error;
        return false;
    }
    if (actualBytes) *actualBytes = bytes.size();
    const std::string_view contents(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    if (expectedFingerprint || actualFingerprint) {
        const auto fingerprint = ChartContentFingerprint(contents);
        if (actualFingerprint) *actualFingerprint = fingerprint;
        if (expectedFingerprint && fingerprint != *expectedFingerprint) {
            error = "Difficulty changed on disk; refresh the library.";
            return false;
        }
    }
    core::LoadedChart parsed;
    if (!core::ParseChartJson(contents,parsed,error)) {
        return false;
    }

    std::wstring relative;
    if (!Utf8ToWide(parsed.audioPath, relative)) {
        error = "audioPath: invalid UTF-8 path";
        return false;
    }
    std::error_code status;
    const auto root = std::filesystem::canonical(
        std::filesystem::path(chartFile).parent_path(), status);
    if (status) {
        error = "Chart directory could not be accessed.";
        return false;
    }
    const auto resolved = std::filesystem::canonical(
        root / std::filesystem::path(relative), status);
    if (status) {
        error = "audioPath: referenced song does not exist or cannot be accessed";
        return false;
    }
    if (!IsPathWithinDirectory(root, resolved)) {
        error = "audioPath: resolved file escapes the chart folder";
        return false;
    }
    if (!std::filesystem::is_regular_file(resolved, status) || status) {
        error = "audioPath: referenced song is not a regular file";
        return false;
    }
    chart = std::move(parsed);
    absoluteAudioPath = resolved.wstring();
    error.clear();
    return true;
#else
    (void)chartFile;
    (void)chart;
    (void)absoluteAudioPath;
    (void)expectedFingerprint;
    (void)actualFingerprint;
    error = "Chart files require Windows.";
    return false;
#endif
}

} // namespace locus::app
