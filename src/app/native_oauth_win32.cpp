#include "app/native_oauth.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <cstdint>
#include <utility>

namespace locus::app::oauth {
namespace {
using Bytes = std::array<unsigned char, 32>;

std::string Base64Url(const Bytes& bytes) {
    constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string result;
    result.reserve(43);
    std::uint32_t bits = 0;
    unsigned int count = 0;
    for (auto byte : bytes) {
        bits = (bits << 8) | byte;
        count += 8;
        while (count >= 6) {
            count -= 6;
            result.push_back(alphabet[(bits >> count) & 63U]);
        }
    }
    if (count != 0) result.push_back(alphabet[(bits << (6 - count)) & 63U]);
    return result;
}

std::optional<std::string> RandomSecret() {
    Bytes bytes{};
    if (BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) {
        SecureZeroMemory(bytes.data(), bytes.size());
        return std::nullopt;
    }
    auto secret = Base64Url(bytes);
    SecureZeroMemory(bytes.data(), bytes.size());
    return secret;
}

bool Unreserved(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~';
}
} // namespace

std::optional<std::string> PkceChallenge(std::string_view verifier) {
    if (verifier.size() < 43 || verifier.size() > 128) return std::nullopt;
    for (char c : verifier) if (!Unreserved(c)) return std::nullopt;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        return std::nullopt;
    Bytes hash{};
    // BCryptHash takes a non-const input pointer but does not modify the input.
    const auto status = BCryptHash(algorithm, nullptr, 0,
        reinterpret_cast<PUCHAR>(const_cast<char*>(verifier.data())),
        static_cast<ULONG>(verifier.size()), hash.data(), static_cast<ULONG>(hash.size()));
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (status < 0) return std::nullopt;
    return Base64Url(hash);
}

std::optional<AuthorizationSecrets> GenerateAuthorizationSecrets() {
    auto verifier = RandomSecret(), state = RandomSecret(), nonce = RandomSecret();
    if (!verifier || !state || !nonce) return std::nullopt;
    auto challenge = PkceChallenge(*verifier);
    if (!challenge) return std::nullopt;
    return AuthorizationSecrets{std::move(*verifier), std::move(*challenge),
                                std::move(*state), std::move(*nonce)};
}
} // namespace locus::app::oauth
