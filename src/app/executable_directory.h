#pragma once
#include <filesystem>

namespace locus::app {
// Native executable directory, independent of the process working directory.
// Windows paths are obtained as UTF-16, never via raylib's narrow path API.
std::filesystem::path ExecutableDirectory();
}
