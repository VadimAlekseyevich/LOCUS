#pragma once
#include <optional>
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

#ifdef _WIN32
// Windows CNG supplies entropy and SHA-256. No fallback PRNG.
// A failure aborts the authorization flow.
std::optional<AuthorizationSecrets> GenerateAuthorizationSecrets();
std::optional<std::string> PkceChallenge(std::string_view verifier);
#endif
} // namespace locus::app::oauth
