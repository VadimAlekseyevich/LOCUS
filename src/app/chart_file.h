#pragma once

#include "core/chart_json.h"

#include <string>

namespace locus::app {

// Open UTF-8 chart JSON from a Unicode Windows path, resolve its normalized
// chart-relative local song path, reject symlinks/junctions that escape the
// chart directory. Does not open the audio device or mutate the active game.
bool OpenChartWithAudioPath(const std::wstring& chartFile,
                            core::LoadedChart& chart,
                            std::wstring& absoluteAudioPath,
                            std::string& error);

// Atomic on-volume Windows save: serialize, write a temporary file and
// replace the destination only after the new bytes have been flushed.
// Audio must lie in the selected chart folder or its descendants.
bool SaveChartWithAudioPath(const std::wstring& chartFile,
                            const std::wstring& audioFile,
                            const core::LoadedChart& draft,
                            std::string& error,
                            bool folderLockAlreadyHeld = false);

} // namespace locus::app
