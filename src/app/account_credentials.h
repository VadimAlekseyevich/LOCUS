#pragma once
#include "app/account_protocol.h"

namespace locus::app::account {
#ifdef _WIN32
enum class CredentialStatus { Ok, Missing, Busy, Failed };
struct SavedCredential {
    std::string playerId;
    std::string refresh;
};
// Worker-thread-only store. The global per-Windows-user lease serializes
// rotation across processes and terminal sessions. Local-machine persistence
// in Credential Manager is scoped to the current Windows account, not roaming.
class CredentialStore {
public:
    explicit CredentialStore(const Config& config);
    ~CredentialStore();
    CredentialStore(const CredentialStore&) = delete;
    CredentialStore& operator=(const CredentialStore&) = delete;
    CredentialStatus Acquire();
    CredentialStatus Consume(SavedCredential& credential);
    bool Save(std::string_view playerId, std::string_view refresh);
    bool Erase();
    void Close() noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
#endif
} // namespace locus::app::account
