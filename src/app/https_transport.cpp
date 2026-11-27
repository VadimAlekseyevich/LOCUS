#include "app/https_transport.h"

namespace locus::app::net {
namespace {
bool Alnum(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}
bool Hex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}
bool ValidHost(std::string_view host) {
    if (host.empty() || host.size() > 253 || host.back() == '.') return false;
    while (!host.empty()) {
        const auto dot = host.find('.');
        auto label = host.substr(0, dot);
        if (label.empty() || label.size() > 63 || !Alnum(label.front()) || !Alnum(label.back())) return false;
        for (char c : label) if (!Alnum(c) && c != '-') return false;
        if (dot == std::string_view::npos) return true;
        host.remove_prefix(dot + 1);
    }
    return false;
}
}

std::optional<HttpsOrigin> ParseHttpsOrigin(std::string_view url) {
    constexpr std::string_view scheme = "https://";
    if (url.size() <= scheme.size() || url.size() > 270 ||
        url.substr(0, scheme.size()) != scheme) return std::nullopt;
    url.remove_prefix(scheme.size());
    const auto colon = url.find(':');
    auto host = url.substr(0, colon);
    if (!ValidHost(host)) return std::nullopt;
    unsigned port = 443;
    if (colon != std::string_view::npos) {
        const auto value = url.substr(colon + 1);
        if (value.empty() || value.size() > 5 || value.front() == '0') return std::nullopt;
        port = 0;
        for (char c : value) {
            if (c < '0' || c > '9') return std::nullopt;
            port = port * 10 + static_cast<unsigned>(c - '0');
        }
        if (port == 0 || port > 65535) return std::nullopt;
    }
    return HttpsOrigin{std::string(host), static_cast<std::uint16_t>(port)};
}

bool ValidRequestPath(std::string_view path) {
    if (path.empty() || path.size() > 8192 || path.front() != '/' ||
        (path.size() > 1 && path[1] == '/')) return false;
    for (std::size_t i = 0; i < path.size(); ++i) {
        const char c = path[i];
        if (c == '%') {
            if (path.size() - i < 3 || !Hex(path[i + 1]) || !Hex(path[i + 2])) return false;
            i += 2;
        } else if (!Alnum(c) && std::string_view("-._~!$&'()*+,;=:@/?").find(c) == std::string_view::npos) {
            return false;
        }
    }
    return true;
}

bool ValidBearer(std::string_view token) {
    if (token.empty() || token.size() > 16384) return false;
    bool padding = false, data = false;
    for (char c : token) {
        if (c == '=') { padding = true; continue; }
        if (padding || (!Alnum(c) && std::string_view("-._~+/").find(c) == std::string_view::npos)) return false;
        data = true;
    }
    return data;
}

std::string FormEncode(std::string_view value) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string encoded;
    for (unsigned char c : value) {
        if (Alnum(static_cast<char>(c)) || c == '-' || c == '_' || c == '.' || c == '~')
            encoded.push_back(static_cast<char>(c));
        else if (c == ' ') encoded.push_back('+');
        else {
            encoded.push_back('%');
            encoded.push_back(hex[c >> 4]);
            encoded.push_back(hex[c & 15]);
        }
    }
    return encoded;
}
} // namespace locus::app::net
