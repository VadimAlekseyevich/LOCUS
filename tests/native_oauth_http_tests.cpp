#include "app/native_oauth.h"
#include <cstdlib>
#include <iostream>
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <future>
#include <thread>
#endif

using namespace locus::app::oauth;
using namespace std::chrono_literals;
namespace {
void Check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
const std::string state(43, 's');
const std::string issuer = "https://identity.test/realms/locus";
const std::string target = "/?state=" + state + "&iss=https%3A%2F%2Fidentity.test%2Frealms%2Flocus&code=fixture";
std::string Request(unsigned port, const std::string& url, const std::string& headers = "") {
    return "GET " + url + " HTTP/1.1\r\nHost: 127.0.0.1:" + std::to_string(port) + "\r\n" + headers + "\r\n";
}
#ifdef _WIN32
std::string Send(unsigned port, const std::string& request) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    Check(s != INVALID_SOCKET, "client socket");
    DWORD timeout = 4000;
    Check(setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0,
          "receive timeout");
    Check(setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0,
          "send timeout");
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u_short>(port));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    Check(connect(s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == 0, "connect");
    std::size_t sent = 0;
    while (sent < request.size()) {
        int n = send(s, request.data() + sent, static_cast<int>(request.size() - sent), 0);
        Check(n > 0, "send");
        sent += static_cast<std::size_t>(n);
    }
    char buffer[512];
    std::string response;
    int n;
    while ((n = recv(s, buffer, sizeof(buffer), 0)) > 0) response.append(buffer, static_cast<std::size_t>(n));
    closesocket(s);
    return response;
}
#endif
}
int main() {
    const auto ok = Request(12345, target, "Accept: text/html\r\n");
    Check(ParseHttpCallback(ok, 12345, state, issuer).status == CallbackStatus::Code, "valid request");
    for (const auto& invalid : {
        Request(12346, target),
        Request(12345, target, "HOST: 127.0.0.1:12345\r\n"),
        Request(12345, target, "Content-Length: 1\r\n"),
        Request(12345, target, "Transfer-Encoding: chunked\r\n"),
        Request(12345, target, "Upgrade: websocket\r\n"),
        Request(12345, target, "X-Test: one\r\n folded\r\n"),
        Request(12345, target, "Host : 127.0.0.1:12345\r\n"),
        ok + "body", "POST" + ok.substr(3), ok.substr(0, ok.size()-1),
        "GET " + target + " HTTP/1.1\r\n\r\n",
        Request(12345, target, "X-Large: " + std::string(17000, 'x') + "\r\n")
    }) Check(ParseHttpCallback(invalid, 12345, state, issuer).status == CallbackStatus::Invalid, "invalid HTTP accepted");
#ifdef _WIN32
    std::atomic_bool cancel{false};
    auto listener = LoopbackListener::Open();
    Check(bool(listener), "open listener");
    unsigned port = std::stoul(listener->RedirectUri().substr(std::string("http://127.0.0.1:").size()));
    // Keep a separate Winsock reference while the listening socket closes.
    WSADATA data{};
    Check(WSAStartup(MAKEWORD(2, 2), &data) == 0, "client WSAStartup");
    auto pending = std::async(std::launch::async, [&] { return listener->Wait(state, issuer, cancel, 8s); });
    Check(Send(port, Request(port + 1, target)).find("400 Bad Request") != std::string::npos, "wrong host response");
    auto response = Send(port, Request(port, target));
    Check(response.find("200 OK") != std::string::npos && response.find("code=fixture") == std::string::npos,
          "valid response or credential leak");
    auto result = pending.get();
    Check(result.status == WaitStatus::Code && result.code == "fixture", "listener result");
    Check(listener->Wait(state, issuer, cancel, 1s).status == WaitStatus::Failed, "listener reused");
    WSACleanup();

    auto timed = LoopbackListener::Open();
    Check(bool(timed) && timed->Wait(state, issuer, cancel, 30ms).status == WaitStatus::TimedOut, "timeout");
    auto cancelled = LoopbackListener::Open();
    Check(bool(cancelled), "open cancellation listener");
    auto waiting = std::async(std::launch::async, [&] { return cancelled->Wait(state, issuer, cancel, 5s); });
    cancel.store(true);
    Check(waiting.get().status == WaitStatus::Cancelled, "cancellation");
#endif
    return 0;
}
