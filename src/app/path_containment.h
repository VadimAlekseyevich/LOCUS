#pragma once

#include <filesystem>
#include <system_error>

namespace locus::app {

// True only for a path strictly below an existing directory. Canonicalize
// both sides before comparing filesystem identity: on Windows a resolved
// path can use a different drive spelling, UNC prefix or junction spelling
// from the path supplied by the file picker. Text prefix comparison fails
// in those cases; equivalent() also avoids a sibling-name prefix bypass.
// weakly_canonical handles targets whose final component is not created yet.
inline bool IsPathWithinDirectory(const std::filesystem::path& root,
                                  const std::filesystem::path& candidate) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path boundary = fs::canonical(root, ec);
    if (ec) return false;
    const fs::path resolved = fs::weakly_canonical(candidate, ec);
    if (ec || resolved.empty()) return false;
    for (fs::path parent = resolved.parent_path(); !parent.empty();) {
        ec.clear();
        if (fs::equivalent(parent, boundary, ec) && !ec) return true;
        const fs::path higher = parent.parent_path();
        if (higher.empty() || higher == parent) break;
        parent = higher;
    }
    return false;
}

} // namespace locus::app
