#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace locus::core::utf8 {

// Strict UTF-8 decoding: rejects truncated sequences, overlong encodings,
// surrogate halves and code points above U+10FFFF.
inline bool Next(std::string_view text, std::size_t& offset,
                 std::uint32_t& point) noexcept {
    if (offset >= text.size()) return false;
    const auto first = static_cast<unsigned char>(text[offset]);
    if (first <= 0x7f) {
        point = first;
        ++offset;
        return true;
    }
    unsigned count = 0;
    std::uint32_t scalar = 0;
    if (first >= 0xc2 && first <= 0xdf) {
        count = 2; scalar = first & 0x1f;
    } else if (first >= 0xe0 && first <= 0xef) {
        count = 3; scalar = first & 0x0f;
    } else if (first >= 0xf0 && first <= 0xf4) {
        count = 4; scalar = first & 0x07;
    } else return false;

    if (offset + count > text.size()) return false;
    for (unsigned i = 1; i < count; ++i) {
        const auto part = static_cast<unsigned char>(text[offset + i]);
        if ((part & 0xc0) != 0x80) return false;
        scalar = (scalar << 6) | (part & 0x3f);
    }
    if ((count == 3 && scalar < 0x800) ||
        (count == 4 && scalar < 0x10000) ||
        (scalar >= 0xd800 && scalar <= 0xdfff) ||
        scalar > 0x10ffff) return false;
    point = scalar;
    offset += count;
    return true;
}

inline std::string Encode(std::uint32_t cp) {
    if (cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return {};
    std::string output;
    if (cp < 0x80) output += static_cast<char>(cp);
    else if (cp < 0x800) {
        output += static_cast<char>(0xc0 | (cp >> 6));
        output += static_cast<char>(0x80 | (cp & 0x3f));
    } else if (cp < 0x10000) {
        output += static_cast<char>(0xe0 | (cp >> 12));
        output += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
        output += static_cast<char>(0x80 | (cp & 0x3f));
    } else {
        output += static_cast<char>(0xf0 | (cp >> 18));
        output += static_cast<char>(0x80 | ((cp >> 12) & 0x3f));
        output += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
        output += static_cast<char>(0x80 | (cp & 0x3f));
    }
    return output;
}

inline bool Valid(std::string_view text) noexcept {
    std::size_t offset = 0;
    while (offset < text.size()) {
        std::uint32_t cp = 0;
        if (!Next(text, offset, cp)) return false;
    }
    return true;
}

inline std::size_t Count(std::string_view text) noexcept {
    std::size_t offset = 0, count = 0;
    while (offset < text.size()) {
        std::uint32_t cp = 0;
        if (!Next(text, offset, cp)) return 0;
        ++count;
    }
    return count;
}

inline bool IsControl(std::uint32_t cp) noexcept {
    return cp < 32 || (cp >= 0x7f && cp <= 0x9f);
}

inline bool ValidName(std::string_view name, std::size_t maxCodepoints,
                      std::size_t maxBytes) noexcept {
    if (name.empty() || name.size() > maxBytes) return false;
    std::size_t offset = 0, count = 0;
    bool visible = false;
    while (offset < name.size()) {
        std::uint32_t cp = 0;
        if (!Next(name, offset, cp) || IsControl(cp) || ++count > maxCodepoints)
            return false;
        // Reject entirely blank names, including ordinary Unicode spaces.
        if (cp != ' ' && cp != 0xa0 && cp != 0x2007 &&
            cp != 0x202f && !(cp >= 0x2000 && cp <= 0x200a))
            visible = true;
    }
    return visible;
}

// Truncation counts Unicode scalar values, not bytes. Never split a sequence.
inline std::string Truncate(std::string_view text, std::size_t maxCodepoints) {
    if (!Valid(text)) return {};
    if (Count(text) <= maxCodepoints) return std::string(text);
    if (maxCodepoints == 0) return {};
    const std::size_t characters = maxCodepoints > 3 ? maxCodepoints - 3 : 0;
    std::size_t offset = 0;
    for (std::size_t i = 0; i < characters; ++i) {
        std::uint32_t cp = 0;
        if (!Next(text, offset, cp)) break;
    }
    return std::string(text.substr(0, offset)) +
           std::string(maxCodepoints >= 3 ? 3 : maxCodepoints, '.');
}

// Paste and character input are atomic per Unicode scalar. Invalid clipboard
// contents do not enter the draft; reaching a limit stops at a valid boundary.
inline bool AppendLimited(std::string& target, std::string_view incoming,
                          std::size_t maxCodepoints, std::size_t maxBytes) {
    if (!Valid(target) || !Valid(incoming)) return false;
    std::size_t count = Count(target), offset = 0;
    bool changed = false;
    while (offset < incoming.size()) {
        const std::size_t start = offset;
        std::uint32_t cp = 0;
        if (!Next(incoming, offset, cp)) return changed;
        if (IsControl(cp)) continue;
        if (count >= maxCodepoints ||
            target.size() + offset - start > maxBytes) break;
        target.append(incoming.data() + start, offset - start);
        ++count;
        changed = true;
    }
    return changed;
}

inline void PopBack(std::string& target) noexcept {
    if (target.empty()) return;
    std::size_t start = target.size() - 1;
    while (start > 0 &&
           (static_cast<unsigned char>(target[start]) & 0xc0) == 0x80)
        --start;
    target.erase(start);
}

} // namespace locus::core::utf8
