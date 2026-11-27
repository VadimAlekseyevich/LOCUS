#include "app/https_transport.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <utility>

namespace locus::app::net {
namespace {
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
constexpr std::size_t MaxBody = 4 * 1024 * 1024;
struct Handle {
    HINTERNET value = nullptr;
    ~Handle() { if (value) WinHttpCloseHandle(value); }
    Handle() = default;
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};
std::wstring WideAscii(std::string_view value) {
    return std::wstring(value.begin(), value.end());
}
bool Option(HINTERNET handle, DWORD option, DWORD value) {
    return WinHttpSetOption(handle, option, &value, sizeof(value)) != FALSE;
}
HttpResult Failure(HttpError error) { return {error, 0, {}, {}}; }
HttpResult LastFailure() {
    return Failure(GetLastError() == ERROR_WINHTTP_TIMEOUT ? HttpError::TimedOut : HttpError::Transport);
}
// Query an optional, bounded ASCII response header. Reject malformed values.
bool Header(HINTERNET request, DWORD query, std::string& value) {
    std::array<wchar_t, 512> buffer{};
    DWORD bytes = static_cast<DWORD>(sizeof(buffer));
    if (!WinHttpQueryHeaders(request, query, WINHTTP_HEADER_NAME_BY_INDEX,
                             buffer.data(), &bytes, WINHTTP_NO_HEADER_INDEX)) {
        return GetLastError() == ERROR_WINHTTP_HEADER_NOT_FOUND;
    }
    if (bytes % sizeof(wchar_t) != 0 || bytes >= sizeof(buffer)) return false;
    for (std::size_t i = 0; i < bytes / sizeof(wchar_t); ++i) {
        const auto c = buffer[i];
        if (c < 0x20 || c > 0x7e) return false;
        value.push_back(static_cast<char>(c));
    }
    return true;
}
}

struct HttpsTransport::Impl {
    Handle session;
    Handle connection;
};
HttpsTransport::HttpsTransport(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
HttpsTransport::~HttpsTransport() = default;

std::unique_ptr<HttpsTransport> HttpsTransport::Open(std::string_view trustedOrigin) {
    const auto origin = ParseHttpsOrigin(trustedOrigin);
    if (!origin) return {};
    auto impl = std::make_unique<Impl>();
    // Direct connections in the private slice: no WPAD or ambient proxy credentials.
    impl->session.value = WinHttpOpen(L"LOCUS/1", WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!impl->session.value) return {};
    auto session = impl->session.value;
    constexpr DWORD tls12 = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
#ifdef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3
    if (!Option(session, WINHTTP_OPTION_SECURE_PROTOCOLS, tls12 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3)) {
        if (GetLastError() != ERROR_INVALID_PARAMETER ||
            !Option(session, WINHTTP_OPTION_SECURE_PROTOCOLS, tls12)) return {};
    }
#else
    if (!Option(session, WINHTTP_OPTION_SECURE_PROTOCOLS, tls12)) return {};
#endif
    // Finite per-phase timeouts. No certificate-ignore flags are ever set.
    if (!WinHttpSetTimeouts(session, 5000, 5000, 5000, 5000)) return {};
    const auto host = WideAscii(origin->host);
    impl->connection.value = WinHttpConnect(session, host.c_str(), origin->port, 0);
    if (!impl->connection.value) return {};
    return std::unique_ptr<HttpsTransport>(new HttpsTransport(std::move(impl)));
}

HttpResult HttpsTransport::Get(std::string_view path, std::string_view bearer,
                               std::size_t maxResponse, const std::atomic_bool& cancel) {
    return Request(false, path, bearer, {}, maxResponse, cancel);
}
HttpResult HttpsTransport::PostForm(std::string_view path, std::string_view encodedForm,
                                    std::size_t maxResponse, const std::atomic_bool& cancel) {
    return Request(true, path, {}, encodedForm, maxResponse, cancel);
}
HttpResult HttpsTransport::Request(bool post, std::string_view path, std::string_view bearer,
                                   std::string_view body, std::size_t maxResponse,
                                   const std::atomic_bool& cancel) {
    if (!ValidRequestPath(path) || (!bearer.empty() && !ValidBearer(bearer)) ||
        maxResponse == 0 || maxResponse > MaxBody || body.size() > 65536 ||
        (post && body.empty())) return Failure(HttpError::InvalidInput);
    for (unsigned char c : body)
        if (c < 0x21 || c > 0x7e) return Failure(HttpError::InvalidInput);
    const auto deadline = Clock::now() + 20s;
    auto stopped = [&]() -> HttpError {
        if (cancel.load(std::memory_order_relaxed)) return HttpError::Cancelled;
        if (Clock::now() >= deadline) return HttpError::TimedOut;
        return HttpError::None;
    };
    if (auto error = stopped(); error != HttpError::None) return Failure(error);
    Handle request;
    const auto target = WideAscii(path);
    request.value = WinHttpOpenRequest(impl_->connection.value, post ? L"POST" : L"GET",
        target.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request.value) return LastFailure();
    const auto r = request.value;
    if (!Option(r, WINHTTP_OPTION_DISABLE_FEATURE,
                WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION) ||
        !Option(r, WINHTTP_OPTION_AUTOLOGON_POLICY, WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH) ||
        !Option(r, WINHTTP_OPTION_ENABLE_FEATURE, WINHTTP_ENABLE_SSL_REVOCATION) ||
        !Option(r, WINHTTP_OPTION_MAX_RESPONSE_HEADER_SIZE, 32768) ||
        !Option(r, WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT, 10000))
        return LastFailure();

    std::wstring headers = L"Accept: application/json\r\nAccept-Encoding: identity\r\nCache-Control: no-store\r\n";
    if (post) headers += L"Content-Type: application/x-www-form-urlencoded\r\n";
    if (!bearer.empty()) headers += L"Authorization: Bearer " + WideAscii(bearer) + L"\r\n";
    const auto size = static_cast<DWORD>(body.size());
    const BOOL sent = WinHttpSendRequest(r, headers.c_str(), static_cast<DWORD>(headers.size()),
        body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()), size, size, 0);
    const DWORD sendError = sent ? ERROR_SUCCESS : GetLastError();
    SecureZeroMemory(headers.data(), headers.size() * sizeof(wchar_t));
    if (!sent) { SetLastError(sendError); return LastFailure(); }
    if (auto error = stopped(); error != HttpError::None) return Failure(error);
    if (!WinHttpReceiveResponse(r, nullptr)) return LastFailure();
    if (auto error = stopped(); error != HttpError::None) return Failure(error);
    DWORD status = 0, bytes = sizeof(status);
    if (!WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &bytes, WINHTTP_NO_HEADER_INDEX)) return LastFailure();
    if (status >= 300 && status < 400) return {HttpError::Redirect, status, {}, {}};

    HttpResult result{HttpError::None, status, {}, {}};
    std::string encoding;
    if (!Header(r, WINHTTP_QUERY_CONTENT_ENCODING, encoding) ||
        (!encoding.empty() && encoding != "identity")) return Failure(HttpError::UnsupportedEncoding);
    if (!Header(r, WINHTTP_QUERY_CONTENT_TYPE, result.contentType)) return Failure(HttpError::Transport);
    std::array<char, 8192> buffer{};
    while (true) {
        if (auto error = stopped(); error != HttpError::None) return Failure(error);
        DWORD received = 0;
        // Read at most one byte beyond the limit to detect oversized responses.
        const auto want = static_cast<DWORD>(std::min(buffer.size(), maxResponse - result.body.size() + 1));
        if (!WinHttpReadData(r, buffer.data(), want, &received)) return LastFailure();
        if (auto error = stopped(); error != HttpError::None) return Failure(error);
        if (!received) return result;
        if (received > maxResponse - result.body.size()) return Failure(HttpError::ResponseTooLarge);
        result.body.append(buffer.data(), received);
    }
}
} // namespace locus::app::net
