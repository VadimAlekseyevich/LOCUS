#include "app/https_transport.h"
#include <cstdlib>
#include <iostream>
#include <string>
using namespace locus::app::net;
namespace {
void Check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
}
int main(int argc, char** argv) {
    auto parsed = ParseHttpsOrigin("https://127.0.0.1:5444");
    Check(parsed && parsed->host == "127.0.0.1" && parsed->port == 5444, "loopback HTTPS");
    parsed = ParseHttpsOrigin("https://api.example");
    Check(parsed && parsed->port == 443, "default HTTPS");
    for (const char* bad : {"http://api.example", "https://user@api.example", "https://api.example/",
        "https://api.example?x", "https://api.example#x", "https://api.example:0", "https://api.example:65536",
        "https://api.example:0443", "https://api.example:", "https://api.example\\evil", "https://a..b",
        "https://-a.example", "https://a.example.", "https://[::1]", "https://a%2eb", "https://a b"})
        Check(!ParseHttpsOrigin(bad), "invalid origin accepted");
    Check(ValidRequestPath("/api/v1/maps?after=a%20b&limit=50"), "valid path");
    for (const char* bad : {"", "https://evil.example", "//evil.example", "/\\evil", "/#fragment",
        "/%z0", "/a%2", "/a b", "/a\r\nHost:evil"})
        Check(!ValidRequestPath(bad), "invalid request path");
    Check(ValidBearer("header.payload.sig") && ValidBearer("abc+/=="), "valid bearer");
    for (const char* bad : {"", "==", "a=b", "a\r\nHost: evil", "a b", "a%20"})
        Check(!ValidBearer(bad), "invalid bearer");
    Check(FormEncode("a +&=%/") == "a+%2B%26%3D%25%2F", "form encoding");
    Check(FormEncode("\xC3\xA9") == "%C3%A9", "UTF-8 form encoding");
#ifdef _WIN32
    if (argc == 3) {
        std::atomic_bool cancel{false};
        auto client = HttpsTransport::Open(argv[1]);
        auto untrusted = HttpsTransport::Open(argv[2]);
        Check(bool(client) && bool(untrusted), "open transport");
        auto result = client->Get("/ok", "fixture.token", 64, cancel);
        Check(result.error == HttpError::None && result.status == 200 &&
              result.body == "{}" && result.contentType == "application/json", "trusted HTTPS GET");
        result = client->PostForm("/form", "code=" + FormEncode("a+b&c"), 64, cancel);
        Check(result.error == HttpError::None && result.status == 200 && result.body == "{}", "HTTPS POST form");
        result = client->Get("/redirect", "fixture.token", 64, cancel);
        Check(result.error == HttpError::Redirect && result.status == 302 && result.body.empty(), "redirect followed");
        result = client->Get("/large", {}, 64, cancel);
        Check(result.error == HttpError::ResponseTooLarge && result.body.empty(), "body limit");
        result = client->Get("/encoded", {}, 64, cancel);
        Check(result.error == HttpError::UnsupportedEncoding, "unexpected encoding");
        result = client->Get("/denied", {}, 64, cancel);
        Check(result.error == HttpError::None && result.status == 403, "HTTP status lost");
        result = untrusted->Get("/ok", "fixture.token", 64, cancel);
        Check(result.error == HttpError::Transport, "untrusted TLS accepted");
        cancel.store(true);
        Check(client->Get("/ok", {}, 64, cancel).error == HttpError::Cancelled, "cancelled request");
        Check(client->Get("//evil.example", {}, 64, cancel).error == HttpError::InvalidInput, "absolute target accepted");
    }
#else
    (void)argc; (void)argv;
#endif
    return 0;
}
