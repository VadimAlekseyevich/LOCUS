#pragma once

#include "core/chart_json.h"
#include "core/tempo_map.h"

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

namespace locus::core {

// Stable, platform-independent 128-bit *content fingerprint*, not an ID.
// Gameplay-affecting authored properties are included; presentation name,
// difficultyId, source filename and file layout are deliberately excluded.
// Recompute when launching PLAY or showing historical scores, never per frame.
inline std::string ChartGameplayRevision(const LoadedChart& chart) {
    std::uint64_t first = 14695981039346656037ULL;
    std::uint64_t second = 1099511628211ULL;
    const auto addByte = [&](std::uint8_t value) {
        first = (first ^ value) * 1099511628211ULL;
        second = (second ^ value) * 14029467366897019727ULL;
    };
    const auto add64 = [&](std::uint64_t value) {
        for (int byte = 0; byte < 8; ++byte)
            addByte(static_cast<std::uint8_t>(value >> (byte * 8)));
    };
    const auto addDouble = [&](double value) {
        std::uint64_t bits = 0;
        static_assert(sizeof(bits) == sizeof(value), "IEEE754 binary64 required");
        std::memcpy(&bits, &value, sizeof(bits));
        add64(bits);
    };
    add64(1); // fingerprint algorithm version
    const auto& route = chart.route;
    add64(static_cast<std::uint64_t>(route.gridWidth));
    add64(static_cast<std::uint64_t>(route.gridHeight));
    add64(static_cast<std::uint64_t>(route.start.x));
    add64(static_cast<std::uint64_t>(route.start.y));
    add64(static_cast<std::uint64_t>(route.initialHeading));
    // Grid resolution affects route traversal; editor snap selection does not.
    // Changing only snapping must never invalidate comparable scored attempts.
    add64(static_cast<std::uint64_t>(route.ticksPerBeat));
    // previewLeadBeats belongs to authoring/presentation only. PLAY computes
    // its scored lead-in from first-note time, not this preview setting.
    const auto timing = EffectiveTempoMap(route, chart.offsetSeconds);
    add64(timing.size());
    for (const auto& marker : timing) {
        add64(static_cast<std::uint64_t>(marker.timeMs));
        addDouble(marker.bpm);
    }
    add64(route.turns.size());
    for (const auto& turn : route.turns) {
        add64(static_cast<std::uint64_t>(
            EventTimeMs(route, turn, chart.offsetSeconds)));
        add64(static_cast<std::uint64_t>(turn.direction));
    }
    std::ostringstream text;
    text << std::hex << std::setfill('0')
         << std::setw(16) << first << std::setw(16) << second;
    return text.str();
}

} // namespace locus::core
