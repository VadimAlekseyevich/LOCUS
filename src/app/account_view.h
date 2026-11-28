#pragma once
#include "app/account_session.h"

namespace locus::app::account {
enum class Action { None, SignIn, SignOut };
struct HomeView {
    bool visible = false;
    Action action = Action::None;
    std::string button;
    std::string status;
};

// Shared native/fallback presentation. No credentials or provider-supplied
// error strings enter the UI. A click expresses an explicit action, never
// a toggle against a potentially newer worker state.
inline HomeView Present(const SessionView& view) {
    using P = SessionPhase;
    using E = SignInStatus;
    switch (view.phase) {
    case P::Restoring: return {true, Action::SignOut, "CANCEL", "RESTORING ACCOUNT"};
    case P::SigningIn: return {true, Action::SignOut, "CANCEL", "SIGN IN VIA BROWSER"};
    case P::SignedIn: return {true, Action::SignOut, "SIGN OUT", view.player ? view.player->displayName : "ONLINE"};
    case P::Refreshing: return {true, Action::SignOut, "SIGN OUT", "REFRESHING SESSION"};
    case P::SigningOut: return {true, Action::None, "SIGNING OUT", "CLEARING SESSION"};
    case P::SignedOut: return {true, Action::SignIn, "SIGN IN",
        view.revocationUnconfirmed ? "REMOTE LOGOUT UNCONFIRMED" : "OFFLINE"};
    case P::Failed:
        if (view.error == E::CredentialStorageFailed)
            return {true, Action::SignOut, "CLEAR SESSION", "CREDENTIAL STORAGE ERROR"};
        if (view.revocationUnconfirmed)
            return {true, Action::SignIn, "SIGN IN", "REMOTE LOGOUT UNCONFIRMED"};
        switch (view.error) {
        case E::SessionBusy: return {true, Action::SignIn, "SIGN IN", "ACCOUNT IN ANOTHER INSTANCE"};
        case E::PlayerRejected: return {true, Action::SignIn, "SIGN IN", "ACCOUNT NOT AUTHORIZED"};
        case E::Denied: return {true, Action::SignIn, "SIGN IN", "SIGN IN DENIED"};
        case E::TimedOut: return {true, Action::SignIn, "SIGN IN", "SIGN IN TIMED OUT"};
        case E::BrowserFailed: return {true, Action::SignIn, "SIGN IN", "BROWSER UNAVAILABLE"};
        case E::Cancelled: return {true, Action::SignIn, "SIGN IN", "SIGN IN CANCELED"};
        default: return {true, Action::SignIn, "SIGN IN", "ONLINE UNAVAILABLE"};
        }
    }
    return {};
}
} // namespace locus::app::account
