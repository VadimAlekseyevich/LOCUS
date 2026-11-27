#include "app/native_oauth.h"

namespace locus::app::oauth {
namespace {
bool EqualAsciiCase(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
    for (std::size_t i = 0; i < a.size(); ++i)
        if (lower(a[i]) != lower(b[i])) return false;
    return true;
}

bool HeaderName(std::string_view name) {
    if (name.empty()) return false;
    for (char c : name) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            std::string_view("!#$%&'*+-.^`_|~").find(c) != std::string_view::npos)
            continue;
        return false;
    }
    return true;
}
} // namespace

CallbackResult ParseHttpCallback(std::string_view request, std::uint16_t port,
                                 std::string_view state, std::string_view issuer) {
    if (port == 0 || request.size() > 16384 || request.size() < 4 ||
        request.find("\r\n\r\n") != request.size() - 4) return {};
    const auto first = request.find("\r\n");
    if (first == std::string_view::npos || first > 8205) return {};
    const auto line = request.substr(0, first);
    constexpr std::string_view prefix = "GET ", suffix = " HTTP/1.1";
    if (line.size() <= prefix.size() + suffix.size() ||
        line.substr(0, prefix.size()) != prefix ||
        line.substr(line.size() - suffix.size()) != suffix) return {};
    const auto target = line.substr(prefix.size(), line.size() - prefix.size() - suffix.size());
    request.remove_prefix(first + 2);
    bool hostSeen = false, lengthSeen = false;
    unsigned int count = 0;
    while (request != "\r\n") {
        const auto end = request.find("\r\n");
        if (end == std::string_view::npos || ++count > 64) return {};
        const auto header = request.substr(0, end);
        const auto colon = header.find(':');
        if (colon == std::string_view::npos || !HeaderName(header.substr(0, colon))) return {};
        const auto name = header.substr(0, colon);
        auto value = header.substr(colon + 1);
        for (unsigned char c : value)
            if ((c < 0x20 && c != '\t') || c == 0x7f) return {};
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1);
        if (EqualAsciiCase(name, "Host")) {
            if (hostSeen || value != "127.0.0.1:" + std::to_string(port)) return {};
            hostSeen = true;
        } else if (EqualAsciiCase(name, "Content-Length")) {
            if (lengthSeen || value != "0") return {};
            lengthSeen = true;
        } else if (EqualAsciiCase(name, "Transfer-Encoding") ||
                   EqualAsciiCase(name, "Expect") || EqualAsciiCase(name, "Upgrade")) {
            return {};
        }
        request.remove_prefix(end + 2);
    }
    if (!hostSeen) return {};
    return ParseCallback(target, state, issuer);
}
} // namespace locus::app::oauth
