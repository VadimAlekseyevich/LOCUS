#pragma once

#include "app/chart_playback.h"
#include "app/direction_input_events.h"
#include "app/global_offset_calibrator.h"
#include "app/hit_flash.h"
#include "app/demo_transport.h"
#include "core/judgement.h"
#include "core/play_mods.h"

#include <array>
#include <optional>
#include <utility>

namespace locus::app {

// Audio time and event index for scored practice from the editor playhead.
struct EditorTestOrigin {
    double audioSeconds = 0.0;
    int firstScoredBeat = 0;
};

struct ArrowInput {
    // Up, Right, Down, Left, as in core::Heading.
    std::array<bool, 4> pressed{};
    std::array<double, 4> capturedAt{};
    std::array<bool, 4> held{};
    const DirectionKeyBatch* events = nullptr;
    bool precise = false;
};

// Coordinates the actual Play session, not the UI screens. Non-owning refs:
// the application keeps the current chart, audio resources and judge alive.
// Chart ownership, chart editing and screen transitions remain outside.
class GameSession {
public:
    GameSession(const core::Chart& chart, core::TurnJudge& judge,
                DemoTransport& demo, ChartPlayback& local) noexcept
        : chart_(chart), judge_(judge), demo_(demo), local_(local) {}

    void SetTestOrigin(EditorTestOrigin origin) noexcept { testOrigin_ = origin; }
    void ClearTestOrigin() noexcept { testOrigin_.reset(); }
    bool IsEditorTest() const noexcept { return testOrigin_.has_value(); }
    // The caller validates replay identity and reproduced result before
    // activating this mode. Live directions never reach the judge in it.
    void SetReplay(core::ReplayTrace trace) {
        replay_ = std::move(trace);
        replayCursor_.Reset();
        ClearTestOrigin();
    }
    void ClearReplay() noexcept { replay_.reset(); replayCursor_.Reset(); }
    bool IsReplay() const noexcept { return replay_.has_value(); }

    void Start(bool customChart, double now);
    void Restart(bool customChart, double now);
    void Stop(bool customChart);
    void TogglePause(bool customChart, double now);
    void SetMods(const core::PlayMods& mods) noexcept {
        SetRatePercent(mods.RatePercent());
        modFlags_ = core::ValidModFlags(mods.flags) ? mods.flags : 0;
    }
    void SetRatePercent(int percent) noexcept {
        if (core::ValidModRate(percent)) ratePercent_ = percent;
    }
    // New attempts are always fixed-rate. Historical result summaries can
    // still contain the legacy variable-rate flag.
    bool VariablePracticeRate() const noexcept { return variablePracticeRate_; }
    void SetGlobalOffsetMs(int milliseconds) noexcept {
        globalOffsetMs_ = milliseconds;
    }
    int GlobalOffsetMs() const noexcept { return globalOffsetMs_; }
    double CalibratedTime(double songSeconds,
                          double rate = 1.0) const noexcept {
        return CalibratedSongSeconds(songSeconds, globalOffsetMs_, rate);
    }
    void Update(bool customChart, double now, const ArrowInput& arrows);
    const HitFlash& Flash() const noexcept { return hitFlash_; }
    unsigned TakeHitSoundCount() noexcept { return std::exchange(hitSoundCount_,0u); }

private:
    void UpdateReplay(bool customChart, double now);
    const core::Chart& chart_;
    core::TurnJudge& judge_;
    DemoTransport& demo_;
    ChartPlayback& local_;
    std::optional<EditorTestOrigin> testOrigin_;
    HitFlash hitFlash_;
    unsigned hitSoundCount_=0;
    int globalOffsetMs_ = 0;
    bool variablePracticeRate_ = false;
    int ratePercent_ = 100;
    core::ModFlags modFlags_ = 0;
    std::optional<core::ReplayTrace> replay_;
    core::ReplayCursor replayCursor_;
    double replayFinishSeconds_ = 0.0;

    // Audio is allowed to end before the last note's late 180ms window.
    // In that case, scoring continues for a brief SILENT tail using the
    // monotonic clock, so a final miss is not assigned prematurely.
    double audioEndedAt_ = -1.0;
    double audioEndSongSeconds_ = 0.0;
};

} // namespace locus::app
