#pragma once

#include <filesystem>
#include <string>

namespace locus::app {

// Opens a real local folder in Windows Explorer using Unicode paths.
// OS-specific APIs are isolated from translation units that include raylib.
bool OpenFolderInExplorer(const std::filesystem::path& folder,
                          std::string& error);

} // namespace locus::app
