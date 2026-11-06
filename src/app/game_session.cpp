#include "app/game_session.h"

#include "core/route.h"

#include <algorithm>

namespace locus::app {

void GameSession::Start(bool customChart, double now) {
    chord_.Reset();
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
    chord_.Reset();
    hitFlash_.Reset();
    audioEndedAt_ = -1.0;
    audioEndSongSeconds_ = 0.0;
    if (customChart) local_.Stop();
    else demo_.Stop();
}

void GameSession::TogglePause(bool customChart, double now) {
    chord_.Reset();
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
    // During the silent tail, allow late directional input up to the
    // standard judgement window after the final chart note.
    if ((playing && !paused) || audioAlreadyFinished) {
        if (const auto* target = judge_.NextEvent()) {
            const core::Heading incoming = core::IncomingHeadingAt(chart_, target->beat);
            const core::Heading expected =
                core::HeadingAfterTurn(incoming, target->direction);
            const auto stroke = chord_.Observe(
                {arrows.pressed, arrows.held, arrows.capturedAt},
                expected, incoming);
            if (stroke) {
                const double timestamp = audioEndedAt_ >= 0.0
                    ? audioEndSongSeconds_ +
                        std::max(0.0, stroke->capturedAt - audioEndedAt_) *
                            (customChart ? local_.Speed() : 1.0)
                    : (customChart ? local_.AudioSeconds(stroke->capturedAt)
                                   : demo_.SongSeconds(stroke->capturedAt));
                const auto judgement = judge_.PressHeading(stroke->outgoing, timestamp);
                if (judgement && !judgement->wrongDirection &&
                    judgement->grade != core::Grade::Miss)
                    hitFlash_.Trigger(judgement->grade, now);
            }
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
