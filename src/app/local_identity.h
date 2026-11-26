#pragma once
#include <cstdint>
#include <random>
#include <string>
namespace locus::app {
// Opaque 128-bit local identity; generated once per new entity. Not a
// credential or proof of ownership. Supported toolchains use OS entropy.
inline std::string NewLocalId() {
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
