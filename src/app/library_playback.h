#pragma once

#include "app/song_library.h"

#include <string>

namespace locus::core { class TurnJudge; }
namespace locus::app {
class LocalMusic;
class ChartPlayback;
class GameSession;

// Non-owning lifetime contract: activeChart is the same object referenced by
// the judge and GameSession. Only its CONTENTS are replaced after validation.
struct LibraryPlaybackContext {
    LocalMusic& song;
    std::wstring& loadedAudioPath;
    core::Chart& activeChart;
    std::string& chartTitle;
    ChartPlayback& playback;
    core::TurnJudge& judge;
    GameSession& session;
    bool& customChart;
};

// Does not mutate the output or source chart on validation failure. This pure
// preparation path also lets headless tests cover invalid song metadata.
bool PrepareLibraryDifficulty(const LibraryChart& selected, double audioDuration,
                              core::LoadedChart& prepared, std::string& error);

// A matching path is reusable only if the audio decoder is actually ready.
// Paths must identify this difficulty's revision, not the logical song folder.
bool RequiresLibraryAudioLoad(const LibraryChart& selected,
                              const std::wstring& loadedAudioPath,
                              bool audioReady) noexcept;

// Decode (if required), validate a copy and only THEN replace the current
// chart/audio/session metadata. Failure must preserve every active reference.
bool ActivateLibraryDifficulty(const LibraryChart& selected,
                               const LibraryPlaybackContext& context,
                               double now, std::string& error);

} // namespace locus::app
