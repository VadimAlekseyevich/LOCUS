#include "app/account_session.h"
#include <cstdlib>
#include <iostream>
#include <thread>

using namespace locus::app::account;
using namespace std::chrono_literals;
namespace {
void Check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
using Clock = std::chrono::steady_clock;
struct Control {
    std::atomic_bool entered{false}, release{false};
    std::atomic_int logins{0}, refreshes{0}, revokes{0};
    std::string revoked; // read only after synchronized terminal snapshot
    bool blockLogin = false, due = false, wrongOwner = false, failRefresh = false, failRevoke = false;
};
class Backend final : public SessionBackend {
public:
    explicit Backend(Control& control) : c(control) {}
    SignInResult SignIn(const std::atomic_bool& cancel) override {
        ++c.logins;
        c.entered = true;
        // Deliberately ignore cancellation in this fake to model a late
        // response from a blocking call; the manager must discard it.
        while (c.blockLogin && !c.release.load()) std::this_thread::sleep_for(1ms);
        (void)cancel;
        return {SignInStatus::Success, Session{{"player-a", "Alpha"}, {"access.a", "refresh.a", 120},
            Clock::now() + (c.due ? 20s : 120s)}};
    }
    SignInResult Refresh(const Session&, const std::atomic_bool&) override {
        ++c.refreshes;
        if (c.failRefresh) return {SignInStatus::ExchangeFailed, {}};
        return {SignInStatus::Success, Session{{c.wrongOwner ? "player-b" : "player-a", "Alpha"},
            {"access.b", "refresh.b", 120}, Clock::now() + 120s}};
    }
    bool Revoke(const Session& current, const std::atomic_bool&) override {
        c.revoked = current.tokens.refresh;
        ++c.revokes;
        return !c.failRevoke;
    }
private:
    Control& c;
};
template<class F> void Until(F predicate, const char* message) {
    const auto deadline = Clock::now() + 3s;
    while (!predicate()) {
        Check(Clock::now() < deadline, message);
        std::this_thread::sleep_for(1ms);
    }
}
void Wait(SessionManager& manager, SessionPhase phase) {
    Until([&]{ return manager.Snapshot().phase == phase; }, "session phase timeout");
}
}
int main() {
    {
        Control c; c.blockLogin = true;
        SessionManager manager(std::make_unique<Backend>(c));
        Check(manager.BeginSignIn() && !manager.BeginSignIn(), "duplicate login");
        Until([&]{ return c.entered.load(); }, "login did not start");
        manager.SignOut();
        Check(!manager.Snapshot().player && !manager.BeginSignIn(), "signout race");
        c.release = true;
        Wait(manager, SessionPhase::SignedOut);
        Check(c.revokes == 1 && c.revoked == "refresh.a" && !manager.Snapshot().player,
            "late login was not revoked");
        Check(manager.BeginSignIn(), "relogin failed");
        Wait(manager, SessionPhase::SignedIn);
        Check(manager.Snapshot().player->id == "player-a", "missing player");
        manager.SignOut();
        Wait(manager, SessionPhase::SignedOut);
        Check(c.revokes == 2, "second signout");
    }
    {
        Control c; c.due = true;
        SessionManager manager(std::make_unique<Backend>(c));
        Check(manager.BeginSignIn(), "login");
        Until([&]{ return c.refreshes == 1 && manager.Snapshot().phase == SessionPhase::SignedIn; }, "refresh");
        manager.SignOut();
        Wait(manager, SessionPhase::SignedOut);
        Check(c.revoked == "refresh.b", "old refresh credential revoked instead of rotated one");
    }
    for (bool wrongOwner : {false, true}) {
        Control c; c.due = true; c.wrongOwner = wrongOwner;
        c.failRefresh = !wrongOwner; c.failRevoke = true;
        SessionManager manager(std::make_unique<Backend>(c));
        Check(manager.BeginSignIn(), "login");
        Wait(manager, SessionPhase::Failed);
        const auto view = manager.Snapshot();
        Check(!view.player && view.revocationUnconfirmed && c.refreshes == 1 && c.revokes == 1,
            "failed refresh did not clear identity/stop retry");
        Check(view.error == (wrongOwner ? SignInStatus::PlayerRejected : SignInStatus::ExchangeFailed),
            "wrong failure reason");
    }
    return 0;
}
