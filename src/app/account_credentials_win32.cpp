#include "app/account_credentials.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincred.h>
#include <sddl.h>
#include <vector>
#include <utility>

namespace locus::app::account {
namespace {
bool PlayerId(std::string_view id) {
    if (id.size() != 36 || id == "00000000-0000-0000-0000-000000000000") return false;
    for (std::size_t i = 0; i < id.size(); ++i) {
        const char c = id[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) { if (c != '-') return false; }
        else if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}
std::wstring Wide(std::string_view value) {
    return {value.begin(), value.end()};
}
std::wstring UserMutexName() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return {};
    struct TokenGuard { HANDLE value; ~TokenGuard() { CloseHandle(value); } } guard{token};
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    if (!size || size > 65536) return {};
    std::vector<BYTE> bytes(size);
    if (!GetTokenInformation(token, TokenUser, bytes.data(), size, &size)) return {};
    LPWSTR sid = nullptr;
    if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(bytes.data())->User.Sid, &sid)) return {};
    struct SidGuard { LPWSTR value; ~SidGuard() { LocalFree(value); } } sidGuard{sid};
    return std::wstring(L"Global\\LOCUS.Account.v1.") + sid;
}
void Clear(std::string& value) {
    SecureZeroMemory(value.data(), value.size());
    value.clear();
}
}
struct CredentialStore::Impl {
    std::wstring target;
    HANDLE mutex = nullptr;
    DWORD owner = 0;
};

CredentialStore::CredentialStore(const Config& config) : impl_(std::make_unique<Impl>()) {
    if (DiscoveryTarget(config))
        impl_->target = Wide("LOCUS/account/v1|" + config.issuer + "|" + config.apiOrigin + "|" + config.clientId);
}
CredentialStore::~CredentialStore() { Close(); }

CredentialStatus CredentialStore::Acquire() {
    if (impl_->owner == GetCurrentThreadId()) return CredentialStatus::Ok;
    if (impl_->owner || impl_->target.empty()) return CredentialStatus::Failed;
    const auto name = UserMutexName();
    if (name.empty()) return CredentialStatus::Failed;
    HANDLE mutex = CreateMutexW(nullptr, FALSE, name.c_str());
    if (!mutex) return CredentialStatus::Failed;
    const auto result = WaitForSingleObject(mutex, 0);
    if (result != WAIT_OBJECT_0 && result != WAIT_ABANDONED) {
        CloseHandle(mutex);
        return result == WAIT_TIMEOUT ? CredentialStatus::Busy : CredentialStatus::Failed;
    }
    impl_->mutex = mutex;
    impl_->owner = GetCurrentThreadId();
    return CredentialStatus::Ok;
}

bool CredentialStore::Erase() {
    if (impl_->owner != GetCurrentThreadId()) return false;
    if (CredDeleteW(impl_->target.c_str(), CRED_TYPE_GENERIC, 0)) return true;
    return GetLastError() == ERROR_NOT_FOUND;
}
CredentialStatus CredentialStore::Consume(SavedCredential& credential) {
    Clear(credential.refresh);
    credential.playerId.clear();
    if (impl_->owner != GetCurrentThreadId()) return CredentialStatus::Failed;
    PCREDENTIALW record = nullptr;
    if (!CredReadW(impl_->target.c_str(), CRED_TYPE_GENERIC, 0, &record))
        return GetLastError() == ERROR_NOT_FOUND ? CredentialStatus::Missing : CredentialStatus::Failed;
    struct Guard {
        PCREDENTIALW value;
        ~Guard() {
            if (value->CredentialBlob) SecureZeroMemory(value->CredentialBlob, value->CredentialBlobSize);
            CredFree(value);
        }
    } guard{record};
    std::string id, refresh;
    if (record->UserName) {
        const std::wstring_view name(record->UserName);
        if (name.size() == 36) {
            for (wchar_t c : name) {
                if (c > 127) { id.clear(); break; }
                id += static_cast<char>(c);
            }
        }
    }
    if (record->CredentialBlob && record->CredentialBlobSize > 0 &&
        record->CredentialBlobSize <= CRED_MAX_CREDENTIAL_BLOB_SIZE)
        refresh.assign(reinterpret_cast<const char*>(record->CredentialBlob), record->CredentialBlobSize);
    // Consume before sending a rotating token. A crash or ambiguous network
    // failure can require login again, but never replays the saved token.
    const bool erased = Erase();
    if (!erased || !PlayerId(id) || !net::ValidBearer(refresh)) {
        Clear(refresh);
        return CredentialStatus::Failed;
    }
    credential = {std::move(id), std::move(refresh)};
    return CredentialStatus::Ok;
}
bool CredentialStore::Save(std::string_view id, std::string_view refresh) {
    if (impl_->owner != GetCurrentThreadId() || !PlayerId(id) || !net::ValidBearer(refresh) ||
        refresh.size() > CRED_MAX_CREDENTIAL_BLOB_SIZE) return false;
    auto name = Wide(id);
    CREDENTIALW record{};
    record.Type = CRED_TYPE_GENERIC;
    record.TargetName = impl_->target.data();
    record.UserName = name.data();
    record.CredentialBlobSize = static_cast<DWORD>(refresh.size());
    record.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(refresh.data()));
    record.Persist = CRED_PERSIST_LOCAL_MACHINE;
    return CredWriteW(&record, 0) != FALSE;
}
void CredentialStore::Close() noexcept {
    if (!impl_->mutex) return;
    // The session worker always closes its own lease before joining.
    if (impl_->owner == GetCurrentThreadId()) ReleaseMutex(impl_->mutex);
    CloseHandle(impl_->mutex);
    impl_->mutex = nullptr;
    impl_->owner = 0;
}
} // namespace locus::app::account
