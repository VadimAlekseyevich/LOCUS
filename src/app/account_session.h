#pragma once
#include "app/account_protocol.h"
#include <condition_variable>
#include <mutex>
#include <thread>

namespace locus::app::account {
// This interface is used only by the session owner thread. Transport adapters
// may return a new session but must never publish a profile themselves.
class SessionBackend {
public:
    virtual ~SessionBackend() = default;
    virtual SignInResult SignIn(const std::atomic_bool& cancel) = 0;
    virtual SignInResult Restore(const std::atomic_bool&) { return {SignInStatus::NoSavedSession, {}}; }
    // Called only after the manager validates the new session; no access
    // token may be persisted. Default backend is volatile.
    virtual bool Remember(const Session&) { return true; }
    virtual bool Forget() { return true; }
    virtual void Close() noexcept {}
    virtual SignInResult Refresh(const Session& current, const std::atomic_bool& cancel) = 0;
    virtual bool Revoke(const Session& current, const std::atomic_bool& cancel) = 0;
};

enum class SessionPhase { SignedOut, Restoring, SigningIn, SignedIn, Refreshing, SigningOut, Failed };
struct SessionView {
    SessionPhase phase = SessionPhase::SignedOut;
    std::optional<Player> player;
    SignInStatus error = SignInStatus::Success;
    // True if the local session was discarded without confirmed remote revocation.
    bool revocationUnconfirmed = false;
    std::uint64_t revision = 0;
};

// One sleeping worker owns credentials and all network operations. The UI
// sees a credential-free copy, never waits for I/O, and can invalidate a
// pending operation immediately. Destruction cancels and joins the worker.
class SessionManager {
public:
    explicit SessionManager(std::unique_ptr<SessionBackend> backend);
    ~SessionManager();
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;
    bool BeginSignIn();
    bool BeginRestore();
    void SignOut();
    SessionView Snapshot() const;
private:
    void Run();
    void Publish(SessionPhase phase, std::optional<Player> player = {},
                 SignInStatus error = SignInStatus::Success, bool unconfirmed = false);
    std::unique_ptr<SessionBackend> backend_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    SessionView view_;
    bool shutdown_ = false;
    bool desired_ = false;
    bool restoring_ = false;
    bool logoutRequested_ = false;
    std::atomic_bool cancel_{false};
    std::atomic_bool stop_{false};
    std::thread worker_;
};

#ifdef _WIN32
std::unique_ptr<SessionBackend> CreateWindowsSessionBackend(Config config);
#endif
} // namespace locus::app::account
