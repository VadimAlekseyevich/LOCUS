#include "app/account_protocol.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <utility>

namespace locus::app::account {
namespace {
using namespace std::chrono_literals;
SignInResult Failure(SignInStatus status) { return {status, {}}; }
SignInStatus NetworkStatus(const net::HttpResult& result, SignInStatus fallback) {
    if (result.error == net::HttpError::Cancelled) return SignInStatus::Cancelled;
    if (result.error == net::HttpError::TimedOut) return SignInStatus::TimedOut;
    return fallback;
}
bool LaunchBrowser(const std::string& url) {
    const std::wstring wide(url.begin(), url.end());
    auto result = ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
}
}
SignInResult SignInWithBrowser(const Config& config, const std::atomic_bool& cancel) {
    if (cancel.load(std::memory_order_relaxed)) return Failure(SignInStatus::Cancelled);
    const auto target = DiscoveryTarget(config);
    if (!target) return Failure(SignInStatus::InvalidConfiguration);
    auto issuer = net::HttpsTransport::Open(target->origin);
    auto api = net::HttpsTransport::Open(config.apiOrigin);
    if (!issuer || !api) return Failure(SignInStatus::TransportFailed);
    const auto discovery = issuer->Get(target->discoveryPath, {}, DiscoveryLimit, cancel);
    const auto provider = ParseDiscovery(config, discovery);
    if (!provider) return Failure(NetworkStatus(discovery, SignInStatus::DiscoveryFailed));
    auto secrets = oauth::GenerateAuthorizationSecrets();
    auto listener = oauth::LoopbackListener::Open();
    if (!secrets || !listener) return Failure(SignInStatus::TransportFailed);
    const auto redirect = listener->RedirectUri();
    const auto url = AuthorizationUrl(config, *provider, redirect, *secrets);
    if (!url) return Failure(SignInStatus::InvalidConfiguration);
    if (cancel.load(std::memory_order_relaxed)) return Failure(SignInStatus::Cancelled);
    if (!LaunchBrowser(*url)) return Failure(SignInStatus::BrowserFailed);
    auto callback = listener->Wait(secrets->state, config.issuer, cancel, 5min);
    listener.reset();
    switch (callback.status) {
    case oauth::WaitStatus::Denied: return Failure(SignInStatus::Denied);
    case oauth::WaitStatus::Cancelled: return Failure(SignInStatus::Cancelled);
    case oauth::WaitStatus::TimedOut: return Failure(SignInStatus::TimedOut);
    case oauth::WaitStatus::Failed: return Failure(SignInStatus::TransportFailed);
    case oauth::WaitStatus::Code: break;
    }
    auto form = CodeForm(config, redirect, callback.code, secrets->verifier);
    SecureZeroMemory(callback.code.data(), callback.code.size());
    SecureZeroMemory(secrets->verifier.data(), secrets->verifier.size());
    if (!form) return Failure(SignInStatus::ExchangeFailed);
    const auto issuedBefore = std::chrono::steady_clock::now();
    auto exchange = issuer->PostForm(provider->tokenPath, *form, TokenLimit, cancel);
    SecureZeroMemory(form->data(), form->size());
    auto tokens = ParseTokens(exchange);
    SecureZeroMemory(exchange.body.data(), exchange.body.size());
    if (!tokens) return Failure(NetworkStatus(exchange, SignInStatus::ExchangeFailed));
    const auto profile = api->Get("/api/v1/me", tokens->access, PlayerLimit, cancel);
    auto player = ParsePlayer(profile);
    if (!player || cancel.load(std::memory_order_relaxed)) {
        SecureZeroMemory(tokens->access.data(), tokens->access.size());
        SecureZeroMemory(tokens->refresh.data(), tokens->refresh.size());
        return Failure(cancel.load(std::memory_order_relaxed) ? SignInStatus::Cancelled :
            NetworkStatus(profile, profile.status == 401 || profile.status == 403 ?
                SignInStatus::PlayerRejected : SignInStatus::TransportFailed));
    }
    const auto expiresAt = issuedBefore + std::chrono::seconds(tokens->expiresIn);
    if (expiresAt <= std::chrono::steady_clock::now()) {
        SecureZeroMemory(tokens->access.data(), tokens->access.size());
        SecureZeroMemory(tokens->refresh.data(), tokens->refresh.size());
        return Failure(SignInStatus::TimedOut);
    }
    return {SignInStatus::Success, Session{std::move(*player), std::move(*tokens), expiresAt}};
}
} // namespace locus::app::account
