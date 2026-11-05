#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace locus::app {

// Windows-only implementation; the header exposes no Win32 types.
// The audio decoder lives in a separate compilation unit.
bool ReadLocalFileBytes(const std::wstring& path,
                        std::vector<std::uint8_t>& bytes,
                        std::string& error,
                        std::size_t maxBytes = 256U * 1024U * 1024U);
std::string DisplayLocalFileName(const std::wstring& path);

} // namespace locus::app
