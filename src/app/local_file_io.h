#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace locus::app {

// Windows-only implementation; the header exposes no Win32 types.
// The audio decoder lives in a separate compilation unit.
bool ReadLocalFileBytes(const std::wstring& path,
                        std::vector<std::uint8_t>& bytes,
                        std::string& error);
std::string DisplayLocalFileName(const std::wstring& path);

} // namespace locus::app
