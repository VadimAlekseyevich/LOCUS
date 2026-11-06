#pragma once

#include <filesystem>
#include <string>

namespace locus::app {

// Every imported difficulty publishes its own immutable audio revision BEFORE
// atomically saving the chart JSON that references it. Failed chart writes
// may leave an unreferenced revision but never change existing song bytes.
bool InstallAudioRevision(const std::filesystem::path& source,
                          const std::filesystem::path& songFolder,
                          std::filesystem::path& installed,
                          std::string& error);

// The library groups legacy song.wav and revisioned files belonging to the
// same logical song, while each difficulty retains its actual audio path.
std::filesystem::path LogicalSongAudioPath(
    const std::filesystem::path& installedAudio);

} // namespace locus::app
