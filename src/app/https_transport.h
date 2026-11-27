#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace locus::app::net {
// A trusted configuration origin, not a URL obtained from arbitrary content.
// ASCII DNS/IPv4 only; no userinfo, path, query, fragment or implicit HTTP.
struct HttpsOrigin {
    std::string host;
    std::uint16_t port = 443;
};
std::optional<HttpsOrigin> ParseHttpsOrigin(std::string_view url);
bool ValidRequestPath(std::string_view path);
bool ValidBearer(std::string_view token);
std::string FormEncode(std::string_view value);

enum class HttpError { None, InvalidInput, Cancelled, TimedOut, Transport, Redirect, ResponseTooLarge, UnsupportedEncoding };
struct HttpResult {
    HttpError error = HttpError::Transport;
    unsigned int status = 0;
    std::string body;
    std::string contentType;
};

#ifdef _WIN32
// Single-owner synchronous transport, for a background account/network worker.
// Reuses the WinHTTP session/connection. No concurrent use, no UI-thread I/O.
class HttpsTransport {
public:
    static std::unique_ptr<HttpsTransport> Open(std::string_view trustedOrigin);
    ~HttpsTransport();
    HttpsTransport(const HttpsTransport&) = delete;
    HttpsTransport& operator=(const HttpsTransport&) = delete;

    HttpResult Get(std::string_view path, std::string_view bearer,
                   std::size_t maxResponse, const std::atomic_bool& cancel);
    HttpResult PostForm(std::string_view path, std::string_view encodedForm,
                        std::size_t maxResponse, const std::atomic_bool& cancel);
private:
    struct Impl;
    explicit HttpsTransport(std::unique_ptr<Impl> impl);
    HttpResult Request(bool post, std::string_view path, std::string_view bearer,
                       std::string_view body, std::size_t maxResponse,
                       const std::atomic_bool& cancel);
    std::unique_ptr<Impl> impl_;
};
#endif
} // namespace locus::app::net
