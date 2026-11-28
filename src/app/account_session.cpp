#include "app/account_session.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace locus::app::account {
namespace {
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

// Best-effort clearing of the worker's owned token buffers. This does not
// claim to erase every transient allocator/transport copy of a credential.
void Clear(std::string& value) noexcept {
    volatile char* bytes = value.data();
    for (std::size_t i = 0; i < value.size(); ++i) bytes[i] = 0;
    value.clear();
}
void Discard(std::optional<Session>& session) noexcept {
    if (session) {
        Clear(session->tokens.access);
        Clear(session->tokens.refresh);
        session.reset();
    }
}
bool Usable(const Session& session) {
    return !session.player.id.empty() && !session.player.displayName.empty() &&
        net::ValidBearer(session.tokens.access) && net::ValidBearer(session.tokens.refresh) &&
        session.tokens.expiresIn >= 30 && session.tokens.expiresIn <= 600 &&
        session.expiresAt > Clock::now() + 5s;
}
Clock::time_point RefreshAt(const Session& session) {
    return session.expiresAt - std::min(30s, std::chrono::seconds(session.tokens.expiresIn / 2));
}
}

SessionManager::SessionManager(std::unique_ptr<SessionBackend> backend)
    : backend_(std::move(backend)) {
    if (!backend_) throw std::invalid_argument("Account session backend required");
    worker_ = std::thread([this] { Run(); });
}
SessionManager::~SessionManager() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
        desired_ = false;
        cancel_.store(true, std::memory_order_relaxed);
        stop_.store(true, std::memory_order_relaxed);
    }
    wake_.notify_one();
    worker_.join();
}
void SessionManager::Publish(SessionPhase phase, std::optional<Player> player,
                             SignInStatus error, bool unconfirmed) {
    view_ = {phase, std::move(player), error, unconfirmed, view_.revision + 1};
}
bool SessionManager::BeginSignIn() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (shutdown_ || (view_.phase != SessionPhase::SignedOut && view_.phase != SessionPhase::Failed))
        return false;
    cancel_.store(false, std::memory_order_relaxed);
    desired_ = true;
    Publish(SessionPhase::SigningIn);
    wake_.notify_one();
    return true;
}
void SessionManager::SignOut() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (view_.phase == SessionPhase::SignedOut || view_.phase == SessionPhase::SigningOut) return;
    if (view_.phase == SessionPhase::Failed) {
        Publish(SessionPhase::SignedOut, {}, SignInStatus::Success, view_.revocationUnconfirmed);
        return;
    }
    desired_ = false;
    cancel_.store(true, std::memory_order_relaxed);
    // Clear the public identity now, even if a blocking transport call has
    // not yet returned. No new login until the old operation is drained.
    Publish(SessionPhase::SigningOut);
    wake_.notify_one();
}
SessionView SessionManager::Snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return view_;
}

void SessionManager::Run() {
    std::optional<Session> session;
    std::unique_lock<std::mutex> lock(mutex_);
    auto revoke = [&](const Session& value) {
        try { return backend_->Revoke(value, stop_); }
        catch (...) { return false; }
    };
    while (!shutdown_) {
        if (!desired_) {
            if (view_.phase == SessionPhase::SigningOut) {
                lock.unlock();
                const bool confirmed = !session || revoke(*session);
                Discard(session);
                lock.lock();
                if (!shutdown_) Publish(SessionPhase::SignedOut, {}, SignInStatus::Success, !confirmed);
                continue;
            }
            wake_.wait(lock, [this] { return shutdown_ || desired_ || view_.phase == SessionPhase::SigningOut; });
            continue;
        }

        const bool refreshing = session.has_value();
        if (refreshing) {
            if (wake_.wait_until(lock, RefreshAt(*session), [this] { return shutdown_ || !desired_; }))
                continue;
            Publish(SessionPhase::Refreshing, session->player);
        }
        lock.unlock();
        SignInResult result;
        try {
            result = refreshing ? backend_->Refresh(*session, cancel_) : backend_->SignIn(cancel_);
        } catch (...) {
            result.status = SignInStatus::TransportFailed;
        }
        lock.lock();
        if (shutdown_) {
            Discard(result.session);
            break;
        }
        if (!desired_) {
            // Sign-out won the race. Keep any newly rotated credential
            // exclusively for revocation; never publish its identity.
            if (result.session) {
                Discard(session);
                session = std::move(result.session);
            }
            continue;
        }
        const bool wrongOwner = refreshing && result.session &&
            session->player.id != result.session->player.id;
        const bool reusedRefresh = refreshing && result.session &&
            session->tokens.refresh == result.session->tokens.refresh;
        if (result.status == SignInStatus::Success && result.session && !wrongOwner &&
            !reusedRefresh && Usable(*result.session)) {
            Discard(session);
            session = std::move(result.session);
            Publish(SessionPhase::SignedIn, session->player);
            continue;
        }
        const auto error = wrongOwner ? SignInStatus::PlayerRejected :
            result.status == SignInStatus::Success ? SignInStatus::ExchangeFailed : result.status;
        // A refresh request can have consumed the old token even if its
        // response was lost. Do not retry it, and do not retain old access.
        if (result.session) {
            Discard(session);
            session = std::move(result.session);
        }
        desired_ = false;
        Publish(SessionPhase::SigningOut, {}, error);
        lock.unlock();
        const bool confirmed = !session || revoke(*session);
        Discard(session);
        lock.lock();
        if (!shutdown_) Publish(SessionPhase::Failed, {}, error, !confirmed);
    }
    lock.unlock();
    // Process exit is not a user-requested logout. No blocking revocation on
    // shutdown; access credentials expire naturally and are not persisted.
    Discard(session);
}
} // namespace locus::app::account
