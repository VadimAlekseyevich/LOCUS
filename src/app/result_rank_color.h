#pragma once

#include "core/judgement.h"
#include "raylib.h"

namespace locus::app {

// Native raylib fallback uses the same palette as common.rcss.
inline constexpr Color ResultRankColor(core::ResultRank rank) noexcept {
    switch (rank) {
        case core::ResultRank::SS: return {255,240,174,255};
        case core::ResultRank::S: return {255,213,127,255};
        case core::ResultRank::A: return {156,240,187,255};
        case core::ResultRank::B: return {148,215,255,255};
        case core::ResultRank::C: return {198,164,255,255};
        case core::ResultRank::D: return {243,156,174,255};
        default: return {190,196,215,255};
    }
}

} // namespace locus::app
