#include "app/native_oauth.h"
#include <cstdlib>
#include <iostream>
#include <string>

using namespace locus::app::oauth;
namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
}
int main() {
    const std::string state(43, 's');
    const std::string issuer = "https://127.0.0.1:5444/realms/locus";
    const std::string query = "/?state=" + state +
        "&iss=https%3A%2F%2F127.0.0.1%3A5444%2Frealms%2Flocus";
    auto good = ParseCallback(query + "&session_state=fixture&code=a%2Bb.c", state, issuer);
    Require(good.status == CallbackStatus::Code && good.code == "a+b.c", "valid code rejected");
    auto denied = ParseCallback(query + "&error=access_denied&error_description=No+access", state, issuer);
    Require(denied.status == CallbackStatus::Denied && denied.code.empty(), "denial rejected");
    const std::string bad[] = {
        query + "&code=", query + "&code=x&code=y", query + "&code=x&%63ode=y",
        query + "&state=" + state + "&code=x", query + "&code=x&iss=" + issuer,
        query + "&code=x&error=access_denied", query + "&code=%00",
        query + "&code=%ZZ", query + "&code=x#fragment", query + "&code=x&",
        query + "&code=x%0D%0A", query + "&code=x y", query + "&code=x+y",
        "/other" + query + "&code=x", "http://127.0.0.1" + query + "&code=x",
        "/?state=" + state + "&code=x", "/?iss=" + issuer + "&code=x",
        query + "&code=" + std::string(2049, 'x'),
        query + "&code=" + std::string(8192, 'x')
    };
    for (const auto& target : bad)
        Require(ParseCallback(target, state, issuer).status == CallbackStatus::Invalid, "invalid callback accepted");
    Require(ParseCallback(query + "&code=x", std::string(43, 't'), issuer).status ==
        CallbackStatus::Invalid, "state mismatch accepted");
    Require(ParseCallback(query + "&code=x", state, issuer + "/").status ==
        CallbackStatus::Invalid, "issuer mismatch accepted");
#ifdef _WIN32
    const auto challenge = PkceChallenge("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk");
    Require(challenge && *challenge == "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM",
        "RFC 7636 Appendix B vector mismatch");
    Require(!PkceChallenge("short") && !PkceChallenge(std::string(43, '+')),
        "invalid verifier accepted");
    const auto first = GenerateAuthorizationSecrets(), second = GenerateAuthorizationSecrets();
    Require(first && second, "CNG failed");
    Require(first->verifier.size() == 43 && first->state.size() == 43 &&
        first->nonce.size() == 43 && first->challenge.size() == 43, "wrong secret length");
    Require(first->verifier != first->state && first->state != first->nonce &&
        first->state != second->state && first->verifier != second->verifier, "random secrets reused");
    Require(PkceChallenge(first->verifier) == first->challenge, "inconsistent challenge");
#endif
    return 0;
}
