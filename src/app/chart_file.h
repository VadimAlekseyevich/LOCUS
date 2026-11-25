#pragma once

#include "core/chart_json.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace locus::app {

// Large enough to round-trip the parser's full 65,536-note chart limit,
// including indented JSON. Used consistently by load/save/cache checking.
inline constexpr std::size_t kMaxChartFileBytes = 8U * 1024U * 1024U;

// Open UTF-8 chart JSON from a Unicode Windows path, resolve its normalized
// chart-relative local song path, reject symlinks/junctions that escape the
// chart directory. Does not open the audio device or mutate the active game.
bool OpenChartWithAudioPath(const std::wstring& chartFile,
                            core::LoadedChart& chart,
                            std::wstring& absoluteAudioPath,
                            std::string& error,
                            const std::uint64_t* expectedFingerprint = nullptr,
                            std::uint64_t* actualFingerprint = nullptr,
                            std::size_t* actualBytes = nullptr);

// Replace updates an existing chart after a recoverable backup. CreateNew
// publishes a difficulty without ever overwriting another writer's file.
enum class ChartWriteMode { Replace, CreateNew };

// Atomic on-volume Windows save. Audio must lie in the selected chart folder
// or its descendants. An exclusive staging file is flushed before publish.
bool SaveChartWithAudioPath(const std::wstring& chartFile,
                            const std::wstring& audioFile,
                            const core::LoadedChart& draft,
                            std::string& error,
                            bool folderLockAlreadyHeld = false,
                            ChartWriteMode mode = ChartWriteMode::Replace);

} // namespace locus::app
