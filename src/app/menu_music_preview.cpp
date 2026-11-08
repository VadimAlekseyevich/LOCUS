#include "app/menu_music_preview.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace locus::app {

bool MenuMusicPreview::Initialize(const core::Chart& demo) {
    // Headless checks may have no device: silently keep the preview disabled.
    return demo_.Initialize(demo);
}

bool MenuMusicPreview::Select(const LibrarySong* selected,
                              const LibraryChart* difficulty,
                              double now, std::string& error) {
    const Source desired = selected ? Source::Song : Source::Demo;
    const std::wstring path = selected
        ? (difficulty ? difficulty->audioFile : selected->audioFile).wstring()
        : std::wstring{};
    const int startMs = selected ? selected->previewStartMs : 0;

    if ((switching_ && pending_ == desired && pendingPath_ == path &&
         pendingStartMs_ == startMs) ||
        (!switching_ && active_ == desired && loadedPath_ == path &&
         activeStartMs_ == startMs)) {
        error.clear();
        return true;
    }
    if (desired == Source::Song && path.empty()) {
        error = "Selected song has no audio file.";
        return false;
    }
    if (desired == Source::Song && (path != loadedPath_ || !song_.Ready())) {
        // Prepare the replacement before touching the playing source.
        LocalMusic candidate;
        candidate.SetVolume(0.0f);
        if (!candidate.LoadFromFile(path)) {
            error = candidate.Error();
            error_ = error;
            return false;
        }
        staged_.SwapWith(candidate);
    }
    pending_ = desired;
    pendingPath_ = path;
    pendingStartMs_ = startMs;
    switching_ = true;
    error.clear();
    error_.clear();
    if (active_ == Source::None) {
        ActivatePending(now);
        envelope_.FadeIn(now, kAudioFadeOutSeconds);
    } else envelope_.FadeOut(now);
    return true;
}

void MenuMusicPreview::StopActive() {
    if (active_ == Source::Demo) demo_.Stop();
    if (active_ == Source::Song) song_.Stop();
    active_ = Source::None;
}

void MenuMusicPreview::ActivatePending(double now) {
    StopActive();
    if (pending_ == Source::None) {
        loadedPath_.clear();
        staged_.Shutdown();
        switching_ = false;
        envelope_.Silent();
        return;
    }
    if (pending_ == Source::Demo) {
        demo_.SetVolume(0.0f);
        demo_.Start(now);
        active_ = Source::Demo;
        loadedPath_.clear();
    } else {
        if (pendingPath_ != loadedPath_ || !song_.Ready()) {
            song_.SwapWith(staged_);
            staged_.Shutdown(); // releases old decoder after stop
        }
        loadedPath_ = pendingPath_;
        song_.SetSpeed(1.0, now);
        song_.SetVolume(0.0f);
        if (song_.Ready()) {
            const double from = pendingStartMs_ / 1000.0;
            const double safeFrom = from + 0.10 < song_.DurationSeconds()
                ? from : 0.0;
            song_.StartAt(now, safeFrom);
        }
        active_ = Source::Song;
    }
    activeStartMs_ = pendingStartMs_;
    pending_ = Source::None;
    pendingPath_.clear();
    switching_ = false;
    envelope_.FadeIn(now);
}

void MenuMusicPreview::FadeOut(double now) {
    pending_ = Source::None;
    pendingPath_.clear();
    switching_ = true;
    if (active_ != Source::None) envelope_.FadeOut(now);
    else ActivatePending(now);
}

void MenuMusicPreview::Update(double now, float masterVolume) {
    if (active_ == Source::Song) song_.Update(now);
    if (active_ == Source::Demo) demo_.Update(now);
    if (switching_ && envelope_.Finished(now)) ActivatePending(now);

    const float volume = std::clamp(masterVolume, 0.0f, 1.0f) *
                         envelope_.Gain(now);
    if (active_ == Source::Song) {
        song_.SetVolume(volume);
        if (song_.Finished() && !switching_) {
            // Listen continually in SongSelect; restart using the stored
            // song-time origin rather than falling silent at EOF.
            const double from = activeStartMs_ / 1000.0;
            const double safeFrom = from + 0.10 < song_.DurationSeconds()
                ? from : 0.0;
            song_.StartAt(now, safeFrom);
            envelope_.FadeIn(now);
        }
    } else if (active_ == Source::Demo) {
        demo_.SetVolume(volume);
        if (demo_.Finished() && !switching_) {
            demo_.Start(now);
            envelope_.FadeIn(now);
        }
    }
}

void MenuMusicPreview::Shutdown() {
    StopActive();
    song_.Shutdown();
    staged_.Shutdown();
    demo_.Shutdown();
    loadedPath_.clear();
    pendingPath_.clear();
    switching_ = false;
}

} // namespace locus::app
