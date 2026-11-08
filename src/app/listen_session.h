#pragma once

#include "app/music_picker.h"
#include "app/screen_flow.h"

#include <string>

namespace locus::app {
class LocalMusic;

// Preserves the existing song on picker cancel or decode failure. The
// application supplies a native dialog and monotonic clock; the coordinator
// never owns the audio device or chooses application navigation.
class ListenSession {
public:
    ListenSession(LocalMusic& song, std::wstring& loadedAudioPath,
                  std::string& error,
                  MusicPickResult (*chooseAudio)(),
                  double (*clockNow)()) noexcept
        : song_(song), loadedAudioPath_(loadedAudioPath), error_(error),
          chooseAudio_(chooseAudio), clockNow_(clockNow) {}

    static bool ShouldPauseForDialog(Screen screen, bool playing,
                                     bool paused) noexcept {
        return screen == Screen::Listen && playing && !paused;
    }

    // Returns true only if new music has been decoded and installed. Caller
    // switches to Listen; canceled/error selections keep the old stream.
    bool ChooseAudio(Screen screen, double& now);

    // Standard listening controls, without altering editor/game transports.
    void Update(bool loadedNewSong, bool restartPressed, bool startPressed,
                bool pausePressed, double now);

private:
    LocalMusic& song_;
    std::wstring& loadedAudioPath_;
    std::string& error_;
    MusicPickResult (*chooseAudio_)();
    double (*clockNow_)();
};

} // namespace locus::app
