#include "app/listen_session.h"

#include "app/local_music.h"

namespace locus::app {

bool ListenSession::ChooseAudio(Screen screen, double& now) {
    const bool resume = ShouldPauseForDialog(
        screen, song_.Playing(), song_.Paused());
    if (resume) song_.Pause(now);

    const MusicPickResult picked = chooseAudio_();
    // The native dialog can block for seconds: never resume the song or set
    // its clock using a timestamp from BEFORE the picker was opened.
    now = clockNow_();
    bool loaded = false;
    if (!picked.path.empty()) {
        if (song_.LoadFromFile(picked.path)) {
            loadedAudioPath_ = picked.path;
            error_.clear();
            loaded = true;
        } else {
            error_ = song_.Error();
        }
    } else if (!picked.canceled) {
        error_ = picked.error;
    }
    if (resume && !loaded) song_.Resume(now);
    return loaded;
}

void ListenSession::Update(bool loadedNewSong, bool restartPressed,
                           bool startPressed, bool pausePressed, double now) {
    if (loadedNewSong || restartPressed ||
        (startPressed && song_.Finished())) {
        song_.Start(now);
    } else if (pausePressed) {
        if (song_.Paused()) song_.Resume(now);
        else song_.Pause(now);
    }
    song_.Update(now);
}

} // namespace locus::app
