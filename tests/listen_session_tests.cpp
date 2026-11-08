#include "app/listen_session.h"
#include "app/local_music.h"

#include <iostream>
#include <string>

using namespace locus::app;
namespace {
int pickCalls = 0;
double Now() { return 100.25; }
MusicPickResult Cancel() { ++pickCalls; return {{}, {}, true}; }
MusicPickResult Failure() {
    ++pickCalls; return {{}, "NATIVE DIALOG ERROR", false};
}
MusicPickResult InvalidSong() {
    ++pickCalls; return {L"missing-song-for-listen-session.mp3", {}, false};
}
bool Check(bool condition, const char* message) {
    if (!condition) std::cerr << "LISTEN_SESSION_FAIL " << message << '\n';
    return condition;
}
}

int main() {
    bool ok = true;
    ok &= Check(ListenSession::ShouldPauseForDialog(Screen::Listen, true, false) &&
                !ListenSession::ShouldPauseForDialog(Screen::Listen, true, true) &&
                !ListenSession::ShouldPauseForDialog(Screen::Listen, false, false) &&
                !ListenSession::ShouldPauseForDialog(Screen::Home, true, false),
                "only active unpaused listening is paused for a modal picker");

    LocalMusic music;
    music.SetVolume(0.31f);
    std::wstring loadedAudioPath = L"previous-song.wav";
    std::string error = "OLD ERROR";
    double now = 1.0;
    ListenSession canceled{music, loadedAudioPath, error, Cancel, Now};
    ok &= Check(!canceled.ChooseAudio(Screen::Listen, now) &&
                now == 100.25 && pickCalls == 1 &&
                error == "OLD ERROR" &&
                loadedAudioPath == L"previous-song.wav" &&
                music.Volume() > 0.30f && music.Volume() < 0.32f,
                "canceled music picker leaves existing session and errors intact");

    ListenSession broken{music, loadedAudioPath, error, Failure, Now};
    now = 3.0;
    ok &= Check(!broken.ChooseAudio(Screen::Home, now) &&
                now == 100.25 && error == "NATIVE DIALOG ERROR" &&
                loadedAudioPath == L"previous-song.wav",
                "native picker failure preserves the previous song path");

    ListenSession invalid{music, loadedAudioPath, error, InvalidSong, Now};
    ok &= Check(!invalid.ChooseAudio(Screen::Listen, now) &&
                !error.empty() && error != "NATIVE DIALOG ERROR" &&
                loadedAudioPath == L"previous-song.wav" && !music.Ready(),
                "failed audio decoder never installs an invalid new song");

    // Controls on an unloaded transport do not create sound or mutate the
    // selected song, even when Restart/Pause are requested.
    invalid.Update(false, true, false, false, 110.0);
    invalid.Update(false, false, false, true, 111.0);
    ok &= Check(!music.Playing() && !music.Ready() &&
                loadedAudioPath == L"previous-song.wav",
                "transport controls safely ignore absent decoded audio");
    if (!ok) return 1;
    std::cout << "LISTEN_SESSION_PASS modal=1 cancel=1 decoder=1 controls=1\n";
    return 0;
}
