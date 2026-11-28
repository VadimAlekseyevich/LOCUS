#include "app/account_session.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <utility>

namespace locus::app::account {
namespace {
using Clock = std::chrono::steady_clock;
void Clear(std::string& value) {
    SecureZeroMemory(value.data(), value.size());
    value.clear();
}
SignInStatus Error(const net::HttpResult& result, SignInStatus fallback) {
    if (result.error == net::HttpError::Cancelled) return SignInStatus::Cancelled;
    if (result.error == net::HttpError::TimedOut) return SignInStatus::TimedOut;
    return fallback;
}

class WindowsBackend final : public SessionBackend {
public:
    explicit WindowsBackend(Config config) : config_(std::move(config)) {}
    SignInResult SignIn(const std::atomic_bool& cancel) override {
        // Reset cached endpoints between interactive sessions.
        provider_.reset();
        issuer_.reset();
        api_.reset();
        return SignInWithBrowser(config_, cancel);
    }
    SignInResult Refresh(const Session& current, const std::atomic_bool& cancel) override {
        if (!Connect(cancel)) return {SignInStatus::DiscoveryFailed, {}};
        auto form = RefreshForm(config_, current.tokens.refresh);
        if (!form) return {SignInStatus::ExchangeFailed, {}};
        const auto start = Clock::now();
        auto response = issuer_->PostForm(provider_->tokenPath, *form, TokenLimit, cancel);
        Clear(*form);
        auto tokens = ParseTokens(response);
        Clear(response.body);
        if (!tokens) return {Error(response, SignInStatus::ExchangeFailed), {}};
        const auto expiry = start + std::chrono::seconds(tokens->expiresIn);
        // Even on profile failure return the rotated credential to the owner
        // for cleanup. It will never publish a failed result.
        Session next{current.player, std::move(*tokens), expiry};
        if (next.tokens.refresh == current.tokens.refresh)
            return {SignInStatus::ExchangeFailed, std::move(next)};
        const auto profile = api_->Get("/api/v1/me", next.tokens.access, PlayerLimit, cancel);
        auto player = ParsePlayer(profile);
        if (!player)
            return {Error(profile, profile.status == 401 || profile.status == 403 ?
                SignInStatus::PlayerRejected : SignInStatus::TransportFailed), std::move(next)};
        next.player = std::move(*player);
        return {SignInStatus::Success, std::move(next)};
    }
    bool Revoke(const Session& current, const std::atomic_bool& cancel) override {
        if (!Connect(cancel)) return false;
        auto form = RevocationForm(config_, current.tokens.refresh);
        if (!form) return false;
        auto result = issuer_->PostForm(provider_->revocationPath, *form, TokenLimit, cancel);
        Clear(*form);
        Clear(result.body);
        return result.error == net::HttpError::None && result.status == 200;
    }
private:
    bool Connect(const std::atomic_bool& cancel) {
        if (provider_ && issuer_ && api_) return true;
        const auto target = DiscoveryTarget(config_);
        if (!target) return false;
        auto issuer = net::HttpsTransport::Open(target->origin);
        auto api = net::HttpsTransport::Open(config_.apiOrigin);
        if (!issuer || !api) return false;
        auto provider = ParseDiscovery(config_, issuer->Get(target->discoveryPath, {}, DiscoveryLimit, cancel));
        if (!provider) return false;
        provider_ = std::move(provider);
        issuer_ = std::move(issuer);
        api_ = std::move(api);
        return true;
    }
    Config config_;
    std::optional<Provider> provider_;
    std::unique_ptr<net::HttpsTransport> issuer_, api_;
};
}
std::unique_ptr<SessionBackend> CreateWindowsSessionBackend(Config config) {
    return std::make_unique<WindowsBackend>(std::move(config));
}
} // namespace locus::app::account
