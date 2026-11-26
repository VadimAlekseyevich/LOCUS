#include "app/game_session.h"
#include "app/play_intro_skip.h"
#include "app/sampled_song_time.h"

#include "core/route.h"

#include <algorithm>

namespace locus::app {

void GameSession::Start(bool customChart, double now) {
    variablePracticeRate_ = false;
    hitFlash_.Reset();
    hitSoundCount_=0;
    audioEndedAt_ = -1.0;
    audioEndSongSeconds_ = 0.0;
    judge_.SetMods(testOrigin_ ? 0 : replay_ ? replay_->modFlags : modFlags_);
    const int percent = replay_ ? replay_->ratePercent : ratePercent_;
    if (!testOrigin_) {
        if (customChart) local_.SetSpeed(percent / 100.0, now);
        else demo_.SetSpeed(percent / 100.0, now);
    }
    if (replay_) {
        judge_.SetOffsetSeconds(replay_->offsetSeconds);
        replayCursor_.Reset();
        replayFinishSeconds_ = chart_.turns.empty() ? 0.0 :
            core::EventTimeMs(chart_,chart_.turns.back(),replay_->offsetSeconds)/1000.0 +
                judge_.BadWindowSeconds();
    }
    if (customChart) {
        if (testOrigin_) {
            judge_.ResetFromAudioSeconds(
                CalibratedTime(testOrigin_->audioSeconds, local_.Speed()));
            local_.StartAtAudioSeconds(testOrigin_->audioSeconds, now);
        } else {
            // The chart may put its first turn at the very start of the MP3.
            // Reuse the same lead-in as intro skipping, but allow it to be
            // negative so the listener has time to orient before audio zero.
            // This also applies to Restart and Results -> retry.
            local_.StartAtAudioSeconds(
                ScoredChartStartSeconds(chart_, local_.OffsetSeconds()), now);
        }
    } else {
        demo_.Start(now);
    }
    if (!testOrigin_ && !replay_) judge_.BeginReplayCapture(percent);
}

void GameSession::Restart(bool customChart, double now) {
    Start(customChart, now);
}

void GameSession::Stop(bool customChart) {
    hitFlash_.Reset();
    hitSoundCount_=0;
    audioEndedAt_ = -1.0;
    audioEndSongSeconds_ = 0.0;
    if (customChart) local_.Stop();
    else demo_.Stop();
}

void GameSession::TogglePause(bool customChart, double now) {
    hitFlash_.Reset();
    hitSoundCount_=0;
    if (customChart) {
        if (local_.Paused()) local_.Resume(now);
        else local_.Pause(now);
    } else {
        if (demo_.Paused()) demo_.Resume(now);
        else demo_.Pause(now);
    }
}

void GameSession::UpdateReplay(bool customChart, double now) {
    if (customChart ? local_.Paused() : demo_.Paused()) return;
    if (customChart) local_.Update(now);
    else demo_.Update(now);
    const bool finished = customChart ? local_.Finished() : demo_.Finished();
    const bool playing = customChart ? local_.Playing() : demo_.Playing();
    if (!playing && !finished) return;
    const double rate = customChart ? local_.Speed() : demo_.Speed();
    double songSeconds = customChart ? local_.AudioSeconds(now) : demo_.SongSeconds(now);
    if (finished) {
        if (audioEndedAt_ < 0.0) {
            audioEndedAt_ = now;
            audioEndSongSeconds_ = songSeconds;
        }
        songSeconds = audioEndSongSeconds_ + std::max(0.0,now-audioEndedAt_)*rate;
    }
    // Scoring always uses the recorded event time. This clock only decides
    // when to display its effect, including the same silent tail as PLAY.
    // A stalled original frame can contain a very late expiry; drain after
    // the bounded tail rather than trapping a viewer in silence indefinitely.
    const double cursorSeconds = CalibratedTime(songSeconds,rate);
    const bool drain = finished && songSeconds-audioEndSongSeconds_ >= 2.0;
    while (const auto* event = replayCursor_.NextDue(
               *replay_,cursorSeconds,replayFinishSeconds_,drain)) {
        const auto hit = core::ApplyReplayEvent(judge_,*event);
        if (hit && !hit->wrongDirection && hit->grade!=core::Grade::Miss)
            { hitFlash_.Trigger(*hit,now); ++hitSoundCount_; }
    }
}

void GameSession::Update(bool customChart, double now, const ArrowInput& arrows) {
    if (replay_) {
        UpdateReplay(customChart,now);
        return;
    }
    const bool playing = customChart ? local_.Playing() : demo_.Playing();
    const bool paused = customChart ? local_.Paused() : demo_.Paused();
    const bool audioAlreadyFinished = customChart ? local_.Finished() : demo_.Finished();
    // Sample the actual audio cursor ONCE for this input batch. Reading
    // GetMusicTimePlayed for each historical key timestamp returns the same
    // *current* cursor; projecting from this sample preserves inter-key
    // spacing even if a decoder only exposes buffer-stepped positions.
    const double playbackRate = customChart ? local_.Speed() : demo_.Speed();
    const SampledSongTime inputSongTime{
        now, customChart ? local_.AudioSeconds(now) : demo_.SongSeconds(now),
        playbackRate
    };
    // An event stream is authoritative when the OS backend is present:
    // don't also score raylib's frame snapshot for the same keydown.
    const bool freshInput = arrows.precise
        ? (arrows.events && std::any_of(
            arrows.events->events.begin(),
            arrows.events->events.begin() + arrows.events->size,
            [](const DirectionKeyEvent& e) { return e.pressed; }))
        : std::any_of(arrows.pressed.begin(), arrows.pressed.end(),
                      [](bool pressed) { return pressed; });
    // A key used to skip the intro is consumed, not judged as a note.
    if (playing && !paused && !chart_.turns.empty() &&
        judge_.NextEvent() == &chart_.turns.front() &&
        judge_.Counts().Resolved() == 0 && freshInput) {
        const double current = inputSongTime.songSeconds;
        const double offset = customChart ? local_.OffsetSeconds() : 0.0;
        if (const auto target = IntroSkipDestination(chart_, offset, current)) {
            const bool seeked = customChart
                ? local_.StartAtAudioSeconds(*target, now)
                : demo_.StartAtAudioSeconds(*target, now);
            if (seeked) {
                hitFlash_.Reset();
                hitSoundCount_=0;
                if (customChart) local_.Update(now);
                else demo_.Update(now);
                judge_.Advance(CalibratedTime(*target, customChart ? local_.Speed() : demo_.Speed()));
                return;
            }
        }
    }
    // During the silent tail, allow late directional input up to the
    // standard judgement window after the final chart note.
    if ((playing && !paused) || audioAlreadyFinished) {
        const auto score = [&](core::Heading direction, double capturedAt) {
            const double timestamp = audioEndedAt_ >= 0.0
                ? audioEndSongSeconds_ +
                    std::max(0.0, capturedAt - audioEndedAt_) * playbackRate
                : inputSongTime.At(capturedAt);
            const auto judgement = judge_.PressHeading(
                direction, CalibratedTime(timestamp, playbackRate));
            if (judgement && !judgement->wrongDirection &&
                judgement->grade != core::Grade::Miss)
                { hitFlash_.Trigger(*judgement, now); ++hitSoundCount_; }
        };
        if (arrows.precise) {
            if (arrows.events) {
                for (std::size_t i = 0; i < arrows.events->size; ++i) {
                    const auto& event = arrows.events->events[i];
                    if (event.pressed) score(event.arrow, event.capturedAt);
                }
            }
        } else {
            constexpr core::Heading directions[4] = {
                core::Heading::Up, core::Heading::Right,
                core::Heading::Down, core::Heading::Left
            };
            for (std::size_t i = 0; i < arrows.pressed.size(); ++i)
                if (arrows.pressed[i])
                    score(directions[i], arrows.capturedAt[i]);
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
            judge_.Advance(CalibratedTime(
                audioEndSongSeconds_ +
                std::max(0.0, now - audioEndedAt_) *
                    (customChart ? local_.Speed() : demo_.Speed()),
                customChart ? local_.Speed() : demo_.Speed()));
        } else {
            judge_.Advance(CalibratedTime(
                songTime, customChart ? local_.Speed() : demo_.Speed()));
        }
    }
}

} // namespace locus::app
