#pragma once

#include "app/audio_volume_fade.h"
#include "app/local_music.h"
#include "app/demo_transport.h"
#include "app/song_library.h"

#include <string>

namespace locus::app {

// Dedicated non-scored preview streams. Never seek or steal the active chart's
// music/clock; holding a Song Set selection starts audio at previewStartMs.
// Switching tracks fades OUT first, then fades IN the new source.
class MenuMusicPreview {
public:
    bool Initialize(const core::Chart& demo);
    bool Select(const LibrarySong* song, const LibraryChart* difficulty,
                double now, std::string& error);
    void FadeOut(double now);
    void Update(double now, float masterVolume);
    void Pause(double now);
    void Resume(double now);
    void Shutdown();
    bool HasActiveMusic() const noexcept { return active_ != Source::None; }
    const std::string& Error() const noexcept { return error_; }

private:
    enum class Source { None, Demo, Song };
    Source active_ = Source::None;
    Source pending_ = Source::None;
    LocalMusic song_;
    LocalMusic staged_;
    DemoTransport demo_;
    AudioVolumeFade envelope_;
    std::wstring loadedPath_;
    std::wstring pendingPath_;
    int activeStartMs_ = 0;
    int pendingStartMs_ = 0;
    bool switching_ = false;
    bool paused_ = false;
    double pausedAt_ = 0.0;
    std::string error_;

    void ActivatePending(double now);
    void StopActive();
};
} // namespace locus::app
