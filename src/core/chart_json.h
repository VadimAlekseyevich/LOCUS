#pragma once

#include "core/route.h"

#include <string>
#include <string_view>

namespace locus::core {

// Chart metadata and route value object.
// Chart.lengthBeats is derived from song duration after loading.
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

// Version 3: absolute note timestamps and absolute outgoing compass headings.
bool SerializeChartJson(const LoadedChart& chart, std::string& json,
                        std::string& error);

// A song path must be normalized chart-relative: no drive, URL, slash root,
// empty, "." or ".." segments, or Windows backslashes.
bool ValidChartAudioPath(std::string_view path) noexcept;

// Derive playable beat coverage from audio time [0,duration), BPM and offset.
 // A positive offset can produce negative beat indices before beat zero.
bool ComputeSongBeatRange(double bpm, double offsetSeconds, double durationSeconds,
                          int& firstBeat, int& endBeat, std::string& error);

// Resolve the complete song range. Reject notes outside real audio time,
 // invalid song lengths, and pathological beat counts.
bool PrepareChartForSong(LoadedChart& chart, double songDurationSeconds,
                         std::string& error);

} // namespace locus::core
