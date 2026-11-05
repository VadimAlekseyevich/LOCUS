#include "app/game_session.h"

#include "core/route.h"

namespace locus::app {

void GameSession::Start(bool customChart, double now) {
    judge_.Reset();
    if (customChart) {
        if (testOrigin_) {
            judge_.ResetFromBeat(testOrigin_->firstScoredBeat);
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
    if (customChart) local_.Stop();
    else demo_.Stop();
}

void GameSession::TogglePause(bool customChart, double now) {
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
    if (playing && !paused) {
        if (const auto* target = judge_.NextEvent()) {
            // Convert absolute arrow key presses into relative turns using
            // the *incoming* direction at the scheduled event.
            const auto incoming = core::IncomingHeadingAt(chart_, target->beat);
            const auto turnFor = [&](int direction) {
                return core::ArrowToTurn(incoming,
                                         static_cast<core::Heading>(direction));
            };
            int validCount = 0;
            int first = -1;
            for (int index = 0; index < 4; ++index) {
                if (arrows.pressed[index] && turnFor(index)) {
                    ++validCount;
                    if (first < 0) first = index;
                }
            }
            if (validCount > 1) {
                const auto wrong = target->direction == core::Turn::Left
                    ? core::Turn::Right : core::Turn::Left;
                judge_.Press(wrong, customChart ? local_.AudioSeconds(now)
                                               : demo_.SongSeconds(now));
            } else if (first >= 0) {
                judge_.Press(*turnFor(first),
                             customChart ? local_.AudioSeconds(arrows.capturedAt[first])
                                         : demo_.SongSeconds(arrows.capturedAt[first]));
            }
        }
    }
    if (customChart) local_.Update(now);
    else demo_.Update(now);
    const double songTime = customChart ? local_.AudioSeconds(now)
                                         : demo_.SongSeconds(now);
    if (customChart || demo_.Ready()) judge_.Advance(songTime);
}

} // namespace locus::app
