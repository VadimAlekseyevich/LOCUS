#pragma once
#include <optional>
#include <atomic>
#include <chrono>
#include <memory>
#include <cstdint>
#include <string>
#include <string_view>

namespace locus::app::oauth {
struct AuthorizationSecrets {
    std::string verifier;
    std::string challenge;
    std::string state;
    std::string nonce;
};

enum class CallbackStatus { Invalid, Denied, Code };
struct CallbackResult {
    CallbackStatus status = CallbackStatus::Invalid;
    std::string code;
};

// Accept only the root loopback request target from the pending flow.
// Require the exact issuer (RFC 9207) and state; duplicate fields fail.
// Never expose provider error descriptions to logs or UI.
CallbackResult ParseCallback(std::string_view target, std::string_view expectedState,
                             std::string_view expectedIssuer);

// Parse a complete, bounded HTTP/1.1 loopback request. No body is allowed.
CallbackResult ParseHttpCallback(std::string_view request, std::uint16_t port,
                                 std::string_view state, std::string_view issuer);

#ifdef _WIN32
enum class WaitStatus { Code, Denied, Cancelled, TimedOut, Failed };
struct WaitResult {
    WaitStatus status = WaitStatus::Failed;
    std::string code;
};

// Owns an exclusive, ephemeral IPv4 loopback socket. Open before launching
// the browser; run Wait once on the account worker, never the render thread.
class LoopbackListener {
public:
    static std::unique_ptr<LoopbackListener> Open();
    ~LoopbackListener();
    LoopbackListener(const LoopbackListener&) = delete;
    LoopbackListener& operator=(const LoopbackListener&) = delete;
    std::string RedirectUri() const;
    WaitResult Wait(std::string_view state, std::string_view issuer,
                    const std::atomic_bool& cancelled, std::chrono::milliseconds timeout);
private:
    struct Impl;
    explicit LoopbackListener(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

// Windows CNG supplies entropy and SHA-256. No fallback PRNG.
// A failure aborts the authorization flow.
std::optional<AuthorizationSecrets> GenerateAuthorizationSecrets();
std::optional<std::string> PkceChallenge(std::string_view verifier);
#endif
} // namespace locus::app::oauth
