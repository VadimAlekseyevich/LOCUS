#include "app/native_oauth.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <algorithm>
#include <array>
#include <utility>

namespace locus::app::oauth {
namespace {
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
struct Socket {
    SOCKET value = INVALID_SOCKET;
    ~Socket() { Close(); }
    void Close() {
        if (value != INVALID_SOCKET) closesocket(std::exchange(value, INVALID_SOCKET));
    }
};
enum class Ready { Yes, Cancelled, Timeout, Failed };

Ready Poll(SOCKET socket, bool write, Clock::time_point deadline, const std::atomic_bool& cancel) {
    while (!cancel.load(std::memory_order_relaxed)) {
        const auto remaining = deadline - Clock::now();
        if (remaining <= Clock::duration::zero()) return Ready::Timeout;
        const auto wait = std::min(std::chrono::duration_cast<std::chrono::microseconds>(remaining), 50000us);
        timeval tv{0, static_cast<long>(wait.count())};
        fd_set set;
        FD_ZERO(&set);
        FD_SET(socket, &set);
        const int result = select(0, write ? nullptr : &set, write ? &set : nullptr, nullptr, &tv);
        if (result > 0) return Ready::Yes;
        if (result == SOCKET_ERROR) return Ready::Failed;
    }
    return Ready::Cancelled;
}

bool Nonblocking(SOCKET socket) {
    u_long enabled = 1;
    return ioctlsocket(socket, FIONBIO, &enabled) == 0;
}

CallbackResult ReadRequest(SOCKET socket, std::uint16_t port, std::string_view state,
                           std::string_view issuer, Clock::time_point deadline,
                           const std::atomic_bool& cancel) {
    std::string request;
    request.reserve(2048);
    std::array<char, 2048> buffer{};
    while (request.size() < 16384) {
        if (Poll(socket, false, deadline, cancel) != Ready::Yes) return {};
        const int count = recv(socket, buffer.data(), static_cast<int>(buffer.size()), 0);
        if (count == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) continue;
        if (count <= 0 || static_cast<std::size_t>(count) > 16384 - request.size()) return {};
        request.append(buffer.data(), static_cast<std::size_t>(count));
        if (request.find("\r\n\r\n") != std::string::npos)
            return ParseHttpCallback(request, port, state, issuer);
    }
    return {};
}

void Respond(SOCKET socket, CallbackStatus status, Clock::time_point deadline,
             const std::atomic_bool& cancel) {
    const bool valid = status != CallbackStatus::Invalid;
    const std::string_view body = valid ?
        "Response received. Return to LOCUS to complete sign-in." :
        "Invalid callback.";
    const std::string response = std::string(valid ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 400 Bad Request\r\n") +
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Cache-Control: no-store\r\nReferrer-Policy: no-referrer\r\n"
        "X-Content-Type-Options: nosniff\r\nConnection: close\r\nContent-Length: " +
        std::to_string(body.size()) + "\r\n\r\n" + std::string(body);
    std::size_t sent = 0;
    while (sent < response.size()) {
        if (Poll(socket, true, deadline, cancel) != Ready::Yes) return;
        const int count = send(socket, response.data() + sent, static_cast<int>(response.size() - sent), 0);
        if (count == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) continue;
        if (count <= 0) return;
        sent += static_cast<std::size_t>(count);
    }
}
} // namespace

struct LoopbackListener::Impl {
    bool initialized = false;
    Socket listener;
    std::uint16_t port = 0;
    bool used = false;
    ~Impl() {
        listener.Close();
        if (initialized) WSACleanup();
    }
};

LoopbackListener::LoopbackListener(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
LoopbackListener::~LoopbackListener() = default;

std::unique_ptr<LoopbackListener> LoopbackListener::Open() {
    auto impl = std::make_unique<Impl>();
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return {};
    impl->initialized = true;
    impl->listener.value = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    const auto listener = impl->listener.value;
    if (listener == INVALID_SOCKET) return {};
    BOOL exclusive = TRUE;
    if (setsockopt(listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                   reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) != 0) return {};
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (bind(listener, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0 ||
        listen(listener, 8) != 0 || !Nonblocking(listener)) return {};
    int length = sizeof(addr);
    if (getsockname(listener, reinterpret_cast<sockaddr*>(&addr), &length) != 0) return {};
    impl->port = ntohs(addr.sin_port);
    if (impl->port == 0) return {};
    return std::unique_ptr<LoopbackListener>(new LoopbackListener(std::move(impl)));
}

std::string LoopbackListener::RedirectUri() const {
    return "http://127.0.0.1:" + std::to_string(impl_->port);
}

WaitResult LoopbackListener::Wait(std::string_view state, std::string_view issuer,
                                  const std::atomic_bool& cancel, std::chrono::milliseconds timeout) {
    if (impl_->used || timeout <= 0ms || timeout > 10min) return {};
    impl_->used = true;
    // Always close the listener on return, even on cancellation or exception.
    struct CloseOnExit { Socket& socket; ~CloseOnExit() { socket.Close(); } } close{impl_->listener};
    const auto deadline = Clock::now() + timeout;
    const auto listener = impl_->listener.value;
    while (true) {
        switch (Poll(listener, false, deadline, cancel)) {
        case Ready::Cancelled: return {WaitStatus::Cancelled, {}};
        case Ready::Timeout: return {WaitStatus::TimedOut, {}};
        case Ready::Failed: return {};
        case Ready::Yes: break;
        }
        sockaddr_in peer{};
        int length = sizeof(peer);
        Socket client;
        client.value = accept(listener, reinterpret_cast<sockaddr*>(&peer), &length);
        if (client.value == INVALID_SOCKET) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) continue;
            return {};
        }
        if (peer.sin_family != AF_INET || peer.sin_addr.s_addr != htonl(INADDR_LOOPBACK) ||
            !Nonblocking(client.value)) continue;
        // A slow or malformed local request cannot occupy the listener indefinitely.
        auto result = ReadRequest(client.value, impl_->port, state, issuer,
                                  std::min(deadline, Clock::now() + 2s), cancel);
        Respond(client.value, result.status, std::min(deadline, Clock::now() + 200ms), cancel);
        if (cancel.load(std::memory_order_relaxed)) return {WaitStatus::Cancelled, {}};
        if (Clock::now() >= deadline) return {WaitStatus::TimedOut, {}};
        if (result.status == CallbackStatus::Code) return {WaitStatus::Code, std::move(result.code)};
        if (result.status == CallbackStatus::Denied) return {WaitStatus::Denied, {}};
    }
}
} // namespace locus::app::oauth
