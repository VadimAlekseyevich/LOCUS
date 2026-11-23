#pragma once

#include "core/chart_json.h"

#include <array>
#include <cstdint>
#include <random>
#include <string>

namespace locus::app {

// New identities are allocated only when an authored difficulty is created or
// imported for the first time; a Save/Rename never calls this function.
// std::random_device obtains OS entropy on supported Windows toolchains.
inline std::string NewDifficultyId() {
    std::random_device entropy;
    constexpr char hex[] = "0123456789abcdef";
    std::string id(32, '0');
    for (int word = 0; word < 4; ++word) {
        const std::uint32_t value = static_cast<std::uint32_t>(entropy());
        for (int nibble = 0; nibble < 8; ++nibble)
            id[word * 8 + nibble] =
                hex[(value >> ((7 - nibble) * 4)) & 15U];
    }
    return id;
}

} // namespace locus::app
