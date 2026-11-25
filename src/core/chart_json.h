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
    // Persistent identity for one authored difficulty; never derived from the
    // filename, title or note content. Blank only in external/unsaved drafts.
    std::string difficultyId;
    std::string audioPath;
    double offsetSeconds = 0.0;
    double previewLeadBeats = 4.0;
};

// 128-bit lowercase hexadecimal identifier, generated when a difficulty is
// first installed. The parser and all persistence boundaries share this rule.
inline bool ValidDifficultyId(std::string_view id) noexcept {
    if (id.size() != 32) return false;
    for (char c : id)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            return false;
    return true;
}

// No filesystem or audio device required. Reject malformed/duplicate/unknown
// members instead of changing authored beat positions or directions.
bool ParseChartJson(std::string_view text, LoadedChart& result, std::string& error);

// Versions 4 (taps) and 5 (holds): exact timestamps and absolute headings.
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
