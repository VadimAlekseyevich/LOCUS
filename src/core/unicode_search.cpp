#include "core/unicode_search.h"

#include <utf8proc.h>

namespace locus::core {

std::string NormalizedSearchKey(std::string_view text) {
    if (text.empty()) return {};
    utf8proc_uint8_t* buffer = nullptr;
    const auto count = utf8proc_map(
        reinterpret_cast<const utf8proc_uint8_t*>(text.data()),
        static_cast<utf8proc_ssize_t>(text.size()), &buffer,
        static_cast<utf8proc_option_t>(
            UTF8PROC_STABLE | UTF8PROC_COMPAT |
            UTF8PROC_CASEFOLD | UTF8PROC_COMPOSE));
    if (count < 0 || !buffer) {
        if (buffer) utf8proc_free(buffer);
        // Input boundaries already enforce valid UTF-8; retain literal
        // bytes if a malformed external name nevertheless reaches search.
        return std::string(text);
    }
    std::string normalized(reinterpret_cast<const char*>(buffer),
                           static_cast<std::size_t>(count));
    utf8proc_free(buffer);
    return normalized;
}

} // namespace locus::core
