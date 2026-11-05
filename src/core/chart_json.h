#pragma once

#include "core/route.h"

#include <string>
#include <string_view>

namespace locus::core {

// v1 authoring data plus the already-shared route value object.
// Chart.lengthBeats is derived from the selected song length after loading.
struct LoadedChart {
    Chart route;
    std::string title;
    std::string audioPath;
    double offsetSeconds = 0.0;
    double previewLeadBeats = 4.0;
};

// No filesystem or audio device required. Reject malformed/duplicate/unknown
// members instead of changing authored beat positions or directions.
bool ParseChartJson(std::string_view text, LoadedChart& result, std::string& error);

// A song path must be normalized chart-relative: no drive, URL, slash root,
// empty, "." or ".." segments, or Windows backslashes.
bool ValidChartAudioPath(std::string_view path) noexcept;

// Resolve the number of authored steps against a real audio duration. Reject
// events after song end, invalid song lengths, and pathological beat counts.
bool PrepareChartForSong(LoadedChart& chart, double songDurationSeconds,
                         std::string& error);

} // namespace locus::core
