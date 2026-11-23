#pragma once

#include <string>
#include <string_view>

namespace locus::core {

// Locale-independent NFKC case folding for case-insensitive UTF-8 lookup.
// Output is an opaque search key, never a persisted chart title or ID.
std::string NormalizedSearchKey(std::string_view text);

} // namespace locus::core
