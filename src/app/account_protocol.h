#pragma once
#include "app/https_transport.h"
#include "app/native_oauth.h"
#include <optional>
#include <string>
#include <string_view>

namespace locus::app::account {
// Trusted application/operator configuration. Not supplied by a map or URL.
struct Config {
    std::string issuer;
    std::string apiOrigin;
    std::string clientId;
};
struct Provider {
    std::string origin;
    std::string discoveryPath;
    std::string authorizationPath;
    std::string tokenPath;
    std::string revocationPath;
};
struct Tokens {
    std::string access;
    std::string refresh;
    unsigned int expiresIn = 0;
};
struct Player {
    std::string id;
    std::string displayName;
};

constexpr std::size_t DiscoveryLimit = 64 * 1024;
constexpr std::size_t TokenLimit = 64 * 1024;
constexpr std::size_t PlayerLimit = 16 * 1024;

std::optional<Provider> DiscoveryTarget(const Config& config);
std::optional<Provider> ParseDiscovery(const Config& config, const net::HttpResult& response);
std::optional<std::string> AuthorizationUrl(const Config& config, const Provider& provider,
    std::string_view redirectUri, const oauth::AuthorizationSecrets& secrets);
std::optional<std::string> CodeForm(const Config& config, std::string_view redirectUri,
    std::string_view code, std::string_view verifier);
std::optional<std::string> RefreshForm(const Config& config, std::string_view refresh);
std::optional<std::string> RevocationForm(const Config& config, std::string_view refresh);
std::optional<Tokens> ParseTokens(const net::HttpResult& response);
std::optional<Player> ParsePlayer(const net::HttpResult& response);

enum class SignInStatus { Success, InvalidConfiguration, DiscoveryFailed, BrowserFailed,
    Denied, Cancelled, TimedOut, ExchangeFailed, PlayerRejected, TransportFailed };
struct Session {
    Player player;
    Tokens tokens;
    // Conservative monotonic deadline, measured from the token request start.
    std::chrono::steady_clock::time_point expiresAt;
};
struct SignInResult {
    SignInStatus status = SignInStatus::TransportFailed;
    std::optional<Session> session;
};
#ifdef _WIN32
// Blocking operation for the single account worker only. No UI thread calls,
// no automatic retry of an authorization code, no credential persistence.
SignInResult SignInWithBrowser(const Config& config, const std::atomic_bool& cancel);
#endif
} // namespace locus::app::account
