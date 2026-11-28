#include "app/account_credentials.h"
#include <cstdlib>
#include <future>
#include <iostream>

using namespace locus::app::account;
namespace {
void Check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
}
int main() {
    // Never touch a real account target. Unique client ID per test.
    const auto secrets = locus::app::oauth::GenerateAuthorizationSecrets();
    Check(bool(secrets), "fixture entropy");
    const Config config{"https://identity.invalid/realms/test", "https://api.invalid",
                        "ci-" + secrets->state};
    const std::string id = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
    CredentialStore first(config);
    Check(first.Acquire() == CredentialStatus::Ok, "acquire credential lease");
    Check(first.Erase(), "initial erase");
    Check(first.Save(id, "fixture.refresh"), "credential write");
    auto other = std::async(std::launch::async, [&] {
        CredentialStore second(config);
        return second.Acquire();
    });
    Check(other.get() == CredentialStatus::Busy, "simultaneous session accepted");
    first.Close();
    CredentialStore restored(config);
    Check(restored.Acquire() == CredentialStatus::Ok, "reacquire");
    SavedCredential saved;
    Check(restored.Consume(saved) == CredentialStatus::Ok && saved.playerId == id &&
          saved.refresh == "fixture.refresh", "restore credential");
    Check(restored.Consume(saved) == CredentialStatus::Missing && saved.refresh.empty(), "credential replay");
    Check(!restored.Save(id, std::string(20000, 'a')), "oversized secret accepted");
    Check(!restored.Save("invalid", "fixture.refresh"), "invalid owner accepted");
    Check(restored.Save(id, "rotated.refresh") && restored.Erase(), "rotate/erase");
    Check(restored.Consume(saved) == CredentialStatus::Missing, "logout retained secret");
    restored.Close();
    return 0;
}
