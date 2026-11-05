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

} // namespace locus::app
