#include "app/native_oauth.h"
#include <map>
#include <utility>

namespace locus::app::oauth {
namespace {
int Hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::optional<std::string> Decode(std::string_view field) {
    std::string decoded;
    decoded.reserve(field.size());
    for (std::size_t i = 0; i < field.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(field[i]);
        if (c == '%') {
            if (field.size() - i < 3) return std::nullopt;
            const int hi = Hex(field[i + 1]), lo = Hex(field[i + 2]);
            if (hi < 0 || lo < 0) return std::nullopt;
            c = static_cast<unsigned char>(hi * 16 + lo);
            i += 2;
        } else if (c == '+') {
            c = ' ';
        }
        if (c < 0x20 || c == 0x7f) return std::nullopt;
        decoded.push_back(static_cast<char>(c));
    }
    return decoded;
}

bool VisibleAscii(std::string_view value, std::size_t maxSize) {
    if (value.empty() || value.size() > maxSize) return false;
    for (unsigned char c : value)
        if (c < 0x21 || c > 0x7e) return false;
    return true;
}

bool StateEquals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    unsigned int difference = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        difference |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    return difference == 0;
}
} // namespace

CallbackResult ParseCallback(std::string_view target, std::string_view expectedState,
                             std::string_view expectedIssuer) {
    if (target.size() < 3 || target.size() > 8192 ||
        target.substr(0, 2) != "/?" || target.find('#') != std::string_view::npos ||
        expectedState.size() != 43 || !VisibleAscii(expectedState, 43) ||
        !VisibleAscii(expectedIssuer, 512))
        return {};
    // Reject raw whitespace, controls and non-ASCII in the request line.
    if (!VisibleAscii(target, 8192)) return {};
    std::map<std::string, std::string, std::less<>> fields;
    target.remove_prefix(2);
    while (!target.empty()) {
        if (fields.size() >= 16) return {};
        const auto end = target.find('&');
        const auto field = target.substr(0, end);
        const auto equals = field.find('=');
        if (equals == std::string_view::npos) return {};
        auto key = Decode(field.substr(0, equals));
        auto value = Decode(field.substr(equals + 1));
        if (!key || !value || !VisibleAscii(*key, 64) ||
            !fields.emplace(std::move(*key), std::move(*value)).second)
            return {};
        if (end == std::string_view::npos) break;
        target.remove_prefix(end + 1);
        if (target.empty()) return {};
    }
    const auto state = fields.find("state"), issuer = fields.find("iss");
    if (state == fields.end() || issuer == fields.end() ||
        !StateEquals(state->second, expectedState) || issuer->second != expectedIssuer)
        return {};
    const auto code = fields.find("code"), error = fields.find("error");
    if (code != fields.end() && error != fields.end()) return {};
    if (error != fields.end() && VisibleAscii(error->second, 128))
        return {CallbackStatus::Denied, {}};
    if (code == fields.end() || !VisibleAscii(code->second, 2048)) return {};
    return {CallbackStatus::Code, std::move(code->second)};
}
} // namespace locus::app::oauth
