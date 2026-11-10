#include "app/game_session.h"
#include "app/play_intro_skip.h"

#include "core/route.h"

#include <algorithm>

namespace locus::app {

void GameSession::Start(bool customChart, double now) {
    hitFlash_.Reset();
    audioEndedAt_ = -1.0;
    audioEndSongSeconds_ = 0.0;
    judge_.Reset();
    if (customChart) {
        if (testOrigin_) {
            judge_.ResetFromAudioSeconds(testOrigin_->audioSeconds);
            local_.StartAtAudioSeconds(testOrigin_->audioSeconds, now);
        } else {
            local_.Start(now);
        }
    } else {
        demo_.Start(now);
    }
}

void GameSession::Restart(bool customChart, double now) {
    Start(customChart, now);
}

void GameSession::Stop(bool customChart) {
    hitFlash_.Reset();
    audioEndedAt_ = -1.0;
    audioEndSongSeconds_ = 0.0;
    if (customChart) local_.Stop();
    else demo_.Stop();
}

void GameSession::TogglePause(bool customChart, double now) {
    hitFlash_.Reset();
    if (customChart) {
        if (local_.Paused()) local_.Resume(now);
        else local_.Pause(now);
    } else {
        if (demo_.Paused()) demo_.Resume(now);
        else demo_.Pause(now);
    }
}

void GameSession::SetPracticeSpeed(bool customChart, double speed, double now) {
    if (customChart) local_.SetSpeed(speed, now);
}

void GameSession::Update(bool customChart, double now, const ArrowInput& arrows) {
    const bool playing = customChart ? local_.Playing() : demo_.Playing();
    const bool paused = customChart ? local_.Paused() : demo_.Paused();
    const bool audioAlreadyFinished = customChart ? local_.Finished() : demo_.Finished();
    // Only a *fresh* gameplay direction press can skip a long empty intro.
    // The seek moves both audio and visual clocks. Consume this key press:
    // it is not an attempt to hit the upcoming note.
    if (playing && !paused && !chart_.turns.empty() &&
        judge_.NextEvent() == &chart_.turns.front() &&
        judge_.Counts().Resolved() == 0 &&
        std::any_of(arrows.pressed.begin(), arrows.pressed.end(),
                    [](bool pressed) { return pressed; })) {
        const double current = customChart ? local_.AudioSeconds(now)
                                           : demo_.SongSeconds(now);
        const double offset = customChart ? local_.OffsetSeconds() : 0.0;
        if (const auto target = IntroSkipDestination(chart_, offset, current)) {
            const bool seeked = customChart
                ? local_.StartAtAudioSeconds(*target, now)
                : demo_.StartAtAudioSeconds(*target, now);
            if (seeked) {
                hitFlash_.Reset();
                if (customChart) local_.Update(now);
                else demo_.Update(now);
                judge_.Advance(*target);
                return;
            }
        }
    }
    // During the silent tail, allow late directional input up to the
    // standard judgement window after the final chart note.
    if ((playing && !paused) || audioAlreadyFinished) {
        // Each fresh physical arrow is a separate note input. Two keys
        // sharing a diagonal target are graded at their own key-down times.
        constexpr core::Heading directions[4] = {
            core::Heading::Up, core::Heading::Right,
            core::Heading::Down, core::Heading::Left
        };
        for (std::size_t i = 0; i < arrows.pressed.size(); ++i) {
            if (!arrows.pressed[i]) continue;
            const double capturedAt = arrows.capturedAt[i];
            const double timestamp = audioEndedAt_ >= 0.0
                ? audioEndSongSeconds_ +
                    std::max(0.0, capturedAt - audioEndedAt_) *
                        (customChart ? local_.Speed() : 1.0)
                : (customChart ? local_.AudioSeconds(capturedAt)
                               : demo_.SongSeconds(capturedAt));
            const auto judgement = judge_.PressHeading(directions[i], timestamp);
            if (judgement && !judgement->wrongDirection &&
                judgement->grade != core::Grade::Miss)
                hitFlash_.Trigger(judgement->grade, now);
        }
    }
    if (customChart) local_.Update(now);
    else demo_.Update(now);
    const double songTime = customChart ? local_.AudioSeconds(now)
                                         : demo_.SongSeconds(now);
    if (customChart || demo_.Ready()) {
        const bool finished = customChart ? local_.Finished() : demo_.Finished();
        if (finished) {
            if (audioEndedAt_ < 0.0) {
                audioEndedAt_ = now;
                audioEndSongSeconds_ = songTime;
            }
            judge_.Advance(audioEndSongSeconds_ +
                           std::max(0.0, now - audioEndedAt_) *
                               (customChart ? local_.Speed() : 1.0));
        } else {
            judge_.Advance(songTime);
        }
    }
}

} // namespace locus::app
