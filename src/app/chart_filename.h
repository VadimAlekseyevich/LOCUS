#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace locus::app {

// Turns an authored UTF-8 chart title into a safe default Windows filename.
// This only supplies the Save As suggestion; it never renames existing files.
inline std::wstring SuggestedChartFileName(std::string_view title) {
    std::wstring name;
    name.reserve(title.size());
    for (std::size_t i = 0; i < title.size() && name.size() < 100;) {
        const auto c = static_cast<unsigned char>(title[i]);
        std::uint32_t codepoint = 0;
        std::size_t count = 0;
        if (c < 0x80) {
            codepoint = c;
            count = 1;
        } else if ((c & 0xE0) == 0xC0) {
            codepoint = c & 0x1F;
            count = 2;
        } else if ((c & 0xF0) == 0xE0) {
            codepoint = c & 0x0F;
            count = 3;
        } else if ((c & 0xF8) == 0xF0) {
            codepoint = c & 0x07;
            count = 4;
        }
        bool valid = count > 0 && i + count <= title.size();
        if (valid) {
            for (std::size_t n = 1; n < count; ++n) {
                const auto next = static_cast<unsigned char>(title[i + n]);
                if ((next & 0xC0) != 0x80) { valid = false; break; }
                codepoint = (codepoint << 6) | (next & 0x3F);
            }
            valid &= !(count == 2 && codepoint < 0x80) &&
                     !(count == 3 && codepoint < 0x800) &&
                     !(count == 4 && codepoint < 0x10000) &&
                     !(codepoint >= 0xD800 && codepoint <= 0xDFFF) &&
                     codepoint <= 0x10FFFF;
        }
        if (!valid) {
            ++i;
            name.push_back(L'_');
            continue;
        }
        i += count;
        const bool forbidden = codepoint < 32 || codepoint == 127 ||
            codepoint == '<' || codepoint == '>' || codepoint == ':' ||
            codepoint == '"' || codepoint == '/' || codepoint == '\\' ||
            codepoint == '|' || codepoint == '?' || codepoint == '*';
        if (forbidden) {
            name.push_back(L'_');
        } else if (codepoint <= 0xFFFF) {
            name.push_back(static_cast<wchar_t>(codepoint));
        } else if (sizeof(wchar_t) == 2) {
            if (name.size() + 2 > 100) break;
            codepoint -= 0x10000;
            name.push_back(static_cast<wchar_t>(0xD800 + (codepoint >> 10)));
            name.push_back(static_cast<wchar_t>(0xDC00 + (codepoint & 0x3FF)));
        } else {
            name.push_back(static_cast<wchar_t>(codepoint));
        }
    }

    while (!name.empty() && (name.back() == L' ' || name.back() == L'.'))
        name.pop_back();
    while (!name.empty() && name.front() == L' ')
        name.erase(name.begin());
    if (name.empty()) name = L"Untitled";

    // CON, AUX, COM1, etc. are special device names even with an extension.
    std::wstring stem = name.substr(0, name.find(L'.'));
    for (auto& c : stem) if (c >= L'a' && c <= L'z') c -= L'a' - L'A';
    const bool reserved = stem == L"CON" || stem == L"PRN" || stem == L"AUX" ||
                          stem == L"NUL" ||
                          (stem.size() == 4 &&
                           ((stem.substr(0, 3) == L"COM") ||
                            (stem.substr(0, 3) == L"LPT")) &&
                           stem[3] >= L'1' && stem[3] <= L'9');
    if (reserved) name.insert(name.begin(), L'_');
    return name + L".locus.json";
}

} // namespace locus::app
