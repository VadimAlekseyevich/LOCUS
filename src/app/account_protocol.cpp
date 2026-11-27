#include "app/account_protocol.h"
#include "core/strict_json.h"
#include <algorithm>
#include <initializer_list>
#include <utility>

namespace locus::app::account {
namespace {
using Json = nlohmann::json;
constexpr std::string_view Scope = "locus.player";

bool Unreserved(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~';
}
bool SafeValue(std::string_view value, std::size_t min, std::size_t max) {
    return value.size() >= min && value.size() <= max &&
        std::all_of(value.begin(), value.end(), Unreserved);
}
bool Visible(std::string_view value, std::size_t max) {
    return !value.empty() && value.size() <= max && std::all_of(value.begin(), value.end(),
        [](unsigned char c) { return c >= 0x21 && c <= 0x7e; });
}
bool ValidRedirect(std::string_view redirect) {
    constexpr std::string_view prefix = "http://127.0.0.1:";
    if (redirect.substr(0, prefix.size()) != prefix) return false;
    redirect.remove_prefix(prefix.size());
    if (redirect.empty() || redirect.size() > 5 || redirect.front() == '0') return false;
    unsigned int port = 0;
    for (char c : redirect) {
        if (c < '0' || c > '9') return false;
        port = port * 10 + static_cast<unsigned>(c - '0');
    }
    return port > 0 && port <= 65535;
}
bool ValidEndpointPath(std::string_view path) {
    // Deliberately restrict private provider endpoints to simple absolute
    // paths; no escaping, queries, fragments or dot segments.
    if (!net::ValidRequestPath(path) || path.size() > 1024) return false;
    std::size_t start = 1;
    while (start <= path.size()) {
        const auto end = path.find('/', start);
        const auto part = path.substr(start, end == std::string_view::npos ? end : end - start);
        if (part.empty() || part == "." || part == ".." ||
            !std::all_of(part.begin(), part.end(), Unreserved)) return false;
        if (end == std::string_view::npos) return true;
        start = end + 1;
    }
    return false;
}
bool JsonType(std::string_view type) {
    const auto separator = type.find(';');
    auto media = type.substr(0, separator);
    while (!media.empty() && media.back() == ' ') media.remove_suffix(1);
    constexpr std::string_view json = "application/json";
    if (media.size() != json.size()) return false;
    for (std::size_t i = 0; i < json.size(); ++i) {
        const char c = media[i];
        if ((c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c) != json[i]) return false;
    }
    return true;
}
std::optional<Json> Object(const net::HttpResult& response, std::size_t limit) {
    if (response.error != net::HttpError::None || response.status != 200 ||
        !JsonType(response.contentType) || response.body.empty() || response.body.size() > limit)
        return {};
    std::string ignored;
    if (!core::detail::ValidateJsonStructure(response.body, ignored)) return {};
    auto value = Json::parse(response.body, nullptr, false);
    if (!value.is_object()) return {};
    return value;
}
const std::string* String(const Json& object, const char* key) {
    auto it = object.find(key);
    return it == object.end() ? nullptr : it->get_ptr<const std::string*>();
}
bool Includes(const Json& object, const char* key, std::string_view value) {
    auto it = object.find(key);
    if (it == object.end() || !it->is_array()) return false;
    for (const auto& entry : *it)
        if (entry.is_string() && entry.get_ref<const std::string&>() == value) return true;
    return false;
}
bool HasScope(std::string_view scopes) {
    while (!scopes.empty()) {
        const auto end = scopes.find(' ');
        if (scopes.substr(0, end) == Scope) return true;
        if (end == std::string_view::npos) break;
        scopes.remove_prefix(end + 1);
    }
    return false;
}
std::string Form(std::initializer_list<std::pair<std::string_view, std::string_view>> fields) {
    std::string form;
    for (auto field : fields) {
        if (!form.empty()) form += '&';
        form += net::FormEncode(field.first);
        form += '=';
        form += net::FormEncode(field.second);
    }
    return form;
}
std::optional<std::string> SameOriginPath(std::string_view url, std::string_view origin) {
    if (url.substr(0, origin.size()) != origin) return {};
    auto path = url.substr(origin.size());
    if (!ValidEndpointPath(path)) return {};
    return std::string(path);
}
bool ValidId(std::string_view id) {
    if (id.size() != 36 || id == "00000000-0000-0000-0000-000000000000") return false;
    for (std::size_t i = 0; i < id.size(); ++i) {
        char c = id[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) { if (c != '-') return false; }
        else if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}
}

std::optional<Provider> DiscoveryTarget(const Config& config) {
    if (config.issuer.size() > 512 || !net::ParseHttpsOrigin(config.apiOrigin) ||
        !SafeValue(config.clientId, 1, 128)) return {};
    const auto slash = config.issuer.find('/', 8);
    const auto origin = config.issuer.substr(0, slash);
    if (!net::ParseHttpsOrigin(origin)) return {};
    const auto path = slash == std::string::npos ? std::string{} : config.issuer.substr(slash);
    if (!path.empty() && !ValidEndpointPath(path)) return {};
    return Provider{origin, path + "/.well-known/openid-configuration", {}, {}, {}};
}

std::optional<Provider> ParseDiscovery(const Config& config, const net::HttpResult& response) {
    auto provider = DiscoveryTarget(config);
    const auto json = Object(response, DiscoveryLimit);
    if (!provider || !json) return {};
    const auto issuer = String(*json, "issuer"), auth = String(*json, "authorization_endpoint"),
        token = String(*json, "token_endpoint"), revoke = String(*json, "revocation_endpoint");
    auto issSupport = json->find("authorization_response_iss_parameter_supported");
    if (!issuer || *issuer != config.issuer || !auth || !token || !revoke ||
        issSupport == json->end() || *issSupport != true ||
        !Includes(*json, "code_challenge_methods_supported", "S256") ||
        !Includes(*json, "response_types_supported", "code")) return {};
    // Public client registration is trusted configuration. Keycloak does not
    // always advertise "none" in token_endpoint_auth_methods_supported;
    // the exchange never sends a client secret or falls back to another grant.
    auto a = SameOriginPath(*auth, provider->origin),
        t = SameOriginPath(*token, provider->origin),
        r = SameOriginPath(*revoke, provider->origin);
    if (!a || !t || !r || *a == *t || *a == *r || *t == *r) return {};
    provider->authorizationPath = std::move(*a);
    provider->tokenPath = std::move(*t);
    provider->revocationPath = std::move(*r);
    return provider;
}

std::optional<std::string> AuthorizationUrl(const Config& config, const Provider& provider,
    std::string_view redirect, const oauth::AuthorizationSecrets& secrets) {
    const auto expected = DiscoveryTarget(config);
    if (!expected || expected->origin != provider.origin ||
        !ValidEndpointPath(provider.authorizationPath) || !ValidRedirect(redirect) ||
        !SafeValue(secrets.challenge, 43, 43) || !SafeValue(secrets.state, 43, 43)) return {};
    // This is an OAuth API authorization flow, not OIDC identity consumption.
    // The only identity used by LOCUS is the resource server's /me response.
    return provider.origin + provider.authorizationPath + "?" + Form({
        {"client_id", config.clientId}, {"redirect_uri", redirect}, {"response_type", "code"},
        {"scope", Scope}, {"state", secrets.state}, {"code_challenge", secrets.challenge},
        {"code_challenge_method", "S256"}});
}

std::optional<std::string> CodeForm(const Config& config, std::string_view redirect,
    std::string_view code, std::string_view verifier) {
    if (!DiscoveryTarget(config) || !ValidRedirect(redirect) || !Visible(code, 2048) ||
        !SafeValue(verifier, 43, 128)) return {};
    return Form({{"grant_type", "authorization_code"}, {"client_id", config.clientId},
        {"redirect_uri", redirect}, {"code", code}, {"code_verifier", verifier}});
}
std::optional<std::string> RefreshForm(const Config& config, std::string_view refresh) {
    if (!DiscoveryTarget(config) || !net::ValidBearer(refresh)) return {};
    return Form({{"grant_type", "refresh_token"}, {"client_id", config.clientId},
        {"refresh_token", refresh}});
}
std::optional<std::string> RevocationForm(const Config& config, std::string_view refresh) {
    if (!DiscoveryTarget(config) || !net::ValidBearer(refresh)) return {};
    return Form({{"client_id", config.clientId}, {"token", refresh}, {"token_type_hint", "refresh_token"}});
}
std::optional<Tokens> ParseTokens(const net::HttpResult& response) {
    const auto json = Object(response, TokenLimit);
    if (!json || json->contains("error")) return {};
    auto access = String(*json, "access_token"), refresh = String(*json, "refresh_token"),
        type = String(*json, "token_type"), scope = String(*json, "scope");
    auto expiry = json->find("expires_in");
    if (!access || !refresh || *access == *refresh || !net::ValidBearer(*access) ||
        !net::ValidBearer(*refresh) || !type || (*type != "Bearer" && *type != "bearer") ||
        !scope || !HasScope(*scope) || expiry == json->end() ||
        !expiry->is_number_integer() || *expiry < 1 || *expiry > 600) return {};
    return Tokens{*access, *refresh, expiry->get<unsigned int>()};
}
std::optional<Player> ParsePlayer(const net::HttpResult& response) {
    const auto json = Object(response, PlayerLimit);
    if (!json) return {};
    auto id = String(*json, "id"), name = String(*json, "displayName");
    if (!id || !ValidId(*id) || !name || !SafeValue(*name, 3, 24)) return {};
    // Mirror the operator handle policy, not the local authoring identity.
    for (char c : *name) if (c == '-' || c == '.' || c == '~') return {};
    return Player{*id, *name};
}
} // namespace locus::app::account
