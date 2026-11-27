#include "app/account_protocol.h"
#include <cstdlib>
#include <iostream>
#include <string>
using namespace locus::app;
namespace {
void Check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
net::HttpResult Json(std::string body) {
    return {net::HttpError::None, 200, std::move(body), "application/json; charset=utf-8"};
}
std::string Replace(std::string text, const std::string& from, const std::string& to) {
    const auto i = text.find(from);
    Check(i != std::string::npos, "fixture string missing");
    text.replace(i, from.size(), to);
    return text;
}
}
int main() {
    const account::Config config{"https://id.example/realms/locus", "https://api.example", "locus-desktop"};
    const auto target = account::DiscoveryTarget(config);
    Check(target && target->origin == "https://id.example" &&
        target->discoveryPath == "/realms/locus/.well-known/openid-configuration", "discovery target");
    const std::string discovery = R"({
        "issuer":"https://id.example/realms/locus",
        "authorization_endpoint":"https://id.example/realms/locus/auth",
        "token_endpoint":"https://id.example/realms/locus/token",
        "revocation_endpoint":"https://id.example/realms/locus/revoke",
        "authorization_response_iss_parameter_supported":true,
        "code_challenge_methods_supported":["S256"],
        "response_types_supported":["code"],
        "token_endpoint_auth_methods_supported":["none"]
    })";
    auto provider = account::ParseDiscovery(config, Json(discovery));
    Check(provider && provider->tokenPath == "/realms/locus/token", "valid discovery");
    Check(bool(account::ParseDiscovery(config, Json(Replace(discovery, "\"none\"", "\"client_secret_basic\"")))),
        "Keycloak public client discovery compatibility");
    for (const auto& bad : {
        Replace(discovery, "https://id.example/realms/locus/token", "https://evil.example/token"),
        Replace(discovery, "https://id.example/realms/locus/token", "https://id.example.evil/token"),
        Replace(discovery, "https://id.example/realms/locus/token", "https://id.example/a/../token"),
        Replace(discovery, "https://id.example/realms/locus/token", "https://id.example/token?x"),
        Replace(discovery, "\"S256\"", "\"plain\""),
        Replace(discovery, ":true", ":false"),
        Replace(discovery, "\"issuer\":", "\"issuer\":\"https://evil.example\",\"issuer\":")
    }) Check(!account::ParseDiscovery(config, Json(bad)), "invalid discovery accepted");
    const oauth::AuthorizationSecrets secrets{std::string(43, 'v'), std::string(43, 'c'),
        std::string(43, 's'), std::string(43, 'n')};
    auto url = account::AuthorizationUrl(config, *provider, "http://127.0.0.1:12345", secrets);
    Check(url && url->find("scope=locus.player&") != std::string::npos &&
        url->find("code_challenge_method=S256") != std::string::npos &&
        url->find("verifier") == std::string::npos, "authorization URL");
    Check(!account::AuthorizationUrl(config, *provider, "http://localhost:12345", secrets), "wrong redirect");
    auto form = account::CodeForm(config, "http://127.0.0.1:12345", "code+&", secrets.verifier);
    Check(form && form->find("code=code%2B%26") != std::string::npos, "code form");
    Check(!account::CodeForm(config, "http://127.0.0.1:0", "code", secrets.verifier), "zero redirect port");
    Check(bool(account::RefreshForm(config, "refresh.token")) &&
        bool(account::RevocationForm(config, "refresh.token")), "refresh/revoke forms");
    const std::string tokens = R"({"access_token":"access.token","refresh_token":"refresh.token","token_type":"Bearer","scope":"locus.player","expires_in":120})";
    auto parsed = account::ParseTokens(Json(tokens));
    Check(parsed && parsed->expiresIn == 120 && parsed->access == "access.token", "token response");
    for (const auto& bad : {
        Replace(tokens, "120", "601"), Replace(tokens, "120", "-1"),
        Replace(tokens, "120", "1.5"), Replace(tokens, "locus.player", "locus.player.other"),
        Replace(tokens, "refresh.token", "access.token"), Replace(tokens, "Bearer", "Basic"),
        Replace(tokens, "\"access_token\":", "\"access_token\":\"other\",\"access_token\":")
    }) Check(!account::ParseTokens(Json(bad)), "invalid token response accepted");
    const std::string profile = R"({"id":"aaaaaaaa-0000-0000-0000-000000000001","displayName":"Test_Player","createdAt":"2026-01-01T00:00:00Z"})";
    Check(bool(account::ParsePlayer(Json(profile))), "server profile");
    auto denied = Json(profile); denied.status = 403;
    Check(!account::ParsePlayer(denied), "403 accepted");
    Check(!account::ParsePlayer(Json(Replace(profile, "Test_Player", "Bad Name"))), "invalid handle");
    return 0;
}
